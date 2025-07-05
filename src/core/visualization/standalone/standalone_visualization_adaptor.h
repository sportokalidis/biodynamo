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

#ifndef CORE_VISUALIZATION_STANDALONE_STANDALONE_VISUALIZATION_ADAPTOR_H_
#define CORE_VISUALIZATION_STANDALONE_STANDALONE_VISUALIZATION_ADAPTOR_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "core/visualization/standalone/vtk_independent_vti_writer.h"
#include "core/visualization/standalone/vtk_independent_vtu_writer.h"

namespace bdm {

class Agent;
class DiffusionGrid;

/// Standalone visualization adaptor that exports VTU/VTI files without ParaView or VTK dependencies
/// This adaptor is designed for environments where full ParaView installation is not available
/// but visualization export is still needed for post-processing with external tools.
class StandaloneVisualizationAdaptor {
 public:
  StandaloneVisualizationAdaptor();
  ~StandaloneVisualizationAdaptor();

  /// Export visualization data for the current simulation step
  void ExportVisualization(uint64_t step);

  /// Initialize the adaptor (called once at simulation start)
  void Initialize();

  /// Finalize the adaptor (called once at simulation end)
  void Finalize();

 private:
  VtkIndependentVtuWriter vtu_writer_;
  VtkIndependentVtiWriter vti_writer_;
  bool initialized_ = false;

  /// Export agents to VTU files
  void ExportAgents(uint64_t step);

  /// Export diffusion grids to VTI files
  void ExportDiffusionGrids(uint64_t step);

  /// Get agents grouped by type
  std::unordered_map<std::string, std::vector<Agent*>> GetAgentsByType();

  /// Get output directory for visualization files
  std::string GetOutputDir() const;

  /// Create directories if they don't exist
  void CreateDirectories(const std::string& path);
};

}  // namespace bdm

#endif  // CORE_VISUALIZATION_STANDALONE_STANDALONE_VISUALIZATION_ADAPTOR_H_
