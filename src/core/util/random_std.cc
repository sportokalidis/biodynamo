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

#include "core/util/random_std.h"
#include <chrono>
#include <iostream>

namespace bdm {
namespace experimental {

// Thread-local global random generator definition
thread_local StdRandomGenerator global_std_rng;

// Function implementations
StdRandomGenerator& GetStdRng() {
  return global_std_rng;
}

void SetStdSeed(uint64_t seed) {
  global_std_rng.SetSeed(seed);
}

// Additional utility functions for the standard random generators

/// Seed the global generator with current time
void SeedStdRngWithTime() {
  auto now = std::chrono::high_resolution_clock::now();
  auto duration = now.time_since_epoch();
  auto seed = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
  SetStdSeed(static_cast<uint64_t>(seed));
}

/// Create a seeded generator for reproducible tests
std::shared_ptr<StdRandomGenerator> CreateSeededGenerator(uint64_t seed) {
  return std::make_shared<StdRandomGenerator>(seed);
}

/// Print information about the random generator
void PrintStdRngInfo() {
  std::cout << "BioDynaMo Standard Random Generator Information:\n";
  std::cout << "  Engine: std::mt19937 (Mersenne Twister)\n";
  std::cout << "  Thread-safe: Yes (thread_local)\n";
  std::cout << "  Seed: User-defined or system time\n";
  std::cout << "  Distributions: C++11 standard library + Boost Math\n";
}

/// Benchmark function to compare performance with ROOT
void BenchmarkStdRng(int num_samples = 1000000) {
  auto start = std::chrono::high_resolution_clock::now();
  
  real_t sum = 0.0;
  for (int i = 0; i < num_samples; ++i) {
    sum += GetStdRng().Uniform();
  }
  
  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  
  std::cout << "Standard RNG Benchmark:\n";
  std::cout << "  Samples: " << num_samples << "\n";
  std::cout << "  Time: " << duration.count() << " ms\n";
  std::cout << "  Rate: " << (num_samples / (duration.count() / 1000.0)) << " samples/sec\n";
  std::cout << "  Sum (for verification): " << sum << "\n";
}

} // namespace experimental
} // namespace bdm
