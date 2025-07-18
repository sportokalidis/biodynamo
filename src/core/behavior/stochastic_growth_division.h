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

#ifndef CORE_BEHAVIOR_STOCHASTIC_GROWTH_DIVISION_H_
#define CORE_BEHAVIOR_STOCHASTIC_GROWTH_DIVISION_H_

#include "core/agent/cell.h"
#include "core/agent/cell_division_event.h"
#include "core/behavior/behavior.h"
#include "core/util/log.h"
#include "core/util/root.h"
#include "core/util/random_hybrid.h"  // Use our new hybrid random system

namespace bdm {

/// Enhanced growth division behavior with stochastic elements
/// This demonstrates how to use the new random number generation system
/// to replace ROOT dependencies while maintaining the same functionality.
class StochasticGrowthDivision : public Behavior {
  BDM_BEHAVIOR_HEADER(StochasticGrowthDivision, Behavior, 1);

 public:
  StochasticGrowthDivision() { AlwaysCopyToNew(); }
  
  StochasticGrowthDivision(real_t threshold_mean, real_t threshold_std,
                          real_t growth_rate_mean, real_t growth_rate_std)
      : threshold_mean_(threshold_mean), threshold_std_(threshold_std),
        growth_rate_mean_(growth_rate_mean), growth_rate_std_(growth_rate_std) {
    // Initialize with random values for this specific agent
    auto* rng = GetHybridRng();
    threshold_ = rng->Gaus(threshold_mean_, threshold_std_);
    growth_rate_ = rng->Gaus(growth_rate_mean_, growth_rate_std_);
    
    // Ensure positive values
    threshold_ = std::max(threshold_, 5.0);
    growth_rate_ = std::max(growth_rate_, 10.0);
  }

  virtual ~StochasticGrowthDivision() = default;

  void Initialize(const NewAgentEvent& event) override {
    Base::Initialize(event);

    auto* other = event.existing_behavior;
    if (auto* sgd = dynamic_cast<StochasticGrowthDivision*>(other)) {
      threshold_mean_ = sgd->threshold_mean_;
      threshold_std_ = sgd->threshold_std_;
      growth_rate_mean_ = sgd->growth_rate_mean_;
      growth_rate_std_ = sgd->growth_rate_std_;
      
      // Each new agent gets its own random parameters
      auto* rng = GetHybridRng();
      threshold_ = rng->Gaus(threshold_mean_, threshold_std_);
      growth_rate_ = rng->Gaus(growth_rate_mean_, growth_rate_std_);
      
      // Ensure positive values
      threshold_ = std::max(threshold_, 5.0);
      growth_rate_ = std::max(growth_rate_, 10.0);
    } else {
      Log::Fatal("StochasticGrowthDivision::Initialize",
                 "event.existing_behavior was not of type StochasticGrowthDivision");
    }
  }

  void Run(Agent* agent) override {
    if (auto* cell = dynamic_cast<Cell*>(agent)) {
      auto* rng = GetHybridRng();
      
      if (cell->GetDiameter() <= threshold_) {
        // Add stochastic variation to growth rate
        real_t stochastic_growth = rng->Gaus(growth_rate_, growth_rate_ * 0.1);
        stochastic_growth = std::max(stochastic_growth, 0.0);
        
        cell->ChangeVolume(stochastic_growth);
        
        // Small probability of spontaneous division even below threshold
        if (rng->Uniform() < spontaneous_division_prob_) {
          cell->Divide();
        }
      } else {
        // Probability-based division above threshold
        real_t division_prob = CalculateDivisionProbability(cell->GetDiameter());
        if (rng->Uniform() < division_prob) {
          cell->Divide();
        }
      }
    } else {
      Log::Fatal("StochasticGrowthDivision::Run", "Agent is not a Cell");
    }
  }
  
  /// Get current threshold for this agent
  real_t GetThreshold() const { return threshold_; }
  
  /// Get current growth rate for this agent
  real_t GetGrowthRate() const { return growth_rate_; }
  
  /// Get the random implementation being used
  std::string GetRandomImplementation() const {
    return GetHybridRng()->GetImplementation();
  }

