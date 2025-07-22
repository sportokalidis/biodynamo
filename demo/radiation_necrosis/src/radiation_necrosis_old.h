// -----------------------------------------------------------------------------
//
// Copyright (C) 2024 for the benefit of the BioDynaMo collaboration. 
// All Rights Reserved.
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
// \title Radiation Necrosis Model
// \visualize
//
// This model simulates radiation-induced necrosis in brain tissue
// with multiple cell types, radiation therapy, and inflammatory responses.
//

#ifndef DEMO_RADIATION_NECROSIS_H_
#define DEMO_RADIATION_NECROSIS_H_

#include "biodynamo.h"

namespace bdm {
namespace radiation_necrosis {

// Define cell types
enum CellType {
  kHealthyNeuron = 0,
  kHealthyGlia = 1,
  kTumorCell = 2,
  kNecroticCell = 3,
  kInflammatoryCell = 4
};

// Define cell class
class RadiationCell : public Cell {
  BDM_AGENT_HEADER(RadiationCell, Cell, 1);

 public:
  RadiationCell() = default;
  explicit RadiationCell(const Real3& position) : Base(position) {}
  virtual ~RadiationCell() = default;

  void SetCellType(CellType type) { cell_type_ = type; }
  CellType GetCellType() const { return cell_type_; }

  void SetRadiationDose(double dose) { radiation_dose_ = dose; }
  double GetRadiationDose() const { return radiation_dose_; }

  void SetInflammationLevel(double level) { inflammation_level_ = level; }
  double GetInflammationLevel() const { return inflammation_level_; }

  void SetMetabolicHealth(double health) { metabolic_health_ = health; }
  double GetMetabolicHealth() const { return metabolic_health_; }

  void SetDamageLevel(double damage) { damage_level_ = damage; }
  double GetDamageLevel() const { return damage_level_; }

  void SetOxygenLevel(double oxygen) { oxygen_level_ = oxygen; }
  double GetOxygenLevel() const { return oxygen_level_; }

  bool IsAlive() const { 
    return cell_type_ != kNecroticCell && metabolic_health_ > 0.1; 
  }

 private:
  CellType cell_type_ = kHealthyNeuron;
  double radiation_dose_ = 0.0;
  double inflammation_level_ = 0.0;
  double metabolic_health_ = 1.0;
  double damage_level_ = 0.0;
  double oxygen_level_ = 1.0;
};

// Forward declare behavior classes for ROOT dictionary
class RadiationTherapy;
class CellDeath;  
class InflammatoryResponse;

// Radiation therapy behavior
class RadiationTherapy : public Behavior {
  BDM_BEHAVIOR_HEADER(RadiationTherapy, Behavior, 1);

 public:
  RadiationTherapy() = default;
  RadiationTherapy(double dose, int treatment_step) 
      : dose_(dose), treatment_step_(treatment_step), applied_(false) {}
  virtual ~RadiationTherapy() = default;

  void Run(Agent* agent) override {
    // Only apply radiation once per cell
    if (applied_) return;
    
    auto* sim = Simulation::GetActive();
    if (sim->GetScheduler()->GetSimulatedSteps() == treatment_step_) {
      auto* cell = dynamic_cast<RadiationCell*>(agent);
      if (cell && cell->IsAlive()) {
        // Apply radiation dose
        cell->SetRadiationDose(cell->GetRadiationDose() + dose_);
        
        // Immediate cellular response
        CellType type = cell->GetCellType();
        double damage = 0.0;
        
        switch (type) {
          case kHealthyNeuron:
            damage = dose_ * 0.8;  // High sensitivity
            break;
          case kHealthyGlia:
            damage = dose_ * 0.6;  // Moderate sensitivity
            break;
          case kTumorCell:
            damage = dose_ * 1.2;  // Very high sensitivity
            break;
          default:
            damage = dose_ * 0.5;
            break;
        }
        
        cell->SetDamageLevel(cell->GetDamageLevel() + damage);
        cell->SetMetabolicHealth(cell->GetMetabolicHealth() - damage * 0.1);
        
        applied_ = true;  // Mark as applied to prevent re-application
      }
    }
  }

