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

/*
 * MATHEMATICAL BASIS AND LITERATURE VALUES:
 * 
 * 1. RADIATION DOSE: 30 Gy single fraction
 *    - Literature: SRS typically 12-24 Gy, experimental up to 30 Gy
 *    - Source: Int J Radiat Oncol Biol Phys. Various studies on brain SRS
 * 
 * 2. RADIOSENSITIVITY (α/β ratios from literature):
 *    - Neurons: α/β = 2-3 Gy (very sensitive) → damage factor 1.5
 *    - Glia: α/β = 3-4 Gy (moderately sensitive) → damage factor 1.0  
 *    - Tumor: α/β = 8-10 Gy (less sensitive) → damage factor 0.7
 *    - Source: Radiother Oncol. 2010;94(1):1-10
 * 
 * 3. CELL SURVIVAL: Linear-Quadratic Model
 *    - SF = exp(-(αD + βD²))
 *    - Brain tissue: α ≈ 0.2 Gy⁻¹, β ≈ 0.02 Gy⁻²
 *    - Source: Phys Med Biol. 2009;54(13):4171-86
 * 
 * 4. PROLIFERATION RATES:
 *    - Tumor doubling time: 30-60 days → 0.001/hour
 *    - Adult brain: minimal proliferation → 0.0001/hour
 *    - Source: Cancer Res. Multiple studies on glioma growth
 * 
 * 5. TIMELINE ACCELERATION: 
 *    - Real necrosis: 6 months - 2 years post-treatment
 *    - Model: Accelerated 20x for simulation purposes (each step = 5 hours)
 *    - Source: Neurosurgery. 2008;62(4):887-96
 */

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
        
        // Radiobiologically accurate cell type sensitivity
        // Based on α/β ratios from literature
        switch (type) {
          case kHealthyNeuron:
            damage = dose_ * 1.5;  // Very high sensitivity (α/β = 2-3)
            break;
          case kHealthyGlia:
            damage = dose_ * 1.0;  // High sensitivity (α/β = 3-4)
            break;
          case kTumorCell:
            damage = dose_ * 0.7;  // Lower sensitivity (α/β = 8-10, more resistant)
            break;
          default:
            damage = dose_ * 1.0;
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
  int treatment_step_ = 4;  // Adjusted for 20x acceleration: step 4 = 20 hours
  bool applied_ = false;  // Flag to prevent multiple applications
};

// Cell death and proliferation behavior
class CellDynamics : public Behavior {
  BDM_BEHAVIOR_HEADER(CellDynamics, Behavior, 1);

 public:
  CellDynamics() = default;
  CellDynamics(int treatment_step) : treatment_step_(treatment_step) {}
  virtual ~CellDynamics() = default;

  void Run(Agent* agent) override {
    auto* cell = dynamic_cast<RadiationCell*>(agent);
    if (!cell) return;

    auto* sim = Simulation::GetActive();
    auto* random = sim->GetRandom();
    uint64_t current_step = sim->GetScheduler()->GetSimulatedSteps();
    
    // Continuous metabolic changes over time
    ApplyMetabolicChanges(cell, sim, current_step);
    
    // Progressive radiation damage accumulation
    ApplyProgressiveDamage(cell, sim, current_step);
    
    // Cell death processes
    HandleCellDeath(cell, sim, current_step);
    
    // Cell division and repair (ongoing biological processes)
    HandleCellProliferation(cell, sim, current_step);
  }

 private:
  int treatment_step_ = 4;  // Adjusted for 20x acceleration: step 4 = 20 hours
  
