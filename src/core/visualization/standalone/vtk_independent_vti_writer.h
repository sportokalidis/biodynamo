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

#ifndef CORE_VISUALIZATION_STANDALONE_VTK_INDEPENDENT_VTI_WRITER_H_
#define CORE_VISUALIZATION_STANDALONE_VTK_INDEPENDENT_VTI_WRITER_H_

#include <array>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "core/real_t.h"

namespace bdm {

class DiffusionGrid;

/// VTK-independent VTI writer that doesn't depend on VTK/ParaView libraries
/// Uses plain C++ file I/O to write VTI files for diffusion grids in
/// environments where VTK dependencies are not available or desired
class VtkIndependentVtiWriter {
 public:
  VtkIndependentVtiWriter() = default;
  ~VtkIndependentVtiWriter() = default;

  /// Write diffusion grid to a VTI file
  void WriteDiffusionGrid(const std::string& filename,
                          const DiffusionGrid* grid);

  /// Write a parallel VTI file (.pvti) that references individual VTI files
  void WritePvti(const std::string& filename,
                 const std::string& file_prefix,
                 uint64_t num_pieces,
                 const std::array<int, 6>& whole_extent) const;

 private:
  /// Write the VTI header
  void WriteVtiHeader(std::ofstream& file,
                      const std::array<int, 6>& extent,
                      const std::array<real_t, 3>& spacing,
                      const std::array<real_t, 3>& origin,
                      uint64_t num_points) const;

  /// Write scalar data for the diffusion grid
  void WritePointData(std::ofstream& file,
                      const DiffusionGrid* grid) const;

  /// Write the VTI footer
  void WriteVtiFooter(std::ofstream& file) const;

  /// Write PVTI header
  void WritePvtiHeader(std::ofstream& file,
                       const std::array<int, 6>& whole_extent,
                       const std::array<real_t, 3>& spacing,
                       const std::array<real_t, 3>& origin) const;

  /// Write PVTI footer
  void WritePvtiFooter(std::ofstream& file) const;

  /// Get the size of real_t in bits for XML formatting
  int GetRealSize() const { return sizeof(real_t) * 8; }
};

}  // namespace bdm

#endif  // CORE_VISUALIZATION_STANDALONE_VTK_INDEPENDENT_VTI_WRITER_H_
