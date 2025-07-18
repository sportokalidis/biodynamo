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
#ifdef BDM_USE_BOOST_SERIALIZATION
#include "core/util/serialization_std.h"
#define SERIALIZATION_TESTS_ENABLED 1
#else
#define SERIALIZATION_TESTS_ENABLED 0
#endif
#include <gtest/gtest.h>
#include <iostream>
#include <fstream>

namespace bdm {
namespace experimental {

class StdRandomTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Set fixed seed for reproducible tests
    SetStdSeed(42);
  }
};

TEST_F(StdRandomTest, BasicRandomGeneration) {
  auto& rng = GetStdRng();
  
  // Test uniform distribution
  for (int i = 0; i < 100; ++i) {
    real_t val = rng.Uniform();
    EXPECT_GE(val, 0.0);
    EXPECT_LT(val, 1.0);
  }
  
  // Test uniform with range
  for (int i = 0; i < 100; ++i) {
    real_t val = rng.Uniform(5.0, 10.0);
    EXPECT_GE(val, 5.0);
    EXPECT_LT(val, 10.0);
  }
}

TEST_F(StdRandomTest, GaussianDistribution) {
  auto& rng = GetStdRng();
  
  // Generate samples and check basic statistics
  const int n_samples = 10000;
  real_t sum = 0.0;
  real_t sum_sq = 0.0;
  
  for (int i = 0; i < n_samples; ++i) {
    real_t val = rng.Gaussian(0.0, 1.0);
    sum += val;
    sum_sq += val * val;
  }
  
  real_t mean = sum / n_samples;
  real_t variance = (sum_sq / n_samples) - (mean * mean);
  
  // Check if mean is close to 0 and variance close to 1
  EXPECT_NEAR(mean, 0.0, 0.1);
  EXPECT_NEAR(variance, 1.0, 0.1);
}

TEST_F(StdRandomTest, DistributionClasses) {
  auto rng = std::make_shared<StdRandomGenerator>(42);
  
  // Test uniform distribution class
  StdUniformRng uniform_rng(0.0, 10.0);
  uniform_rng.SetRandomGenerator(rng);
  
  for (int i = 0; i < 100; ++i) {
    real_t val = uniform_rng.Sample();
    EXPECT_GE(val, 0.0);
    EXPECT_LT(val, 10.0);
  }
  
  // Test sample arrays
  auto samples = uniform_rng.SampleArray<5>();
  EXPECT_EQ(samples.size(), 5u);
  for (const auto& val : samples) {
    EXPECT_GE(val, 0.0);
    EXPECT_LT(val, 10.0);
  }
}

TEST_F(StdRandomTest, PoissonDistribution) {
  auto rng = std::make_shared<StdRandomGenerator>(42);
  StdPoissonRng poisson_rng(5.0);
  poisson_rng.SetRandomGenerator(rng);
  
  // Generate samples and check they're non-negative integers
  for (int i = 0; i < 100; ++i) {
    int val = poisson_rng.Sample();
    EXPECT_GE(val, 0);
  }
}

TEST_F(StdRandomTest, UserDefinedDistribution) {
  auto rng = std::make_shared<StdRandomGenerator>(42);
  
  // Simple linear function y = x in [0, 1]
  auto linear_func = [](real_t x) -> real_t { return x; };
  
  StdUserDefinedRng user_rng(linear_func, 0.0, 1.0);
  user_rng.SetRandomGenerator(rng);
  
  // Test that samples are in the correct range
  for (int i = 0; i < 100; ++i) {
    real_t val = user_rng.Sample();
    EXPECT_GE(val, 0.0);
    EXPECT_LE(val, 1.0);
  }
}

// Test the new serialization system
#if SERIALIZATION_TESTS_ENABLED
class SerializationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    test_file_ = "test_serialization.dat";
    RemoveFile(test_file_);
  }
  
  void TearDown() override {
    RemoveFile(test_file_);
    RemoveFile(test_file_ + ".sysinfo");
  }
  
  std::string test_file_;
};

// Simple test class with Boost serialization
class TestData {
 public:
  TestData() = default;
  TestData(int i, double d, const std::string& s) : int_val(i), double_val(d), string_val(s) {}
  
  bool operator==(const TestData& other) const {
    return int_val == other.int_val && 
           double_val == other.double_val && 
           string_val == other.string_val;
  }
  
#if SERIALIZATION_TESTS_ENABLED
  BDM_BOOST_SERIALIZABLE(TestData) {
    ar & int_val & double_val & string_val;
  }
#endif
  
  int int_val = 0;
  double double_val = 0.0;
  std::string string_val;
};

TEST_F(SerializationTest, BasicSerialization) {
  TestData original(42, 3.14, "test_string");
  
  // Write object
  WriteBoostObject(test_file_, "test_data", original);
  EXPECT_TRUE(FileExists(test_file_));
  
  // Read object back
  TestData restored;
  bool success = ReadBoostObject(test_file_, "test_data", restored);
  
  EXPECT_TRUE(success);
  EXPECT_EQ(original, restored);
}

TEST_F(SerializationTest, SimpleWrapper) {
  SimpleWrapper<int> original(123);
  
  WriteBoostObject(test_file_, "wrapped_int", original);
  
  SimpleWrapper<int> restored;
  bool success = ReadBoostObject(test_file_, "wrapped_int", restored);
  
  EXPECT_TRUE(success);
  EXPECT_EQ(original.Get(), restored.Get());
}

TEST_F(SerializationTest, BackupRestore) {
  TestData original(100, 2.71, "backup_test");
  
  SimpleBackup<TestData> backup(test_file_);
  backup.BackupObject(original, "test_object");
  
  SimpleBackup<TestData> restore("", test_file_);
  TestData restored;
  bool success = restore.RestoreObject(restored, "test_object");
  
  EXPECT_TRUE(success);
  EXPECT_EQ(original, restored);
}
#endif // SERIALIZATION_TESTS_ENABLED

// Performance comparison test
TEST(PerformanceTest, DISABLED_RandomGenerationSpeed) {
  const int n_samples = 1000000;
  
  std::cout << "\n=== Random Number Generation Performance ===\n";
  
  // Test standard generator
  auto start = std::chrono::high_resolution_clock::now();
  SetStdSeed(42);
  real_t sum = 0.0;
  for (int i = 0; i < n_samples; ++i) {
    sum += GetStdRng().Uniform();
  }
  auto end = std::chrono::high_resolution_clock::now();
  auto std_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  
  std::cout << "Standard C++ RNG:\n";
  std::cout << "  Time: " << std_duration.count() << " ms\n";
  std::cout << "  Rate: " << (n_samples / (std_duration.count() / 1000.0)) << " samples/sec\n";
  std::cout << "  Sum: " << sum << "\n\n";
  
  // This test is disabled by default as it requires ROOT for comparison
  // Enable it manually to compare performance
}

} // namespace experimental
} // namespace bdm
