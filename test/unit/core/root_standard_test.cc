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

#include <TInterpreter.h>

#include <cstdint>

#include "core/agent/cell.h"
#include "gtest/gtest.h"

namespace bdm {

// Compilers disagree on the exact value for a given standard: GCC 13 reports
// the provisional 202100L for C++23 while GCC 14 and Cling report 202302L.
// Compare the language generation instead of the vendor macro value.
constexpr int CxxGeneration(std::int64_t value) {
  if (value <= 201703L) {
    return 17;
  }
  if (value <= 202002L) {
    return 20;
  }
  if (value <= 202302L) {
    return 23;
  }
  return 26;
}

static_assert(__cplusplus >= 201703L, "BioDynaMo requires at least C++17");

// BioDynaMo adopts the standard of the ROOT package it was built against, so
// this checks that the interpreter and compiled code ended up in the same
// language generation, whichever one that is.
TEST(RootStandardTest, InterpreterAndPublicHeadersAgree) {
  TInterpreter::EErrorCode error = TInterpreter::kNoError;
  auto standard = gInterpreter->Calc("__cplusplus", &error);
  ASSERT_EQ(TInterpreter::kNoError, error);
  EXPECT_EQ(CxxGeneration(__cplusplus), CxxGeneration(standard))
      << "compiled __cplusplus=" << __cplusplus
      << ", interpreter __cplusplus=" << standard;

#ifdef USE_DICT
  // The dictionary supplies the public-header paths and configured real type.
  ASSERT_TRUE(gInterpreter->Declare("#include \"core/agent/cell.h\""));
  auto cell_size = gInterpreter->Calc("sizeof(bdm::Cell)", &error);
  ASSERT_EQ(TInterpreter::kNoError, error);
  EXPECT_EQ(sizeof(Cell), static_cast<size_t>(cell_size));
#endif
}

}  // namespace bdm
