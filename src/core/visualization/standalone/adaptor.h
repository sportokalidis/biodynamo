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
//
// StandaloneAdaptor: a VisualizationAdaptor that writes VTK XML files
// (VTU/PVTU for agents, VTI/PVTI for diffusion) using only the C++ standard
// library and ROOT reflection.
//
// It is the dependency-free counterpart of ParaviewAdaptor: no VTK, no Qt,
// no ParaView installation is required.  The class is compiled into a
// separate shared library (libVisualizationAdaptor.so) and loaded at
// runtime by ROOT's TPluginManager, exactly as ParaviewAdaptor is.
//
// Design pattern — Strategy + Plugin:
//   VisualizationAdaptor is an abstract Strategy interface with one method
//   Visualize().  The concrete strategy (this class vs ParaviewAdaptor) is
//   selected at runtime by the "adaptor" key in bdm.toml without recompiling
//   the simulation.  ROOT's plugin system acts as the factory/registry.
//
// Lifecycle:
//   Factory()   — called once by TPluginManager; returns a heap-allocated
//                 instance.  ROOT owns the pointer.
//   Visualize() — called every exported time step by VisualizationOp.
//                 On the first call, it lazily creates the output directory
//                 and the StandaloneExporter.  On every call it delegates
//                 the actual I/O to the exporter.
//   ~StandaloneAdaptor() — deletes the exporter (closes file handles).
//
// -----------------------------------------------------------------------------

#ifndef BDM_SRC_CORE_VISUALIZATION_STANDALONE_ADAPTOR_H_
#define BDM_SRC_CORE_VISUALIZATION_STANDALONE_ADAPTOR_H_

#include "core/util/root.h"                           // BDM_CLASS_DEF_NV
#include "core/visualization/visualization_adaptor.h"  // abstract base

namespace bdm {

// Forward declaration — keeps the exporter header out of translation units
// that only need to know the adaptor exists (e.g. visualization_adaptor.cc).
class StandaloneExporter;

// -----------------------------------------------------------------------------
/// Visualization adaptor that exports simulation state to VTK XML files
/// without linking against VTK or ParaView.
///
/// Registered with ROOT's plugin manager under the name "standalone".
/// Select it in bdm.toml with:
///   [visualization]
///   adaptor = "standalone"
// -----------------------------------------------------------------------------
class StandaloneAdaptor : public VisualizationAdaptor {
 public:
  /// Required by the ROOT plugin system.  TPluginManager calls this static
  /// function (via cling) to create a new instance of the adaptor.
  static StandaloneAdaptor* Factory();

  StandaloneAdaptor();
  ~StandaloneAdaptor() override;

  /// Called every exported time step by VisualizationOp.
  /// Writes agent VTU files and diffusion VTI files if export is enabled
  /// and the current step is a multiple of visualization_interval.
  void Visualize() override;

 private:
  /// True after the output directory and exporter have been created on the
  /// first Visualize() call.  Avoids redundant filesystem operations.
  bool initialized_ = false;

  /// Owns the writer object that performs the actual VTK XML I/O.
  /// Allocated lazily in Visualize(); deleted in the destructor.
  StandaloneExporter* exporter_ = nullptr;

  // ROOT dictionary macro — generates RTTI metadata so that TPluginManager
  // can resolve and call Factory() by name at runtime.
  // "NV" = no virtual ROOT I/O (this class is not persisted to ROOT files).
  // "1"  = class version for ROOT I/O versioning.
  BDM_CLASS_DEF_NV(StandaloneAdaptor, 1);
};

}  // namespace bdm

#endif  // BDM_SRC_CORE_VISUALIZATION_STANDALONE_ADAPTOR_H_
