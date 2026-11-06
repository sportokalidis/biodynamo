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

#ifndef UNIT_CORE_VISUALIZATION_STANDALONE_STANDALONE_EXPORTER_TEST_H_
#define UNIT_CORE_VISUALIZATION_STANDALONE_STANDALONE_EXPORTER_TEST_H_

#include "core/agent/cell.h"

namespace bdm {
namespace standalone_exporter_test_internal {

// A plain `int` member (rather than `real_t`) keeps ReadScalarMember's type
// dispatch unambiguous regardless of whether the build uses float or double.
class StandaloneTestCell : public Cell {
  BDM_AGENT_HEADER(StandaloneTestCell, Cell, 1);

 public:
  StandaloneTestCell() = default;

  int extra_field_ = 0;
};

}  // namespace standalone_exporter_test_internal
}  // namespace bdm

#endif  // UNIT_CORE_VISUALIZATION_STANDALONE_STANDALONE_EXPORTER_TEST_H_
