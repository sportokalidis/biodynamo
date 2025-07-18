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

/// @file demo_std_replacements.cc
/// @brief Demonstration of ROOT replacement with standard C++ libraries
/// This demo shows how to use the new standard C++ based implementations
/// as alternatives to ROOT functionality in BioDynaMo.

#include <iostream>
#include <vector>
#include <chrono>
#include <iomanip>

// Include new standard implementations
#include "core/util/random_std.h"
#include "core/util/random_compat.h"

// Include serialization only if available
#ifdef BDM_USE_BOOST_SERIALIZATION
#include "core/util/serialization_std.h" 
#define SERIALIZATION_AVAILABLE 1
#else
#define SERIALIZATION_AVAILABLE 0
#endif

using namespace bdm;
using namespace bdm::experimental;

/// Simple agent class for demonstration
class DemoAgent {
 public:
  DemoAgent() = default;
  DemoAgent(int id, double diameter, const std::string& type)
    : id_(id), diameter_(diameter), type_(type) {}
  
  // Getters
  int GetId() const { return id_; }
  double GetDiameter() const { return diameter_; }
  const std::string& GetType() const { return type_; }
  
  // Setters
  void SetDiameter(double diameter) { diameter_ = diameter; }
  void SetType(const std::string& type) { type_ = type; }
  
  bool operator==(const DemoAgent& other) const {
    return id_ == other.id_ && 
           std::abs(diameter_ - other.diameter_) < 1e-9 &&
           type_ == other.type_;
  }
  
  void Print() const {
    std::cout << "Agent[" << id_ << "]: diameter=" << diameter_ 
              << ", type=" << type_ << std::endl;
  }
  
#if SERIALIZATION_AVAILABLE
  // Boost serialization support (only if available)
  template <class Archive>
  void serialize(Archive& ar, const unsigned int version) {
    ar & id_ & diameter_ & type_;
  }
#endif

 private:
  int id_ = 0;
  double diameter_ = 1.0;
  std::string type_ = "cell";
};

/// Demo 1: Random number generation comparison
void DemoRandomGeneration() {
  std::cout << "\n=== Demo 1: Random Number Generation ===\n";
  
  // Using the new standard C++ generator
  std::cout << "Using Standard C++ Random Generator:\n";
  SetStdSeed(42);
  auto& std_rng = GetStdRng();
  
  std::cout << "Uniform samples [0,1): ";
  for (int i = 0; i < 10; ++i) {
    std::cout << std::fixed << std::setprecision(3) << std_rng.Uniform() << " ";
  }
  std::cout << "\n";
  
  std::cout << "Gaussian samples (μ=0, σ=1): ";
  for (int i = 0; i < 10; ++i) {
    std::cout << std::fixed << std::setprecision(3) << std_rng.Gaussian() << " ";
  }
  std::cout << "\n";
  
  std::cout << "Poisson samples (λ=3): ";
  for (int i = 0; i < 10; ++i) {
    std::cout << std_rng.Poisson(3.0) << " ";
  }
  std::cout << "\n";
  
  // Using distribution classes
  std::cout << "\nUsing Distribution Classes:\n";
  auto rng = std::make_shared<StdRandomGenerator>(42);
  
  StdUniformRng uniform_dist(5.0, 15.0);
  uniform_dist.SetRandomGenerator(rng);
  
  std::cout << "Uniform samples [5,15): ";
  auto samples = uniform_dist.SampleArray<10>();
  for (const auto& sample : samples) {
    std::cout << std::fixed << std::setprecision(1) << sample << " ";
  }
  std::cout << "\n";
  
  // Using compatibility layer
  std::cout << "\nUsing Compatibility Layer:\n";
  std::cout << "Implementation: " << compat::GetUniversalRng().GetImplementation() << "\n";
  BDM_RNG_SET_SEED(42);
  std::cout << "Uniform samples: ";
  for (int i = 0; i < 5; ++i) {
    std::cout << std::fixed << std::setprecision(3) << BDM_RNG_UNIFORM() << " ";
  }
  std::cout << "\n";
}

/// Demo 2: Serialization comparison  
void DemoSerialization() {
  std::cout << "\n=== Demo 2: Serialization ===\n";

#if SERIALIZATION_AVAILABLE
  std::cout << "Using Boost.Serialization\n";
  
  // Create test agents
  std::vector<DemoAgent> agents;
  agents.emplace_back(1, 10.5, "neuron");
  agents.emplace_back(2, 8.2, "astrocyte");
  agents.emplace_back(3, 12.1, "microglia");
  
  std::cout << "Original agents:\n";
  for (const auto& agent : agents) {
    agent.Print();
  }
  
  const std::string filename = "demo_agents.dat";
  
  // Serialize agents
  try {
    WriteBoostObject(filename, "agent_list", agents);
    std::cout << "\nAgents serialized to " << filename << "\n";
    
    // Deserialize agents
    std::vector<DemoAgent> restored_agents;
    bool success = ReadBoostObject(filename, "agent_list", restored_agents);
    
    if (success) {
      std::cout << "\nRestored agents:\n";
      for (const auto& agent : restored_agents) {
        agent.Print();
      }
      
      // Verify they're identical
      bool identical = (agents.size() == restored_agents.size());
      if (identical) {
        for (size_t i = 0; i < agents.size(); ++i) {
          if (!(agents[i] == restored_agents[i])) {
            identical = false;
            break;
          }
        }
      }
      
      std::cout << "\nSerialization test: " << (identical ? "PASSED" : "FAILED") << "\n";
    } else {
      std::cout << "Failed to restore agents!\n";
    }
    
    // Cleanup
    RemoveFile(filename);
    
  } catch (const std::exception& e) {
    std::cout << "Serialization error: " << e.what() << "\n";
  }
#else
  std::cout << "Boost serialization not available.\n";
  std::cout << "To enable: cmake -DBDM_USE_BOOST_SERIALIZATION=ON ..\n";
  std::cout << "Make sure Boost is installed on your system.\n";
#endif
}

