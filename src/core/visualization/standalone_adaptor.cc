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

#include "core/visualization/standalone_adaptor.h"

#include "core/param/param.h"
#include "core/scheduler.h"
#include "core/simulation.h"
#include "core/util/log.h"

namespace bdm {

// -----------------------------------------------------------------------------
StandaloneAdaptor::StandaloneAdaptor() 
    : adaptor_(new StandaloneVisualizationAdaptor()) {}

// -----------------------------------------------------------------------------
StandaloneAdaptor::~StandaloneAdaptor() {
  if (adaptor_) {
    adaptor_->Finalize();
    delete adaptor_;
  }
}

// -----------------------------------------------------------------------------
void StandaloneAdaptor::Visualize() {
  auto* sim = Simulation::GetActive();
  auto* param = sim->GetParam();
  
  if (!param->export_visualization) {
    return;
  }

  // Initialize on first call
  if (!initialized_) {
    adaptor_->Initialize();
    initialized_ = true;
  }

  // Export visualization for current step
  auto current_step = sim->GetScheduler()->GetSimulatedSteps();
  adaptor_->ExportVisualization(current_step);
}

}  // namespace bdm
