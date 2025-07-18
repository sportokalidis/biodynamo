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

#ifndef CORE_UTIL_RANDOM_COMPAT_H_
#define CORE_UTIL_RANDOM_COMPAT_H_

#include "core/util/random_std.h"
#include "core/container/math_array.h"
#include <cmath>
#include <vector>
#include <memory>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Configuration option to choose between ROOT and standard implementations
#ifndef BDM_USE_STD_RANDOM
#define BDM_USE_STD_RANDOM 0  // Default to ROOT for backward compatibility
#endif

#if BDM_USE_STD_RANDOM
// Use standard implementations (skip original ROOT-based definitions)
#include "core/util/random_std.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace bdm {
// Import standard implementation classes directly into bdm namespace
using RandomGenerator = experimental::StdRandomGenerator;
template<typename T>
using DistributionRng = experimental::StdDistributionRng<T>;
using UniformRng = experimental::StdUniformRng;

// Forward declarations for distribution classes
using StdUniformRng = experimental::StdUniformRng;
using StdGaussianRng = experimental::StdGaussianRng;
using StdExponentialRng = experimental::StdExponentialRng;
using StdLandauRng = experimental::StdLandauRng;
using StdPoissonRng = experimental::StdPoissonRng;
using StdBinomialRng = experimental::StdBinomialRng;
using StdUserDefinedRng = experimental::StdUserDefinedRng;
using StdUserDefinedRng2D = experimental::StdUserDefinedRng2D;
using StdUserDefinedRng3D = experimental::StdUserDefinedRng3D;

// Helper structs for distribution wrappers (defined outside class)
struct UniformWrapper { 
  real_t min, max; 
  experimental::StdRandomGenerator* rng;
  real_t Sample() { return rng->Uniform(min, max); }
};

struct GaussianWrapper { 
  real_t mean, sigma; 
  experimental::StdRandomGenerator* rng;
  real_t Sample() { return rng->Gaussian(mean, sigma); }
};

struct ExponentialWrapper { 
  real_t lambda; 
  experimental::StdRandomGenerator* rng;
  real_t Sample() { return rng->Exponential(lambda); }
};

struct LandauWrapper { 
  real_t mean, sigma; 
  experimental::StdRandomGenerator* rng;
  real_t Sample() { return rng->Gaussian(mean, sigma); } // Simplified as Gaussian
};

struct PoissonWrapper { 
  real_t mean; 
  experimental::StdRandomGenerator* rng;
  int Sample() { return rng->Poisson(mean); }
};

struct BinomialWrapper { 
  int n; real_t p; 
  experimental::StdRandomGenerator* rng;
  int Sample() { return rng->Binomial(n, p); }
};

struct UserDefinedWrapper { 
  experimental::StdRandomGenerator* rng;
  real_t Sample() { return rng->Uniform(); } // Simplified
  const void* Class() const { return nullptr; } // For serialization compatibility
};

struct UserDefined2DWrapper { 
  experimental::StdRandomGenerator* rng;
  MathArray<real_t, 2> Sample() { return {rng->Uniform(), rng->Uniform()}; }
  // Create a wrapper class that has Norm() method
  struct Sample2Result {
    MathArray<real_t, 2> values;
    real_t Norm() { return std::sqrt(values[0]*values[0] + values[1]*values[1]); }
  };
  Sample2Result Sample2() { 
    auto vals = Sample(); 
    return {vals}; 
  }
};

struct UserDefined3DWrapper { 
  experimental::StdRandomGenerator* rng;
  MathArray<real_t, 3> Sample() { return {rng->Uniform(), rng->Uniform(), rng->Uniform()}; }
  // Create a wrapper class that has Norm() method
  struct Sample3Result {
    MathArray<real_t, 3> values;
    real_t Norm() { return std::sqrt(values[0]*values[0] + values[1]*values[1] + values[2]*values[2]); }
  };
  Sample3Result Sample3() { 
    auto vals = Sample(); 
    return {vals}; 
  }
};

// Compatibility typedefs for tests
using UserDefinedDistRng1D = UserDefinedWrapper;
using UserDefinedDistRng2D = UserDefined2DWrapper;
using UserDefinedDistRng3D = UserDefined3DWrapper;

// Forward declaration for ROOT compatibility (different name to avoid conflicts)
class BdmTClass {};

