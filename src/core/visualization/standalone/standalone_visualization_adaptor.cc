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

#include "core/visualization/standalone/standalone_visualization_adaptor.h"

#include <algorithm>
#include <filesystem>
#include <sstream>

#include "core/agent/agent.h"
#include "core/diffusion/diffusion_grid.h"
#include "core/param/param.h"
#include "core/resource_manager.h"
#include "core/scheduler.h"
#include "core/simulation.h"
#include "core/util/log.h"
#include "core/util/string.h"
#include "core/util/thread_info.h"

namespace bdm {

// -----------------------------------------------------------------------------
StandaloneVisualizationAdaptor::StandaloneVisualizationAdaptor() = default;

// -----------------------------------------------------------------------------
StandaloneVisualizationAdaptor::~StandaloneVisualizationAdaptor() = default;

// -----------------------------------------------------------------------------
void StandaloneVisualizationAdaptor::Initialize() {
  if (initialized_) {
    return;
  }

  auto* sim = Simulation::GetActive();
  auto* param = sim->GetParam();
  
  if (!param->export_visualization) {
    return;
  }

  // Create output directory structure
  auto output_dir = GetOutputDir();
  CreateDirectories(output_dir);

  initialized_ = true;
  Log::Info("SimpleVisualizationAdaptor", "Initialized VTK-independent visualization export");
}

// -----------------------------------------------------------------------------
void StandaloneVisualizationAdaptor::Finalize() {
  if (!initialized_) {
    return;
  }

  Log::Info("SimpleVisualizationAdaptor", "Finalized visualization export");
}

// -----------------------------------------------------------------------------
void StandaloneVisualizationAdaptor::ExportVisualization(uint64_t step) {
  auto* sim = Simulation::GetActive();
  auto* param = sim->GetParam();
  
  if (!param->export_visualization || !initialized_) {
    return;
  }

  // Check if we should export this step
  if (step % param->visualization_interval != 0) {
    return;
  }

  Log::Info("SimpleVisualizationAdaptor", "Exporting visualization for step ", step);

  ExportAgents(step);
  ExportDiffusionGrids(step);
}

// -----------------------------------------------------------------------------
void StandaloneVisualizationAdaptor::ExportAgents(uint64_t step) {
  auto* sim = Simulation::GetActive();
  auto* param = sim->GetParam();
  
  if (param->visualize_agents.empty()) {
    return;
  }

  auto agents_by_type = GetAgentsByType();
  auto output_dir = GetOutputDir();
  auto* tinfo = ThreadInfo::GetInstance();
  auto num_threads = tinfo->GetMaxThreads();

  // Export each agent type
  for (const auto& [type_name, agents] : agents_by_type) {
    if (agents.empty()) {
      continue;
    }

    // Check if this agent type should be visualized
    bool should_visualize = false;
    for (const auto& visualize_config : param->visualize_agents) {
      if (visualize_config.first == type_name) {
        should_visualize = true;
        break;
      }
    }

    if (!should_visualize) {
      continue;
    }

    if (num_threads > 1) {
      // Parallel export - divide agents among threads
      auto agents_per_thread = agents.size() / num_threads;
      auto remainder = agents.size() % num_threads;

      #pragma omp parallel for schedule(static, 1)
      for (int tid = 0; tid < num_threads; ++tid) {
        auto start = tid * agents_per_thread;
        auto end = start + agents_per_thread;
        if (tid == num_threads - 1) {
          end += remainder;
        }

        std::vector<Agent*> thread_agents(agents.begin() + start, 
                                         agents.begin() + end);
        
        auto filename = Concat(output_dir, "/", type_name, "-", step, "_", tid, ".vtu");
        vtu_writer_.WriteAgents(filename, thread_agents);
      }

      // Write parallel VTU file
      auto pvtu_filename = Concat(output_dir, "/", type_name, "-", step, ".pvtu");
      auto file_prefix = Concat(type_name, "-", step);
      vtu_writer_.WritePvtu(pvtu_filename, file_prefix, num_threads);
    } else {
      // Single-threaded export
      auto filename = Concat(output_dir, "/", type_name, "-", step, ".vtu");
      vtu_writer_.WriteAgents(filename, agents);
    }
  }
}

// -----------------------------------------------------------------------------
void StandaloneVisualizationAdaptor::ExportDiffusionGrids(uint64_t step) {
  auto* sim = Simulation::GetActive();
  auto* param = sim->GetParam();
  auto* rm = sim->GetResourceManager();
  
  if (param->visualize_diffusion.empty()) {
    return;
  }

  auto output_dir = GetOutputDir();

  // Export each diffusion grid
  rm->ForEachDiffusionGrid([&](DiffusionGrid* grid) {
    const std::string& grid_name = grid->GetContinuumName();
    
    // Check if this grid should be visualized
    bool should_visualize = false;
    for (const auto& visualize_config : param->visualize_diffusion) {
      if (visualize_config.name == grid_name) {
        should_visualize = true;
        break;
      }
    }

    if (!should_visualize) {
      return;
    }

    auto filename = Concat(output_dir, "/", grid_name, "-", step, ".vti");
    vti_writer_.WriteDiffusionGrid(filename, grid);
  });
}

// -----------------------------------------------------------------------------
std::unordered_map<std::string, std::vector<Agent*>> 
StandaloneVisualizationAdaptor::GetAgentsByType() {
  auto* sim = Simulation::GetActive();
  auto* rm = sim->GetResourceManager();
  
  std::unordered_map<std::string, std::vector<Agent*>> agents_by_type;
  
  rm->ForEachAgent([&](Agent* agent) {
    std::string type_name = agent->GetTypeName();
    agents_by_type[type_name].push_back(agent);
  });
  
  return agents_by_type;
}

// -----------------------------------------------------------------------------
std::string StandaloneVisualizationAdaptor::GetOutputDir() const {
  auto* sim = Simulation::GetActive();
  return sim->GetOutputDir();
}

// -----------------------------------------------------------------------------
void StandaloneVisualizationAdaptor::CreateDirectories(const std::string& path) {
  try {
    std::filesystem::create_directories(path);
  } catch (const std::exception& e) {
    Log::Error("SimpleVisualizationAdaptor", "Failed to create directory: ", path, 
               " Error: ", e.what());
  }
}

}  // namespace bdm
