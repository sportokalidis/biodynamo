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

#include "core/visualization/standalone/vtk_independent_vti_writer.h"

#include <iomanip>
#include <sstream>

#include "core/diffusion/diffusion_grid.h"
#include "core/real_t.h"
#include "core/util/log.h"
#include "core/util/string.h"

namespace bdm {

// -----------------------------------------------------------------------------
void VtkIndependentVtiWriter::WriteDiffusionGrid(const std::string& filename,
                                         const DiffusionGrid* grid) {
  std::ofstream file(filename);
  if (!file.is_open()) {
    Log::Error("VtkIndependentVtiWriter", "Failed to open file: ", filename);
    return;
  }

  // Get grid dimensions and properties
  auto dimensions = grid->GetDimensions();
  auto resolution = grid->GetResolution();
  real_t box_length = grid->GetBoxLength();
  
  // Calculate extent (VTK uses node-centered data)
  std::array<int, 6> extent;
  extent[0] = dimensions[0];  // x min
  extent[1] = dimensions[1];  // x max
  extent[2] = dimensions[2];  // y min
  extent[3] = dimensions[3];  // y max
  extent[4] = dimensions[4];  // z min
  extent[5] = dimensions[5];  // z max
  
  // Calculate origin and spacing
  std::array<real_t, 3> origin = {
    static_cast<real_t>(dimensions[0]) * box_length,
    static_cast<real_t>(dimensions[2]) * box_length,
    static_cast<real_t>(dimensions[4]) * box_length
  };
  
  std::array<real_t, 3> spacing = {box_length, box_length, box_length};
  uint64_t num_points = resolution * resolution * resolution;

  WriteVtiHeader(file, extent, spacing, origin, num_points);
  WritePointData(file, grid);
  WriteVtiFooter(file);

  file.close();
}

// -----------------------------------------------------------------------------
void VtkIndependentVtiWriter::WritePvti(const std::string& filename,
                                 const std::string& file_prefix,
                                 uint64_t num_pieces,
                                 const std::array<int, 6>& whole_extent) const {
  std::ofstream file(filename);
  if (!file.is_open()) {
    Log::Error("VtkIndependentVtiWriter", "Failed to open file: ", filename);
    return;
  }

  // Create default spacing and origin for PVTI
  std::array<real_t, 3> spacing = {1.0, 1.0, 1.0};
  std::array<real_t, 3> origin = {0.0, 0.0, 0.0};
  
  WritePvtiHeader(file, whole_extent, spacing, origin);

  // Write piece references
  for (uint64_t i = 0; i < num_pieces; ++i) {
    file << "    <Piece Source=\"" << file_prefix << "_" << i << ".vti\"/>\n";
  }

  WritePvtiFooter(file);
  file.close();
}

// -----------------------------------------------------------------------------
void VtkIndependentVtiWriter::WriteVtiHeader(std::ofstream& file,
                                     const std::array<int, 6>& extent,
                                     const std::array<real_t, 3>& spacing,
                                     const std::array<real_t, 3>& origin,
                                     uint64_t num_points) const {
  file << "<?xml version=\"1.0\"?>\n";
  file << "<VTKFile type=\"ImageData\" version=\"1.0\" "
       << "byte_order=\"LittleEndian\">\n";
  file << "  <ImageData WholeExtent=\""
       << extent[0] << " " << extent[1] << " "
       << extent[2] << " " << extent[3] << " "
       << extent[4] << " " << extent[5] << "\" "
       << "Origin=\""
       << origin[0] << " " << origin[1] << " " << origin[2] << "\" "
       << "Spacing=\""
       << spacing[0] << " " << spacing[1] << " " << spacing[2] << "\">\n";
  file << "    <Piece Extent=\""
       << extent[0] << " " << extent[1] << " "
       << extent[2] << " " << extent[3] << " "
       << extent[4] << " " << extent[5] << "\">\n";
}

// -----------------------------------------------------------------------------
void VtkIndependentVtiWriter::WritePointData(std::ofstream& file, 
                                     const DiffusionGrid* grid) const {
  file << "      <PointData>\n";
  
  // Concentration data
  file << "        <DataArray type=\"Float" << GetRealSize() 
       << "\" Name=\"" << grid->GetContinuumName() << "_Concentration\" "
       << "NumberOfComponents=\"1\" format=\"ascii\">\n";
  
  auto dimensions = grid->GetDimensions();
  int nx = dimensions[1] - dimensions[0] + 1;
  int ny = dimensions[3] - dimensions[2] + 1;
  int nz = dimensions[5] - dimensions[4] + 1;
  
  for (int z = 0; z < nz; ++z) {
    for (int y = 0; y < ny; ++y) {
      for (int x = 0; x < nx; ++x) {
        Real3 coord = {
          static_cast<real_t>(dimensions[0] + x),
          static_cast<real_t>(dimensions[2] + y),
          static_cast<real_t>(dimensions[4] + z)
        };
        real_t concentration = grid->GetValue(coord);
        file << "          " << concentration << "\n";
      }
    }
  }
  
  file << "        </DataArray>\n";
  
  // Gradient data (if available)
  file << "        <DataArray type=\"Float" << GetRealSize() 
       << "\" Name=\"" << grid->GetContinuumName() << "_Gradient\" "
       << "NumberOfComponents=\"3\" format=\"ascii\">\n";
  
  for (int z = 0; z < nz; ++z) {
    for (int y = 0; y < ny; ++y) {
      for (int x = 0; x < nx; ++x) {
        Real3 coord = {
          static_cast<real_t>(dimensions[0] + x),
          static_cast<real_t>(dimensions[2] + y),
          static_cast<real_t>(dimensions[4] + z)
        };
        auto gradient = grid->GetGradient(coord);
        file << "          " << gradient[0] << " " << gradient[1] << " " 
             << gradient[2] << "\n";
      }
    }
  }
  
  file << "        </DataArray>\n";
  file << "      </PointData>\n";
}

// -----------------------------------------------------------------------------
void VtkIndependentVtiWriter::WriteVtiFooter(std::ofstream& file) const {
  file << "    </Piece>\n";
  file << "  </ImageData>\n";
  file << "</VTKFile>\n";
}

// -----------------------------------------------------------------------------
void VtkIndependentVtiWriter::WritePvtiHeader(std::ofstream& file,
                                      const std::array<int, 6>& whole_extent,
                                      const std::array<real_t, 3>& spacing,
                                      const std::array<real_t, 3>& origin) const {
  file << "<?xml version=\"1.0\"?>\n";
  file << "<VTKFile type=\"PImageData\" version=\"1.0\" "
       << "byte_order=\"LittleEndian\">\n";
  file << "  <PImageData WholeExtent=\""
       << whole_extent[0] << " " << whole_extent[1] << " "
       << whole_extent[2] << " " << whole_extent[3] << " "
       << whole_extent[4] << " " << whole_extent[5] << "\" "
       << "GhostLevel=\"0\">\n";
  
  // Point data arrays
  file << "    <PPointData>\n";
  file << "      <PDataArray type=\"Float" << GetRealSize() 
       << "\" Name=\"Concentration\" NumberOfComponents=\"1\"/>\n";
  file << "      <PDataArray type=\"Float" << GetRealSize() 
       << "\" Name=\"Gradient\" NumberOfComponents=\"3\"/>\n";
  file << "    </PPointData>\n";
}

// -----------------------------------------------------------------------------
void VtkIndependentVtiWriter::WritePvtiFooter(std::ofstream& file) const {
  file << "  </PImageData>\n";
  file << "</VTKFile>\n";
}

}  // namespace bdm