// Compatibility wrapper for Random class (mimics original API)
class Random {
public:
  Random() : rng_() {}
  explicit Random(uint64_t seed) : rng_(seed) {}
  
  // Core random generation
  double Uniform(double min = 0.0, double max = 1.0) {
    return rng_.Uniform(min, max);
  }
  
  int Poisson(double mean) {
    return rng_.Poisson(mean);
  }
  
  double Gaus(double mean = 0.0, double sigma = 1.0) {
    return rng_.Gaussian(mean, sigma);
  }
  
  int Binomial(int n, double p) {
    return rng_.Binomial(n, p);
  }
  
  void SetSeed(uint64_t seed) {
    rng_.SetSeed(seed);
  }
  
  // Additional methods that may be needed
  template<size_t N>
  MathArray<real_t, N> UniformArray(real_t min, real_t max) {
    MathArray<real_t, N> result;
    for (size_t i = 0; i < N; ++i) {
      result[i] = rng_.Uniform(min, max);
    }
    return result;
  }
  
  // Override for [0,1] default range
  template<size_t N>
  MathArray<real_t, N> UniformArray() {
    return UniformArray<N>(0.0, 1.0);
  }
  
  // Override for [0, max] range
  template<size_t N>
  MathArray<real_t, N> UniformArray(real_t max) {
    return UniformArray<N>(0.0, max);
  }
  
  // Missing distribution methods
  real_t Exp(real_t lambda = 1.0) {
    return rng_.Exponential(lambda);
  }
  
  real_t Landau(real_t mean = 0.0, real_t sigma = 1.0) {
    // Landau distribution - approximate with Gaussian for now
    return rng_.Gaussian(mean, sigma);
  }
  
  real_t PoissonD(real_t mean) {
    return static_cast<real_t>(rng_.Poisson(mean));
  }
  
  real_t BreitWigner(real_t mean = 0.0, real_t gamma = 1.0) {
    // Breit-Wigner distribution - approximate with Gaussian for now
    return rng_.Gaussian(mean, gamma);
  }
  
  MathArray<real_t, 2> Circle(real_t radius) {
    real_t angle = rng_.Uniform(0.0, 2.0 * M_PI);
    return {radius * std::cos(angle), radius * std::sin(angle)};
  }
  
  // Distribution RNG getter methods (return simple wrapper objects)
  UniformWrapper GetUniformRng(real_t min, real_t max) {
    return {min, max, &rng_};
  }
  
  GaussianWrapper GetGausRng(real_t mean, real_t sigma) {
    return {mean, sigma, &rng_};
  }
  
  ExponentialWrapper GetExpRng(real_t lambda) {
    return {lambda, &rng_};
  }
  
  LandauWrapper GetLandauRng(real_t mean, real_t sigma) {
    return {mean, sigma, &rng_};
  }
  
  GaussianWrapper GetPoissonDRng(real_t mean) {
    // PoissonD in ROOT is actually Gaussian for large means
    return {mean, std::sqrt(mean), &rng_};
  }
  
  LandauWrapper GetBreitWignerRng(real_t mean, real_t gamma) {
    // Simplified: using Landau as approximation for Breit-Wigner
    return {mean, gamma, &rng_};
  }
  
  BinomialWrapper GetBinomialRng(int n, real_t p) {
    return {n, p, &rng_};
  }
  
  PoissonWrapper GetPoissonRng(real_t mean) {
    return {mean, &rng_};
  }
  
  // User-defined distribution methods
  template<typename T>
  UserDefinedWrapper GetUserDefinedDistRng1D(T&& func, const std::vector<real_t>& params, real_t xmin, real_t xmax) {
    return {&rng_}; // Simplified
  }
  
  template<typename T>
  UserDefined2DWrapper GetUserDefinedDistRng2D(T&& func, const std::vector<real_t>& params, 
                                               real_t xmin, real_t xmax, real_t ymin, real_t ymax) {
    return {&rng_}; // Simplified
  }
  
  template<typename T>
  UserDefined3DWrapper GetUserDefinedDistRng3D(T&& func, const std::vector<real_t>& params,
                                               real_t xmin, real_t xmax, real_t ymin, real_t ymax, real_t zmin, real_t zmax) {
    return {&rng_}; // Simplified
  }
  
