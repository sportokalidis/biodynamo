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
// This demo showcases the RegulatoryNetwork behavior, which uses Boost.Odeint
// to integrate systems of ODEs. The example solves the Van der Pol oscillator:
//
//   d[x0]/d[t] = alpha * x1
//   d[x1]/d[t] = beta * x1 * (1 - x0^2) - gamma * x0
//
// A single cell carries the RegulatoryNetwork behavior with the Rosenbrock
// implicit solver. The extracellular environment includes a diffusion grid
// ("cytokine") whose concentration can be read inside the ODE system, showing
// how regulatory networks can couple to the spatial environment.
//
// Reference:
//   https://juliareach.github.io/ReachabilityAnalysis.jl/dev/generated_examples/VanDerPol/

#ifndef DEMO_REGULATORY_NETWORK_H_
#define DEMO_REGULATORY_NETWORK_H_

#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "biodynamo.h"
#include "core/behavior/regulatory_network.h"

namespace bdm {

enum Substances { kCytokine };

// ---------------------------------------------------------------------------
// ODE parameters for the Van der Pol oscillator
// ---------------------------------------------------------------------------
class ODE_parameters {
 public:
  ODE_parameters(real_t dt, real_t a, real_t b, real_t c)
      : time_step_(dt), alpha_(a), beta_(b), gamma_(c) {}
  ODE_parameters(const ODE_parameters& p)
      : time_step_(p.time_step()),
        alpha_(p.alpha()),
        beta_(p.beta()),
        gamma_(p.gamma()) {}

  real_t time_step() const { return time_step_; }
  real_t alpha() const { return alpha_; }
  real_t beta() const { return beta_; }
  real_t gamma() const { return gamma_; }

 private:
  real_t time_step_;
  real_t alpha_, beta_, gamma_;
};

// ---------------------------------------------------------------------------
// RHS of the ODE system
// ---------------------------------------------------------------------------
struct ODE_system : public ODE_parameters {
  ODE_system(Cell* cell, const ODE_parameters& p,
             const std::map<std::string, DiffusionGrid*>& dg_map)
      : ODE_parameters(p), c_(cell), dg_(dg_map) {}

  void operator()(const boost_vector_t& x, boost_vector_t& dxdt,
                  real_t /*t*/) const {
    // Read extracellular concentration (demonstrates coupling to environment)
    auto& crd = c_->GetPosition();
    auto& dg = dg_.find("cytokine")->second;
    // ecm_c can be used in the ODE system if needed
    const real_t ecm_c = dg->GetValue(crd);
    (void)ecm_c;  // suppress unused warning in this demo

    dxdt[0] = alpha() * x[1];
    dxdt[1] = beta() * x[1] - beta() * x[1] * x[0] * x[0] - gamma() * x[0];
  }

 private:
  Cell* c_;
  std::map<std::string, DiffusionGrid*> dg_;
};

// ---------------------------------------------------------------------------
// Jacobian of the ODE system (required for Rosenbrock solver)
// ---------------------------------------------------------------------------
struct ODE_jacobian : public ODE_parameters {
  ODE_jacobian(Cell* cell, const ODE_parameters& p,
               const std::map<std::string, DiffusionGrid*>& dg_map)
      : ODE_parameters(p), c_(cell), dg_(dg_map) {}

  void operator()(const boost_vector_t& x, boost_matrix_t& jac, real_t /*t*/,
                  boost_vector_t& dfdt) const {
    jac(0, 0) = 0.0;
    jac(0, 1) = alpha();
    jac(1, 0) = -2.0 * beta() * x[0] * x[1] - gamma();
    jac(1, 1) = beta() - beta() * x[0] * x[0];

    dfdt[0] = 0.0;
    dfdt[1] = 0.0;
  }

 private:
  Cell* c_;
  std::map<std::string, DiffusionGrid*> dg_;
};

// ---------------------------------------------------------------------------
// Observer — called after each internal ODE step
// ---------------------------------------------------------------------------
struct ODE_output {
  ODE_output(Cell* cell,
             const std::map<std::string, DiffusionGrid*>& dg_map)
      : c_(cell), dg_(dg_map) {}

  void operator()(const boost_vector_t& x, real_t t) {
    std::cout << c_->GetUid() << " : " << c_->GetPosition() << " : " << t
              << " : " << x[0] << ' ' << x[1] << std::endl;
  }

 private:
  Cell* c_;
  std::map<std::string, DiffusionGrid*> dg_;
};

// ---------------------------------------------------------------------------
// Main simulation function
// ---------------------------------------------------------------------------
inline int Simulate(int argc, const char** argv) {
  auto set_parameters = [](Param* param) {
    param->use_progress_bar = false;
    param->bound_space = Param::BoundSpaceMode::kClosed;
    param->min_bound = -10.0;
    param->max_bound = +10.0;
    param->export_visualization = true;
    param->visualization_interval = 1;
    param->visualize_agents["Cell"] = {"diameter_", "volume_"};
    param->statistics = false;
    param->simulation_time_step = 1.0;
    param->visualize_diffusion = {
        Param::VisualizeDiffusion{"cytokine", true, true}};
    param->calculate_gradients = false;
    param->diffusion_method = "euler";
  };

  Simulation sim(argc, argv, set_parameters);
  auto* rm = sim.GetResourceManager();

  // BioDynaMo simulation time-step
  const real_t dt_BDM = sim.GetParam()->simulation_time_step;
  // Regulatory network ODE solver time-step
  const real_t dt_RN = 1000.0;
  // Van der Pol parameters: alpha, beta, gamma
  const ODE_parameters rn_p(dt_RN, 1.e+0, 1.e+3, 1.e+0);
  // Diffusion grid resolution
  int n_DG = 51;

  // Set up diffusion grid (no diffusion / no decay in this demo)
  ModelInitializer::DefineSubstance(kCytokine, "cytokine", 0. / dt_BDM,
                                    0. / dt_BDM, n_DG);
  ModelInitializer::AddBoundaryConditions(
      kCytokine, BoundaryConditionType::kNeumann,
      std::make_unique<ConstantBoundaryCondition>(0));

  // Reference to the diffusion grid (passed into ODE functors)
  std::map<std::string, DiffusionGrid*> dg_m;
  dg_m.insert(std::make_pair("cytokine", rm->GetDiffusionGrid("cytokine")));

  // Create a single cell with the RegulatoryNetwork behavior
  auto* cell = new Cell({0.01, 0.02, 0.03});
  cell->SetDiameter(1.0);
  cell->AddBehavior(new RegulatoryNetwork(
      rn_p.time_step(), {1., 1.}, ODE_solver::Rosenbrock,
      ODE_system(cell, rn_p, dg_m), ODE_jacobian(cell, rn_p, dg_m),
      ODE_output(cell, dg_m)));

  rm->AddAgent(cell);

  // Run one BioDynaMo time-step (the ODE integrator takes many sub-steps)
  sim.GetScheduler()->Simulate(1);

  // Print final species concentrations
  auto* agent = rm->GetAgent(AgentUid(0));
  const auto* behavior = agent->GetAllBehaviors()[0];
  auto* rn = dynamic_cast<const RegulatoryNetwork*>(behavior);
  const auto& species = rn->GetSpecies();
  std::cout << "\nFinal species concentrations after integration:\n";
  for (size_t i = 0; i < rn->GetNumberOfSpecies(); ++i) {
    std::cout << "  x[" << i << "] = " << species[i] << std::endl;
  }

  std::cout << "\nSimulation completed successfully!\n";
  return 0;
}

}  // namespace bdm

#endif  // DEMO_REGULATORY_NETWORK_H_
