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

}  // namespace bdm