 private:
  double dose_ = 30.0;
  int treatment_step_ = 20;
  bool applied_ = false;  // Flag to prevent multiple applications
};
  double dose_ = 0.0;
  int treatment_step_ = 0;
};

// Cell death behavior
class CellDeath : public Behavior {
  BDM_BEHAVIOR_HEADER(CellDeath, Behavior, 1);

 public:
  CellDeath() = default;
  CellDeath(int treatment_step) : treatment_step_(treatment_step) {}
  virtual ~CellDeath() = default;

  void Run(Agent* agent) override {
    auto* cell = dynamic_cast<RadiationCell*>(agent);
    if (!cell) return;

    // More gradual cell death over time - cells don't die immediately
    auto* sim = Simulation::GetActive();
    uint64_t current_step = sim->GetScheduler()->GetSimulatedSteps();
    
    // Introduce delays - death effects happen hours after radiation
    bool should_die = false;
    
    // Radiation-induced death (delayed by several hours)
    if (cell->GetRadiationDose() > 20.0 && current_step > treatment_step_ + 3) {
      double time_since_radiation = current_step - treatment_step_;
      double death_prob = (1.0 - std::exp(-cell->GetRadiationDose() / 15.0)) * 
                         std::min(1.0, time_since_radiation / 10.0); // Gradual over 10 hours
      if (sim->GetRandom()->Uniform() < death_prob * 0.1) { // 10% chance per hour
        should_die = true;
      }
    }
    
    // Metabolic failure (also gradual)
    if (cell->GetMetabolicHealth() < 0.3 && 
        sim->GetRandom()->Uniform() < 0.05) { // 5% chance per hour
      should_die = true;
    }
    
    // Severe damage (accumulated over time)
    if (cell->GetDamageLevel() > 50.0 && 
        sim->GetRandom()->Uniform() < 0.08) { // 8% chance per hour
      should_die = true;
    }
    
    if (should_die && cell->GetCellType() != kNecroticCell) {
      cell->SetCellType(kNecroticCell);
      cell->SetMetabolicHealth(0.0);
      
      // Change visual properties
      cell->SetDiameter(cell->GetDiameter() * 0.8);
    }
  }

 private:
  int treatment_step_ = 20;
};

// Inflammatory response behavior
class InflammatoryResponse : public Behavior {
  BDM_BEHAVIOR_HEADER(InflammatoryResponse, Behavior, 1);

 public:
  InflammatoryResponse() = default;
  virtual ~InflammatoryResponse() = default;