/// Demo 3: Performance comparison
void DemoPerformance() {
  std::cout << "\n=== Demo 3: Performance Comparison ===\n";
  
  const int n_samples = 1000000;
  std::cout << "Generating " << n_samples << " random numbers...\n";
  
  // Test standard C++ generator
  auto start = std::chrono::high_resolution_clock::now();
  SetStdSeed(42);
  double sum = 0.0;
  for (int i = 0; i < n_samples; ++i) {
    sum += GetStdRng().Uniform();
  }
  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  
  std::cout << "Standard C++ RNG:\n";
  std::cout << "  Time: " << duration.count() << " ms\n";
  std::cout << "  Rate: " << std::fixed << std::setprecision(0) 
            << (n_samples / (duration.count() / 1000.0)) << " samples/sec\n";
  std::cout << "  Sum (verification): " << std::fixed << std::setprecision(3) << sum << "\n";
  
  // Test serialization performance
  std::cout << "\nSerialization Performance:\n";
  
#if SERIALIZATION_AVAILABLE
  DemoAgent test_agent(999, 15.7, "performance_test");
  
  const std::string perf_file = "perf_test.dat";
  const int n_iterations = 1000;
  
  start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < n_iterations; ++i) {
    WriteBoostObject(perf_file, "perf_agent", test_agent);
    DemoAgent restored;
    ReadBoostObject(perf_file, "perf_agent", restored);
  }
  end = std::chrono::high_resolution_clock::now();
  duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  
  std::cout << "Boost Serialization:\n";
  std::cout << "  " << n_iterations << " write/read cycles: " << duration.count() << " ms\n";
  std::cout << "  Rate: " << std::fixed << std::setprecision(1) 
            << (n_iterations / (duration.count() / 1000.0)) << " cycles/sec\n";
  
  RemoveFile(perf_file);
#else
  std::cout << "Boost serialization not available for performance testing.\n";
#endif
}

/// Demo 4: Advanced random distributions
void DemoAdvancedDistributions() {
  std::cout << "\n=== Demo 4: Advanced Distributions ===\n";
  
  auto rng = std::make_shared<StdRandomGenerator>(42);
  
  // User-defined distribution: simple parabola
  auto parabola = [](double x) -> double { 
    return 4.0 * x * (1.0 - x); // Peak at x=0.5
  };
  
  StdUserDefinedRng user_dist(parabola, 0.0, 1.0);
  user_dist.SetRandomGenerator(rng);
  
  std::cout << "User-defined parabolic distribution samples:\n";
  for (int i = 0; i < 20; ++i) {
    std::cout << std::fixed << std::setprecision(3) << user_dist.Sample() << " ";
    if ((i + 1) % 10 == 0) std::cout << "\n";
  }
  
  // Show statistics
  const int n_stats = 100000;
  double sum = 0.0;
  double sum_sq = 0.0;
  for (int i = 0; i < n_stats; ++i) {
    double val = user_dist.Sample();
    sum += val;
    sum_sq += val * val;
  }
  
  double mean = sum / n_stats;
  double variance = (sum_sq / n_stats) - (mean * mean);
  
  std::cout << "\nStatistics from " << n_stats << " samples:\n";
  std::cout << "  Mean: " << std::fixed << std::setprecision(3) << mean << "\n";
  std::cout << "  Variance: " << std::fixed << std::setprecision(3) << variance << "\n";
  std::cout << "  (Expected mean for parabola: 0.5)\n";
}

int main() {
  std::cout << "BioDynaMo ROOT Replacement Demo\n";
  std::cout << "===============================\n";
  std::cout << "This demo shows how standard C++ libraries can replace\n";
  std::cout << "some ROOT functionality in BioDynaMo.\n";
  
  try {
    DemoRandomGeneration();
    DemoSerialization();
    DemoPerformance();
    DemoAdvancedDistributions();
    
    std::cout << "\n=== Summary ===\n";
    std::cout << "✓ Random number generation with std::random\n";
#if SERIALIZATION_AVAILABLE
    std::cout << "✓ Serialization with Boost.Serialization\n";
#else
    std::cout << "- Serialization (Boost not available)\n";
#endif
    std::cout << "✓ Performance testing\n";
    std::cout << "✓ Advanced user-defined distributions\n";
    std::cout << "✓ Backward compatibility layer\n";
    std::cout << "\nAll available demos completed successfully!\n";
    std::cout << "\nTo enable all features:\n";
    std::cout << "  cmake -DBDM_USE_STD_RANDOM=ON -DBDM_USE_BOOST_SERIALIZATION=ON ..\n";
#if !SERIALIZATION_AVAILABLE
    std::cout << "\nTo install Boost:\n";
    std::cout << "  Ubuntu/Debian: sudo apt-get install libboost-all-dev\n";
    std::cout << "  macOS: brew install boost\n";
    std::cout << "  CentOS/RHEL: sudo yum install boost-devel\n";
#endif
    
  } catch (const std::exception& e) {
    std::cerr << "Demo error: " << e.what() << std::endl;
    return 1;
  }
  
  return 0;
}
