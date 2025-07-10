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

#ifndef CORE_VISUALIZATION_STANDALONE_VTK_INDEPENDENT_VTU_WRITER_H_
#define CORE_VISUALIZATION_STANDALONE_VTK_INDEPENDENT_VTU_WRITER_H_

#include <array>
#include <fstream>
#include <string>
#include <vector>

#include "core/agent/agent.h"
#include "core/real_t.h"

namespace bdm {

class DiffusionGrid;

/// VTK-independent VTU writer that doesn't depend on VTK/ParaView libraries
/// Uses plain C++ file I/O to write VTU files for visualization export
/// in environments where VTK dependencies are not available or desired
class VtkIndependentVtuWriter {
 public:
  VtkIndependentVtuWriter() = default;
  ~VtkIndependentVtuWriter() = default;

  /// Write agents to a VTU file
  void WriteAgents(const std::string& filename, 
                   const std::vector<Agent*>& agents);

  /// Write a parallel VTU file (.pvtu) that references individual VTU files
  void WritePvtu(const std::string& filename,
                 const std::string& file_prefix,
                 uint64_t num_pieces);

 private:
  /// Write the VTU header
  void WriteVtuHeader(std::ofstream& file, uint64_t num_points);

  /// Write point data (positions)
  void WritePoints(std::ofstream& file, const std::vector<Agent*>& agents);

  /// Write agent attribute data
  void WritePointData(std::ofstream& file, const std::vector<Agent*>& agents);

  /// Write cell connectivity data
  void WriteCells(std::ofstream& file, uint64_t num_cells);

  /// Write the VTU footer
  void WriteVtuFooter(std::ofstream& file);

  /// Write PVTU header
  void WritePvtuHeader(std::ofstream& file);

  /// Write PVTU footer
  void WritePvtuFooter(std::ofstream& file);

  /// Get the size of real_t in bits for XML formatting
  int GetRealSize() const { return sizeof(real_t) * 8; }
};

}  // namespace bdm

#endif  // CORE_VISUALIZATION_STANDALONE_VTK_INDEPENDENT_VTU_WRITER_H_