  void Run(Agent* agent) override {
    auto* cell = dynamic_cast<RadiationCell*>(agent);
    if (!cell || !cell->IsAlive()) return;

    // Count necrotic neighbors
    int necrotic_neighbors = 0;
    auto* sim = Simulation::GetActive();
    auto* env = sim->GetEnvironment();
    
    // Search for neighbors
    Real3 position = cell->GetPosition();
    auto search_radius = cell->GetDiameter() * 1.5;
    
    auto check_necrotic_neighbors = L2F([&](Agent* neighbor, real_t squared_distance) {
      auto* neighbor_cell = dynamic_cast<RadiationCell*>(neighbor);
      if (neighbor_cell && neighbor_cell->GetCellType() == kNecroticCell) {
        necrotic_neighbors++;
      }
    });
    
    env->ForEachNeighbor(check_necrotic_neighbors, *cell, search_radius);

    // Increase inflammation based on necrotic neighbors
    if (necrotic_neighbors > 0) {
      double inflammation_increase = necrotic_neighbors * 0.1;
      cell->SetInflammationLevel(cell->GetInflammationLevel() + inflammation_increase);
      
      // Inflammation causes additional damage
      double inflammation_damage = cell->GetInflammationLevel() * 0.05;
      cell->SetDamageLevel(cell->GetDamageLevel() + inflammation_damage);
      cell->SetMetabolicHealth(cell->GetMetabolicHealth() - inflammation_damage * 0.02);
    }
    
    // Natural inflammation decay
    cell->SetInflammationLevel(cell->GetInflammationLevel() * 0.99);
  }
};

inline int Simulate(int argc, const char** argv) {
  Simulation simulation(argc, argv);
  
  // Create cells
  auto* rm = simulation.GetResourceManager();
  
  std::cout << "Creating 1500 healthy brain cells..." << std::endl;
  
  // Create healthy brain tissue
  for (int i = 0; i < 1500; ++i) {
    Real3 position = {simulation.GetRandom()->Uniform(-200, 200),
                      simulation.GetRandom()->Uniform(-200, 200), 
                      simulation.GetRandom()->Uniform(-200, 200)};
    
    auto* cell = new RadiationCell(position);
    
    // 70% neurons, 30% glia
    if (simulation.GetRandom()->Uniform() < 0.7) {
      cell->SetCellType(kHealthyNeuron);
      cell->SetDiameter(15.0);
    } else {
      cell->SetCellType(kHealthyGlia);
      cell->SetDiameter(12.0);
    }
    
    cell->SetMetabolicHealth(1.0);
    cell->SetOxygenLevel(1.0);
    rm->AddAgent(cell);
  }
  
  std::cout << "Creating 3 tumor metastases..." << std::endl;
  
  // Create small tumor metastases
  for (int i = 0; i < 3; ++i) {
    Real3 center = {simulation.GetRandom()->Uniform(-150, 150),
                    simulation.GetRandom()->Uniform(-150, 150),
                    simulation.GetRandom()->Uniform(-150, 150)};
    
    // Create a small cluster of tumor cells
    for (int j = 0; j < 8; ++j) {
      Real3 position = center + Real3{simulation.GetRandom()->Uniform(-10, 10),
                                      simulation.GetRandom()->Uniform(-10, 10),
                                      simulation.GetRandom()->Uniform(-10, 10)};
      
      auto* tumor_cell = new RadiationCell(position);
      tumor_cell->SetCellType(kTumorCell);
      tumor_cell->SetDiameter(18.0);
      tumor_cell->SetMetabolicHealth(1.2);  // More metabolically active
      rm->AddAgent(tumor_cell);
    }
  }
  
  std::cout << "Setting up radiation therapy protocol..." << std::endl;
  std::cout << "Treatment will start at step 20" << std::endl;
  std::cout << "Single fraction of 30 Gy" << std::endl;
  
  // Add behaviors to all cells
  rm->ForEachAgent([&](Agent* agent) {
    auto* cell = dynamic_cast<RadiationCell*>(agent);
    if (cell) {
      cell->AddBehavior(new RadiationTherapy(30.0, 20)); // 30 Gy at step 20
      cell->AddBehavior(new CellDeath(20));               // Treatment at step 20
      cell->AddBehavior(new InflammatoryResponse());
    }
  });
  
  std::cout << "\nStarting simulation for 100 time steps..." << std::endl;
  std::cout << "Timeline (each step = 1 hour):" << std::endl;
  std::cout << "- Steps 0-19: Initial tumor growth (20 hours)" << std::endl;
  std::cout << "- Step 20: Radiation therapy delivery" << std::endl;
  std::cout << "- Steps 21-100: Post-radiation effects and necrosis development (80 hours)" << std::endl;

  // Run simulation with progress updates
  for (int step = 0; step < 1000; ++step) {
    simulation.GetScheduler()->Simulate(1);
    
    // Progress updates at key milestones
    if (step == 19) {
      std::cout << "Step " << (step + 1) << ": Pre-treatment phase complete" << std::endl;
    } else if (step == 20) {
      std::cout << "Step " << (step + 1) << ": Radiation therapy delivered (30 Gy)" << std::endl;
    } else if (step == 30) {
      std::cout << "Step " << (step + 1) << ": Early post-radiation response (10 hours)" << std::endl;
    } else if (step == 50) {
      std::cout << "Step " << (step + 1) << ": Mid-term effects developing (30 hours)" << std::endl;
    } else if (step == 80) {
      std::cout << "Step " << (step + 1) << ": Late effects and necrosis (60 hours)" << std::endl;
    } else if (step == 99) {
      std::cout << "Step " << (step + 1) << ": Simulation complete (80 hours post-radiation)" << std::endl;
    }
  }

  std::cout << "\nSimulation completed!" << std::endl;
  return 0;
}

}  // namespace radiation_necrosis
}  // namespace bdm

#endif  // DEMO_RADIATION_NECROSIS_H_
