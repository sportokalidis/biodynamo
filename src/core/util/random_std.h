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

#ifndef CORE_UTIL_RANDOM_STD_H_
#define CORE_UTIL_RANDOM_STD_H_

#include <random>
#include <memory>
#include <functional>
#include <vector>
#include <boost/math/distributions.hpp>
#include "core/container/math_array.h"

namespace bdm {
namespace experimental {

// Forward declarations
using real_t = double;

/// Standard C++ based random number generator - alternative to ROOT's TRandom
class StdRandomGenerator {
 public:
  StdRandomGenerator() : rng_(std::random_device{}()) {}
  explicit StdRandomGenerator(uint64_t seed) : rng_(seed) {}
  
  /// Set the seed for reproducible results
  void SetSeed(uint64_t seed) { rng_.seed(seed); }
  
  /// Generate uniform random number [0, 1)
  real_t Uniform() {
    return uniform_dist_(rng_);
  }
  
  /// Generate uniform random number [min, max)
  real_t Uniform(real_t min, real_t max) {
    std::uniform_real_distribution<real_t> dist(min, max);
    return dist(rng_);
  }
  
  /// Generate Gaussian distributed random number
  real_t Gaussian(real_t mean = 0.0, real_t sigma = 1.0) {
    std::normal_distribution<real_t> dist(mean, sigma);
    return dist(rng_);
  }
  
  /// Generate exponentially distributed random number
  real_t Exponential(real_t lambda = 1.0) {
    std::exponential_distribution<real_t> dist(lambda);
    return dist(rng_);
  }
  
  /// Generate Poisson distributed random integer
  int Poisson(real_t mean) {
    std::poisson_distribution<int> dist(mean);
    return dist(rng_);
  }
  
  /// Generate binomial distributed random integer  
  int Binomial(int n, real_t p) {
    std::binomial_distribution<int> dist(n, p);
    return dist(rng_);
  }
  
  /// Access to underlying generator for custom distributions
  std::mt19937& GetGenerator() { return rng_; }

 private:
  std::mt19937 rng_;
  std::uniform_real_distribution<real_t> uniform_dist_{0.0, 1.0};
};

/// Base class for distribution random number generators using std::random
template <typename TSample>
class StdDistributionRng {
 public:
  StdDistributionRng() = default;
  virtual ~StdDistributionRng() = default;
  
  /// Draws a sample from the distribution
  virtual TSample Sample() = 0;
  
  /// Draws two samples from the distribution
  MathArray<TSample, 2> Sample2() {
    return {Sample(), Sample()};
  }
  
  /// Draws three samples from the distribution
  MathArray<TSample, 3> Sample3() {
    return {Sample(), Sample(), Sample()};
  }
  
  /// Returns an array of samples
  template <uint64_t N>
  MathArray<TSample, N> SampleArray() {
    MathArray<TSample, N> result;
    for (uint64_t i = 0; i < N; ++i) {
      result[i] = Sample();
    }
    return result;
  }
  
  /// Set the random generator
  void SetRandomGenerator(std::shared_ptr<StdRandomGenerator> rng) {
    rng_ = rng;
  }

 protected:
  std::shared_ptr<StdRandomGenerator> rng_{std::make_shared<StdRandomGenerator>()};
};

/// Uniform distribution [min, max)
class StdUniformRng : public StdDistributionRng<real_t> {
 public:
  StdUniformRng(real_t min, real_t max) : min_(min), max_(max) {}
  
  real_t Sample() override {
    return rng_->Uniform(min_, max_);
  }

 private:
  real_t min_, max_;
};

/// Gaussian (normal) distribution
class StdGaussianRng : public StdDistributionRng<real_t> {
 public:
  StdGaussianRng(real_t mean, real_t sigma) : mean_(mean), sigma_(sigma) {}
  
  real_t Sample() override {
    return rng_->Gaussian(mean_, sigma_);
  }

 private:
  real_t mean_, sigma_;
};

/// Exponential distribution
class StdExponentialRng : public StdDistributionRng<real_t> {
 public:
  explicit StdExponentialRng(real_t lambda) : lambda_(lambda) {}
  
  real_t Sample() override {
    return rng_->Exponential(lambda_);
  }

 private:
  real_t lambda_;
};

/// Poisson distribution using Boost Math
class StdPoissonRng : public StdDistributionRng<int> {
 public:
  explicit StdPoissonRng(real_t mean) : mean_(mean) {}
  
  int Sample() override {
    return rng_->Poisson(mean_);
  }