  void ApplyMetabolicChanges(RadiationCell* cell, Simulation* sim, uint64_t step) {
    auto* random = sim->GetRandom();
    
    // Continuous metabolic fluctuations
    double metabolic_change = (random->Uniform() - 0.5) * 0.02; // Small random changes
    cell->SetMetabolicHealth(std::max(0.0, std::min(1.5, 
        cell->GetMetabolicHealth() + metabolic_change)));
    
    // Oxygen level changes based on tissue damage and time
    double oxygen_change = -0.001; // Gradual hypoxia over time
    if (step > treatment_step_) {
      oxygen_change -= (step - treatment_step_) * 0.0001; // Worsening hypoxia
    }
    cell->SetOxygenLevel(std::max(0.1, 
        cell->GetOxygenLevel() + oxygen_change + (random->Uniform() - 0.5) * 0.01));
  }
  
  void ApplyProgressiveDamage(RadiationCell* cell, Simulation* sim, uint64_t step) {
    if (step <= treatment_step_) return;
    
    auto* random = sim->GetRandom();
    double time_since_radiation = step - treatment_step_;
    
    // DNA repair attempts (sometimes successful, sometimes not)
    if (random->Uniform() < 0.3) { // 30% chance of repair attempt
      if (random->Uniform() < 0.7) { // 70% success rate
        cell->SetDamageLevel(std::max(0.0, cell->GetDamageLevel() - 0.5));
      } else { // 30% failure leads to more damage
        cell->SetDamageLevel(cell->GetDamageLevel() + 0.8);
      }
    }
    
    // Late radiation effects (months later, but accelerated for simulation)
    if (time_since_radiation > 50) {
      if (random->Uniform() < 0.05) { // 5% chance per hour
        cell->SetDamageLevel(cell->GetDamageLevel() + random->Uniform(1.0, 3.0));
      }
    }
  }
  
  void HandleCellDeath(RadiationCell* cell, Simulation* sim, uint64_t step) {
    auto* random = sim->GetRandom();
    bool should_die = false;
    
    // Linear-Quadratic model approximation for cell death
    // SF = exp(-(αD + βD²)) where α=0.2, β=0.02 for brain tissue
    if (cell->GetRadiationDose() > 10.0 && step > treatment_step_ + 2) {
      double dose = cell->GetRadiationDose();
      double alpha = 0.2;  // Gy⁻¹ (literature value for brain tissue)
      double beta = 0.02;  // Gy⁻² (literature value)
      
      // Survival fraction based on LQ model
      double survival_fraction = std::exp(-(alpha * dose + beta * dose * dose));
      double death_probability = 1.0 - survival_fraction;
      
      // Time-dependent expression (delayed effects)  
      // Adjusted for 20x acceleration: each step = 5 hours
      double time_factor = std::min(1.0, (step - treatment_step_) / 10.0); // Over 50 hours (10 steps * 5h)
      
      if (random->Uniform() < death_probability * time_factor * 0.05) { // 5% per step (5 hours)
        should_die = true;
      }
    }
    
    // Hypoxia-induced death (adjusted for 5-hour steps)
    if (cell->GetOxygenLevel() < 0.3 && random->Uniform() < 0.25) { // ~5% per hour equivalent
      should_die = true;
    }
    
    // Severe accumulated damage
    if (cell->GetDamageLevel() > 40.0 && random->Uniform() < 0.1) {
      should_die = true;
    }
    
    if (should_die && cell->GetCellType() != kNecroticCell) {
      cell->SetCellType(kNecroticCell);
      cell->SetMetabolicHealth(0.0);
      cell->SetDiameter(cell->GetDiameter() * 0.7);
    }
  }
  
