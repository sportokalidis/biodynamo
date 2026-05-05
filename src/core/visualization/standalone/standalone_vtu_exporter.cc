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

#include "core/visualization/standalone/standalone_vtu_exporter.h"

#include <fstream>
#include <regex>
#include <sstream>
#include <vector>

// ROOT reflection — used only by ReadScalarMember to locate data member
// offsets at runtime without a compile-time dependency on user agent types.
#include "TClass.h"
#include "TDataMember.h"

#include "core/agent/agent.h"           // Agent base class, GetPosition(), GetUid()
#include "core/agent/cell.h"            // Cell subclass for mass/volume/traction
#include "core/diffusion/diffusion_grid.h"  // DiffusionGrid, GetAllConcentrations()
#include "core/param/param.h"           // Param::VisualizeDiffusion
#include "core/resource_manager.h"      // ForEachAgent(), GetDiffusionGrid()
#include "core/simulation.h"            // Simulation::GetActive()
#include "core/util/thread_info.h"      // GetMaxThreads()

namespace bdm {

// Forward declarations — these file-scope helpers are defined later in this
// translation unit; declaring them here keeps WriteStep() at the top.
static std::vector<std::string> LoadAdditionalMembers();
static bool ReadScalarMember(const Agent* agent, const std::string& name,
                             double& out);

// -----------------------------------------------------------------------------
StandaloneVtuExporter::StandaloneVtuExporter(const std::string& output_dir)
    : output_dir_(output_dir) {}

// -----------------------------------------------------------------------------
StandaloneVtuExporter::~StandaloneVtuExporter() = default;

// -----------------------------------------------------------------------------
/// Export all agents for the current simulation step.
///
/// Layout overview
/// ───────────────
/// Each agent is represented as a VTK_VERTEX cell (type 1): a degenerate
/// zero-dimensional cell with exactly one node.  This means
///   NumberOfPoints == NumberOfCells
/// and the connectivity array is the trivial identity [0, 1, 2, …, n-1].
///
/// All per-agent attributes (diameter, mass, etc.) are stored as PointData
/// because VTK_VERTEX cells collapse point and cell concepts to the same
/// thing — each "cell" is just a labelled point.
///
/// Parallelism
/// ───────────
/// The agent list is partitioned into equal-sized slices; each OpenMP thread
/// writes exactly one .vtu piece file.  All data arrays are pre-extracted
/// into flat contiguous vectors on the calling thread before the parallel
/// loop so that each thread operates on read-only slices — no mutexes needed.
void StandaloneVtuExporter::WriteStep() {
  auto* sim = Simulation::GetActive();
  auto* rm  = sim->GetResourceManager();

  // ── Step 1: Collect per-agent data into flat arrays ──────────────────────
  //
  // Flat vectors (not vector-of-structs) allow each parallel thread to slice
  // out its range with a single pointer arithmetic operation.
  std::vector<Agent*>   agents;  // raw pointers — agents are owned by the RM
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

  // ── Step 2: Extract typed fields into flat arrays ─────────────────────────
  //
  // Done here (single thread) so the parallel writer loop only reads.
  // dynamic_cast is used because Mass/Volume/TractionForce belong to Cell,
  // not the base Agent class.  Non-Cell agents receive zero-filled entries.
  std::vector<double> diam(n), mass(n), volume(n), traction(n * 3);
  for (size_t i = 0; i < n; ++i) {
    Agent* a  = agents[i];
    diam[i]   = a->GetDiameter();
    if (auto* cell = dynamic_cast<Cell*>(a)) {
      mass[i]            = cell->GetMass();
      volume[i]          = cell->GetVolume();
      const auto& tf     = cell->GetTractorForce();
      traction[i*3 + 0]  = tf[0];
      traction[i*3 + 1]  = tf[1];
      traction[i*3 + 2]  = tf[2];
    } else {
      mass[i] = volume[i] = traction[i*3] = traction[i*3+1] = traction[i*3+2]
              = 0.0;
    }
  }

  // ── Step 3: Read user-defined extra scalar fields via ROOT reflection ──────
  //
  // additional_data_members in bdm.toml lets users export any numeric C++
  // data member of their custom agent type without modifying BioDynaMo.
  // ReadScalarMember() uses TDataMember::GetOffset() to locate the field
  // at runtime — the field name is resolved against the actual derived type.
  auto extra = LoadAdditionalMembers();
  std::vector<std::vector<double>> extra_vals(extra.size());
  for (size_t f = 0; f < extra.size(); ++f) {
    extra_vals[f].resize(n);
    for (size_t i = 0; i < n; ++i) {
      double val = 0.0;
      ReadScalarMember(agents[i], extra[f], val);
      extra_vals[f][i] = val;
    }
  }

  // ── Step 4: Compute piece partition ──────────────────────────────────────
  //
  // One piece per OpenMP thread, capped so no piece is empty.
  // ceiling division: per_piece = ceil(n / num_pieces)
  auto*    tinfo      = ThreadInfo::GetInstance();
  uint64_t max_threads = tinfo->GetMaxThreads();
  uint64_t num_pieces  = std::max<uint64_t>(1, std::min<uint64_t>(max_threads, n));
  uint64_t per_piece   = (n + num_pieces - 1) / num_pieces;

  // ── Step 5: Write one .vtu file per piece in parallel ─────────────────────
  //
  // schedule(static,1): thread p writes piece p.  Static scheduling is
  // correct here because each piece involves roughly the same amount of I/O.
  #pragma omp parallel for schedule(static, 1)
  for (uint64_t p = 0; p < num_pieces; ++p) {
    uint64_t begin = p * per_piece;
    if (begin >= n) continue;  // guard for over-partitioned last threads
    uint64_t count = std::min<uint64_t>(per_piece, n - begin);

    // ── 5a: Open the piece file ───────────────────────────────────────────
    std::ostringstream vtu_name;
    vtu_name << output_dir_ << "/agents_" << step_ << "_p" << p << ".vtu";
    std::ofstream vtu(vtu_name.str(), std::ios::binary);

    // ── 5b: Write XML header ──────────────────────────────────────────────
    //
    // header_type="UInt32" means each appended array is preceded by a 4-byte
    // length field (not 8-byte), keeping the header compact.
    vtu << "<?xml version=\"1.0\"?>\n";
    vtu << "<VTKFile type=\"UnstructuredGrid\" version=\"0.1\""
           " byte_order=\"LittleEndian\" header_type=\"UInt32\">\n";
    vtu << "  <UnstructuredGrid>\n";
    // For VTK_VERTEX, NumberOfPoints == NumberOfCells (one point per cell).
    vtu << "    <Piece NumberOfPoints=\"" << count
        << "\" NumberOfCells=\""          << count << "\">\n";
    vtu << "      <PointData>\n";

    // ── 5c: Build appended-data metadata table ────────────────────────────
    //
    // The VTK "appended raw" format separates metadata (XML tags with offsets)
    // from binary payloads.  We must know all byte offsets before writing any
    // XML, so we collect (tag, size, data_ptr) entries first, then emit all
    // XML, then emit all binary.
    //
    // Each entry in `metas` stores:
    //   tag   — the complete XML <DataArray .../> string
    //   nbytes — byte count of the raw payload (excluding the 4-byte header)
    //   data  — pointer into an already-allocated buffer (no copy here)
    struct ArrayMeta { std::string tag; uint32_t nbytes; const char* data; };
    std::vector<ArrayMeta> metas;
    uint32_t offset = 0;  // running byte offset into the binary block

    // append_array: registers one array and advances the offset counter.
    // offset is the position of the 4-byte length prefix in the binary block,
    // so the actual payload starts at offset+4.
    auto append_array = [&](const std::string& type, const std::string& name,
                            int components, const void* buf,
                            size_t elems, size_t elem_size) {
      uint32_t bytes = static_cast<uint32_t>(elems * components * elem_size);
      std::ostringstream tag;
      tag << "        <DataArray type=\"" << type << "\" Name=\"" << name
          << "\" NumberOfComponents=\"" << components
          << "\" format=\"appended\" offset=\"" << offset << "\"/>\n";
      metas.push_back({tag.str(), bytes, reinterpret_cast<const char*>(buf)});
      offset += 4 + bytes;  // 4 = sizeof(UInt32 length prefix)
    };

    // ── 5d: Register all PointData arrays ────────────────────────────────
    //
    // Slices into the pre-extracted flat arrays using `begin` as the offset.
    // No data is copied — the pointers reference the original buffers.
    append_array("UInt64",  "Cell_ID",      1, &ids[begin],            count, sizeof(uint64_t));
    for (size_t f = 0; f < extra.size(); ++f)
      append_array("Float64", extra[f],     1, &extra_vals[f][begin],  count, sizeof(double));
    append_array("Float64", "Diameter",     1, &diam[begin],           count, sizeof(double));
    append_array("Float64", "Mass",         1, &mass[begin],           count, sizeof(double));
    append_array("Float64", "Volume",       1, &volume[begin],         count, sizeof(double));
    // TractionForce has 3 components (x,y,z) stored interleaved in `traction`.
    append_array("Float64", "TractionForce",3, &traction[begin * 3],   count, sizeof(double));

    // Emit XML tags for all PointData arrays now that offsets are finalised.
    for (auto& m : metas) vtu << m.tag;
    vtu << "      </PointData>\n";

    // ── 5e: Register Points (XYZ coordinates) ────────────────────────────
    //
    // `points` stores coordinates interleaved as x0,y0,z0,x1,y1,z1,…
    // The slice for this piece starts at begin*3 (3 doubles per agent).
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

    // ── 5f: Build VTK_VERTEX cell topology ───────────────────────────────
    //
    // VTK_VERTEX (type code 1) is a degenerate 0-D cell with one node.
    // Connectivity is the identity permutation [0,1,…,count-1] — each cell
    // references exactly its own point.
    // Offsets are the cumulative node count per cell: [1,2,3,…,count].
    // All cell types are 1 (VTK_VERTEX).
    std::vector<int32_t> conn(count), offs(count);
    std::vector<uint8_t> types_arr(count);
    for (uint64_t i = 0; i < count; ++i) {
      conn[i]      = static_cast<int32_t>(i);
      offs[i]      = static_cast<int32_t>(i + 1);
      types_arr[i] = 1;  // VTK_VERTEX
    }

    vtu << "      <Cells>\n";
    {
      std::ostringstream tag;
      tag << "        <DataArray type=\"Int32\" Name=\"connectivity\""
             " format=\"appended\" offset=\"" << offset << "\"/>\n";
      vtu << tag.str();
      metas.push_back({"", static_cast<uint32_t>(count * sizeof(int32_t)),
                       reinterpret_cast<const char*>(conn.data())});
      offset += 4 + static_cast<uint32_t>(count * sizeof(int32_t));
    }
    {
      std::ostringstream tag;
      tag << "        <DataArray type=\"Int32\" Name=\"offsets\""
             " format=\"appended\" offset=\"" << offset << "\"/>\n";
      vtu << tag.str();
      metas.push_back({"", static_cast<uint32_t>(count * sizeof(int32_t)),
                       reinterpret_cast<const char*>(offs.data())});
      offset += 4 + static_cast<uint32_t>(count * sizeof(int32_t));
    }
    {
      std::ostringstream tag;
      tag << "        <DataArray type=\"UInt8\" Name=\"types\""
             " format=\"appended\" offset=\"" << offset << "\"/>\n";
      vtu << tag.str();
      metas.push_back({"", static_cast<uint32_t>(count * sizeof(uint8_t)),
                       reinterpret_cast<const char*>(types_arr.data())});
      offset += 4 + static_cast<uint32_t>(count * sizeof(uint8_t));
    }
    vtu << "      </Cells>\n";

    vtu << "    </Piece>\n";
    vtu << "  </UnstructuredGrid>\n";

    // ── 5g: Write the binary AppendedData block ───────────────────────────
    //
    // The block opens with "_" (VTK spec requirement).  Each array entry is
    // written as [4-byte UInt32 payload-length][raw bytes].  The offsets
    // recorded in the XML header above are byte positions relative to the
    // character immediately after "_".
    vtu << "  <AppendedData encoding=\"raw\">\n_";
    for (auto& m : metas) {
      uint32_t sz = m.nbytes;
      vtu.write(reinterpret_cast<const char*>(&sz), sizeof(uint32_t));
      vtu.write(m.data, sz);
    }
    vtu << "\n  </AppendedData>\n</VTKFile>\n";
    vtu.close();
  }

  // ── Step 6: Write the parallel index (.pvtu) on the main thread ──────────
  WritePvtu(static_cast<int>(num_pieces));
  // Increment step counter so filenames for the next export don't collide.
  step_++;
}

// -----------------------------------------------------------------------------
/// Parse bdm.toml to extract the list of extra scalar member names declared
/// under `additional_data_members = ["field1_", "field2_"]`.
///
/// A lightweight hand-rolled parser is used instead of a full TOML library
/// to avoid adding a dependency.  Only one regex is needed because the field
/// is always on a single line in the format BioDynaMo writes/expects.
static std::vector<std::string> LoadAdditionalMembers() {
  std::vector<std::string> names;
  std::ifstream ifs("bdm.toml");
  if (!ifs) return names;  // no config file — return empty list silently

  std::string line;
  // Matches: additional_data_members = ["name1", "name2", ...]
  // Capture group 1 captures the content between the brackets.
  std::regex re(R"(additional_data_members\s*=\s*\[([^\]]+)\])");
  while (std::getline(ifs, line)) {
    std::smatch m;
    if (std::regex_search(line, m, re)) {
      std::string inner = m[1].str();
      // Extract individual quoted names from the bracket content.
      std::regex name_re("\"([^\"]+)\"");
      for (std::sregex_iterator it(inner.begin(), inner.end(), name_re), end;
           it != end; ++it)
        names.push_back((*it)[1].str());
    }
  }
  return names;
}

// -----------------------------------------------------------------------------
/// Read one scalar numeric data member from an agent object using ROOT's
/// runtime reflection API.
///
/// TClass::GetClass(typeid(*agent)) resolves the actual derived type (e.g.
/// "MyCell"), not the base class, so user-defined fields on custom agent
/// subclasses are found correctly.
///
/// TDataMember::GetOffset() returns the byte offset of the field from the
/// beginning of the object — equivalent to offsetof() but computed at
/// runtime from the ROOT dictionary.
///
/// @param agent  Pointer to the agent whose field we want to read.
/// @param name   C++ data member name as a string (e.g. "my_field_").
/// @param out    Output: the field value converted to double.
/// @return       true if the field was found and read; false otherwise.
static bool ReadScalarMember(const Agent* agent, const std::string& name,
                             double& out) {
  TClass* cls = TClass::GetClass(typeid(*agent));
  if (!cls) return false;
  TDataMember* dm = cls->GetDataMember(name.c_str());
  if (!dm)  return false;

  const char* addr = reinterpret_cast<const char*>(agent) + dm->GetOffset();
  std::string tname = dm->GetTypeName();

  if (tname == "double") { out = *reinterpret_cast<const double*>(addr); return true; }
  if (tname == "float")  { out = static_cast<double>(*reinterpret_cast<const float*>(addr));  return true; }
  if (tname == "int")    { out = static_cast<double>(*reinterpret_cast<const int*>(addr));    return true; }
  if (tname == "uint64_t" || tname == "unsigned long" || tname == "unsigned long long") {
    out = static_cast<double>(*reinterpret_cast<const unsigned long long*>(addr));
    return true;
  }
  return false;  // unsupported type — silently skip
}

// -----------------------------------------------------------------------------
/// Write the parallel agent index file (agents_{step}.pvtu).
///
/// The PVTU file does not contain any simulation data — it is a lightweight
/// XML index that tells ParaView which .vtu files form the complete dataset
/// and what named arrays each piece file contains.  ParaView requires that
/// every array declared in PPointData/PPoints is present in every piece file.
void StandaloneVtuExporter::WritePvtu(int pieces) const {
  std::ostringstream name;
  name << output_dir_ << "/agents_" << step_ << ".pvtu";
  std::ofstream pvtu(name.str());

  pvtu << "<?xml version=\"1.0\"?>\n";
  pvtu << "<VTKFile type=\"PUnstructuredGrid\" version=\"0.1\""
          " byte_order=\"LittleEndian\">\n";
  pvtu << "  <PUnstructuredGrid>\n";

  // PPointData: declares the schema (name, type, components) of every
  // PointData array across all pieces.  No actual data is stored here.
  pvtu << "    <PPointData>\n";
  pvtu << "      <PDataArray type=\"UInt64\"  Name=\"Cell_ID\""
          " NumberOfComponents=\"1\"/>\n";
  // Mirror any extra fields listed in bdm.toml so ParaView discovers them.
  for (const auto& field : LoadAdditionalMembers())
    pvtu << "      <PDataArray type=\"Float64\" Name=\"" << field
         << "\" NumberOfComponents=\"1\"/>\n";
  pvtu << "      <PDataArray type=\"Float64\" Name=\"Diameter\""
          " NumberOfComponents=\"1\"/>\n";
  pvtu << "      <PDataArray type=\"Float64\" Name=\"Mass\""
          " NumberOfComponents=\"1\"/>\n";
  pvtu << "      <PDataArray type=\"Float64\" Name=\"Volume\""
          " NumberOfComponents=\"1\"/>\n";
  pvtu << "      <PDataArray type=\"Float64\" Name=\"TractionForce\""
          " NumberOfComponents=\"3\"/>\n";
  pvtu << "    </PPointData>\n";

  // PPoints: declares the coordinate array schema.
  pvtu << "    <PPoints>\n";
  pvtu << "      <PDataArray type=\"Float64\" NumberOfComponents=\"3\"/>\n";
  pvtu << "    </PPoints>\n";

  // One <Piece> entry per .vtu file, using relative paths so the dataset is
  // portable when the output directory is moved.
  for (int i = 0; i < pieces; ++i)
    pvtu << "    <Piece Source=\"agents_" << step_ << "_p" << i << ".vtu\"/>\n";

  pvtu << "  </PUnstructuredGrid>\n</VTKFile>\n";
  pvtu.close();
}

// -----------------------------------------------------------------------------
/// Export diffusion grids for the current simulation step.
///
/// Iterates over every entry in param->visualize_diffusion (populated from
/// [[visualize_diffusion]] blocks in bdm.toml) and writes one set of VTU
/// piece files plus one PVTU index file per substance.
///
/// Cell representation
/// ───────────────────
/// Each diffusion box is exported as a VTK_VOXEL (type 11) — an axis-aligned
/// hexahedron with 8 explicit corner nodes.  This cell type is required for
/// volume rendering in ParaView (the GPU volume mapper decomposes voxels
/// into tetrahedra internally).
///
/// Data placement: CellData (not PointData)
/// ─────────────────────────────────────────
/// BioDynaMo stores one concentration value per box center using a
/// cell-centered finite-difference scheme.  VTK CellData maps directly to
/// this layout: one scalar per cell (voxel), no interpolation to corners.
/// Using PointData instead would imply corner-interpolated data, which would
/// misrepresent the simulation's discretisation.
///
/// Z-slab parallelism
/// ──────────────────
/// The grid is partitioned into horizontal slabs along the Z axis.  Each
/// slab contains `boxes_per_piece` complete XY layers.  Each OpenMP thread
/// writes its own .vtu file for its slab independently.
void StandaloneVtuExporter::WriteDiffusionStep() {
  auto* sim   = Simulation::GetActive();
  auto* rm    = sim->GetResourceManager();
  auto* param = sim->GetParam();

  for (const auto& vd : param->visualize_diffusion) {
    auto* grid = rm->GetDiffusionGrid(vd.name);
    if (!grid) continue;  // substance not yet initialised — skip silently

    // ── Grid geometry ────────────────────────────────────────────────────
    const auto num_boxes = grid->GetNumBoxesArray();   // {nx, ny, nz} box counts
    const auto dims      = grid->GetDimensions();       // {xmin,xmax,ymin,ymax,zmin,zmax}
    const auto box       = grid->GetBoxLength();        // side length of each cubic box

    const size_t nx = static_cast<size_t>(num_boxes[0]);
    const size_t ny = static_cast<size_t>(num_boxes[1]);
    const size_t nz = static_cast<size_t>(num_boxes[2]);
    // VTK_VOXEL corner nodes: one more node than boxes in each dimension.
    const size_t nx_nodes = nx + 1;
    const size_t ny_nodes = ny + 1;

    // Raw concentration and gradient arrays from the grid.
    // Layout: row-major [k][j][i] with k=Z, j=Y, i=X (Z-fastest-varying
    // when interpreting as a flat 1-D index = k*nx*ny + j*nx + i).
    // Gradients are interleaved: gx,gy,gz for box 0, gx,gy,gz for box 1, …
    // These pointers are zero-copy references into the grid's own storage.
    const real_t* conc = grid->GetAllConcentrations();
    const real_t* grad = grid->GetAllGradients();

    // ── Partition the Z dimension into slabs ─────────────────────────────
    //
    // num_pieces is capped at nz so no piece has zero Z-layers.
    auto*    tinfo          = ThreadInfo::GetInstance();
    uint64_t max_threads    = tinfo->GetMaxThreads();
    uint64_t num_pieces     = std::min<uint64_t>(std::max<uint64_t>(1, nz),
                                                 std::max<uint64_t>(1, max_threads));
    uint64_t boxes_per_piece = (nz + num_pieces - 1) / num_pieces;  // ceil

    #pragma omp parallel for schedule(static, 1)
    for (uint64_t p = 0; p < num_pieces; ++p) {
      uint64_t k_begin = p * boxes_per_piece;
      if (k_begin >= nz) continue;  // over-partitioned last thread — skip
      uint64_t k_len = std::min<uint64_t>(boxes_per_piece, nz - k_begin);

      // ── Piece geometry ─────────────────────────────────────────────────
      //
      // k_len box layers require (k_len+1) node layers in Z (one extra for
      // the top face of the last box layer).
      const size_t   nz_nodes_piece = static_cast<size_t>(k_len + 1);
      const uint64_t piece_points   = static_cast<uint64_t>(nx_nodes * ny_nodes
                                                             * nz_nodes_piece);
      const uint64_t piece_cells    = static_cast<uint64_t>(nx * ny * k_len);

      // ── Build corner node coordinate array ────────────────────────────
      //
      // Nodes are at exact grid vertices (no +0.5 offset — that would give
      // box centers).  dims[4] is zmin; dims[2] is ymin; dims[0] is xmin.
      // The inner loops visit nodes in x-major, y-minor, z-outermost order
      // to match the node-ID formula:  id = i + j*nx_nodes + kk*(nx_nodes*ny_nodes)
      std::vector<double> points(piece_points * 3);
      {
        size_t idx = 0;
        for (size_t kk = 0; kk < nz_nodes_piece; ++kk) {
          double z = dims[4] + box * static_cast<double>(k_begin + kk);
          for (size_t j = 0; j < ny_nodes; ++j) {
            double y = dims[2] + box * static_cast<double>(j);
            for (size_t i = 0; i < nx_nodes; ++i) {
              points[idx++] = dims[0] + box * static_cast<double>(i);
              points[idx++] = y;
              points[idx++] = z;
            }
          }
        }
      }

      // ── Build VTK_VOXEL connectivity ──────────────────────────────────
      //
      // VTK_VOXEL (type 11) node ordering (from the VTK file format spec):
      //
      //        6───────7
      //       /|      /|        n0 = (i,   j,   k  )  →  base
      //      4───────5 |        n1 = (i+1, j,   k  )  →  base + 1
      //      | |     | |        n2 = (i,   j+1, k  )  →  base + sj
      //      | 2─────|─3        n3 = (i+1, j+1, k  )  →  base + 1 + sj
      //      |/      |/         n4 = (i,   j,   k+1)  →  base + sk
      //      0───────1          n5 = (i+1, j,   k+1)  →  base + 1 + sk
      //                         n6 = (i,   j+1, k+1)  →  base + sj + sk
      //                         n7 = (i+1, j+1, k+1)  →  base + 1 + sj + sk
      //
      // Important: n2/n3 are (i,j+1) then (i+1,j+1) — NOT the same as
      // VTK_HEXAHEDRON which uses counterclockwise winding.  Swapping n2/n3
      // or n6/n7 produces invalid voxels that the volume renderer rejects.
      //
      // `sj` and `sk` are the node-index strides for one Y and one Z step.
      // Pre-computing them outside the loop avoids repeated multiplications.
      const uint32_t sj = static_cast<uint32_t>(nx_nodes);
      const uint32_t sk = static_cast<uint32_t>(nx_nodes * ny_nodes);

      // Pre-allocate to the exact required size — no dynamic resizing during
      // the hot loop; direct pointer writes are faster than push_back.
      std::vector<uint32_t> connectivity(piece_cells * 8);
      std::vector<uint32_t> offsets(piece_cells);
      // All cells are VTK_VOXEL (11) — fill the types array uniformly.
      std::vector<uint8_t>  types(piece_cells, static_cast<uint8_t>(11));

      {
        uint64_t ci = 0;  // linear cell index within this piece
        for (uint64_t k = 0; k < k_len; ++k) {
          for (uint64_t j = 0; j < ny; ++j) {
            for (uint64_t i = 0; i < nx; ++i) {
              // base: local node index of the (i,j,k) corner of this voxel.
              uint32_t base = static_cast<uint32_t>(i)
                            + static_cast<uint32_t>(j) * sj
                            + static_cast<uint32_t>(k) * sk;
              uint32_t* c = &connectivity[ci * 8];
              c[0] = base;                 // n0
              c[1] = base + 1;             // n1
              c[2] = base + sj;            // n2
              c[3] = base + 1 + sj;        // n3
              c[4] = base + sk;            // n4
              c[5] = base + 1 + sk;        // n5
              c[6] = base + sj + sk;       // n6
              c[7] = base + 1 + sj + sk;   // n7
              // offset[ci] is the cumulative number of nodes up to and
              // including cell ci.  Every VTK_VOXEL has 8 nodes, so
              // offset[ci] = (ci+1)*8.
              offsets[ci] = static_cast<uint32_t>((ci + 1) * 8);
              ++ci;
            }
          }
        }
      }

      // ── Open piece file and write XML header ───────────────────────────
      std::ostringstream vtu_name;
      vtu_name << output_dir_ << "/diffusion_" << vd.name << "_"
               << step_ << "_p" << p << ".vtu";
      std::ofstream vtu(vtu_name.str(), std::ios::binary);

      vtu << "<?xml version=\"1.0\"?>\n";
      vtu << "<VTKFile type=\"UnstructuredGrid\" version=\"0.1\""
             " byte_order=\"LittleEndian\" header_type=\"UInt32\">\n";
      vtu << "  <UnstructuredGrid>\n";
      vtu << "    <Piece NumberOfPoints=\"" << piece_points
          << "\" NumberOfCells=\""          << piece_cells << "\">\n";

      // rt: VTK type string matching the simulation's real_t precision.
      // BioDynaMo defaults to double; can be overridden with -Dreal_t=float.
      const char* rt = (sizeof(real_t) == 4) ? "Float32" : "Float64";

      // ── Accumulate appended-data metadata ─────────────────────────────
      struct ArrayMeta { std::string tag; uint32_t nbytes; const char* data; };
      std::vector<ArrayMeta> metas;
      uint32_t offset_bytes = 0;

      auto append_array = [&](const std::string& type, const std::string& name,
                              int components, const void* buf,
                              size_t elems, size_t elem_size) {
        uint32_t bytes = static_cast<uint32_t>(elems * components * elem_size);
        std::ostringstream tag;
        tag << "        <DataArray type=\"" << type << "\" Name=\"" << name
            << "\" NumberOfComponents=\"" << components
            << "\" format=\"appended\" offset=\"" << offset_bytes << "\"/>\n";
        metas.push_back({tag.str(), bytes, reinterpret_cast<const char*>(buf)});
        offset_bytes += 4 + bytes;
      };

      // ── CellData: concentration and/or gradient ────────────────────────
      //
      // data_start is the flat index of the first box in this Z-slab.
      // The grid stores boxes in Z-outer, Y-middle, X-inner order, so
      // boxes for Z-slab k_begin start at offset k_begin * nx * ny.
      //
      // Zero-copy: append_array stores a pointer into the grid's own memory.
      // The DiffusionGrid object outlives this write call, so the pointer
      // remains valid until vtu.close() completes.
      vtu << "      <CellData>\n";
      const uint64_t data_start = k_begin * static_cast<uint64_t>(nx * ny);
      if (vd.concentration)
        append_array(rt, "Substance Concentration", 1,
                     &conc[data_start], piece_cells, sizeof(real_t));
      if (vd.gradient)
        // Gradient array stores 3 components per box interleaved, so the
        // element start for this slab is data_start * 3.
        append_array(rt, "Diffusion Gradient", 3,
                     &grad[data_start * 3], piece_cells, sizeof(real_t));
      for (auto& m : metas) vtu << m.tag;
      vtu << "      </CellData>\n";

      // ── Points ────────────────────────────────────────────────────────
      vtu << "      <Points>\n";
      {
        std::ostringstream tag;
        tag << "        <DataArray type=\"Float64\" NumberOfComponents=\"3\""
               " format=\"appended\" offset=\"" << offset_bytes << "\"/>\n";
        vtu << tag.str();
        metas.push_back({"", static_cast<uint32_t>(points.size() * sizeof(double)),
                         reinterpret_cast<const char*>(points.data())});
        offset_bytes += 4 + static_cast<uint32_t>(points.size() * sizeof(double));
      }
      vtu << "      </Points>\n";

      // ── Cells (connectivity / offsets / types) ─────────────────────────
      vtu << "      <Cells>\n";
      {
        std::ostringstream tag;
        tag << "        <DataArray type=\"UInt32\" Name=\"connectivity\""
               " format=\"appended\" offset=\"" << offset_bytes << "\"/>\n";
        vtu << tag.str();
        metas.push_back({"",
                         static_cast<uint32_t>(connectivity.size() * sizeof(uint32_t)),
                         reinterpret_cast<const char*>(connectivity.data())});
        offset_bytes += 4 + static_cast<uint32_t>(connectivity.size() * sizeof(uint32_t));
      }
      {
        std::ostringstream tag;
        tag << "        <DataArray type=\"UInt32\" Name=\"offsets\""
               " format=\"appended\" offset=\"" << offset_bytes << "\"/>\n";
        vtu << tag.str();
        metas.push_back({"",
                         static_cast<uint32_t>(offsets.size() * sizeof(uint32_t)),
                         reinterpret_cast<const char*>(offsets.data())});
        offset_bytes += 4 + static_cast<uint32_t>(offsets.size() * sizeof(uint32_t));
      }
      {
        std::ostringstream tag;
        tag << "        <DataArray type=\"UInt8\" Name=\"types\""
               " format=\"appended\" offset=\"" << offset_bytes << "\"/>\n";
        vtu << tag.str();
        metas.push_back({"",
                         static_cast<uint32_t>(types.size() * sizeof(uint8_t)),
                         reinterpret_cast<const char*>(types.data())});
        offset_bytes += 4 + static_cast<uint32_t>(types.size() * sizeof(uint8_t));
      }
      vtu << "      </Cells>\n";

      // ── Binary AppendedData block ──────────────────────────────────────
      vtu << "    </Piece>\n  </UnstructuredGrid>\n";
      vtu << "  <AppendedData encoding=\"raw\">\n_";
      for (auto& m : metas) {
        uint32_t sz = m.nbytes;
        vtu.write(reinterpret_cast<const char*>(&sz), sizeof(uint32_t));
        vtu.write(m.data, sz);
      }
      vtu << "\n  </AppendedData>\n</VTKFile>\n";
      vtu.close();
    }  // end omp parallel for

    // Write the parallel index file for this substance on the main thread,
    // after all piece files are complete.
    WriteDiffusionPvtu(vd.name, vd.concentration, vd.gradient,
                       static_cast<int>(num_pieces));
  }  // end for each visualize_diffusion entry
}

// -----------------------------------------------------------------------------
/// Write the parallel diffusion index file (diffusion_{name}_{step}.pvtu).
///
/// Declares the PCellData schema so that ParaView discovers the concentration
/// and gradient arrays when it opens the .pvtu file, before loading any piece.
void StandaloneVtuExporter::WriteDiffusionPvtu(const std::string& name,
                                               bool has_concentration,
                                               bool has_gradient,
                                               int pieces) const {
  const char* rt = (sizeof(real_t) == 4) ? "Float32" : "Float64";

  std::ostringstream pvtu_name;
  pvtu_name << output_dir_ << "/diffusion_" << name << "_" << step_ << ".pvtu";
  std::ofstream pvtu(pvtu_name.str());

  pvtu << "<?xml version=\"1.0\"?>\n";
  pvtu << "<VTKFile type=\"PUnstructuredGrid\" version=\"0.1\""
          " byte_order=\"LittleEndian\">\n";
  pvtu << "  <PUnstructuredGrid>\n";

  // PCellData mirrors the CellData schema from the .vtu piece files.
  pvtu << "    <PCellData>\n";
  if (has_concentration)
    pvtu << "      <PDataArray type=\"" << rt
         << "\" Name=\"Substance Concentration\" NumberOfComponents=\"1\"/>\n";
  if (has_gradient)
    pvtu << "      <PDataArray type=\"" << rt
         << "\" Name=\"Diffusion Gradient\" NumberOfComponents=\"3\"/>\n";
  pvtu << "    </PCellData>\n";

  // PPoints declares the coordinate schema (Float64, 3 components).
  pvtu << "    <PPoints>\n";
  pvtu << "      <PDataArray type=\"Float64\" NumberOfComponents=\"3\"/>\n";
  pvtu << "    </PPoints>\n";

  // One <Piece> per Z-slab .vtu file, using relative paths.
  for (int i = 0; i < pieces; ++i)
    pvtu << "    <Piece Source=\"diffusion_" << name << "_"
         << step_ << "_p" << i << ".vtu\"/>\n";

  pvtu << "  </PUnstructuredGrid>\n</VTKFile>\n";
  pvtu.close();
}

}  // namespace bdm