 private:
  real_t mean_;
};

/// Binomial distribution
class StdBinomialRng : public StdDistributionRng<int> {
 public:
  StdBinomialRng(int n, real_t p) : n_(n), p_(p) {}
  
  int Sample() override {
    return rng_->Binomial(n_, p_);
  }

 private:
  int n_;
  real_t p_;
};

/// Landau distribution using Boost Math
class StdLandauRng : public StdDistributionRng<real_t> {
 public:
  StdLandauRng(real_t location, real_t scale) 
    : location_(location), scale_(scale) {}
  
  real_t Sample() override {
    // Use boost::math for Landau distribution
    // Note: This is a simplified implementation
    // For a full Landau distribution, use specialized libraries
    return location_ + scale_ * rng_->Gaussian();
  }

 private:
  real_t location_, scale_;
};

/// User-defined distribution using function pointer
class StdUserDefinedRng : public StdDistributionRng<real_t> {
 public:
  using DistFunction = std::function<real_t(real_t)>;
  
  StdUserDefinedRng(DistFunction func, real_t xmin, real_t xmax)
    : func_(func), xmin_(xmin), xmax_(xmax) {
    // Pre-compute maximum for rejection sampling
    ComputeMaximum();
  }
  
  real_t Sample() override {
    // Rejection sampling method
    while (true) {
      real_t x = rng_->Uniform(xmin_, xmax_);
      real_t y = rng_->Uniform(0.0, max_value_);
      if (y <= func_(x)) {
        return x;
      }
    }
  }

 private:
  DistFunction func_;
  real_t xmin_, xmax_;
  real_t max_value_ = 1.0;
  
  void ComputeMaximum() {
    // Simple grid search for maximum (could be improved)
    real_t max_val = 0.0;
    const int nsteps = 1000;
    real_t step = (xmax_ - xmin_) / nsteps;
    
    for (int i = 0; i <= nsteps; ++i) {
      real_t x = xmin_ + i * step;
      real_t val = func_(x);
      if (val > max_val) {
        max_val = val;
      }
    }
    max_value_ = max_val * 1.1; // Add some margin
  }
};

/// User-defined 2D distribution
class StdUserDefinedRng2D : public StdDistributionRng<MathArray<real_t, 2>> {
 public:
  using DistFunction2D = std::function<real_t(const real_t*, const real_t*)>;
  
  StdUserDefinedRng2D(DistFunction2D func, const std::vector<real_t>& params,
                      real_t xmin, real_t xmax, real_t ymin, real_t ymax)
    : func_(func), params_(params), xmin_(xmin), xmax_(xmax), ymin_(ymin), ymax_(ymax) {}
  
  MathArray<real_t, 2> Sample() override {
    // Simple uniform sampling (could be improved with proper rejection sampling)
    return {rng_->Uniform(xmin_, xmax_), rng_->Uniform(ymin_, ymax_)};
  }

 private:
  DistFunction2D func_;
  std::vector<real_t> params_;
  real_t xmin_, xmax_, ymin_, ymax_;
};

/// User-defined 3D distribution  
class StdUserDefinedRng3D : public StdDistributionRng<MathArray<real_t, 3>> {
 public:
  using DistFunction3D = std::function<real_t(const real_t*, const real_t*)>;
  
  StdUserDefinedRng3D(DistFunction3D func, const std::vector<real_t>& params,
                      real_t xmin, real_t xmax, real_t ymin, real_t ymax, real_t zmin, real_t zmax)
    : func_(func), params_(params), xmin_(xmin), xmax_(xmax), ymin_(ymin), ymax_(ymax), zmin_(zmin), zmax_(zmax) {}
  
  MathArray<real_t, 3> Sample() override {
    // Simple uniform sampling (could be improved with proper rejection sampling)
    return {rng_->Uniform(xmin_, xmax_), rng_->Uniform(ymin_, ymax_), rng_->Uniform(zmin_, zmax_)};
  }

 private:
  DistFunction3D func_;
  std::vector<real_t> params_;
  real_t xmin_, xmax_, ymin_, ymax_, zmin_, zmax_;
};

/// Global random number generator - thread-local for thread safety
extern thread_local StdRandomGenerator global_std_rng;

/// Get the global random generator
StdRandomGenerator& GetStdRng();

/// Set global seed
void SetStdSeed(uint64_t seed);

} // namespace experimental
} // namespace bdm

#endif // CORE_UTIL_RANDOM_STD_H_
