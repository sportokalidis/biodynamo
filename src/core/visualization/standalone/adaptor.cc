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

#include "core/visualization/standalone/adaptor.h"

#include <filesystem>  // std::filesystem::create_directories (C++17)

#include "core/param/param.h"       // export_visualization, visualization_interval
#include "core/scheduler.h"         // GetSimulatedSteps()
#include "core/simulation.h"        // Simulation::GetActive(), GetOutputDir()
#include "core/visualization/standalone/standalone_vtu_exporter.h"

namespace bdm {

// -----------------------------------------------------------------------------
/// ROOT plugin entry point.  TPluginManager resolves "Factory" by name via
/// cling and calls it to obtain a heap-allocated adaptor instance.  The
/// returned pointer is owned by the caller (VisualizationAdaptor::Create).
StandaloneAdaptor* StandaloneAdaptor::Factory() { return new StandaloneAdaptor(); }

// -----------------------------------------------------------------------------
StandaloneAdaptor::StandaloneAdaptor() = default;

// -----------------------------------------------------------------------------
/// Deletes the exporter, which flushes and closes any open file streams.
StandaloneAdaptor::~StandaloneAdaptor() { delete exporter_; }

// -----------------------------------------------------------------------------
/// Called by VisualizationOp every simulation step (but see the interval
/// guard below).  Two-phase design:
///
///  Phase 1 — lazy initialisation (runs only on the very first call):
///    Creates the output sub-directory "viz/" inside the simulation's output
///    directory and allocates the StandaloneVtuExporter.  This is deferred
///    to the first Visualize() call rather than done in the constructor
///    because sim->GetOutputDir() is only valid after the scheduler has
///    started and created the directory.
///
///  Phase 2 — I/O gate (runs every call):
///    Checks two conditions before performing any disk I/O:
///      1. param->export_visualization must be true  (bdm.toml: export = true)
///      2. The current step must be divisible by visualization_interval
///         (bdm.toml: interval = N)
///    This mirrors the gating logic in ParaviewAdaptor so that both adaptors
///    write files at exactly the same simulation steps.
void StandaloneAdaptor::Visualize() {
  auto* sim   = Simulation::GetActive();
  auto* param = sim->GetParam();

  // ----- Phase 1: lazy init ------------------------------------------------
  if (!initialized_) {
    // Append "/viz" so visualization files are isolated from other output
    // (ROOT analysis files, logs, etc.) in the same output directory.
    std::string out_dir = sim->GetOutputDir() + "/viz";
    // create_directories is equivalent to "mkdir -p": it succeeds even if
    // intermediate directories already exist.
    std::filesystem::create_directories(out_dir);
    exporter_    = new StandaloneVtuExporter(out_dir);
    initialized_ = true;
  }

  // ----- Phase 2: I/O gate -------------------------------------------------
  // GetSimulatedSteps() returns the total number of steps completed so far
  // (incremented by the scheduler after each step, before Visualize is called).
  uint64_t total_steps = sim->GetScheduler()->GetSimulatedSteps();
  if (param->export_visualization &&
      (total_steps % param->visualization_interval == 0)) {
    // Write all agent positions and per-agent scalar/vector attributes.
    exporter_->WriteStep();
    // Write concentration and gradient fields for every diffusion substance
    // listed under [[visualize_diffusion]] in bdm.toml.
    exporter_->WriteDiffusionStep();
  }
}

}  // namespace bdm
