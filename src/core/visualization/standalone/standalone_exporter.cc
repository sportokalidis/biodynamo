// -----------------------------------------------------------------------------
//
// Copyright (C) 2021 CERN & University of Surrey for the benefit of the
// BioDynaMo collaboration. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
//
// See the LICENSE file distributed with this work for details.
// See the NOTICE file distributed with this work for additional information
// regarding copyright ownership.
//
// -----------------------------------------------------------------------------

#include "core/visualization/standalone/standalone_exporter.h"

#include <cstring>
#include <fstream>
#include <regex>
#include <sstream>
#include <vector>

// ROOT reflection — used only by ReadScalarMember to locate data member
// offsets at runtime without a compile-time dependency on user agent types.
#include "TClass.h"
#include "TDataMember.h"

#include "core/agent/agent.h"           // Agent base class, GetPosition(), GetUid()
#include "core/diffusion/diffusion_grid.h"  // DiffusionGrid, GetAllConcentrations()
#include "core/param/param.h"           // Param::VisualizeDiffusion
#include "core/resource_manager.h"      // ForEachAgent(), GetDiffusionGrid()
#include "core/simulation.h"            // Simulation::GetActive()
#include "core/util/thread_info.h"      // GetMaxThreads()

namespace bdm {


// VTK type string and element byte size for one scalar C++ data member.
struct NativeType { const char* vtk; size_t size; };

static NativeType GetNativeType(const std::string& cpp_type) {
  if (cpp_type == "float")                                        return {"Float32", 4};
  if (cpp_type == "int"   || cpp_type == "Int_t")                return {"Int32",   4};
  if (cpp_type == "unsigned int" || cpp_type == "UInt_t")        return {"UInt32",  4};
  if (cpp_type == "short" || cpp_type == "Short_t")              return {"Int16",   2};
  if (cpp_type == "unsigned short" || cpp_type == "UShort_t")    return {"UInt16",  2};
  if (cpp_type == "uint64_t" || cpp_type == "unsigned long"
                             || cpp_type == "unsigned long long"
                             || cpp_type == "ULong64_t")         return {"UInt64",  8};
  if (cpp_type == "int64_t"  || cpp_type == "long long"
                             || cpp_type == "Long64_t")          return {"Int64",   8};
  return {"Float64", 8};  // double or unknown — safe fallback
}

// Copy elem_size raw bytes of field `name` from agent into `out`.
// Requires the same TDataMember lookup as ReadScalarMember but stores the
// value in its native binary representation instead of converting to double.
static void ReadRawMember(const Agent* agent, const std::string& name,
                          char* out, size_t elem_size) {
  TClass*      cls = TClass::GetClass(typeid(*agent));
  TDataMember* dm  = cls ? cls->GetDataMember(name.c_str()) : nullptr;
  if (dm) {
    std::memcpy(out,
                reinterpret_cast<const char*>(agent) + dm->GetOffset(),
                elem_size);
  } else {
    std::memset(out, 0, elem_size);
  }
}

// -----------------------------------------------------------------------------
/// Parse bdm.toml to extract scalar member names from additional_data_members.
///
/// Called once from the constructor; the result is cached in extra_members_.
/// A hand-rolled regex parser is used to avoid adding a TOML library dependency.
static std::vector<std::string> LoadAdditionalMembers() {
  std::vector<std::string> names;
  std::ifstream ifs("bdm.toml");
  if (!ifs) return names;

  std::string line;
  // Matches: additional_data_members = ["name1", "name2", ...]
  std::regex re(R"(additional_data_members\s*=\s*\[([^\]]+)\])");
  while (std::getline(ifs, line)) {
    std::smatch m;
    if (std::regex_search(line, m, re)) {
      std::string inner = m[1].str();
      std::regex name_re("\"([^\"]+)\"");
      for (std::sregex_iterator it(inner.begin(), inner.end(), name_re), end;
           it != end; ++it)
        names.push_back((*it)[1].str());
    }
  }
  return names;
}

// -----------------------------------------------------------------------------
StandaloneExporter::StandaloneExporter(const std::string& output_dir)
    : output_dir_(output_dir),
      // Load member names once at construction; bdm.toml does not change at runtime.
      extra_members_(LoadAdditionalMembers()) {}

// -----------------------------------------------------------------------------
StandaloneExporter::~StandaloneExporter() = default;

// -----------------------------------------------------------------------------
/// Export all agents for the current simulation step.
///
/// Each agent is represented as a VTK_VERTEX cell (type 1): a degenerate
/// zero-dimensional cell with exactly one node.  This means
///   NumberOfPoints == NumberOfCells
/// and the connectivity array is the trivial identity [0, 1, 2, …, n-1].
///
/// All per-agent attributes are stored as PointData because VTK_VERTEX cells
/// collapse point and cell concepts — each "cell" is a single labelled point.
///
/// Fields exported:
///   Cell_ID   — always (unique 64-bit identifier)
///   Diameter  — always (fundamental geometry)
///   <extra>   — user-listed additional_data_members from bdm.toml (cached
///               in extra_members_; no repeated file read per step).
///   Mass, Volume, TractionForce are NOT exported by default; users who need
///   them can list "mass_", "volume_" etc. in additional_data_members.
///
/// The agent list is partitioned into equal-sized slices; each OpenMP thread
/// writes exactly one .vtu piece file.  All data arrays are pre-extracted into
/// flat contiguous vectors before the parallel loop so each thread works on
/// read-only slices — no mutexes needed.
///
/// step_ is NOT incremented here.  WriteDiffusionStep() increments it after
/// writing diffusion files so both file families share the same suffix N.
void StandaloneExporter::WriteStep() {
  auto* sim = Simulation::GetActive();
  auto* rm  = sim->GetResourceManager();

  // ── Collect per-agent data into flat arrays ──────────────────────────────
  //
  // Flat vectors (not vector-of-structs) allow each parallel thread to slice
  // out its range with a single pointer arithmetic operation.
  std::vector<Agent*>   agents;
  std::vector<double>   points;  // interleaved x,y,z for all agents
  std::vector<uint64_t> ids;     // unique agent identifier (UID index)

  rm->ForEachAgent([&](Agent* a) {
    agents.push_back(a);
    const auto& pos = a->GetPosition();
    points.push_back(pos[0]);
    points.push_back(pos[1]);
    points.push_back(pos[2]);
    ids.push_back(a->GetUid().GetIndex());
  });

  const size_t n = ids.size();

  // ── Extract Diameter ──────────────────────────────────────────────────────
  std::vector<double> diam(n);
  for (size_t i = 0; i < n; ++i)
    diam[i] = agents[i]->GetDiameter();

  // ── Read user-defined extra scalar fields via ROOT reflection ─────────────
  //
  // extra_members_ was loaded from bdm.toml once in the constructor; no file
  // I/O happens here.  Each field is stored in its native binary type so that
  // the VTU file uses the correct VTK type (Int32 for int fields, Float32 for
  // float, etc.) rather than widening everything to Float64.
  struct ExtraField {
    NativeType          type;           // vtk type name + element byte size
    std::vector<char>   data;           // n * type.size raw bytes
  };
  std::vector<ExtraField> extra_fields(extra_members_.size());
  if (n > 0) {
    for (size_t f = 0; f < extra_members_.size(); ++f) {
      // Resolve type from the first agent; all agents share the same class.
      TClass*      cls = TClass::GetClass(typeid(*agents[0]));
      TDataMember* dm  = cls ? cls->GetDataMember(extra_members_[f].c_str())
                             : nullptr;
      extra_fields[f].type = dm ? GetNativeType(dm->GetTypeName())
                                : NativeType{"Float64", 8};
      extra_fields[f].data.resize(n * extra_fields[f].type.size, 0);
      for (size_t i = 0; i < n; ++i)
        ReadRawMember(agents[i], extra_members_[f],
                      extra_fields[f].data.data() + i * extra_fields[f].type.size,
                      extra_fields[f].type.size);
    }
  }

  // ── Compute piece partition ───────────────────────────────────────────────
  auto*    tinfo      = ThreadInfo::GetInstance();
  uint64_t max_threads = tinfo->GetMaxThreads();
  uint64_t num_pieces  = std::max<uint64_t>(1, std::min<uint64_t>(max_threads, n));
  uint64_t per_piece   = (n + num_pieces - 1) / num_pieces;  // ceiling division

  // ── Write one .vtu file per piece in parallel ─────────────────────────────
  #pragma omp parallel for schedule(static, 1)
  for (uint64_t p = 0; p < num_pieces; ++p) {
    uint64_t begin = p * per_piece;
    if (begin >= n) continue;
    uint64_t count = std::min<uint64_t>(per_piece, n - begin);

    std::ostringstream vtu_name;
    vtu_name << output_dir_ << "/agents_" << step_ << "_p" << p << ".vtu";
    std::ofstream vtu(vtu_name.str(), std::ios::binary);

    // header_type="UInt32" means each appended array is preceded by a 4-byte
    // length field (not 8-byte), keeping the per-array overhead minimal.
    vtu << "<?xml version=\"1.0\"?>\n";
    vtu << "<VTKFile type=\"UnstructuredGrid\" version=\"0.1\""
           " byte_order=\"LittleEndian\" header_type=\"UInt32\">\n";
    vtu << "  <UnstructuredGrid>\n";
    // NumberOfCells=0: VTK_VERTEX topology is trivially the identity and adds
    // no information.  ParaView renders point clouds correctly without it.
    vtu << "    <Piece NumberOfPoints=\"" << count
        << "\" NumberOfCells=\"0\">\n";
    vtu << "      <PointData>\n";

    // The VTK "appended raw" format separates metadata (XML tags with offsets)
    // from binary payloads.  All byte offsets must be known before writing
    // any XML, so we collect (tag, size, data_ptr) entries first.
    // offset is the byte position of the 4-byte length prefix in the binary
    // block; the actual payload starts at offset+4.
    struct ArrayMeta { std::string tag; uint32_t nbytes; const char* data; };
    std::vector<ArrayMeta> metas;
    uint32_t offset = 0;

    auto append_array = [&](const std::string& type, const std::string& name,
                            int components, const void* buf,
                            size_t elems, size_t elem_size) {
      uint32_t bytes = static_cast<uint32_t>(elems * components * elem_size);
      std::ostringstream tag;
      tag << "        <DataArray type=\"" << type << "\" Name=\"" << name
          << "\" NumberOfComponents=\"" << components
          << "\" format=\"appended\" offset=\"" << offset << "\"/>\n";
      metas.push_back({tag.str(), bytes, reinterpret_cast<const char*>(buf)});
      offset += 4 + bytes;
    };

    // Slices into pre-extracted flat arrays — no data is copied.
    append_array("UInt64",  "Cell_ID",  1, &ids[begin],  count, sizeof(uint64_t));
    append_array("Float64", "Diameter", 1, &diam[begin], count, sizeof(double));
    for (size_t f = 0; f < extra_fields.size(); ++f) {
      const auto& ef = extra_fields[f];
      append_array(ef.type.vtk, extra_members_[f], 1,
                   ef.data.data() + begin * ef.type.size,
                   count, ef.type.size);
    }

    for (auto& m : metas) vtu << m.tag;
    vtu << "      </PointData>\n";

    vtu << "      <Points>\n";
    {
      std::ostringstream tag;
      tag << "        <DataArray type=\"Float64\" NumberOfComponents=\"3\""
             " format=\"appended\" offset=\"" << offset << "\"/>\n";
      vtu << tag.str();
      metas.push_back({"", static_cast<uint32_t>(count * 3 * sizeof(double)),
                       reinterpret_cast<const char*>(&points[begin * 3])});
      offset += 4 + static_cast<uint32_t>(count * 3 * sizeof(double));
    }
    vtu << "      </Points>\n";
    vtu << "      <Cells/>\n";

    vtu << "    </Piece>\n";
    vtu << "  </UnstructuredGrid>\n";

    // Each array in the AppendedData block: [4-byte UInt32 length][raw bytes].
    // Offsets in the XML header are byte positions relative to the character
    // immediately after the sentinel "_".
    vtu << "  <AppendedData encoding=\"raw\">\n_";
    for (auto& m : metas) {
      uint32_t sz = m.nbytes;
      vtu.write(reinterpret_cast<const char*>(&sz), sizeof(uint32_t));
      vtu.write(m.data, sz);
    }
    vtu << "\n  </AppendedData>\n</VTKFile>\n";
    vtu.close();
  }

  // step_ is NOT incremented here — WriteDiffusionStep() does it after both
  // file families are written so agents_{N} and diffusion_{name}_{N} always
  // share the same N.
  std::vector<std::pair<std::string, std::string>> extra_type_info;
  for (size_t f = 0; f < extra_members_.size(); ++f)
    extra_type_info.emplace_back(extra_members_[f],
                                 n > 0 ? extra_fields[f].type.vtk : "Float64");
  WritePvtu(static_cast<int>(num_pieces), extra_type_info);
}

// -----------------------------------------------------------------------------
/// Write the parallel agent index file (agents_{step}.pvtu).
///
/// Mirrors the PointData schema from the piece files so that ParaView can
/// discover available fields without loading every piece.
/// Uses extra_members_ (cached at construction) — no file I/O per call.
void StandaloneExporter::WritePvtu(
    int pieces,
    const std::vector<std::pair<std::string, std::string>>& extra_type_info)
    const {
  std::ostringstream name;
  name << output_dir_ << "/agents_" << step_ << ".pvtu";
  std::ofstream pvtu(name.str());

  pvtu << "<?xml version=\"1.0\"?>\n";
  pvtu << "<VTKFile type=\"PUnstructuredGrid\" version=\"0.1\""
          " byte_order=\"LittleEndian\">\n";
  pvtu << "  <PUnstructuredGrid>\n";

  pvtu << "    <PPointData>\n";
  pvtu << "      <PDataArray type=\"UInt64\"  Name=\"Cell_ID\""
          " NumberOfComponents=\"1\"/>\n";
  pvtu << "      <PDataArray type=\"Float64\" Name=\"Diameter\""
          " NumberOfComponents=\"1\"/>\n";
  for (const auto& fi : extra_type_info)
    pvtu << "      <PDataArray type=\"" << fi.second << "\" Name=\"" << fi.first
         << "\" NumberOfComponents=\"1\"/>\n";
  pvtu << "    </PPointData>\n";

  pvtu << "    <PPoints>\n";
  pvtu << "      <PDataArray type=\"Float64\" NumberOfComponents=\"3\"/>\n";
  pvtu << "    </PPoints>\n";

  for (int i = 0; i < pieces; ++i)
    pvtu << "    <Piece Source=\"agents_" << step_ << "_p" << i << ".vtu\"/>\n";

  pvtu << "  </PUnstructuredGrid>\n</VTKFile>\n";
  pvtu.close();
}

// -----------------------------------------------------------------------------
/// Dispatch diffusion export to the VTI (ImageData) implementation and
/// increment the step counter.
///
/// step_ is incremented here (not in WriteStep) so that agents_{N}_*.vtu and
/// diffusion_{name}_{N}_*.vti always share the same N for the same export.
/// To switch to the VTK_VOXEL UnstructuredGrid output, replace the
/// WriteDiffusionStepVti() call below with WriteDiffusionStepVtu().
void StandaloneExporter::WriteDiffusionStep() {
  WriteDiffusionStepVti();
  step_++;
}

// -----------------------------------------------------------------------------
/// Export diffusion grids as VTK ImageData (.vti / .pvti).
///
/// Each diffusion substance in [[visualize_diffusion]] is written as a
/// parallel ImageData dataset.  The grid is encoded with three scalars:
/// Origin at the grid corner, Spacing = box length, and WholeExtent
/// 0..N in each dimension (N+1 nodes, N cells).  No explicit geometry arrays
/// (Points or Cells) are written.
///
/// Data is stored as CellData — one value per diffusion box, matching
/// BioDynaMo's cell-centred finite-difference storage.
///
/// Z-slab parallelism: the grid is partitioned horizontally; each OpenMP
/// thread writes one .vti piece file independently.
///
/// Piece extent convention: pieces cover non-overlapping cell ranges.
/// Piece p covers cells k_begin .. k_begin+k_len-1, expressed in node
/// indices as extent k_begin .. k_begin+k_len.
void StandaloneExporter::WriteDiffusionStepVti() {
  auto* sim   = Simulation::GetActive();
  auto* rm    = sim->GetResourceManager();
  auto* param = sim->GetParam();

  for (const auto& vd : param->visualize_diffusion) {
    auto* grid = rm->GetDiffusionGrid(vd.name);
    if (!grid) continue;

    // ── Grid geometry ────────────────────────────────────────────────────
    const auto num_boxes = grid->GetNumBoxesArray();   // {nx, ny, nz}
    const auto dims      = grid->GetDimensions();       // {xmin,xmax,ymin,ymax,zmin,zmax}
    const auto box       = grid->GetBoxLength();

    const size_t nx = static_cast<size_t>(num_boxes[0]);
    const size_t ny = static_cast<size_t>(num_boxes[1]);
    const size_t nz = static_cast<size_t>(num_boxes[2]);

    // Origin at the grid corner (first node of the lattice).
    const double ox = dims[0];
    const double oy = dims[2];
    const double oz = dims[4];

    // Zero-copy pointers into the grid's own concentration/gradient arrays.
    // The DiffusionGrid object outlives all write calls in this loop.
    const real_t* conc = grid->GetAllConcentrations();
    const real_t* grad = grid->GetAllGradients();

    // ── Partition the Z dimension into slabs ─────────────────────────────
    auto*    tinfo           = ThreadInfo::GetInstance();
    uint64_t max_threads     = tinfo->GetMaxThreads();
    uint64_t num_pieces      = std::min<uint64_t>(std::max<uint64_t>(1, nz),
                                                  std::max<uint64_t>(1, max_threads));
    uint64_t boxes_per_piece = (nz + num_pieces - 1) / num_pieces;  // ceil

    const char* rt = (sizeof(real_t) == 4) ? "Float32" : "Float64";

    #pragma omp parallel for schedule(static, 1)
    for (uint64_t p = 0; p < num_pieces; ++p) {
      uint64_t k_begin = p * boxes_per_piece;
      if (k_begin >= nz) continue;
      uint64_t k_len = std::min<uint64_t>(boxes_per_piece, nz - k_begin);

      // CellData pieces are non-overlapping: node extent k_begin .. k_begin+k_len
      // represents exactly k_len cells.  No shared-boundary convention needed.
      const uint64_t k_end      = k_begin + k_len;
      const uint64_t piece_cells = static_cast<uint64_t>(nx) *
                                   static_cast<uint64_t>(ny) * k_len;

      std::ostringstream vti_name;
      vti_name << output_dir_ << "/diffusion_" << vd.name << "_"
               << step_ << "_p" << p << ".vti";
      std::ofstream vti(vti_name.str(), std::ios::binary);

      vti << "<?xml version=\"1.0\"?>\n";
      vti << "<VTKFile type=\"ImageData\" version=\"0.1\""
             " byte_order=\"LittleEndian\" header_type=\"UInt32\">\n";
      // WholeExtent 0..N means N+1 nodes → N cells per dimension.
      vti << "  <ImageData"
          << " WholeExtent=\"0 " << nx << " 0 " << ny
          << " 0 " << nz << "\""
          << " Origin=\""  << ox << " " << oy << " " << oz << "\""
          << " Spacing=\"" << box << " " << box << " " << box << "\">\n";
      vti << "    <Piece Extent=\"0 " << nx << " 0 " << ny
          << " " << k_begin << " " << k_end << "\">\n";
      vti << "      <CellData>\n";

      struct ArrayMeta { std::string tag; uint32_t nbytes; const char* data; };
      std::vector<ArrayMeta> metas;
      uint32_t offset_bytes = 0;

      auto append_array = [&](const std::string& type, const std::string& aname,
                              int components, const void* buf,
                              size_t elems, size_t elem_size) {
        uint32_t bytes = static_cast<uint32_t>(elems * components * elem_size);
        std::ostringstream tag;
        tag << "        <DataArray type=\"" << type << "\" Name=\"" << aname
            << "\" NumberOfComponents=\"" << components
            << "\" format=\"appended\" offset=\"" << offset_bytes << "\"/>\n";
        metas.push_back({tag.str(), bytes, reinterpret_cast<const char*>(buf)});
        offset_bytes += 4 + bytes;
      };

      // Grid layout is Z-outer, Y-middle, X-inner; the slab is contiguous.
      const uint64_t data_start = k_begin * static_cast<uint64_t>(nx * ny);
      if (vd.concentration)
        append_array(rt, "Substance Concentration", 1,
                     &conc[data_start], piece_cells, sizeof(real_t));
      if (vd.gradient)
        // Gradients are interleaved (gx,gy,gz per box).
        append_array(rt, "Diffusion Gradient", 3,
                     &grad[data_start * 3], piece_cells, sizeof(real_t));

      for (auto& m : metas) vti << m.tag;
      vti << "      </CellData>\n";
      vti << "    </Piece>\n";
      vti << "  </ImageData>\n";

      vti << "  <AppendedData encoding=\"raw\">\n_";
      for (auto& m : metas) {
        uint32_t sz = m.nbytes;
        vti.write(reinterpret_cast<const char*>(&sz), sizeof(uint32_t));
        vti.write(m.data, sz);
      }
      vti << "\n  </AppendedData>\n</VTKFile>\n";
      vti.close();
    }

    WriteDiffusionPvti(vd.name, vd.concentration, vd.gradient,
                       nx, ny, nz, ox, oy, oz, box,
                       static_cast<int>(num_pieces),
                       static_cast<size_t>(boxes_per_piece));
  }
}

// -----------------------------------------------------------------------------
/// Write the parallel VTI index file (diffusion_{name}_{step}.pvti).
///
/// PImageData format carries WholeExtent, Origin, and Spacing at the top
/// level so that ParaView can reconstruct the full grid without loading
/// individual piece files first.
///
/// Each <Piece> entry lists its non-overlapping Z-slab cell range, expressed
/// as node-index extent k_begin .. k_begin+k_len.
void StandaloneExporter::WriteDiffusionPvti(
    const std::string& name, bool has_concentration, bool has_gradient,
    std::size_t nx, std::size_t ny, std::size_t nz,
    double ox, double oy, double oz, double spacing,
    int pieces, std::size_t boxes_per_piece) const {
  const char* rt = (sizeof(real_t) == 4) ? "Float32" : "Float64";

  std::ostringstream pvti_name;
  pvti_name << output_dir_ << "/diffusion_" << name << "_" << step_ << ".pvti";
  std::ofstream pvti(pvti_name.str());

  pvti << "<?xml version=\"1.0\"?>\n";
  pvti << "<VTKFile type=\"PImageData\" version=\"0.1\""
          " byte_order=\"LittleEndian\">\n";
  pvti << "  <PImageData"
       << " WholeExtent=\"0 " << nx << " 0 " << ny
       << " 0 " << nz << "\""
       << " GhostLevel=\"0\""
       << " Origin=\""  << ox << " " << oy << " " << oz << "\""
       << " Spacing=\"" << spacing << " " << spacing << " " << spacing << "\">\n";

  pvti << "    <PCellData>\n";
  if (has_concentration)
    pvti << "      <PDataArray type=\"" << rt
         << "\" Name=\"Substance Concentration\" NumberOfComponents=\"1\"/>\n";
  if (has_gradient)
    pvti << "      <PDataArray type=\"" << rt
         << "\" Name=\"Diffusion Gradient\" NumberOfComponents=\"3\"/>\n";
  pvti << "    </PCellData>\n";

  for (int i = 0; i < pieces; ++i) {
    size_t k0 = static_cast<size_t>(i) * boxes_per_piece;
    size_t k1 = std::min(k0 + boxes_per_piece, nz);  // non-overlapping cell ranges
    pvti << "    <Piece"
         << " Extent=\"0 " << nx << " 0 " << ny
         << " " << k0 << " " << k1 << "\""
         << " Source=\"diffusion_" << name << "_" << step_ << "_p" << i
         << ".vti\"/>\n";
  }

  pvti << "  </PImageData>\n</VTKFile>\n";
  pvti.close();
}

// -----------------------------------------------------------------------------
/// Export diffusion grids as VTK UnstructuredGrid (.vtu / .pvtu).
///
/// Each diffusion box is represented as a VTK_VOXEL cell (type 11) with
/// eight explicit corner nodes.  Corner node coordinates are computed from
/// the grid origin and box length.  Concentration and gradient are stored as
/// CellData — one value per voxel centre, matching BioDynaMo's cell-centred
/// finite-difference storage.
///
/// Z-slab parallelism: the grid is partitioned horizontally; each OpenMP
/// thread builds the corner nodes and connectivity arrays for its slab
/// independently and writes one .vtu piece file.
///
/// This method does not increment step_.  WriteDiffusionStep() manages the
/// counter; to activate VTU output, replace the WriteDiffusionStepVti()
/// call in WriteDiffusionStep() with this method.
void StandaloneExporter::WriteDiffusionStepVtu() {
  auto* sim   = Simulation::GetActive();
  auto* rm    = sim->GetResourceManager();
  auto* param = sim->GetParam();

  for (const auto& vd : param->visualize_diffusion) {
    auto* grid = rm->GetDiffusionGrid(vd.name);
    if (!grid) continue;

    const auto num_boxes = grid->GetNumBoxesArray();
    const auto dims      = grid->GetDimensions();
    const auto box       = grid->GetBoxLength();

    const size_t nx = static_cast<size_t>(num_boxes[0]);
    const size_t ny = static_cast<size_t>(num_boxes[1]);
    const size_t nz = static_cast<size_t>(num_boxes[2]);

    const real_t* conc = grid->GetAllConcentrations();
    const real_t* grad = grid->GetAllGradients();

    auto*    tinfo           = ThreadInfo::GetInstance();
    uint64_t max_threads     = tinfo->GetMaxThreads();
    uint64_t num_pieces      = std::min<uint64_t>(std::max<uint64_t>(1, nz),
                                                  std::max<uint64_t>(1, max_threads));
    uint64_t boxes_per_piece = (nz + num_pieces - 1) / num_pieces;

    const char* rt = (sizeof(real_t) == 4) ? "Float32" : "Float64";

    #pragma omp parallel for schedule(static, 1)
    for (uint64_t p = 0; p < num_pieces; ++p) {
      uint64_t k_begin = p * boxes_per_piece;
      if (k_begin >= nz) continue;
      uint64_t k_len = std::min<uint64_t>(boxes_per_piece, nz - k_begin);

      const uint64_t num_nodes = (nx+1) * (ny+1) * (k_len+1);
      const uint64_t num_cells = nx * ny * k_len;
      const uint64_t stride_j  = nx + 1;
      const uint64_t stride_k  = (nx + 1) * (ny + 1);

      // ── Corner node coordinates ───────────────────────────────────────
      // Node (i, j, kl) sits at the corner of voxel (i,j,kl).
      // kl is the local Z index within this slab; the global Z index is
      // k_begin + kl, giving z = dims[4] + (k_begin + kl) * box.
      std::vector<double> pts(num_nodes * 3);
      {
        size_t idx = 0;
        for (uint64_t kl = 0; kl <= k_len; ++kl)
          for (uint64_t j = 0; j <= ny; ++j)
            for (uint64_t i = 0; i <= nx; ++i) {
              pts[idx++] = dims[0] + i * box;
              pts[idx++] = dims[2] + j * box;
              pts[idx++] = dims[4] + (k_begin + kl) * box;
            }
      }

      // ── VTK_VOXEL connectivity ────────────────────────────────────────
      // VTK_VOXEL node ordering (type 11):
      //   v0=(i,j,k)  v1=(i+1,j,k)  v2=(i,j+1,k)  v3=(i+1,j+1,k)
      //   v4=(i,j,k+1) v5=(i+1,j,k+1) v6=(i,j+1,k+1) v7=(i+1,j+1,k+1)
      std::vector<int32_t> conn(num_cells * 8);
      std::vector<int32_t> offs(num_cells);
      std::vector<uint8_t> types(num_cells, 11);
      {
        size_t cidx = 0;
        for (uint64_t kl = 0; kl < k_len; ++kl) {
          for (uint64_t j = 0; j < ny; ++j) {
            for (uint64_t i = 0; i < nx; ++i) {
              uint64_t b = kl * stride_k + j * stride_j + i;
              conn[cidx++] = static_cast<int32_t>(b);
              conn[cidx++] = static_cast<int32_t>(b + 1);
              conn[cidx++] = static_cast<int32_t>(b + stride_j);
              conn[cidx++] = static_cast<int32_t>(b + stride_j + 1);
              conn[cidx++] = static_cast<int32_t>(b + stride_k);
              conn[cidx++] = static_cast<int32_t>(b + stride_k + 1);
              conn[cidx++] = static_cast<int32_t>(b + stride_k + stride_j);
              conn[cidx++] = static_cast<int32_t>(b + stride_k + stride_j + 1);
            }
          }
        }
        for (uint64_t c = 0; c < num_cells; ++c)
          offs[c] = static_cast<int32_t>((c + 1) * 8);
      }

      // Grid layout: Z-outer, Y-middle, X-inner.
      const uint64_t data_start = k_begin * static_cast<uint64_t>(nx * ny);

      std::ostringstream vtu_name;
      vtu_name << output_dir_ << "/diffusion_" << vd.name << "_"
               << step_ << "_p" << p << ".vtu";
      std::ofstream vtu(vtu_name.str(), std::ios::binary);

      vtu << "<?xml version=\"1.0\"?>\n";
      vtu << "<VTKFile type=\"UnstructuredGrid\" version=\"0.1\""
             " byte_order=\"LittleEndian\" header_type=\"UInt32\">\n";
      vtu << "  <UnstructuredGrid>\n";
      vtu << "    <Piece NumberOfPoints=\"" << num_nodes
          << "\" NumberOfCells=\""          << num_cells << "\">\n";
      vtu << "      <CellData>\n";

      struct ArrayMeta { std::string tag; uint32_t nbytes; const char* data; };
      std::vector<ArrayMeta> metas;
      uint32_t offset = 0;

      auto append = [&](const std::string& type, const std::string& aname,
                        int components, const void* buf,
                        size_t elems, size_t elem_size) {
        uint32_t bytes = static_cast<uint32_t>(elems * components * elem_size);
        std::ostringstream tag;
        tag << "        <DataArray type=\"" << type << "\" Name=\"" << aname
            << "\" NumberOfComponents=\"" << components
            << "\" format=\"appended\" offset=\"" << offset << "\"/>\n";
        metas.push_back({tag.str(), bytes, reinterpret_cast<const char*>(buf)});
        offset += 4 + bytes;
      };

      if (vd.concentration)
        append(rt, "Substance Concentration", 1,
               &conc[data_start], num_cells, sizeof(real_t));
      if (vd.gradient)
        append(rt, "Diffusion Gradient", 3,
               &grad[data_start * 3], num_cells, sizeof(real_t));

      for (auto& m : metas) vtu << m.tag;
      vtu << "      </CellData>\n";

      vtu << "      <Points>\n";
      {
        std::ostringstream tag;
        tag << "        <DataArray type=\"Float64\" NumberOfComponents=\"3\""
               " format=\"appended\" offset=\"" << offset << "\"/>\n";
        vtu << tag.str();
        metas.push_back({"", static_cast<uint32_t>(num_nodes * 3 * sizeof(double)),
                         reinterpret_cast<const char*>(pts.data())});
        offset += 4 + static_cast<uint32_t>(num_nodes * 3 * sizeof(double));
      }
      vtu << "      </Points>\n";

      vtu << "      <Cells>\n";
      auto add_cell_arr = [&](const std::string& type, const std::string& aname,
                              const void* buf, size_t bytes) {
        std::ostringstream tag;
        tag << "        <DataArray type=\"" << type << "\" Name=\"" << aname
            << "\" format=\"appended\" offset=\"" << offset << "\"/>\n";
        vtu << tag.str();
        metas.push_back({"", static_cast<uint32_t>(bytes),
                         reinterpret_cast<const char*>(buf)});
        offset += 4 + static_cast<uint32_t>(bytes);
      };
      add_cell_arr("Int32", "connectivity", conn.data(),
                   conn.size() * sizeof(int32_t));
      add_cell_arr("Int32", "offsets",      offs.data(),
                   offs.size() * sizeof(int32_t));
      add_cell_arr("UInt8", "types",        types.data(),
                   types.size() * sizeof(uint8_t));
      vtu << "      </Cells>\n";

      vtu << "    </Piece>\n  </UnstructuredGrid>\n";

      vtu << "  <AppendedData encoding=\"raw\">\n_";
      for (auto& m : metas) {
        uint32_t sz = m.nbytes;
        vtu.write(reinterpret_cast<const char*>(&sz), sizeof(uint32_t));
        vtu.write(m.data, sz);
      }
      vtu << "\n  </AppendedData>\n</VTKFile>\n";
      vtu.close();
    }

    WriteDiffusionVtuIndex(vd.name, vd.concentration, vd.gradient,
                           static_cast<int>(num_pieces));
  }
}

// -----------------------------------------------------------------------------
/// Write the parallel UnstructuredGrid index file for diffusion
/// (diffusion_{name}_{step}.pvtu).
///
/// Lists all piece files and mirrors the CellData schema so that ParaView
/// can discover available fields without loading every piece.
void StandaloneExporter::WriteDiffusionVtuIndex(
    const std::string& name, bool has_concentration, bool has_gradient,
    int pieces) const {
  const char* rt = (sizeof(real_t) == 4) ? "Float32" : "Float64";

  std::ostringstream pvtu_name;
  pvtu_name << output_dir_ << "/diffusion_" << name << "_" << step_ << ".pvtu";
  std::ofstream pvtu(pvtu_name.str());

  pvtu << "<?xml version=\"1.0\"?>\n";
  pvtu << "<VTKFile type=\"PUnstructuredGrid\" version=\"0.1\""
          " byte_order=\"LittleEndian\">\n";
  pvtu << "  <PUnstructuredGrid>\n";

  pvtu << "    <PCellData>\n";
  if (has_concentration)
    pvtu << "      <PDataArray type=\"" << rt
         << "\" Name=\"Substance Concentration\" NumberOfComponents=\"1\"/>\n";
  if (has_gradient)
    pvtu << "      <PDataArray type=\"" << rt
         << "\" Name=\"Diffusion Gradient\" NumberOfComponents=\"3\"/>\n";
  pvtu << "    </PCellData>\n";

  pvtu << "    <PPoints>\n";
  pvtu << "      <PDataArray type=\"Float64\" NumberOfComponents=\"3\"/>\n";
  pvtu << "    </PPoints>\n";

  for (int i = 0; i < pieces; ++i)
    pvtu << "    <Piece Source=\"diffusion_" << name << "_" << step_
         << "_p" << i << ".vtu\"/>\n";

  pvtu << "  </PUnstructuredGrid>\n</VTKFile>\n";
  pvtu.close();
}

}  // namespace bdm
