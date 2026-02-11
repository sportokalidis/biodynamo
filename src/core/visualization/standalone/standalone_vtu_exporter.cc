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

  std::vector<double> points;
  std::vector<uint64_t> ids;

  rm->ForEachAgent([&](Agent* a) {
    const auto& pos = a->GetPosition();
    points.push_back(pos[0]);
    points.push_back(pos[1]);
    points.push_back(pos[2]);
    ids.push_back(a->GetUid().GetIndex());
  });

  std::ostringstream vtu_name;
  vtu_name << output_dir_ << "/agents_" << step_ << ".vtu";
  std::ofstream vtu(vtu_name.str());
  vtu << "<?xml version=\"1.0\"?>\n";
  vtu << "<VTKFile type=\"UnstructuredGrid\" version=\"0.1\" byte_order=\"LittleEndian\">\n";
  vtu << "  <UnstructuredGrid>\n";
  vtu << "    <Piece NumberOfPoints=\"" << ids.size() << "\" NumberOfCells=\"" << ids.size() << "\">\n";
  vtu << "      <PointData>\n";
  // Cell_ID
  vtu << "        <DataArray type=\"UInt64\" Name=\"Cell_ID\" NumberOfComponents=\"1\" format=\"ascii\">\n";
  for (auto id : ids) { vtu << id << " "; }
  vtu << "\n        </DataArray>\n";
  // Config-driven dynamic scalar fields via ROOT reflection
  {
    auto extra = LoadAdditionalMembers();
    for (const auto& field : extra) {
      vtu << "        <DataArray type=\"Float64\" Name=\"" << field
          << "\" NumberOfComponents=\"1\" format=\"ascii\">\n";
      rm->ForEachAgent([&](Agent* agent) {
        double val = 0.0;
        if (ReadScalarMember(agent, field, val)) { vtu << ' ' << val; }
        else { vtu << ' ' << 0.0; }
      });
      vtu << "\n        </DataArray>\n";
    }
  }
  // Diameter
  vtu << "        <DataArray type=\"Float64\" Name=\"Diameter\" NumberOfComponents=\"1\" format=\"ascii\">\n";
  rm->ForEachAgent([&](Agent* agent) { vtu << ' ' << agent->GetDiameter(); });
  vtu << "\n        </DataArray>\n";
  // Mass (if Cell)
  vtu << "        <DataArray type=\"Float64\" Name=\"Mass\" NumberOfComponents=\"1\" format=\"ascii\">\n";
  rm->ForEachAgent([&](Agent* agent) {
    if (auto* cell = dynamic_cast<Cell*>(agent)) { vtu << ' ' << cell->GetMass(); }
    else { vtu << ' ' << 0.0; }
  });
  vtu << "\n        </DataArray>\n";
  // Volume (if Cell)
  vtu << "        <DataArray type=\"Float64\" Name=\"Volume\" NumberOfComponents=\"1\" format=\"ascii\">\n";
  rm->ForEachAgent([&](Agent* agent) {
    if (auto* cell = dynamic_cast<Cell*>(agent)) { vtu << ' ' << cell->GetVolume(); }
    else { vtu << ' ' << 0.0; }
  });
  vtu << "\n        </DataArray>\n";
  // TractionForce (vector for Cell)
  vtu << "        <DataArray type=\"Float64\" Name=\"TractionForce\" NumberOfComponents=\"3\" format=\"ascii\">\n";
  rm->ForEachAgent([&](Agent* agent) {
    if (auto* cell = dynamic_cast<Cell*>(agent)) {
      const auto& tf = cell->GetTractorForce();
      vtu << ' ' << tf[0] << ' ' << tf[1] << ' ' << tf[2];
    } else { vtu << ' ' << 0.0 << ' ' << 0.0 << ' ' << 0.0; }
  });
  vtu << "\n        </DataArray>\n";
  vtu << "      </PointData>\n";
  vtu << "      <Points>\n";
  vtu << "        <DataArray type=\"Float64\" NumberOfComponents=\"3\" format=\"ascii\">\n";
  for (size_t i = 0; i < points.size(); i += 3) {
    vtu << points[i] << " " << points[i + 1] << " " << points[i + 2] << "\n";
  }
  vtu << "        </DataArray>\n";
  vtu << "      </Points>\n";
  vtu << "      <Cells>\n";
  vtu << "        <DataArray type=\"Int32\" Name=\"connectivity\" format=\"ascii\">\n";
  for (size_t i = 0; i < ids.size(); ++i) { vtu << i << " "; }
  vtu << "\n        </DataArray>\n";
  vtu << "        <DataArray type=\"Int32\" Name=\"offsets\" format=\"ascii\">\n";
  for (size_t i = 1; i <= ids.size(); ++i) { vtu << i << " "; }
  vtu << "\n        </DataArray>\n";
  vtu << "        <DataArray type=\"UInt8\" Name=\"types\" format=\"ascii\">\n";
  for (size_t i = 0; i < ids.size(); ++i) { vtu << 1 << " "; }
  vtu << "\n        </DataArray>\n";
  vtu << "      </Cells>\n";
  vtu << "    </Piece>\n";
  vtu << "  </UnstructuredGrid>\n";
  vtu << "</VTKFile>\n";
  vtu.close();

  WritePvtu(1);
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
    pvtu << "    <Piece Source=\"agents_" << step_ << ".vtu\"/>\n";
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

    std::vector<double> points;
    points.reserve(total_points * 3);
    for (size_t k = 0; k < nz_nodes; ++k) {
      double z = dims[4] + box * static_cast<double>(k);
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

    // Create voxel cells connectivity (VTK_VOXEL = 11)
    auto node_id = [&](size_t i, size_t j, size_t k) -> uint64_t {
      return static_cast<uint64_t>(i + j * nx_nodes + k * nx_nodes * ny_nodes);
    };
    std::vector<uint32_t> connectivity;
    connectivity.reserve(total_cells * 8);
    std::vector<uint32_t> offsets;
    offsets.reserve(total_cells);
    std::vector<uint8_t> types;
    types.reserve(total_cells);

    uint64_t cell_counter = 0;
    for (size_t k = 0; k < nz; ++k) {
      for (size_t j = 0; j < ny; ++j) {
        for (size_t i = 0; i < nx; ++i) {
          // VTK_VOXEL corner ordering
          uint64_t n0 = node_id(i,     j,     k);
          uint64_t n1 = node_id(i + 1, j,     k);
          uint64_t n2 = node_id(i + 1, j + 1, k);
          uint64_t n3 = node_id(i,     j + 1, k);
          uint64_t n4 = node_id(i,     j,     k + 1);
          uint64_t n5 = node_id(i + 1, j,     k + 1);
          uint64_t n6 = node_id(i + 1, j + 1, k + 1);
          uint64_t n7 = node_id(i,     j + 1, k + 1);
          connectivity.push_back(static_cast<uint32_t>(n0));
          connectivity.push_back(static_cast<uint32_t>(n1));
          connectivity.push_back(static_cast<uint32_t>(n2));
          connectivity.push_back(static_cast<uint32_t>(n3));
          connectivity.push_back(static_cast<uint32_t>(n4));
          connectivity.push_back(static_cast<uint32_t>(n5));
          connectivity.push_back(static_cast<uint32_t>(n6));
          connectivity.push_back(static_cast<uint32_t>(n7));
          cell_counter++;
          offsets.push_back(static_cast<uint32_t>(cell_counter * 8));
          types.push_back(static_cast<uint8_t>(11));
        }
      }
    }

    const real_t* conc = grid->GetAllConcentrations();
    const real_t* grad = grid->GetAllGradients();

    std::ostringstream vtu_name;
    vtu_name << output_dir_ << "/diffusion_" << vd.name << "_" << step_ << ".vtu";
    std::ofstream vtu(vtu_name.str());
    vtu << "<?xml version=\"1.0\"?>\n";
    vtu << "<VTKFile type=\"UnstructuredGrid\" version=\"0.1\" byte_order=\"LittleEndian\">\n";
    vtu << "  <UnstructuredGrid>\n";
    vtu << "    <Piece NumberOfPoints=\"" << total_points << "\" NumberOfCells=\"" << total_cells << "\">\n";
    vtu << "      <CellData>\n";
    if (vd.concentration) {
      vtu << "        <DataArray type=\"" << (sizeof(real_t)==4?"Float32":"Float64") << "\" Name=\"Substance Concentration\" NumberOfComponents=\"1\" format=\"ascii\">\n";
      for (uint64_t i = 0; i < total_cells; ++i) { vtu << ' ' << static_cast<double>(conc[i]); }
      vtu << "\n        </DataArray>\n";
    }
    if (vd.gradient) {
      vtu << "        <DataArray type=\"" << (sizeof(real_t)==4?"Float32":"Float64") << "\" Name=\"Diffusion Gradient\" NumberOfComponents=\"3\" format=\"ascii\">\n";
      for (uint64_t i = 0; i < total_cells; ++i) {
        const double gx = static_cast<double>(grad[i*3+0]);
        const double gy = static_cast<double>(grad[i*3+1]);
        const double gz = static_cast<double>(grad[i*3+2]);
        vtu << ' ' << gx << ' ' << gy << ' ' << gz;
      }
      vtu << "\n        </DataArray>\n";
    }
    vtu << "      </CellData>\n";
    vtu << "      <Points>\n";
    vtu << "        <DataArray type=\"Float64\" NumberOfComponents=\"3\" format=\"ascii\">\n";
    for (size_t i = 0; i < points.size(); i += 3) {
      vtu << points[i] << " " << points[i + 1] << " " << points[i + 2] << "\n";
    }
    vtu << "        </DataArray>\n";
    vtu << "      </Points>\n";
    vtu << "      <Cells>\n";
    vtu << "        <DataArray type=\"Int32\" Name=\"connectivity\" format=\"ascii\">\n";
    for (size_t idx = 0; idx < connectivity.size(); ++idx) { vtu << connectivity[idx] << ' '; }
    vtu << "\n        </DataArray>\n";
    vtu << "        <DataArray type=\"Int32\" Name=\"offsets\" format=\"ascii\">\n";
    for (size_t idx = 0; idx < offsets.size(); ++idx) { vtu << offsets[idx] << ' '; }
    vtu << "\n        </DataArray>\n";
    vtu << "        <DataArray type=\"UInt8\" Name=\"types\" format=\"ascii\">\n";
    for (size_t idx = 0; idx < types.size(); ++idx) { vtu << static_cast<int>(types[idx]) << ' '; }
    vtu << "\n        </DataArray>\n";
    vtu << "      </Cells>\n";
    vtu << "    </Piece>\n";
    vtu << "  </UnstructuredGrid>\n";
    vtu << "</VTKFile>\n";
    vtu.close();

    // Emit matching PVTU header so ParaView can load in parallel mode
    WriteDiffusionPvtu(vd.name, vd.concentration, vd.gradient, 1);
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
    pvtu << "    <Piece Source=\"diffusion_" << name << "_" << step_ << ".vtu\"/>\n";
  }
  pvtu << "  </PUnstructuredGrid>\n";
  pvtu << "</VTKFile>\n";
  pvtu.close();
}

}  // namespace bdm
