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

#include <iostream>
#include "core/util/string.h"
#include "gtest/gtest.h"

// Count currently-failing tests, one by one (not by suite): a suite counts
// as failed as soon as any test in it fails, so mixing suite- and test-level
// counts (as an earlier version of this file did) can make the total go
// negative and never return to zero, even after every test has passed.
int CountFailedTests() {
  auto unit_test = ::testing::UnitTest::GetInstance();
  int failed_cnt = 0;
  for (int i = 0; i < unit_test->total_test_suite_count(); ++i) {
    const auto& test_case = *unit_test->GetTestSuite(i);
    for (int j = 0; j < test_case.total_test_count(); ++j) {
      if (test_case.GetTestInfo(j)->result()->Failed()) {
        failed_cnt++;
      }
    }
  }
  return failed_cnt;
}

// Build a gtest filter selecting the currently-failing FLAKY_ tests.
void HandleFlakyTests(std::stringstream& filter) {
  auto unit_test = ::testing::UnitTest::GetInstance();
  for (int i = 0; i < unit_test->total_test_suite_count(); ++i) {
    const auto& test_case = *unit_test->GetTestSuite(i);
    for (int j = 0; j < test_case.total_test_count(); ++j) {
      const auto& test_info = *test_case.GetTestInfo(j);
      if (test_info.result()->Failed() &&
          bdm::StartsWith(test_case.name(), "FLAKY_")) {
        filter << test_case.name() << "." << test_info.name() << ":";
      }
    }
  }
}

int main(int argc, char** argv) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  ::testing::InitGoogleTest(&argc, argv);
  RUN_ALL_TESTS();
  int failed_cnt = CountFailedTests();

  int repeat = 2;
  // Repeat failing flaky tests up to `repeat` times
  while (repeat-- > 0 && failed_cnt != 0) {
    std::stringstream filter;
    HandleFlakyTests(filter);
    if (filter.str() == "") {
      // Remaining failures are not flaky; nothing left to retry.
      break;
    }
    ::testing::GTEST_FLAG(filter) = filter.str().c_str();
    std::cout << "Rerunning the following failed flaky test(s):" << std::endl;
    RUN_ALL_TESTS();
    failed_cnt = CountFailedTests();
  }
  return failed_cnt;
}