 private:
  // Population parameters (shared across all agents)
  real_t threshold_mean_ = 40.0;
  real_t threshold_std_ = 5.0;
  real_t growth_rate_mean_ = 300.0;
  real_t growth_rate_std_ = 50.0;
  
  // Individual agent parameters (random variation)
  real_t threshold_ = 40.0;
  real_t growth_rate_ = 300.0;
  
  // Additional stochastic parameters
  real_t spontaneous_division_prob_ = 0.001;  // 0.1% chance per step
  
  /// Calculate probability of division based on size
  real_t CalculateDivisionProbability(real_t diameter) const {
    // Sigmoid function: higher diameter = higher division probability
    real_t excess = diameter - threshold_;
    real_t sigmoid = 1.0 / (1.0 + std::exp(-excess / 5.0));
    return sigmoid * 0.1;  // Max 10% chance per step
  }
};

/// Factory function to create stochastic growth behavior with different presets
namespace stochastic_growth {

/// Create fast-growing, highly variable cells
inline StochasticGrowthDivision* FastGrowing() {
  return new StochasticGrowthDivision(35.0, 8.0, 400.0, 80.0);
}

/// Create slow-growing, consistent cells  
inline StochasticGrowthDivision* SlowGrowing() {
  return new StochasticGrowthDivision(50.0, 2.0, 200.0, 20.0);
}

/// Create highly variable cells
inline StochasticGrowthDivision* HighVariability() {
  return new StochasticGrowthDivision(40.0, 15.0, 300.0, 100.0);
}

} // namespace stochastic_growth

/// Utility class to analyze growth behavior statistics
class GrowthAnalyzer {
 public:
  static void AnalyzePopulation(const std::vector<Agent*>& agents) {
    std::vector<real_t> thresholds, growth_rates, diameters;
    int count_with_behavior = 0;
    
    for (auto* agent : agents) {
      if (auto* cell = dynamic_cast<Cell*>(agent)) {
        diameters.push_back(cell->GetDiameter());
        
        // Check if it has our stochastic behavior
        auto behaviors = cell->GetAllBehaviors();
        for (auto* behavior : behaviors) {
          if (auto* sgd = dynamic_cast<StochasticGrowthDivision*>(behavior)) {
            thresholds.push_back(sgd->GetThreshold());
            growth_rates.push_back(sgd->GetGrowthRate());
            count_with_behavior++;
            break;
          }
        }
      }
    }
    
    std::cout << "\n=== Growth Behavior Analysis ===\n";
    std::cout << "Total agents: " << agents.size() << "\n";
    std::cout << "With stochastic growth: " << count_with_behavior << "\n";
    
    if (!diameters.empty()) {
      auto [min_d, max_d] = std::minmax_element(diameters.begin(), diameters.end());
      std::cout << "Diameter range: " << *min_d << " - " << *max_d << "\n";
    }
    
    if (!thresholds.empty()) {
      real_t mean_threshold = std::accumulate(thresholds.begin(), thresholds.end(), 0.0) / thresholds.size();
      std::cout << "Mean division threshold: " << mean_threshold << "\n";
    }
    
    if (!growth_rates.empty()) {
      real_t mean_growth = std::accumulate(growth_rates.begin(), growth_rates.end(), 0.0) / growth_rates.size();
      std::cout << "Mean growth rate: " << mean_growth << "\n";
    }
    
    if (count_with_behavior > 0) {
      // Get random implementation info from first agent
      for (auto* agent : agents) {
        if (auto* cell = dynamic_cast<Cell*>(agent)) {
          auto behaviors = cell->GetAllBehaviors();
          for (auto* behavior : behaviors) {
            if (auto* sgd = dynamic_cast<StochasticGrowthDivision*>(behavior)) {
              std::cout << "Random implementation: " << sgd->GetRandomImplementation() << "\n";
              goto done_checking;
            }
          }
        }
      }
      done_checking:;
    }
    std::cout << "=============================\n";
  }
};

}  // namespace bdm

#endif  // CORE_BEHAVIOR_STOCHASTIC_GROWTH_DIVISION_H_
