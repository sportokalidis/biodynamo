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

#include "core/visualization/standalone/vtk_independent_vtu_writer.h"

#include <iomanip>
#include <sstream>

#include "core/agent/cell.h"
#include "core/diffusion/diffusion_grid.h"
#include "core/util/log.h"
#include "core/util/string.h"

namespace bdm {

// -----------------------------------------------------------------------------
void VtkIndependentVtuWriter::WriteAgents(const std::string& filename,
                                  const std::vector<Agent*>& agents) {
  std::ofstream file(filename);
  if (!file.is_open()) {
    Log::Error("SimpleVtuWriter", "Failed to open file: ", filename);
    return;
  }

  WriteVtuHeader(file, agents.size());
  WritePoints(file, agents);
  WritePointData(file, agents);
  WriteCells(file, agents.size());
  WriteVtuFooter(file);

  file.close();
}

// -----------------------------------------------------------------------------
void VtkIndependentVtuWriter::WritePvtu(const std::string& filename,
                                 const std::string& file_prefix,
                                 uint64_t num_pieces) {
  std::ofstream file(filename);
  if (!file.is_open()) {
    Log::Error("SimpleVtuWriter", "Failed to open file: ", filename);
    return;
  }

  WritePvtuHeader(file);

  // Write piece references
  for (uint64_t i = 0; i < num_pieces; ++i) {
    file << "    <Piece Source=\"" << file_prefix << "_" << i << ".vtu\"/>\n";
  }

  WritePvtuFooter(file);
  file.close();
}

// -----------------------------------------------------------------------------
void VtkIndependentVtuWriter::WriteVtuHeader(std::ofstream& file, uint64_t num_points) {
  file << "<?xml version=\"1.0\"?>\n";
  file << "<VTKFile type=\"UnstructuredGrid\" version=\"1.0\" "
       << "byte_order=\"LittleEndian\">\n";
  file << "  <UnstructuredGrid>\n";
  file << "    <Piece NumberOfPoints=\"" << num_points 
       << "\" NumberOfCells=\"" << num_points << "\">\n";
}

// -----------------------------------------------------------------------------
void VtkIndependentVtuWriter::WritePoints(std::ofstream& file, 
                                  const std::vector<Agent*>& agents) {
  file << "      <Points>\n";
  file << "        <DataArray type=\"Float" << GetRealSize() 
       << "\" NumberOfComponents=\"3\" format=\"ascii\">\n";
  
  for (const auto* agent : agents) {
    const auto& pos = agent->GetPosition();
    file << "          " << pos[0] << " " << pos[1] << " " << pos[2] << "\n";
  }
  
  file << "        </DataArray>\n";
  file << "      </Points>\n";
}

// -----------------------------------------------------------------------------
void VtkIndependentVtuWriter::WritePointData(std::ofstream& file, 
                                      const std::vector<Agent*>& agents) {
  file << "      <PointData>\n";
  
  // Agent ID
  file << "        <DataArray type=\"UInt64\" Name=\"AgentID\" "
       << "NumberOfComponents=\"1\" format=\"ascii\">\n";
  for (const auto* agent : agents) {
    file << "          " << agent->GetUid().GetIndex() << "\n";
  }
  file << "        </DataArray>\n";
  
  // Diameter
  file << "        <DataArray type=\"Float" << GetRealSize() 
       << "\" Name=\"Diameter\" NumberOfComponents=\"1\" format=\"ascii\">\n";
  for (const auto* agent : agents) {
    file << "          " << agent->GetDiameter() << "\n";
  }
  file << "        </DataArray>\n";
  
  // Position (for reference)
  file << "        <DataArray type=\"Float" << GetRealSize() 
       << "\" Name=\"Position\" NumberOfComponents=\"3\" format=\"ascii\">\n";
  for (const auto* agent : agents) {
    const auto& pos = agent->GetPosition();
    file << "          " << pos[0] << " " << pos[1] << " " << pos[2] << "\n";
  }
  file << "        </DataArray>\n";
  
  // Cell-specific data
  file << "        <DataArray type=\"Float" << GetRealSize() 
       << "\" Name=\"Volume\" NumberOfComponents=\"1\" format=\"ascii\">\n";
  for (const auto* agent : agents) {
    if (const auto* cell = dynamic_cast<const Cell*>(agent)) {
      file << "          " << cell->GetVolume() << "\n";
    } else {
      file << "          0.0\n";
    }
  }
  file << "        </DataArray>\n";
  
  file << "        <DataArray type=\"Float" << GetRealSize() 
       << "\" Name=\"Mass\" NumberOfComponents=\"1\" format=\"ascii\">\n";
  for (const auto* agent : agents) {
    if (const auto* cell = dynamic_cast<const Cell*>(agent)) {
      file << "          " << cell->GetMass() << "\n";
    } else {
      file << "          0.0\n";
    }
  }
  file << "        </DataArray>\n";
  
  file << "      </PointData>\n";
}

// -----------------------------------------------------------------------------
void VtkIndependentVtuWriter::WriteCells(std::ofstream& file, uint64_t num_cells) {
  file << "      <Cells>\n";
  
  // Connectivity (each point is its own cell)
  file << "        <DataArray type=\"UInt64\" Name=\"connectivity\" "
       << "format=\"ascii\">\n";
  for (uint64_t i = 0; i < num_cells; ++i) {
    file << "          " << i << "\n";
  }
  file << "        </DataArray>\n";
  
  // Offsets (cumulative count)
  file << "        <DataArray type=\"UInt64\" Name=\"offsets\" "
       << "format=\"ascii\">\n";
  for (uint64_t i = 1; i <= num_cells; ++i) {
    file << "          " << i << "\n";
  }
  file << "        </DataArray>\n";
  
  // Cell types (1 = vertex)
  file << "        <DataArray type=\"UInt8\" Name=\"types\" "
       << "format=\"ascii\">\n";
  for (uint64_t i = 0; i < num_cells; ++i) {
    file << "          1\n";
  }
  file << "        </DataArray>\n";
  
  file << "      </Cells>\n";
}

// -----------------------------------------------------------------------------
void VtkIndependentVtuWriter::WriteVtuFooter(std::ofstream& file) {
  file << "    </Piece>\n";
  file << "  </UnstructuredGrid>\n";
  file << "</VTKFile>\n";
}

// -----------------------------------------------------------------------------
void VtkIndependentVtuWriter::WritePvtuHeader(std::ofstream& file) {
  file << "<?xml version=\"1.0\"?>\n";
  file << "<VTKFile type=\"PUnstructuredGrid\" version=\"1.0\" "
       << "byte_order=\"LittleEndian\">\n";
  file << "  <PUnstructuredGrid GhostLevel=\"0\">\n";
  
  // Point data arrays
  file << "    <PPointData>\n";
  file << "      <PDataArray type=\"UInt64\" Name=\"AgentID\" "
       << "NumberOfComponents=\"1\"/>\n";
  file << "      <PDataArray type=\"Float" << GetRealSize() 
       << "\" Name=\"Diameter\" NumberOfComponents=\"1\"/>\n";
  file << "      <PDataArray type=\"Float" << GetRealSize() 
       << "\" Name=\"Position\" NumberOfComponents=\"3\"/>\n";
  file << "      <PDataArray type=\"Float" << GetRealSize() 
       << "\" Name=\"Volume\" NumberOfComponents=\"1\"/>\n";
  file << "      <PDataArray type=\"Float" << GetRealSize() 
       << "\" Name=\"Mass\" NumberOfComponents=\"1\"/>\n";
  file << "    </PPointData>\n";
  
  // Points
  file << "    <PPoints>\n";
  file << "      <PDataArray type=\"Float" << GetRealSize() 
       << "\" NumberOfComponents=\"3\"/>\n";
  file << "    </PPoints>\n";
}

// -----------------------------------------------------------------------------
void VtkIndependentVtuWriter::WritePvtuFooter(std::ofstream& file) {
  file << "  </PUnstructuredGrid>\n";
  file << "</VTKFile>\n";
}

}  // namespace bdm
