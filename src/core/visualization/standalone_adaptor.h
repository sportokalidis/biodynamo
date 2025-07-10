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

#ifndef CORE_VISUALIZATION_STANDALONE_ADAPTOR_H_
#define CORE_VISUALIZATION_STANDALONE_ADAPTOR_H_

#include "core/visualization/standalone/standalone_visualization_adaptor.h"
#include "core/visualization/visualization_adaptor.h"

namespace bdm {

/// Standalone visualization adaptor that integrates with the VisualizationAdaptor interface
/// but uses VTK-independent export for environments without ParaView dependencies
class StandaloneAdaptor : public VisualizationAdaptor {
 public:
  StandaloneAdaptor();
  ~StandaloneAdaptor() override;

  void Visualize() override;

 private:
  StandaloneVisualizationAdaptor* adaptor_;
  bool initialized_ = false;
};

}  // namespace bdm

#endif  // CORE_VISUALIZATION_STANDALONE_ADAPTOR_H_
