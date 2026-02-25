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
// \title Radiation Necrosis Model - Enhanced for Visualization
// \visualize
//
// This model implements radiation necrosis with enhanced visualization
// based on the 2024 paper computational approach
//

#ifndef DEMO_RADIATION_NECROSIS_H_
#define DEMO_RADIATION_NECROSIS_H_

#include "biodynamo.h"

namespace bdm {
namespace radiation_necrosis {

// Cell types based on paper's 6 key populations
enum CellType {
  kProliferatingTumor = 0,     // Proliferating tumor cells
  kDamagedTumor = 1,           // Damaged tumor cells  
  kHealthyBrain = 2,           // Normal healthy brain cells
  kDamagedBrain = 3,           // Damaged healthy brain cells
  kNecroticCell = 4,           // Necrotic cells
  kActivatedImmune = 5,        // Activated immune cells
  kNonActivatedImmune = 6      // Non-activated immune cells
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

  bool IsAlive() const { 
    return cell_type_ != kNecroticCell; 
  }

  // Paper-specific properties
  void SetDamageLevel(double damage) { damage_level_ = damage; }
  double GetDamageLevel() const { return damage_level_; }
  
  void SetBirthStep(uint64_t step) { birth_step_ = step; }
  uint64_t GetBirthStep() const { return birth_step_; }

  // Functions based on paper's cell populations
  bool IsTumorCell() const {
    return cell_type_ == kProliferatingTumor || cell_type_ == kDamagedTumor;
  }
  
  bool IsHealthyBrainCell() const {
    return cell_type_ == kHealthyBrain || cell_type_ == kDamagedBrain;
  }
  
  bool IsImmuneCell() const {
    return cell_type_ == kActivatedImmune || cell_type_ == kNonActivatedImmune;
  }

 private:
  CellType cell_type_ = kHealthyBrain;
  double damage_level_ = 0.0;
  uint64_t birth_step_ = 0;
};

// Enhanced Radiation Necrosis behavior - no dynamic cell creation for stability
class RadiationNecrosis : public Behavior {
  BDM_BEHAVIOR_HEADER(RadiationNecrosis, Behavior, 1);

 public:
  RadiationNecrosis() = default;
  virtual ~RadiationNecrosis() = default;

  void Run(Agent* agent) override {
    auto* cell = bdm_static_cast<RadiationCell*>(agent);
    auto* sim = Simulation::GetActive();
    auto current_step = sim->GetScheduler()->GetSimulatedSteps();
    
    // Apply SRS treatment at step 1
    if (current_step == 1) {
      ApplySRSTreatment(cell, sim);
    }
    
    // Progressive radiation necrosis development
    if (current_step > 1) {
      ApplyProgressiveNecrosis(cell, sim, current_step);
    }
  }

 private:
  void ApplySRSTreatment(RadiationCell* cell, Simulation* sim) {
    CellType type = cell->GetCellType();
    auto* random = sim->GetRandom();
    
    switch (type) {
      case kProliferatingTumor:
        // Tumors get damaged but some survive
        if (random->Uniform() < 0.7) {
          cell->SetCellType(kDamagedTumor);
          cell->SetDamageLevel(0.8);
          cell->SetDiameter(12.0); // Slightly smaller
        } else if (random->Uniform() < 0.2) {
          cell->SetCellType(kNecroticCell);
          cell->SetDiameter(8.0); // Much smaller
        }
        break;
        
      case kHealthyBrain:
        // AGGRESSIVE radiation necrosis - most brain tissue affected
        if (random->Uniform() < 0.9) { // 90% of brain tissue damaged!
          cell->SetCellType(kDamagedBrain);
          cell->SetDamageLevel(0.9);
          cell->SetDiameter(8.0); // Smaller than healthy (12.0)
        }
        if (random->Uniform() < 0.3) { // 30% immediate necrosis
          cell->SetCellType(kNecroticCell);
          cell->SetDiameter(4.0); // Very small for clear visualization
        }
        break;
        
      case kNonActivatedImmune:
        // Immune cells activate in response to radiation
        if (random->Uniform() < 0.8) {
          cell->SetCellType(kActivatedImmune);
          cell->SetDiameter(10.0); // Larger when activated
        }
        break;
        
      default:
        break;
    }
  }
  