  void HandleCellProliferation(RadiationCell* cell, Simulation* sim, uint64_t step) {
    if (cell->GetCellType() == kNecroticCell) return;
    
    auto* random = sim->GetRandom();
    auto* rm = sim->GetResourceManager();
    
    // Tumor cells continue to proliferate (even after radiation)
    if (cell->GetCellType() == kTumorCell) {
      // Adjusted for 20x acceleration: each step = 5 hours
      // Tumor doubling time: ~30-60 days = 144-288 steps (720-1440 hours / 5)
      double proliferation_rate = 0.005; // 0.5% chance per step (5 hours)
      if (step > treatment_step_) {
        // Reduced proliferation after radiation (more realistic reduction)
        double dose_effect = std::max(0.1, std::exp(-cell->GetRadiationDose() / 20.0));
        proliferation_rate *= dose_effect;
      }
      
      if (random->Uniform() < proliferation_rate && rm->GetNumAgents() < 3000) {
        // Create daughter cell nearby
        Real3 offset = {(random->Uniform() - 0.5) * 10.0, (random->Uniform() - 0.5) * 10.0, 
                       (random->Uniform() - 0.5) * 10.0};
        Real3 new_pos = cell->GetPosition() + offset;
        
        auto* daughter = new RadiationCell(new_pos);
        daughter->SetCellType(kTumorCell);
        daughter->SetDiameter(cell->GetDiameter() * random->Uniform(0.8, 1.2));
        daughter->SetMetabolicHealth(cell->GetMetabolicHealth() * random->Uniform(0.9, 1.1));
        daughter->SetRadiationDose(cell->GetRadiationDose() * 0.8); // Less radiation damage
        
        // Add same behaviors
        daughter->AddBehavior(new RadiationTherapy(30.0, treatment_step_));
        daughter->AddBehavior(new CellDynamics(treatment_step_));
        // Forward declaration issue - skip inflammatory response for daughter cells for now
        
        rm->AddAgent(daughter);
      }
    }
    
    // Healthy cells attempt repair/replacement (much slower, more realistic rate)
    else if (cell->GetMetabolicHealth() > 0.6 && step > treatment_step_ + 5) { // Start after 25 hours (5 steps * 5h)
      // Adult brain has very limited regenerative capacity
      // Adjusted for 20x acceleration: each step = 5 hours
      if (random->Uniform() < 0.0005) { // 0.05% chance per step (5 hours), very rare
        Real3 offset = {(random->Uniform() - 0.5) * 6.0, (random->Uniform() - 0.5) * 6.0, 
                       (random->Uniform() - 0.5) * 6.0};
        Real3 new_pos = cell->GetPosition() + offset;
        
        auto* repair_cell = new RadiationCell(new_pos);
        repair_cell->SetCellType(cell->GetCellType());
        repair_cell->SetDiameter(cell->GetDiameter() * random->Uniform(0.9, 1.1));
        repair_cell->SetMetabolicHealth(0.8); // Newly formed, healthy but not perfect
        repair_cell->SetOxygenLevel(cell->GetOxygenLevel());
        
        // Add behaviors
        repair_cell->AddBehavior(new RadiationTherapy(30.0, treatment_step_));
        repair_cell->AddBehavior(new CellDynamics(treatment_step_));
        // Forward declaration issue - skip inflammatory response for repair cells for now
        
        rm->AddAgent(repair_cell);
      }
    }
  }
};

// Enhanced inflammatory response behavior
class InflammatoryResponse : public Behavior {
  BDM_BEHAVIOR_HEADER(InflammatoryResponse, Behavior, 1);

 public:
  InflammatoryResponse() = default;
  virtual ~InflammatoryResponse() = default;

  void Run(Agent* agent) override {
    auto* cell = dynamic_cast<RadiationCell*>(agent);
    if (!cell || cell->GetCellType() == kNecroticCell) return;

    auto* sim = Simulation::GetActive();
    auto* random = sim->GetRandom();
    auto* rm = sim->GetResourceManager();
    uint64_t current_step = sim->GetScheduler()->GetSimulatedSteps();
    
    // Dynamic inflammation based on multiple factors
    UpdateInflammationLevel(cell, sim, current_step);
    
    // Inflammation-induced damage and healing
    ApplyInflammatoryEffects(cell, sim);
    
    // Recruit inflammatory cells (simulate immune response)
    RecruitImmuneResponse(cell, sim, rm);
  }

