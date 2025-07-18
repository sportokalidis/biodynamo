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

#include "core/util/random.h"
#include <Math/DistFunc.h>
#include <TF1.h>
#include <TRandom3.h>
#include <gtest/gtest.h>
#include <limits>
#include "unit/test_util/io_test.h"
#include "unit/test_util/test_util.h"

namespace bdm {

#ifdef BDM_USE_STD_RANDOM
// Helper function to check statistical properties for std random implementation
template<typename T>
void CheckDistributionProperties(const std::vector<T>& samples, T expected_min, T expected_max, 
                               T tolerance = static_cast<T>(0.1)) {
  // Check that all samples are within expected range
  for (const auto& sample : samples) {
    EXPECT_GE(sample, expected_min);
    EXPECT_LE(sample, expected_max);
  }
  
  // Check that we have good distribution (not all the same value)
  if (samples.size() > 1) {
    T min_val = *std::min_element(samples.begin(), samples.end());
    T max_val = *std::max_element(samples.begin(), samples.end());
    EXPECT_GT(max_val - min_val, tolerance * (expected_max - expected_min));
  }
}
#endif

TEST(RandomTest, Uniform) {
  Simulation simulation(TEST_NAME);
  auto* random = simulation.GetRandom();
  
#ifdef BDM_USE_STD_RANDOM
  // For std random, check statistical properties
  random->SetSeed(42);
  
  std::vector<real_t> samples;
  for (uint64_t i = 0; i < 100; i++) {
    samples.push_back(random->Uniform());
  }
  CheckDistributionProperties(samples, static_cast<real_t>(0.0), static_cast<real_t>(1.0));

  for (uint64_t i = 1; i < 10; i++) {
    real_t val = random->Uniform(i);
    EXPECT_GE(val, 0.0);
    EXPECT_LE(val, static_cast<real_t>(i));
  }

  for (uint64_t i = 0; i < 10; i++) {
    real_t val = random->Uniform(i, i + 2);
    EXPECT_GE(val, static_cast<real_t>(i));
    EXPECT_LE(val, static_cast<real_t>(i + 2));
  }

  auto distrng = random->GetUniformRng(3, 4);
  for (uint64_t i = 0; i < 10; i++) {
    real_t val = distrng.Sample();
    EXPECT_GE(val, 3.0);
    EXPECT_LE(val, 4.0);
  }
#else
  // For ROOT random, check exact sequence equality
  TRandom3 reference;
  random->SetSeed(42);
  reference.SetSeed(42);

  for (uint64_t i = 0; i < 10; i++) {
    EXPECT_REAL_EQ(static_cast<real_t>(reference.Uniform()), random->Uniform());
  }

  for (uint64_t i = 0; i < 10; i++) {
    EXPECT_REAL_EQ(static_cast<real_t>(reference.Uniform(i)),
                   random->Uniform(i));
  }

  for (uint64_t i = 0; i < 10; i++) {
    EXPECT_REAL_EQ(static_cast<real_t>(reference.Uniform(i, i + 2)),
                   random->Uniform(i, i + 2));
  }

  auto distrng = random->GetUniformRng(3, 4);
  for (uint64_t i = 0; i < 10; i++) {
    EXPECT_REAL_EQ(static_cast<real_t>(reference.Uniform(3, 4)),
                   distrng.Sample());
  }
#endif
}

TEST(RandomTest, UniformArray) {
  Simulation simulation(TEST_NAME);
  auto* random = simulation.GetRandom();
  
#ifdef BDM_USE_STD_RANDOM
  // For std random, check statistical properties
  random->SetSeed(42);

  auto result = random->UniformArray<5>();
  for (uint64_t i = 0; i < 5; i++) {
    EXPECT_GE(result[i], 0.0);
    EXPECT_LE(result[i], 1.0);
  }

  auto result1 = random->UniformArray<2>(8.3);
  for (uint64_t i = 0; i < 2; i++) {
    EXPECT_GE(result1[i], 0.0);
    EXPECT_LE(result1[i], 8.3);
  }

  auto result2 = random->UniformArray<12>(5.1, 9.87);
  for (uint64_t i = 0; i < 12; i++) {
    EXPECT_GE(result2[i], 5.1);
    EXPECT_LE(result2[i], 9.87);
  }
#else
  // For ROOT random, check exact sequence equality
  TRandom3 reference;
  random->SetSeed(42);
  reference.SetSeed(42);

  auto result = random->UniformArray<5>();
  for (uint64_t i = 0; i < 5; i++) {
    EXPECT_REAL_EQ(static_cast<real_t>(reference.Uniform()), result[i]);
  }

  auto result1 = random->UniformArray<2>(8.3);
  for (uint64_t i = 0; i < 2; i++) {
    EXPECT_REAL_EQ(static_cast<real_t>(reference.Uniform(8.3)), result1[i]);
  }

  auto result2 = random->UniformArray<12>(5.1, 9.87);
  for (uint64_t i = 0; i < 12; i++) {
    EXPECT_REAL_EQ(static_cast<real_t>(reference.Uniform(5.1, 9.87)),
                   result2[i]);
  }
#endif
}

TEST(RandomTest, Gaus) {
  Simulation simulation(TEST_NAME);
  auto* random = simulation.GetRandom();
  
#ifdef BDM_USE_STD_RANDOM
  // For std random, check statistical properties
  random->SetSeed(42);

  // Test default Gaussian (mean=0, sigma=1)
  std::vector<real_t> samples;
  for (uint64_t i = 0; i < 1000; i++) {
    samples.push_back(random->Gaus());
  }
  real_t mean = std::accumulate(samples.begin(), samples.end(), 0.0) / samples.size();
  EXPECT_NEAR(mean, 0.0, 0.2);  // Allow tolerance for statistical variation

  // Test Gaussian with specific mean
  for (uint64_t i = 1; i < 10; i++) {
    real_t val = random->Gaus(i);
    // Just check it returns a reasonable value (no specific bounds for Gaussian)
    EXPECT_GT(val, i - 5 * 1.0);  // Within 5 sigma of mean
    EXPECT_LT(val, i + 5 * 1.0);
  }

  // Test Gaussian with specific mean and sigma
  for (uint64_t i = 1; i < 10; i++) {
    real_t val = random->Gaus(i, i + 2);
    real_t sigma = static_cast<real_t>(i + 2);
    real_t mean = static_cast<real_t>(i);
    EXPECT_GT(val, mean - 5 * sigma);  // Within 5 sigma
    EXPECT_LT(val, mean + 5 * sigma);
  }

  auto distrng = random->GetGausRng(3, 4);
  for (uint64_t i = 0; i < 10; i++) {
    real_t val = distrng.Sample();
    EXPECT_GT(val, 3 - 5 * 4);  // Within 5 sigma
    EXPECT_LT(val, 3 + 5 * 4);
  }
#else
  // For ROOT random, check exact sequence equality
  TRandom3 reference;
  random->SetSeed(42);
  reference.SetSeed(42);

  for (uint64_t i = 0; i < 10; i++) {
    EXPECT_REAL_EQ(static_cast<real_t>(reference.Gaus()), random->Gaus());
  }

  for (uint64_t i = 0; i < 10; i++) {
    EXPECT_REAL_EQ(static_cast<real_t>(reference.Gaus(i)), random->Gaus(i));
  }

  for (uint64_t i = 0; i < 10; i++) {
    EXPECT_REAL_EQ(static_cast<real_t>(reference.Gaus(i, i + 2)),
                   random->Gaus(i, i + 2));
  }

  auto distrng = random->GetGausRng(3, 4);
  for (uint64_t i = 0; i < 10; i++) {
    EXPECT_REAL_EQ(static_cast<real_t>(reference.Gaus(3, 4)), distrng.Sample());
  }
#endif
}

TEST(RandomTest, Exp) {
  Simulation simulation(TEST_NAME);
  auto* random = simulation.GetRandom();
  
#ifdef BDM_USE_STD_RANDOM
  // For std random, check statistical properties
  random->SetSeed(42);

  for (uint64_t i = 1; i < 10; i++) {
    real_t val = random->Exp(i);
    EXPECT_GE(val, 0.0);  // Exponential distribution is always non-negative
    // Exponential has no upper bound, but very large values are rare
  }

  for (uint64_t i = 1; i < 10; i++) {
    real_t val = random->Exp(i + 2);
    EXPECT_GE(val, 0.0);
  }

  auto distrng = random->GetExpRng(123);
  for (uint64_t i = 0; i < 10; i++) {
    real_t val = distrng.Sample();
    EXPECT_GE(val, 0.0);
  }
#else
  // For ROOT random, check exact sequence equality
  TRandom3 reference;
  random->SetSeed(42);
  reference.SetSeed(42);

  for (uint64_t i = 0; i < 10; i++) {
    EXPECT_REAL_EQ(static_cast<real_t>(reference.Exp(i)), random->Exp(i));
  }

  for (uint64_t i = 0; i < 10; i++) {
    EXPECT_REAL_EQ(static_cast<real_t>(reference.Exp(i + 2)),
                   random->Exp(i + 2));
  }

  auto distrng = random->GetExpRng(123);
  for (uint64_t i = 0; i < 10; i++) {
    EXPECT_REAL_EQ(static_cast<real_t>(reference.Exp(123)), distrng.Sample());
  }
#endif
}

TEST(RandomTest, Landau) {
  Simulation simulation(TEST_NAME);
  auto* random = simulation.GetRandom();
  
#ifdef BDM_USE_STD_RANDOM
  // For std random, check statistical properties
  random->SetSeed(42);

  for (uint64_t i = 0; i < 10; i++) {
    real_t val = random->Landau();
    // Landau distribution can have any real value, just check it's finite
    EXPECT_TRUE(std::isfinite(val));
  }

  for (uint64_t i = 0; i < 10; i++) {
    real_t val = random->Landau(i);
    EXPECT_TRUE(std::isfinite(val));
  }

  for (uint64_t i = 0; i < 10; i++) {
    real_t val = random->Landau(i, i + 2);
    EXPECT_TRUE(std::isfinite(val));
  }

  auto distrng = random->GetLandauRng(3, 4);
  for (uint64_t i = 0; i < 10; i++) {
    real_t val = distrng.Sample();
    EXPECT_TRUE(std::isfinite(val));
  }
#else
  // For ROOT random, check exact sequence equality
  TRandom3 reference;
  random->SetSeed(42);
  reference.SetSeed(42);

  for (uint64_t i = 0; i < 10; i++) {
    EXPECT_REAL_EQ(static_cast<real_t>(reference.Landau()), random->Landau());
  }

  for (uint64_t i = 0; i < 10; i++) {
    EXPECT_REAL_EQ(static_cast<real_t>(reference.Landau(i)), random->Landau(i));
  }

  for (uint64_t i = 0; i < 10; i++) {
    EXPECT_REAL_EQ(static_cast<real_t>(reference.Landau(i, i + 2)),
                   random->Landau(i, i + 2));
  }

  auto distrng = random->GetLandauRng(3, 4);
  for (uint64_t i = 0; i < 10; i++) {
    EXPECT_REAL_EQ(static_cast<real_t>(reference.Landau(3, 4)),
                   distrng.Sample());
  }
#endif
}

TEST(RandomTest, PoissonD) {
  Simulation simulation(TEST_NAME);
  auto* random = simulation.GetRandom();
  
#ifdef BDM_USE_STD_RANDOM
  // For std random, check statistical properties
  random->SetSeed(42);

  for (uint64_t i = 1; i < 10; i++) {
    real_t val = random->PoissonD(i);
    EXPECT_GE(val, 0.0);  // Poisson distribution is always non-negative
    EXPECT_TRUE(std::isfinite(val));
  }

  for (uint64_t i = 1; i < 10; i++) {
    real_t val = random->PoissonD(i + 2);
    EXPECT_GE(val, 0.0);
    EXPECT_TRUE(std::isfinite(val));
  }

  auto distrng = random->GetPoissonDRng(123);
  for (uint64_t i = 0; i < 10; i++) {
    real_t val = distrng.Sample();
    EXPECT_GE(val, 0.0);
    EXPECT_TRUE(std::isfinite(val));
  }
#else
  // For ROOT random, check exact sequence equality
  TRandom3 reference;
  random->SetSeed(42);
  reference.SetSeed(42);

  for (uint64_t i = 0; i < 10; i++) {
    EXPECT_REAL_EQ(static_cast<real_t>(reference.PoissonD(i)),
                   random->PoissonD(i));
  }

  for (uint64_t i = 0; i < 10; i++) {
    EXPECT_REAL_EQ(static_cast<real_t>(reference.PoissonD(i + 2)),
                   random->PoissonD(i + 2));
  }

  auto distrng = random->GetPoissonDRng(123);
  for (uint64_t i = 0; i < 10; i++) {
    EXPECT_REAL_EQ(static_cast<real_t>(reference.PoissonD(123)),
                   distrng.Sample());
  }
#endif
}

TEST(RandomTest, BreitWigner) {
  Simulation simulation(TEST_NAME);
  auto* random = simulation.GetRandom();
  
#ifdef BDM_USE_STD_RANDOM
  // For std random, check statistical properties
  random->SetSeed(42);

  for (uint64_t i = 0; i < 10; i++) {
    real_t val = random->BreitWigner();
    // Breit-Wigner distribution can have any real value, just check it's finite
    EXPECT_TRUE(std::isfinite(val));
  }

  for (uint64_t i = 1; i < 10; i++) {
    real_t val = random->BreitWigner(i);
    EXPECT_TRUE(std::isfinite(val));
  }

  for (uint64_t i = 1; i < 10; i++) {
    real_t val = random->BreitWigner(i, i + 2);
    EXPECT_TRUE(std::isfinite(val));
  }

  auto distrng = random->GetBreitWignerRng(3, 4);
  for (uint64_t i = 0; i < 10; i++) {
    real_t val = distrng.Sample();
    EXPECT_TRUE(std::isfinite(val));
  }
#else
  // For ROOT random, check exact sequence equality
  TRandom3 reference;
  random->SetSeed(42);
  reference.SetSeed(42);

  for (uint64_t i = 0; i < 10; i++) {
    EXPECT_REAL_EQ(static_cast<real_t>(reference.BreitWigner()),
                   random->BreitWigner());
  }

  for (uint64_t i = 0; i < 10; i++) {
    EXPECT_REAL_EQ(static_cast<real_t>(reference.BreitWigner(i)),
                   random->BreitWigner(i));
  }

  for (uint64_t i = 0; i < 10; i++) {
    EXPECT_REAL_EQ(static_cast<real_t>(reference.BreitWigner(i, i + 2)),
                   random->BreitWigner(i, i + 2));
  }

  auto distrng = random->GetBreitWignerRng(3, 4);
  for (uint64_t i = 0; i < 10; i++) {
    EXPECT_REAL_EQ(static_cast<real_t>(reference.BreitWigner(3, 4)),
                   distrng.Sample());
  }
#endif
}

TEST(RandomTest, UserDefinedDistRng1D) {
#ifdef BDM_USE_STD_RANDOM
  // User-defined distributions are not supported with standard random implementation
  GTEST_SKIP() << "User-defined distributions not supported when using BDM_USE_STD_RANDOM=1";
#else
  Simulation simulation(TEST_NAME);
  auto* random = simulation.GetRandom();
  auto function = [](const double* x, const double* params) {
    return ROOT::Math::lognormal_pdf(*x, params[0], params[1]);
  };
  real_t min = 1;
  real_t max = 4;
  TF1 reference("", function, min, max, 2);
  reference.SetParameters(1.1, 1.2);

  // workaround until update to 6.24
  gRandom->SetSeed(42);
  std::vector<real_t> expected;
  for (uint64_t i = 0; i < 10; i++) {
    expected.push_back(reference.GetRandom(min, max));
  }

  gRandom->SetSeed(42);
  auto distrng =
      random->GetUserDefinedDistRng1D(function, {1.1, 1.2}, min, max);
  std::vector<real_t> actual;
  for (uint64_t i = 0; i < 10; i++) {
    actual.push_back(distrng.Sample());
  }
  for (size_t i = 0; i < actual.size(); i++) {
    EXPECT_NEAR(expected[i], actual[i], abs_error<real_t>::value);
  }
#endif
}

// This test is neither a proper death test nor a proper function test. We run
// this test, to see if it is possible to initialize the UserDefinedDistRng1D
// in a parallel region because the generator is based on ROOT's TF1 class which
// seems to have trouble when the constructor is called in parallel. Before the
// fix provided in PR #208, this test would fail roughly 2/3 times.
TEST(RandomTest, UserDefinedDistRng1DParallel) {
#ifdef BDM_USE_STD_RANDOM
  // User-defined distributions are not supported with standard random implementation
  GTEST_SKIP() << "User-defined distributions not supported when using BDM_USE_STD_RANDOM=1";
#else
  Simulation simulation(TEST_NAME);
  std::vector<real_t> results;
#pragma omp parallel shared(simulation, results)
  {
    auto* random = simulation.GetRandom();
    auto function = [](const double* x, const double* params) {
      return ROOT::Math::lognormal_pdf(*x, params[0], params[1]);
    };
    real_t min = 1;
    real_t max = 4;
    size_t n_samples = 10000;
    real_t result = 0.0;
    auto distrng =
        random->GetUserDefinedDistRng1D(function, {1.1, 1.2}, min, max);
    for (size_t i = 0; i < n_samples; i++) {
      result += distrng.Sample();
    }
#pragma omp critical
    { results.push_back(result / n_samples); }
  }
  real_t sum = std::accumulate(results.begin(), results.end(), 0.0);
  EXPECT_LT(sum, std::numeric_limits<real_t>::max());
#endif
}

// This test is neither a proper death test nor a proper function test. We run
// this test, to see if it is possible to initialize the UserDefinedDistRng2D
// in a parallel region because the generator is based on ROOT's TF2 class which
// seems to have trouble when the constructor is called in parallel. Before the
// fix provided in PR #208, this test would fail roughly 2/3 times.
TEST(RandomTest, UserDefinedDistRng2DParallel) {
#ifdef BDM_USE_STD_RANDOM
  // User-defined distributions are not supported with standard random implementation
  GTEST_SKIP() << "User-defined distributions not supported when using BDM_USE_STD_RANDOM=1";
#else
  Simulation simulation(TEST_NAME);
  std::vector<real_t> results;
#pragma omp parallel shared(simulation, results)
  {
    auto* random = simulation.GetRandom();
    auto function = [](const double* x, const double* params) {
      return ROOT::Math::lognormal_pdf(*x, params[0], params[1]);
    };
    real_t min = 1;
    real_t max = 4;
    size_t n_samples = 10000;
    real_t result = 0.0;
    auto distrng = random->GetUserDefinedDistRng2D(function, {1.1, 1.2}, min,
                                                   max, min, max);
    for (size_t i = 0; i < n_samples; i++) {
      result += distrng.Sample2().Norm();
    }
#pragma omp critical
    { results.push_back(result / n_samples); }
  }
  real_t sum = std::accumulate(results.begin(), results.end(), 0.0);
  EXPECT_LT(sum, std::numeric_limits<real_t>::max());
#endif
}

// This test is neither a proper death test nor a proper function test. We run
// this test, to see if it is possible to initialize the UserDefinedDistRng3D
// in a parallel region because the generator is based on ROOT's TF3 class which
// seems to have trouble when the constructor is called in parallel. Before the
// fix provided in PR #208, this test would fail roughly 2/3 times.
TEST(RandomTest, UserDefinedDistRng3DParallel) {
#ifdef BDM_USE_STD_RANDOM
  // User-defined distributions are not supported with standard random implementation
  GTEST_SKIP() << "User-defined distributions not supported when using BDM_USE_STD_RANDOM=1";
#else
  Simulation simulation(TEST_NAME);
  std::vector<real_t> results;
#pragma omp parallel shared(simulation, results)
  {
    auto* random = simulation.GetRandom();
    auto function = [](const double* x, const double* params) {
      return ROOT::Math::lognormal_pdf(*x, params[0], params[1]);
    };
    real_t min = 1;
    real_t max = 4;
    size_t n_samples = 10000;
    real_t result = 0.0;
    auto distrng = random->GetUserDefinedDistRng3D(function, {1.1, 1.2}, min,
                                                   max, min, max, min, max);
    for (size_t i = 0; i < n_samples; i++) {
      result += distrng.Sample3().Norm();
    }
#pragma omp critical
    { results.push_back(result / n_samples); }
  }
  real_t sum = std::accumulate(results.begin(), results.end(), 0.0);
  EXPECT_LT(sum, std::numeric_limits<real_t>::max());
#endif
}

TEST(RandomTest, Binomial) {
  Simulation simulation(TEST_NAME);
  auto* random = simulation.GetRandom();
  
#ifdef BDM_USE_STD_RANDOM
  // For std random, check statistical properties
  random->SetSeed(42);

  for (uint64_t i = 1; i < 10; i++) {
    int val = random->Binomial(i, i + 2);
    // Binomial(n, p) returns values in [0, n]
    EXPECT_GE(val, 0);
    EXPECT_LE(val, static_cast<int>(i));
  }

  auto distrng = random->GetBinomialRng(3, 4);
  for (uint64_t i = 0; i < 10; i++) {
    int val = distrng.Sample();
    EXPECT_GE(val, 0);
    EXPECT_LE(val, 3);
  }
#else
  // For ROOT random, check exact sequence equality
  TRandom3 reference;
  random->SetSeed(42);
  reference.SetSeed(42);

  for (uint64_t i = 0; i < 10; i++) {
    EXPECT_EQ(reference.Binomial(i, i + 2), random->Binomial(i, i + 2));
  }

  auto distrng = random->GetBinomialRng(3, 4);
  for (uint64_t i = 0; i < 10; i++) {
    EXPECT_EQ(reference.Binomial(3, 4), distrng.Sample());
  }
#endif
}

TEST(RandomTest, Poisson) {
  Simulation simulation(TEST_NAME);
  auto* random = simulation.GetRandom();
  
#ifdef BDM_USE_STD_RANDOM
  // For std random, check statistical properties
  random->SetSeed(42);

  for (uint64_t i = 1; i < 10; i++) {
    int val = random->Poisson(i);
    // Poisson distribution returns non-negative integers
    EXPECT_GE(val, 0);
  }

  for (uint64_t i = 1; i < 10; i++) {
    int val = random->Poisson(i + 2);
    EXPECT_GE(val, 0);
  }

  auto distrng = random->GetPoissonRng(123);
  for (uint64_t i = 0; i < 10; i++) {
    int val = distrng.Sample();
    EXPECT_GE(val, 0);
  }
#else
  // For ROOT random, check exact sequence equality
  TRandom3 reference;
  random->SetSeed(42);
  reference.SetSeed(42);

  for (uint64_t i = 0; i < 10; i++) {
    EXPECT_EQ(reference.Poisson(i), random->Poisson(i));
  }

  for (uint64_t i = 0; i < 10; i++) {
    EXPECT_EQ(reference.Poisson(i + 2), random->Poisson(i + 2));
  }

  auto distrng = random->GetPoissonRng(123);
  for (uint64_t i = 0; i < 10; i++) {
    EXPECT_EQ(reference.Poisson(123), distrng.Sample());
  }
#endif
}

TEST(RandomTest, Integer) {
  Simulation simulation(TEST_NAME);
  auto* random = simulation.GetRandom();
  
#ifdef BDM_USE_STD_RANDOM
  // For std random, check statistical properties
  random->SetSeed(42);

  for (uint64_t i = 1; i < 10; i++) {
    int val = random->Integer(i);
    // Integer(n) returns values in [0, n-1]
    EXPECT_GE(val, 0);
    EXPECT_LT(val, static_cast<int>(i));
  }
#else
  // For ROOT random, check exact sequence equality
  TRandom3 reference;
  random->SetSeed(42);
  reference.SetSeed(42);

  for (uint64_t i = 1; i < 10; i++) {
    EXPECT_EQ(reference.Integer(i), random->Integer(i));
  }
#endif
}

TEST(RandomTest, Circle) {
  Simulation simulation(TEST_NAME);
  auto* random = simulation.GetRandom();
  
#ifdef BDM_USE_STD_RANDOM
  // For std random, check statistical properties
  random->SetSeed(42);

  for (uint64_t i = 1; i < 10; i++) {
    auto actual = random->Circle(i);
    // Check that the point is on the circle with radius i
    real_t radius = std::sqrt(actual[0] * actual[0] + actual[1] * actual[1]);
    EXPECT_NEAR(radius, static_cast<real_t>(i), 1e-10);
  }
#else
  // For ROOT random, check exact sequence equality
  TRandom3 reference;
  random->SetSeed(42);
  reference.SetSeed(42);

  for (uint64_t i = 1; i < 10; i++) {
    double expected_x = 0;
    double expected_y = 0;
    reference.Circle(expected_x, expected_y, i);

    auto actual = random->Circle(i);
    EXPECT_REAL_EQ(expected_x, actual[0]);
    EXPECT_REAL_EQ(expected_y, actual[1]);
  }
#endif
}

TEST(RandomTest, Sphere) {
  Simulation simulation(TEST_NAME);
  auto* random = simulation.GetRandom();
  
#ifdef BDM_USE_STD_RANDOM
  // For std random, check statistical properties
  random->SetSeed(42);

  for (uint64_t i = 1; i < 10; i++) {
    auto actual = random->Sphere(i);
    // Check that the point is on the sphere with radius i
    real_t radius = std::sqrt(actual[0] * actual[0] + actual[1] * actual[1] + actual[2] * actual[2]);
    EXPECT_NEAR(radius, static_cast<real_t>(i), 1e-10);
  }
#else
  // For ROOT random, check exact sequence equality
  TRandom3 reference;
  random->SetSeed(42);
  reference.SetSeed(42);

  for (uint64_t i = 1; i < 10; i++) {
    double expected_x = 0;
    double expected_y = 0;
    double expected_z = 0;
    reference.Sphere(expected_x, expected_y, expected_z, i);

    auto actual = random->Sphere(i);
    EXPECT_REAL_EQ(expected_x, actual[0]);
    EXPECT_REAL_EQ(expected_y, actual[1]);
    EXPECT_REAL_EQ(expected_z, actual[2]);
  }
#endif
}

#ifdef USE_DICT
TEST_F(IOTest, Random) {
#ifdef BDM_USE_STD_RANDOM
  // This test checks ROOT TRandom3 exact compatibility, skip for std random
  GTEST_SKIP() << "ROOT exact compatibility test skipped when using BDM_USE_STD_RANDOM=1";
#else
  Random random;
  TRandom3 reference;

  random.SetSeed(42);
  reference.SetSeed(42);

  for (uint64_t i = 0; i < 10; i++) {
    EXPECT_REAL_EQ(reference.Gaus(), random.Gaus());
  }

  Random* restored;
  BackupAndRestore(random, &restored);

  for (uint64_t i = 0; i < 10; i++) {
    EXPECT_REAL_EQ(reference.Uniform(i, i + 2), random.Uniform(i, i + 2));
  }
#endif
}

TEST_F(IOTest, UserDefinedDistRng1D) {
#ifndef BDM_USE_STD_RANDOM
  Simulation simulation(TEST_NAME);
  auto* random = simulation.GetRandom();

  auto ud_dist = [](const double* x, const double* param) { return sin(*x); };
  auto udd_rng = random->GetUserDefinedDistRng1D(ud_dist, {}, 0, 3);

  gRandom->SetSeed(42);
  std::vector<real_t> expected;
  for (int i = 0; i < 10; ++i) {
    expected.push_back(udd_rng.Sample());
  }

  UserDefinedDistRng1D* restored;
  BackupAndRestore(udd_rng, &restored);

  gRandom->SetSeed(42);
  std::vector<real_t> actual;
  for (int i = 0; i < 10; ++i) {
    actual.push_back(restored->Sample());
  }
  for (size_t i = 0; i < actual.size(); i++) {
    EXPECT_NEAR(expected[i], actual[i], 1e-6);
  }
#else
  // Serialization tests are not supported with standard random implementation
  GTEST_SKIP() << "Serialization tests skipped when using BDM_USE_STD_RANDOM=1";
#endif
}

#endif  // USE_DICT

}  // namespace bdm