  void ApplyProgressiveNecrosis(RadiationCell* cell, Simulation* sim, uint64_t step) {
    auto* random = sim->GetRandom();
    
    // Progressive necrosis development over months (key RN characteristic)
    if (cell->GetCellType() == kDamagedBrain) {
      // Time-dependent necrosis progression
      double months = step * 0.1; // Convert steps to months
      double necrosis_rate = 0.02 + (months * 0.005); // Increases over time
      
      if (random->Uniform() < necrosis_rate) {
        cell->SetCellType(kNecroticCell);
        cell->SetDiameter(3.0); // Very small for dramatic visual effect
      }
    }
    
    // Damaged tumor cells also become necrotic but slower
    if (cell->GetCellType() == kDamagedTumor) {
      if (random->Uniform() < 0.01) {
        cell->SetCellType(kNecroticCell);
        cell->SetDiameter(5.0);
      }
    }
    
    // Secondary necrosis: healthy cells near necrotic regions become damaged
    if (cell->GetCellType() == kHealthyBrain && step > 30) {
      double spread_rate = (step - 30) * 0.0003;
      if (random->Uniform() < spread_rate) {
        cell->SetCellType(kDamagedBrain);
        cell->SetDamageLevel(0.8);
        cell->SetDiameter(8.0);
      }
    }
    
    // Tumor regrowth (slower than necrosis - key for β calculation)
    if (cell->GetCellType() == kProliferatingTumor && step > 50) {
      if (random->Uniform() < 0.005) {
        double current_diameter = cell->GetDiameter();
        cell->SetDiameter(std::min(20.0, current_diameter * 1.02)); // Slow growth
      }
    }
  }
};

// Main simulation with enhanced visualization focus
inline int Simulate(int argc, const char** argv) {
  Simulation simulation(argc, argv);
  
  auto* rm = simulation.GetResourceManager();
  
  std::cout << "=== Enhanced Radiation Necrosis Simulation ===" << std::endl;
  std::cout << "Designed for clear ParaView visualization of RN development" << std::endl;
  std::cout << "Key features: Aggressive brain tissue necrosis, progressive spreading\n" << std::endl;
  
  // Create brain tissue in organized pattern for better visualization
  std::cout << "Creating organized brain tissue..." << std::endl;
  
  // Healthy brain tissue in a regular grid for clear visualization
  for (int x = -50; x <= 50; x += 10) {
    for (int y = -50; y <= 50; y += 10) {
      for (int z = -20; z <= 20; z += 10) {
        Real3 position = {static_cast<double>(x), static_cast<double>(y), static_cast<double>(z)};
        
        auto* cell = new RadiationCell(position);
        cell->SetCellType(kHealthyBrain);
        cell->SetDiameter(12.0);
        cell->SetBirthStep(0);
        rm->AddAgent(cell);
      }
    }
  }
  
  // Create 3 distinct tumor clusters
  std::cout << "Creating tumor metastases..." << std::endl;
  Real3 tumor_centers[3] = {{-30, -30, 0}, {30, 30, 0}, {0, -30, 15}};
  
  for (int t = 0; t < 3; ++t) {
    for (int i = -5; i <= 5; i += 5) {
      for (int j = -5; j <= 5; j += 5) {
        for (int k = -3; k <= 3; k += 3) {
          Real3 position = tumor_centers[t] + Real3{static_cast<double>(i), 
                                                   static_cast<double>(j), 
                                                   static_cast<double>(k)};
          
          auto* tumor_cell = new RadiationCell(position);
          tumor_cell->SetCellType(kProliferatingTumor);
          tumor_cell->SetDiameter(16.0); // Larger than brain cells
          tumor_cell->SetBirthStep(0);
          rm->AddAgent(tumor_cell);
        }
      }
    }
  }
  
  // Immune cells distributed throughout
  std::cout << "Creating immune surveillance..." << std::endl;
  for (int i = 0; i < 100; ++i) {
    Real3 position = {simulation.GetRandom()->Uniform(-60, 60),
                      simulation.GetRandom()->Uniform(-60, 60),
                      simulation.GetRandom()->Uniform(-30, 30)};
    
    auto* immune_cell = new RadiationCell(position);
    immune_cell->SetCellType(kNonActivatedImmune);
    immune_cell->SetDiameter(8.0);
    immune_cell->SetBirthStep(0);
    rm->AddAgent(immune_cell);
  }
  
  std::cout << "Initial populations created:" << std::endl;
  std::cout << "- " << (11*11*5) << " healthy brain cells (organized grid)" << std::endl;
  std::cout << "- " << (3*3*3*3) << " proliferating tumor cells (3 clusters)" << std::endl;
  std::cout << "- 100 immune cells" << std::endl;
  
  // Add radiation necrosis behavior to all cells
  rm->ForEachAgent([&](Agent* agent) {
    auto* cell = dynamic_cast<RadiationCell*>(agent);
    if (cell) {
      cell->AddBehavior(new RadiationNecrosis());
    }
  });
  
  std::cout << "\nStarting enhanced radiation necrosis simulation..." << std::endl;
  std::cout << "Watch for:" << std::endl;
  std::cout << "- Immediate brain tissue damage (step 1)" << std::endl;
  std::cout << "- Progressive necrosis spreading (steps 2-100)" << std::endl;
  std::cout << "- Secondary tissue damage (steps 30+)" << std::endl;
  std::cout << "- Tumor vs necrosis growth patterns" << std::endl;
  
  // Run simulation with detailed progress tracking
  for (int step = 0; step < 100; ++step) { // Shorter for stability
    simulation.GetScheduler()->Simulate(1);
    
    // Count cell types every 20 steps
    if (step % 20 == 0 && step > 0) {
      int counts[7] = {0};
      
      rm->ForEachAgent([&](Agent* agent) {
        auto* cell = dynamic_cast<RadiationCell*>(agent);
        if (cell) {
          counts[static_cast<int>(cell->GetCellType())]++;
        }
      });
      
      double months = step * 0.1;
      std::cout << "Month " << std::fixed << std::setprecision(1) << months << ": ";
      std::cout << "Necrotic=" << counts[4] << ", Damaged brain=" << counts[3] 
                << ", Healthy brain=" << counts[2] << ", Tumors=" << counts[0] << std::endl;
      
      // Calculate necrosis percentage
      double necrosis_percent = (counts[4] * 100.0) / (counts[2] + counts[3] + counts[4] + 1);
      std::cout << "  -> Brain necrosis: " << std::fixed << std::setprecision(1) 
                << necrosis_percent << "%" << std::endl;
    }
  }
  
  // Final summary
  int final_counts[7] = {0};
  rm->ForEachAgent([&](Agent* agent) {
    auto* cell = dynamic_cast<RadiationCell*>(agent);
    if (cell) {
      final_counts[static_cast<int>(cell->GetCellType())]++;
    }
  });
  
  std::cout << "\n=== FINAL RADIATION NECROSIS RESULTS ===" << std::endl;
  std::cout << "Proliferating tumor: " << final_counts[0] << std::endl;
  std::cout << "Damaged tumor: " << final_counts[1] << std::endl;
  std::cout << "Healthy brain: " << final_counts[2] << std::endl;
  std::cout << "Damaged brain: " << final_counts[3] << std::endl;
  std::cout << "NECROTIC CELLS: " << final_counts[4] << " <<<--- RADIATION NECROSIS" << std::endl;
  std::cout << "Activated immune: " << final_counts[5] << std::endl;
  std::cout << "Non-activated immune: " << final_counts[6] << std::endl;
  
  double necrosis_rate = (final_counts[4] * 100.0) / 
                        (final_counts[2] + final_counts[3] + final_counts[4] + 1);
  std::cout << "\nRadiation Necrosis Rate: " << std::fixed << std::setprecision(1) 
            << necrosis_rate << "% of brain tissue" << std::endl;
  
  if (necrosis_rate > 50) {
    std::cout << "SEVERE RADIATION NECROSIS - clearly visible in ParaView!" << std::endl;
  } else if (necrosis_rate > 20) {
    std::cout << "MODERATE RADIATION NECROSIS - should be visible in ParaView" << std::endl;
  }
  
  std::cout << "\nParaView Visualization Tips:" << std::endl;
  std::cout << "- Color by 'cell_type_' to see different cell populations" << std::endl;
  std::cout << "- Necrotic cells (type 4) are VERY SMALL (diameter 3-5)" << std::endl;
  std::cout << "- Healthy brain cells (type 2) are medium (diameter 12)" << std::endl;
  std::cout << "- Tumor cells (type 0) are large (diameter 16+)" << std::endl;
  std::cout << "- Use 'Glyph' filter to show cell sizes clearly" << std::endl;
  
  return 0;
}

}  // namespace radiation_necrosis
}  // namespace bdm

#endif  // DEMO_RADIATION_NECROSIS_H_
