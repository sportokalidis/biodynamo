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
//
// \visualize
//
// Same model as the `diffusion` demo (8 cells at the corners of a cube
// chemotaxing towards a 9th, substance-secreting cell at the center), kept
// deliberately small since the point of this demo is to exercise the
// StandaloneExporter's output, not the chemotaxis physics. bdm.toml sets
// `adaptor = "standalone"` and configures both diffusion-field export and
// an additional per-agent data member, so every export code path in
// src/core/visualization/standalone/standalone_exporter.cc is exercised by
// an actual simulation run through the normal `biodynamo run` pipeline.
//

#ifndef DEMO_STANDALONE_VISUALIZATION_H_
#define DEMO_STANDALONE_VISUALIZATION_H_

#include "biodynamo.h"

namespace bdm {

// List the extracellular substances
enum Substances { kKalium };

inline int Simulate(int argc, const char** argv) {
  // Initialize BioDynaMo
  Simulation simulation(argc, argv);

  // Define the substance that the central cell secretes
  ModelInitializer::DefineSubstance(kKalium, "Kalium", 0.4, 0, 25);
  ModelInitializer::AddBoundaryConditions(
      kKalium, BoundaryConditionType::kNeumann,
      std::make_unique<ConstantBoundaryCondition>(0));

  // Create 8 cells in a 2x2x2 grid setup
  auto construct = [&](const Real3& position) {
    Cell* cell = new Cell(position);
    cell->SetDiameter(30);
    cell->SetMass(1.0);
    cell->AddBehavior(new Chemotaxis("Kalium", 0.5));
    return cell;
  };
  ModelInitializer::Grid3D(2, 100, construct);

  // The cell responsible for secretion
  auto* secreting_cell = new Cell({50, 50, 50});
  secreting_cell->AddBehavior(new Secretion("Kalium", 4));
  simulation.GetExecutionContext()->AddAgent(secreting_cell);

  // A handful of steps is enough to produce several export cycles; the
  // physics converging further isn't the point of this demo.
  simulation.GetScheduler()->Simulate(10);
  std::cout << "Simulation completed successfully!\n";
  return 0;
}

}  // namespace bdm

#endif  // DEMO_STANDALONE_VISUALIZATION_H_