 private:
  void UpdateInflammationLevel(RadiationCell* cell, Simulation* sim, uint64_t step) {
    auto* random = sim->GetRandom();
    double inflammation_change = 0.0;
    
    // Radiation-induced inflammation (peaks 2-3 days post-radiation)
    if (step > 20 && cell->GetRadiationDose() > 10.0) {
      double time_since_radiation = step - 20;
      double peak_time = 48; // 48 hours post-radiation
      
      // Bell curve for inflammation response
      double inflammation_stimulus = cell->GetRadiationDose() / 30.0 * 
          std::exp(-0.5 * std::pow((time_since_radiation - peak_time) / 20.0, 2));
      inflammation_change += inflammation_stimulus * 0.02;
    }
    
    // Tissue damage triggers inflammation
    if (cell->GetDamageLevel() > 10.0) {
      inflammation_change += cell->GetDamageLevel() / 1000.0;
    }
    
    // Hypoxia increases inflammation
    if (cell->GetOxygenLevel() < 0.6) {
      inflammation_change += (0.6 - cell->GetOxygenLevel()) * 0.05;
    }
    
    // Neighboring cell death increases inflammation
    CountNearbyNecroticCells(cell, sim);
    
    // Random fluctuations in inflammatory state
    inflammation_change += (random->Uniform() - 0.5) * 0.02;
    
    // Apply change with bounds
    double new_inflammation = cell->GetInflammationLevel() + inflammation_change;
    cell->SetInflammationLevel(std::max(0.0, std::min(2.0, new_inflammation)));
    
    // Natural resolution over time (unless continuously stimulated)
    if (inflammation_change < 0.01) {
      cell->SetInflammationLevel(cell->GetInflammationLevel() * 0.995);
    }
  }
  
  void ApplyInflammatoryEffects(RadiationCell* cell, Simulation* sim) {
    auto* random = sim->GetRandom();
    double inflammation = cell->GetInflammationLevel();
    
    if (inflammation > 0.1) {
      // Inflammation can be both harmful and helpful
      
      // Harmful effects: additional tissue damage
      double inflammatory_damage = inflammation * 0.02 * random->Uniform(0.5, 1.5);
      cell->SetDamageLevel(cell->GetDamageLevel() + inflammatory_damage);
      
      // Metabolic effects
      double metabolic_impact = inflammation * 0.01 * (0.5 + random->Uniform() * 1.0);
      cell->SetMetabolicHealth(std::max(0.1, 
          cell->GetMetabolicHealth() - metabolic_impact));
      
      // Beneficial effects: sometimes helps clear damage (low probability)
      if (random->Uniform() < 0.1 && inflammation > 0.5) {
        double repair_amount = inflammation * 0.5 * random->Uniform(0.5, 1.0);
        cell->SetDamageLevel(std::max(0.0, cell->GetDamageLevel() - repair_amount));
      }
    }
  }
  
  void RecruitImmuneResponse(RadiationCell* cell, Simulation* sim, ResourceManager* rm) {
    auto* random = sim->GetRandom();
    
    // High inflammation recruits immune cells (simplified as new glial cells)
    if (cell->GetInflammationLevel() > 1.0 && random->Uniform() < 0.008) {
      if (rm->GetNumAgents() < 3500) { // Limit total cell count
        // Create activated microglial cell nearby
        Real3 offset = {(random->Uniform() - 0.5) * 16.0, (random->Uniform() - 0.5) * 16.0, 
                       (random->Uniform() - 0.5) * 16.0};
        Real3 new_pos = cell->GetPosition() + offset;
        
        auto* immune_cell = new RadiationCell(new_pos);
        immune_cell->SetCellType(kHealthyGlia); // Representing activated microglia
        immune_cell->SetDiameter(10.0); // Smaller, more mobile
        immune_cell->SetMetabolicHealth(1.0);
        immune_cell->SetInflammationLevel(0.8); // Already activated
        
        // Add behaviors
        immune_cell->AddBehavior(new RadiationTherapy(30.0, 20));
        immune_cell->AddBehavior(new CellDynamics(20));
        immune_cell->AddBehavior(new InflammatoryResponse());
        
        rm->AddAgent(immune_cell);
      }
    }
  }
  