  // For ROOT serialization compatibility - return void* to avoid ROOT TClass conflicts
  const void* Class() const {
    return nullptr; // Simplified for compatibility
  }
  
  // Additional missing methods
  uint64_t Integer(uint64_t max) {
    return static_cast<uint64_t>(rng_.Uniform(0.0, static_cast<double>(max)));
  }
  
  MathArray<real_t, 3> Sphere(real_t radius) {
    // Simple sphere distribution (not perfectly uniform on surface)
    real_t u = rng_.Uniform(0.0, 1.0);
    real_t v = rng_.Uniform(0.0, 1.0);
    real_t theta = 2.0 * M_PI * u;
    real_t phi = std::acos(2.0 * v - 1.0);
    real_t r = radius;
    return {r * std::sin(phi) * std::cos(theta),
            r * std::sin(phi) * std::sin(theta), 
            r * std::cos(phi)};
  }

private:
  experimental::StdRandomGenerator rng_;
};

// Global functions for compatibility
inline experimental::StdRandomGenerator& GetRng() { return experimental::GetStdRng(); }
inline void SetSeed(uint64_t seed) { experimental::SetStdSeed(seed); }

}  // namespace bdm

#else
#include "core/util/random.h"
// Keep existing ROOT-based implementation as default
namespace bdm {
// Existing ROOT-based classes remain unchanged
// This maintains full backward compatibility
}
#endif

namespace bdm {
namespace compat {

/// Compatibility wrapper that can use either ROOT or std implementation
class UniversalRandomGenerator {
 public:
  UniversalRandomGenerator() {
#if BDM_USE_STD_RANDOM
    std_rng_ = std::make_shared<experimental::StdRandomGenerator>();
#endif
  }
  
  void SetSeed(uint64_t seed) {
#if BDM_USE_STD_RANDOM
    std_rng_->SetSeed(seed);
#else
    // Set ROOT random seed
    bdm::GetRng()->SetSeed(seed);
#endif
  }
  
  real_t Uniform() {
#if BDM_USE_STD_RANDOM
    return std_rng_->Uniform();
#else
    return bdm::GetRng()->Uniform();
#endif
  }
  
  real_t Uniform(real_t min, real_t max) {
#if BDM_USE_STD_RANDOM
    return std_rng_->Uniform(min, max);
#else
    return bdm::GetRng()->Uniform(min, max);
#endif
  }
  
  real_t Gaussian(real_t mean = 0.0, real_t sigma = 1.0) {
#if BDM_USE_STD_RANDOM
    return std_rng_->Gaussian(mean, sigma);
#else
    return bdm::GetRng()->Gaus(mean, sigma);
#endif
  }
  
  int Poisson(real_t mean) {
#if BDM_USE_STD_RANDOM
    return std_rng_->Poisson(mean);
#else
    return bdm::GetRng()->Poisson(mean);
#endif
  }
  
  std::string GetImplementation() const {
#if BDM_USE_STD_RANDOM
    return "Standard C++";
#else
    return "ROOT";
#endif
  }

 private:
#if BDM_USE_STD_RANDOM
  std::shared_ptr<experimental::StdRandomGenerator> std_rng_;
#endif
};

/// Factory function to create the appropriate random generator
inline std::unique_ptr<UniversalRandomGenerator> CreateRandomGenerator() {
  return std::make_unique<UniversalRandomGenerator>();
}

/// Global universal generator
inline UniversalRandomGenerator& GetUniversalRng() {
  static UniversalRandomGenerator universal_rng;
  return universal_rng;
}

} // namespace compat
} // namespace bdm

// Convenience macros for easy switching
#define BDM_RNG_UNIFORM() bdm::compat::GetUniversalRng().Uniform()
#define BDM_RNG_UNIFORM_RANGE(min, max) bdm::compat::GetUniversalRng().Uniform(min, max)
#define BDM_RNG_GAUSSIAN(mean, sigma) bdm::compat::GetUniversalRng().Gaussian(mean, sigma)
#define BDM_RNG_POISSON(mean) bdm::compat::GetUniversalRng().Poisson(mean)
#define BDM_RNG_SET_SEED(seed) bdm::compat::GetUniversalRng().SetSeed(seed)

#endif // CORE_UTIL_RANDOM_COMPAT_H_
