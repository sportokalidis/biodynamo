#include "core/visualization/standalone/standalone_vtu_exporter.h"

#include <fstream>
#include <sstream>
#include <vector>
#include <regex>

#include "TClass.h"
#include "TDataMember.h"

#include "core/agent/agent.h"
#include "core/agent/cell.h"
#include "core/resource_manager.h"
#include "core/simulation.h"
#include "core/param/param.h"
#include "core/diffusion/diffusion_grid.h"
#include "core/util/thread_info.h"

namespace bdm {

// Forward declarations for helpers used in WriteStep
static std::vector<std::string> LoadAdditionalMembers();
static bool ReadScalarMember(const Agent* agent, const std::string& name, double& out);

StandaloneVtuExporter::StandaloneVtuExporter(const std::string& output_dir)
    : output_dir_(output_dir) {}

StandaloneVtuExporter::~StandaloneVtuExporter() = default;

void StandaloneVtuExporter::WriteStep() {
  auto* sim = Simulation::GetActive();
  auto* rm = sim->GetResourceManager();

  // Collect agents and data into contiguous arrays for slicing
  std::vector<Agent*> agents;
  std::vector<double> points;
  std::vector<uint64_t> ids;
  rm->ForEachAgent([&](Agent* a) {
    agents.push_back(a);
    const auto& pos = a->GetPosition();
    points.push_back(pos[0]);
    points.push_back(pos[1]);
    points.push_back(pos[2]);
    ids.push_back(a->GetUid().GetIndex());
  });

  const size_t n = ids.size();
  // Precompute static fields
  std::vector<double> diam(n);
  std::vector<double> mass(n);
  std::vector<double> volume(n);
  std::vector<double> traction(n * 3);
  for (size_t i = 0; i < n; ++i) {
    Agent* agent = agents[i];
    diam[i] = agent->GetDiameter();
    if (auto* cell = dynamic_cast<Cell*>(agent)) {
      mass[i] = cell->GetMass();
      volume[i] = cell->GetVolume();
      const auto& tf = cell->GetTractorForce();
      traction[i * 3 + 0] = tf[0];
      traction[i * 3 + 1] = tf[1];
      traction[i * 3 + 2] = tf[2];
    } else {
      mass[i] = 0.0;
      volume[i] = 0.0;
      traction[i * 3 + 0] = 0.0;
      traction[i * 3 + 1] = 0.0;
      traction[i * 3 + 2] = 0.0;
    }
  }

  // Config-driven extra scalar fields
  auto extra = LoadAdditionalMembers();
  std::vector<std::vector<double>> extra_vals;
  extra_vals.resize(extra.size());
  for (size_t f = 0; f < extra.size(); ++f) {
    extra_vals[f].resize(n);
    for (size_t i = 0; i < n; ++i) {
      double val = 0.0;
      if (ReadScalarMember(agents[i], extra[f], val)) { extra_vals[f][i] = val; }
      else { extra_vals[f][i] = 0.0; }
    }
  }

  // Partition into pieces
  auto* tinfo = ThreadInfo::GetInstance();
  uint64_t max_threads = tinfo->GetMaxThreads();
  uint64_t num_pieces = std::max<uint64_t>(1, std::min<uint64_t>(max_threads, n));
  uint64_t per_piece = (n + num_pieces - 1) / num_pieces; // ceil

  // Write each piece in parallel
  #pragma omp parallel for schedule(static, 1)
  for (uint64_t p = 0; p < num_pieces; ++p) {
    uint64_t begin = p * per_piece;
    if (begin >= n) { continue; }
    uint64_t count = std::min<uint64_t>(per_piece, n - begin);

    std::ostringstream vtu_name;
    vtu_name << output_dir_ << "/agents_" << step_ << "_p" << p << ".vtu";
    std::ofstream vtu(vtu_name.str(), std::ios::binary);
    vtu << "<?xml version=\"1.0\"?>\n";
    vtu << "<VTKFile type=\"UnstructuredGrid\" version=\"0.1\" byte_order=\"LittleEndian\" header_type=\"UInt32\">\n";
    vtu << "  <UnstructuredGrid>\n";
    vtu << "    <Piece NumberOfPoints=\"" << count << "\" NumberOfCells=\"" << count << "\">\n";
    vtu << "      <PointData>\n";

    // Prepare appended arrays metadata and compute offsets
    struct ArrayMeta { std::string tag; uint32_t nbytes; const char* data; };
    std::vector<ArrayMeta> metas;
    uint32_t offset = 0;

    auto append_array = [&](const std::string& type, const std::string& name,
                            int components, const void* buf, size_t elems, size_t elem_size) {
      uint32_t bytes = static_cast<uint32_t>(elems * components * elem_size);
      std::ostringstream tag;
      tag << "        <DataArray type=\"" << type << "\" Name=\"" << name
          << "\" NumberOfComponents=\"" << components << "\" format=\"appended\" offset=\"" << offset << "\"/>\n";
      metas.push_back({tag.str(), bytes, reinterpret_cast<const char*>(buf)});
      // each appended chunk has a 4-byte length header
      offset += 4 + bytes;
    };

    // Cell_ID
    append_array("UInt64", "Cell_ID", 1, &ids[begin], count, sizeof(uint64_t));
    // Extra scalar fields
    for (size_t f = 0; f < extra.size(); ++f) {
      append_array("Float64", extra[f], 1, &extra_vals[f][begin], count, sizeof(double));
    }
    // Diameter, Mass, Volume
    append_array("Float64", "Diameter", 1, &diam[begin], count, sizeof(double));
    append_array("Float64", "Mass", 1, &mass[begin], count, sizeof(double));
    append_array("Float64", "Volume", 1, &volume[begin], count, sizeof(double));
    // TractionForce (3 components)
    append_array("Float64", "TractionForce", 3, &traction[begin * 3], count, sizeof(double));

    // Emit PointData tags
    for (auto& m : metas) { vtu << m.tag; }
    vtu << "      </PointData>\n";

    // Points
    size_t points_count = count; // tuples
    vtu << "      <Points>\n";
    {
      std::ostringstream tag;
      tag << "        <DataArray type=\"Float64\" NumberOfComponents=\"3\" format=\"appended\" offset=\"" << offset << "\"/>\n";
      vtu << tag.str();
      size_t elems = points_count * 3;
      metas.push_back({std::string(), static_cast<uint32_t>(elems * sizeof(double)), reinterpret_cast<const char*>(&points[begin * 3])});
      offset += 4 + static_cast<uint32_t>(elems * sizeof(double));
    }
    vtu << "      </Points>\n";

    // Cells arrays: connectivity, offsets, types
    std::vector<int32_t> conn(count);
    std::vector<int32_t> offs(count);
    std::vector<uint8_t> types_arr(count);
    for (uint64_t i = 0; i < count; ++i) { conn[i] = static_cast<int32_t>(i); offs[i] = static_cast<int32_t>(i + 1); types_arr[i] = static_cast<uint8_t>(1); }

    vtu << "      <Cells>\n";
    {
      std::ostringstream tag;
      tag << "        <DataArray type=\"Int32\" Name=\"connectivity\" format=\"appended\" offset=\"" << offset << "\"/>\n";
      vtu << tag.str();
      metas.push_back({std::string(), static_cast<uint32_t>(count * sizeof(int32_t)), reinterpret_cast<const char*>(conn.data())});
      offset += 4 + static_cast<uint32_t>(count * sizeof(int32_t));
    }
    {
      std::ostringstream tag;
      tag << "        <DataArray type=\"Int32\" Name=\"offsets\" format=\"appended\" offset=\"" << offset << "\"/>\n";
      vtu << tag.str();
      metas.push_back({std::string(), static_cast<uint32_t>(count * sizeof(int32_t)), reinterpret_cast<const char*>(offs.data())});
      offset += 4 + static_cast<uint32_t>(count * sizeof(int32_t));
    }
    {
      std::ostringstream tag;
      tag << "        <DataArray type=\"UInt8\" Name=\"types\" format=\"appended\" offset=\"" << offset << "\"/>\n";
      vtu << tag.str();
      metas.push_back({std::string(), static_cast<uint32_t>(count * sizeof(uint8_t)), reinterpret_cast<const char*>(types_arr.data())});
      offset += 4 + static_cast<uint32_t>(count * sizeof(uint8_t));
    }
    vtu << "      </Cells>\n";

    vtu << "    </Piece>\n";
    vtu << "  </UnstructuredGrid>\n";

    // Write appended data block
    vtu << "  <AppendedData encoding=\"raw\">\n";
    vtu << "_"; // required underscore prefix
    for (auto& m : metas) {
      uint32_t sz = m.nbytes;
      vtu.write(reinterpret_cast<const char*>(&sz), sizeof(uint32_t));
      vtu.write(m.data, sz);
    }
    vtu << "\n  </AppendedData>\n";
    vtu << "</VTKFile>\n";
    vtu.close();
  }

  // Emit PVTU referencing all pieces
  WritePvtu(static_cast<int>(num_pieces));
  step_++;
}

// Very lightweight parser to extract additional_data_members from bdm.toml
static std::vector<std::string> LoadAdditionalMembers() {
  std::vector<std::string> names;
  std::ifstream ifs("bdm.toml");
  if (!ifs) { return names; }
  std::string line;
  std::regex re(R"(additional_data_members\s*=\s*\[([^\]]+)\])");
  while (std::getline(ifs, line)) {
    std::smatch m;
    if (std::regex_search(line, m, re)) {
      std::string inner = m[1].str();
      std::regex name_re("\"([^\"]+)\"");
      std::sregex_iterator begin(inner.begin(), inner.end(), name_re);
      std::sregex_iterator end;
      for (std::sregex_iterator it = begin; it != end; ++it) {
        names.push_back((*it)[1].str());
      }
    }
  }
  return names;
}

// Helper to read a scalar numeric data member via ROOT reflection
static bool ReadScalarMember(const Agent* agent, const std::string& name, double& out) {
  TClass* cls = TClass::GetClass(typeid(*agent));
  if (!cls) { return false; }
  TDataMember* dm = cls->GetDataMember(name.c_str());
  if (!dm) { return false; }
  const char* base = reinterpret_cast<const char*>(agent);
  const char* addr = base + dm->GetOffset();
  std::string tname = dm->GetTypeName();
  if (tname == "double") { out = *reinterpret_cast<const double*>(addr); return true; }
  if (tname == "float")  { out = *reinterpret_cast<const float*>(addr);  return true; }
  if (tname == "int")    { out = *reinterpret_cast<const int*>(addr);    out = static_cast<double>(out); return true; }
  if (tname == "uint64_t" || tname == "unsigned long" || tname == "unsigned long long") {
    out = static_cast<double>(*reinterpret_cast<const unsigned long long*>(addr));
    return true;
  }
  return false;
}

void StandaloneVtuExporter::WritePvtu(int pieces) const {
  std::ostringstream pvtu_name;
  pvtu_name << output_dir_ << "/agents_" << step_ << ".pvtu";
  std::ofstream pvtu(pvtu_name.str());
  pvtu << "<?xml version=\"1.0\"?>\n";
  pvtu << "<VTKFile type=\"PUnstructuredGrid\" version=\"0.1\" byte_order=\"LittleEndian\">\n";
  pvtu << "  <PUnstructuredGrid>\n";
  // Mirror PointData arrays from VTU so ParaView knows available fields
  pvtu << "    <PPointData>\n";
  // Cell_ID
  pvtu << "      <PDataArray type=\"UInt64\" Name=\"Cell_ID\" NumberOfComponents=\"1\"/>\n";
  // Config-driven dynamic scalar fields
  {
    auto extra = LoadAdditionalMembers();
    for (const auto& field : extra) {
      pvtu << "      <PDataArray type=\"Float64\" Name=\"" << field
           << "\" NumberOfComponents=\"1\"/>\n";
    }
  }
  // Diameter, Mass, Volume
  pvtu << "      <PDataArray type=\"Float64\" Name=\"Diameter\" NumberOfComponents=\"1\"/>\n";
  pvtu << "      <PDataArray type=\"Float64\" Name=\"Mass\" NumberOfComponents=\"1\"/>\n";
  pvtu << "      <PDataArray type=\"Float64\" Name=\"Volume\" NumberOfComponents=\"1\"/>\n";
  // TractionForce (vector)
  pvtu << "      <PDataArray type=\"Float64\" Name=\"TractionForce\" NumberOfComponents=\"3\"/>\n";
  pvtu << "    </PPointData>\n";
  pvtu << "    <PPoints>\n";
  pvtu << "      <PDataArray type=\"Float64\" NumberOfComponents=\"3\"/>\n";
  pvtu << "    </PPoints>\n";
  for (int i = 0; i < pieces; ++i) {
    pvtu << "    <Piece Source=\"agents_" << step_ << "_p" << i << ".vtu\"/>\n";
  }
  pvtu << "  </PUnstructuredGrid>\n";
  pvtu << "</VTKFile>\n";
  pvtu.close();
}

void StandaloneVtuExporter::WriteDiffusionStep() {
  auto* sim = Simulation::GetActive();
  auto* rm = sim->GetResourceManager();
  auto* param = sim->GetParam();

  for (const auto& vd : param->visualize_diffusion) {
    auto* grid = rm->GetDiffusionGrid(vd.name);
    if (!grid) { continue; }
    const auto num_boxes = grid->GetNumBoxesArray();
    const auto dims = grid->GetDimensions();
    const auto box = grid->GetBoxLength();
    const uint64_t total_cells = grid->GetNumBoxes();

    // Build node grid (num_boxes + 1 per dimension)
    const size_t nx = static_cast<size_t>(num_boxes[0]);
    const size_t ny = static_cast<size_t>(num_boxes[1]);
    const size_t nz = static_cast<size_t>(num_boxes[2]);
    const size_t nx_nodes = nx + 1;
    const size_t ny_nodes = ny + 1;
    const size_t nz_nodes = nz + 1;
    const uint64_t total_points = static_cast<uint64_t>(nx_nodes * ny_nodes * nz_nodes);

    const real_t* conc = grid->GetAllConcentrations();
    const real_t* grad = grid->GetAllGradients();

    // Decide number of pieces based on available threads and Z boxes
    auto* tinfo = ThreadInfo::GetInstance();
    uint64_t max_threads = tinfo->GetMaxThreads();
    uint64_t num_pieces = std::min<uint64_t>(std::max<uint64_t>(1, nz), std::max<uint64_t>(1, max_threads));
    // compute boxes per piece (ceil) and handle last remainder
    uint64_t boxes_per_piece = (nz + num_pieces - 1) / num_pieces; // ceil(nz/num_pieces)

    // Parallel piece generation and writing
    #pragma omp parallel for schedule(static, 1)
    for (uint64_t p = 0; p < num_pieces; ++p) {
      uint64_t k_begin = p * boxes_per_piece;
      if (k_begin >= nz) { continue; }
      uint64_t k_len = std::min<uint64_t>(boxes_per_piece, nz - k_begin);

      // Points for this piece: nx_nodes * ny_nodes * (k_len+1)
      const size_t nz_nodes_piece = static_cast<size_t>(k_len + 1);
      std::vector<double> points;
      points.reserve(static_cast<size_t>(nx_nodes * ny_nodes * nz_nodes_piece * 3));
      for (size_t kk = 0; kk < nz_nodes_piece; ++kk) {
        double z = dims[4] + box * static_cast<double>(k_begin + kk);
        for (size_t j = 0; j < ny_nodes; ++j) {
          double y = dims[2] + box * static_cast<double>(j);
          for (size_t i = 0; i < nx_nodes; ++i) {
            double x = dims[0] + box * static_cast<double>(i);
            points.push_back(x);
            points.push_back(y);
            points.push_back(z);
          }
        }
      }

      // Local node id within piece
      auto local_node_id = [&](size_t i, size_t j, size_t kk) -> uint64_t {
        return static_cast<uint64_t>(i + j * nx_nodes + kk * nx_nodes * ny_nodes);
      };

      // Connectivity for this piece
      const uint64_t piece_cells = static_cast<uint64_t>(nx * ny * k_len);
      std::vector<uint32_t> connectivity;
      connectivity.reserve(static_cast<size_t>(piece_cells * 8));
      std::vector<uint32_t> offsets;
      offsets.reserve(static_cast<size_t>(piece_cells));
      std::vector<uint8_t> types;
      types.reserve(static_cast<size_t>(piece_cells));

      uint64_t local_cell_counter = 0;
      for (size_t k = 0; k < k_len; ++k) {
        for (size_t j = 0; j < ny; ++j) {
          for (size_t i = 0; i < nx; ++i) {
            // VTK_VOXEL corner ordering using local node ids
            uint64_t n0 = local_node_id(i,     j,     k);
            uint64_t n1 = local_node_id(i + 1, j,     k);
            uint64_t n2 = local_node_id(i + 1, j + 1, k);
            uint64_t n3 = local_node_id(i,     j + 1, k);
            uint64_t n4 = local_node_id(i,     j,     k + 1);
            uint64_t n5 = local_node_id(i + 1, j,     k + 1);
            uint64_t n6 = local_node_id(i + 1, j + 1, k + 1);
            uint64_t n7 = local_node_id(i,     j + 1, k + 1);
            connectivity.push_back(static_cast<uint32_t>(n0));
            connectivity.push_back(static_cast<uint32_t>(n1));
            connectivity.push_back(static_cast<uint32_t>(n2));
            connectivity.push_back(static_cast<uint32_t>(n3));
            connectivity.push_back(static_cast<uint32_t>(n4));
            connectivity.push_back(static_cast<uint32_t>(n5));
            connectivity.push_back(static_cast<uint32_t>(n6));
            connectivity.push_back(static_cast<uint32_t>(n7));
            local_cell_counter++;
            offsets.push_back(static_cast<uint32_t>(local_cell_counter * 8));
            types.push_back(static_cast<uint8_t>(11));
          }
        }
      }

      // Prepare file name for this piece (binary appended)
      std::ostringstream vtu_name;
      vtu_name << output_dir_ << "/diffusion_" << vd.name << "_" << step_ << "_p" << p << ".vtu";
      std::ofstream vtu(vtu_name.str(), std::ios::binary);
      vtu << "<?xml version=\"1.0\"?>\n";
      vtu << "<VTKFile type=\"UnstructuredGrid\" version=\"0.1\" byte_order=\"LittleEndian\" header_type=\"UInt32\">\n";
      vtu << "  <UnstructuredGrid>\n";
      vtu << "    <Piece NumberOfPoints=\"" << (nx_nodes * ny_nodes * nz_nodes_piece) << "\" NumberOfCells=\"" << piece_cells << "\">\n";

      // Compute type string based on real_t
      const char* rt = (sizeof(real_t) == 4) ? "Float32" : "Float64";

      // Accumulate appended arrays metadata
      struct ArrayMeta { std::string tag; uint32_t nbytes; const char* data; };
      std::vector<ArrayMeta> metas;
      uint32_t offset_bytes = 0;
      auto append_array = [&](const std::string& type, const std::string& name,
                              int components, const void* buf, size_t elems, size_t elem_size,
                              const char* name_attr = nullptr, bool is_points = false,
                              const char* cells_name = nullptr) {
        uint32_t bytes = static_cast<uint32_t>(elems * components * elem_size);
        std::ostringstream tag;
        if (is_points) {
          tag << "        <DataArray type=\"" << type << "\" NumberOfComponents=\"3\" format=\"appended\" offset=\"" << offset_bytes << "\"/>\n";
        } else if (cells_name) {
          tag << "        <DataArray type=\"" << type << "\" Name=\"" << cells_name
              << "\" format=\"appended\" offset=\"" << offset_bytes << "\"/>\n";
        } else {
          tag << "        <DataArray type=\"" << type << "\" Name=\"" << name
              << "\" NumberOfComponents=\"" << components << "\" format=\"appended\" offset=\"" << offset_bytes << "\"/>\n";
        }
        metas.push_back({tag.str(), bytes, reinterpret_cast<const char*>(buf)});
        offset_bytes += 4 + bytes; // 4-byte header per chunk
      };

      // CellData
      vtu << "      <CellData>\n";
      if (vd.concentration) {
        uint64_t start = (k_begin) * static_cast<uint64_t>(nx * ny);
        append_array(rt, "Substance Concentration", 1, &conc[start], piece_cells, sizeof(real_t));
      }
      if (vd.gradient) {
        uint64_t startg = (k_begin) * static_cast<uint64_t>(nx * ny) * 3;
        append_array(rt, "Diffusion Gradient", 3, &grad[startg], piece_cells, sizeof(real_t));
      }
      // Emit tags
      for (auto& m : metas) { vtu << m.tag; }
      vtu << "      </CellData>\n";

      // Points
      vtu << "      <Points>\n";
      {
        size_t elems = points.size(); // already 3 * tuples
        std::ostringstream tag;
        tag << "        <DataArray type=\"Float64\" NumberOfComponents=\"3\" format=\"appended\" offset=\"" << offset_bytes << "\"/>\n";
        vtu << tag.str();
        metas.push_back({std::string(), static_cast<uint32_t>(elems * sizeof(double)), reinterpret_cast<const char*>(points.data())});
        offset_bytes += 4 + static_cast<uint32_t>(elems * sizeof(double));
      }
      vtu << "      </Points>\n";

      // Cells
      vtu << "      <Cells>\n";
      {
        std::ostringstream tag;
        tag << "        <DataArray type=\"Int32\" Name=\"connectivity\" format=\"appended\" offset=\"" << offset_bytes << "\"/>\n";
        vtu << tag.str();
        metas.push_back({std::string(), static_cast<uint32_t>(connectivity.size() * sizeof(int32_t)), reinterpret_cast<const char*>(connectivity.data())});
        offset_bytes += 4 + static_cast<uint32_t>(connectivity.size() * sizeof(int32_t));
      }
      {
        std::ostringstream tag;
        tag << "        <DataArray type=\"Int32\" Name=\"offsets\" format=\"appended\" offset=\"" << offset_bytes << "\"/>\n";
        vtu << tag.str();
        metas.push_back({std::string(), static_cast<uint32_t>(offsets.size() * sizeof(int32_t)), reinterpret_cast<const char*>(offsets.data())});
        offset_bytes += 4 + static_cast<uint32_t>(offsets.size() * sizeof(int32_t));
      }
      {
        std::ostringstream tag;
        tag << "        <DataArray type=\"UInt8\" Name=\"types\" format=\"appended\" offset=\"" << offset_bytes << "\"/>\n";
        vtu << tag.str();
        metas.push_back({std::string(), static_cast<uint32_t>(types.size() * sizeof(uint8_t)), reinterpret_cast<const char*>(types.data())});
        offset_bytes += 4 + static_cast<uint32_t>(types.size() * sizeof(uint8_t));
      }
      vtu << "      </Cells>\n";

      vtu << "    </Piece>\n";
      vtu << "  </UnstructuredGrid>\n";

      // Appended data block
      vtu << "  <AppendedData encoding=\"raw\">\n";
      vtu << "_";
      for (auto& m : metas) {
        uint32_t sz = m.nbytes;
        vtu.write(reinterpret_cast<const char*>(&sz), sizeof(uint32_t));
        vtu.write(m.data, sz);
      }
      vtu << "\n  </AppendedData>\n";
      vtu << "</VTKFile>\n";
      vtu.close();
    }

    // Emit matching PVTU header listing all pieces
    WriteDiffusionPvtu(vd.name, vd.concentration, vd.gradient, static_cast<int>(num_pieces));
  }
}

void StandaloneVtuExporter::WriteDiffusionPvtu(const std::string& name,
                                              bool has_concentration,
                                              bool has_gradient,
                                              int pieces) const {
  const char* rt = (sizeof(real_t) == 4) ? "Float32" : "Float64";
  std::ostringstream pvtu_name;
  pvtu_name << output_dir_ << "/diffusion_" << name << "_" << step_ << ".pvtu";
  std::ofstream pvtu(pvtu_name.str());
  pvtu << "<?xml version=\"1.0\"?>\n";
  pvtu << "<VTKFile type=\"PUnstructuredGrid\" version=\"0.1\" byte_order=\"LittleEndian\">\n";
  pvtu << "  <PUnstructuredGrid>\n";
  pvtu << "    <PCellData>\n";
  if (has_concentration) {
    pvtu << "      <PDataArray type=\"" << rt << "\" Name=\"Substance Concentration\" NumberOfComponents=\"1\"/>\n";
  }
  if (has_gradient) {
    pvtu << "      <PDataArray type=\"" << rt << "\" Name=\"Diffusion Gradient\" NumberOfComponents=\"3\"/>\n";
  }
  pvtu << "    </PCellData>\n";
  pvtu << "    <PPoints>\n";
  pvtu << "      <PDataArray type=\"Float64\" NumberOfComponents=\"3\"/>\n";
  pvtu << "    </PPoints>\n";
  for (int i = 0; i < pieces; ++i) {
    pvtu << "    <Piece Source=\"diffusion_" << name << "_" << step_ << "_p" << i << ".vtu\"/>\n";
  }
  pvtu << "  </PUnstructuredGrid>\n";
  pvtu << "</VTKFile>\n";
  pvtu.close();
}

}  // namespace bdm
