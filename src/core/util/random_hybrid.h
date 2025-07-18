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

#ifndef CORE_UTIL_RANDOM_HYBRID_H_
#define CORE_UTIL_RANDOM_HYBRID_H_

#include "core/util/root.h"

// Check if we should use standard C++ random
#ifndef BDM_USE_STD_RANDOM
#define BDM_USE_STD_RANDOM 0
#endif

#if BDM_USE_STD_RANDOM
#include "core/util/random_std.h"
#else
#include <TRandom.h>
#include <TRandom3.h>
#endif

namespace bdm {

/// Hybrid random number generator that can use either ROOT or standard C++
class HybridRandom {
 public:
  HybridRandom() {
#if BDM_USE_STD_RANDOM
    std_generator_ = std::make_unique<experimental::StdRandomGenerator>();
#else
    root_generator_ = new TRandom3();
#endif
  }
  
  explicit HybridRandom(uint64_t seed) {
#if BDM_USE_STD_RANDOM
    std_generator_ = std::make_unique<experimental::StdRandomGenerator>(seed);
#else
    root_generator_ = new TRandom3(seed);
#endif
  }
  
  ~HybridRandom() {
#if !BDM_USE_STD_RANDOM
    delete root_generator_;
#endif
  }
  
  /// Set the random seed
  void SetSeed(uint64_t seed) {
#if BDM_USE_STD_RANDOM
    std_generator_->SetSeed(seed);
#else
    root_generator_->SetSeed(seed);
#endif
  }
  
  /// Generate uniform random number [0, 1)
  real_t Uniform() {
#if BDM_USE_STD_RANDOM
    return std_generator_->Uniform();
#else
    return root_generator_->Uniform();
#endif
  }
  
  /// Generate uniform random number [min, max)
  real_t Uniform(real_t min, real_t max) {
#if BDM_USE_STD_RANDOM
    return std_generator_->Uniform(min, max);
#else
    return root_generator_->Uniform(min, max);
#endif
  }
  
  /// Generate Gaussian distributed random number
  real_t Gaus(real_t mean = 0.0, real_t sigma = 1.0) {
#if BDM_USE_STD_RANDOM
    return std_generator_->Gaussian(mean, sigma);
#else
    return root_generator_->Gaus(mean, sigma);
#endif
  }
  
  /// Generate exponentially distributed random number
  real_t Exp(real_t tau) {
#if BDM_USE_STD_RANDOM
    return std_generator_->Exponential(1.0 / tau);
#else
    return root_generator_->Exp(tau);
#endif
  }
  
  /// Generate Poisson distributed random integer
  int Poisson(real_t mean) {
#if BDM_USE_STD_RANDOM
    return std_generator_->Poisson(mean);
#else
    return root_generator_->Poisson(mean);
#endif
  }
  
  /// Generate binomial distributed random integer
  int Binomial(int ntot, real_t prob) {
#if BDM_USE_STD_RANDOM
    return std_generator_->Binomial(ntot, prob);
#else
    return root_generator_->Binomial(ntot, prob);
#endif
  }
  
  /// Get implementation info
  std::string GetImplementation() const {
#if BDM_USE_STD_RANDOM
    return "Standard C++";
#else
    return "ROOT";
#endif
  }
  
  /// Get the underlying generator (for advanced usage)
#if BDM_USE_STD_RANDOM
  experimental::StdRandomGenerator* GetStdGenerator() { return std_generator_.get(); }
#else
  TRandom* GetRootGenerator() { return root_generator_; }
#endif

 private:
#if BDM_USE_STD_RANDOM
  std::unique_ptr<experimental::StdRandomGenerator> std_generator_;
#else
  TRandom* root_generator_ = nullptr;
#endif
  
  BDM_CLASS_DEF_NV(HybridRandom, 1);
};

/// Thread-local hybrid random generator
thread_local HybridRandom* hybrid_random_generator = nullptr;

/// Get the global hybrid random generator
inline HybridRandom* GetHybridRng() {
  if (!hybrid_random_generator) {
    hybrid_random_generator = new HybridRandom();
  }
  return hybrid_random_generator;
}

/// Set global seed for hybrid generator
inline void SetHybridSeed(uint64_t seed) {
  GetHybridRng()->SetSeed(seed);
}

/// Clean up thread-local generator
inline void CleanupHybridRng() {
  if (hybrid_random_generator) {
    delete hybrid_random_generator;
    hybrid_random_generator = nullptr;
  }
}

} // namespace bdm

// Convenience macros that automatically use the best implementation
#define BDM_HYBRID_UNIFORM() bdm::GetHybridRng()->Uniform()
#define BDM_HYBRID_UNIFORM_RANGE(min, max) bdm::GetHybridRng()->Uniform(min, max)
#define BDM_HYBRID_GAUSSIAN(mean, sigma) bdm::GetHybridRng()->Gaus(mean, sigma)
#define BDM_HYBRID_POISSON(mean) bdm::GetHybridRng()->Poisson(mean)
#define BDM_HYBRID_BINOMIAL(n, p) bdm::GetHybridRng()->Binomial(n, p)

#endif // CORE_UTIL_RANDOM_HYBRID_H_