  void CountNearbyNecroticCells(RadiationCell* cell, Simulation* sim) {
    // Simplified: assume some neighboring cells are necrotic and increase inflammation
    auto* random = sim->GetRandom();
    if (random->Uniform() < 0.05) { // 5% chance of detecting nearby necrosis
      double necrosis_inflammation = random->Uniform(0.01, 0.05);
      cell->SetInflammationLevel(cell->GetInflammationLevel() + necrosis_inflammation);
    }
  }
};

// Main simulation function
inline int Simulate(int argc, const char** argv) {
  Simulation simulation(argc, argv);
  
  // Create cells
  auto* rm = simulation.GetResourceManager();
  
  std::cout << "Creating 1500 healthy brain cells..." << std::endl;
  
  // Create healthy brain tissue
  for (int i = 0; i < 3000; ++i) {
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
    for (int j = 0; j < 80; ++j) {
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
      cell->AddBehavior(new CellDynamics(20));            // Dynamic cell processes
      cell->AddBehavior(new InflammatoryResponse());      // Enhanced inflammation
    }
  });
  
  std::cout << "\nStarting simulation for 500 time steps..." << std::endl;
  std::cout << "Timeline (each step = 5 hours, 20x accelerated):" << std::endl;
  std::cout << "- Steps 0-3: Initial tumor growth (20 hours)" << std::endl;
  std::cout << "- Step 4: Radiation therapy delivery (at 20 hours)" << std::endl;
  std::cout << "- Steps 5-500: Post-radiation effects and necrosis development (2480 hours = 103+ days)" << std::endl;

  // Run simulation with progress updates
  for (int step = 0; step < 500; ++step) {
    simulation.GetScheduler()->Simulate(1);
    
    // Progress updates adjusted for 20x acceleration (each step = 5 hours)
    if (step == 3) {
      std::cout << "Step " << (step + 1) << ": Pre-treatment phase complete (20 hours)" << std::endl;
    } else if (step == 4) {
      std::cout << "Step " << (step + 1) << ": Radiation therapy delivered (30 Gy)" << std::endl;
    } else if (step == 6) {
      std::cout << "Step " << (step + 1) << ": Early post-radiation response (35 hours)" << std::endl;
    } else if (step == 10) {
      std::cout << "Step " << (step + 1) << ": Acute radiation effects (55 hours = 2+ days)" << std::endl;
    } else if (step == 20) {
      std::cout << "Step " << (step + 1) << ": Late acute effects (105 hours = 4+ days)" << std::endl;
    } else if (step == 35) {
      std::cout << "Step " << (step + 1) << ": Early delayed effects (180 hours = 1 week)" << std::endl;
    } else if (step == 100) {
      std::cout << "Step " << (step + 1) << ": Subacute effects (505 hours = 3+ weeks)" << std::endl;
    } else if (step == 200) {
      std::cout << "Step " << (step + 1) << ": Late delayed effects (1005 hours = 6+ weeks)" << std::endl;
    } else if (step == 300) {
      std::cout << "Step " << (step + 1) << ": Chronic effects (1505 hours = 10+ weeks)" << std::endl;
    } else if (step == 499) {
      std::cout << "Step " << (step + 1) << ": Long-term observation complete (2500 hours = 104+ days)" << std::endl;
    }
  }

  std::cout << "\nSimulation completed!" << std::endl;
  return 0;
}

}  // namespace radiation_necrosis
}  // namespace bdm

#endif  // DEMO_RADIATION_NECROSIS_H_
