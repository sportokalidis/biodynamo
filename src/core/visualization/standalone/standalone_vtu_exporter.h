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
// StandaloneVtuExporter: writes VTK XML Unstructured Grid files (.vtu/.pvtu)
// using only the C++ standard library — no VTK, no ParaView dependency.
//
// Two kinds of output are produced per exported time step:
//
//  Agents  →  agents_{step}_p{p}.vtu  +  agents_{step}.pvtu
//  Diffusion → diffusion_{name}_{step}_p{p}.vtu  +
//              diffusion_{name}_{step}.pvtu
//
// Both agent and diffusion files use the VTK "appended raw binary" layout:
//   • An XML header declares the data arrays and their byte offsets.
//   • A single binary block (AppendedData) holds the raw array bytes.
//   • Each array in the binary block is preceded by a 4-byte UInt32
//     that gives its byte length (required by the VTK file format spec).
//
// Parallelism strategy:
//   Agent export   — agents are partitioned into equal slices; each OpenMP
//                    thread writes one slice to its own .vtu piece file.
//   Diffusion export — the 3-D grid is sliced into horizontal Z-slabs; each
//                    thread writes one slab to its own .vtu piece file.
//   The parallel index (.pvtu) file is always written on the main thread
//   after all piece files are complete.
//
// -----------------------------------------------------------------------------

#ifndef BDM_SRC_CORE_VISUALIZATION_STANDALONE_STANDALONE_VTU_EXPORTER_H_
#define BDM_SRC_CORE_VISUALIZATION_STANDALONE_STANDALONE_VTU_EXPORTER_H_

#include <string>
#include <vector>

namespace bdm {

// -----------------------------------------------------------------------------
/// Writes VTK XML (.vtu / .pvtu) visualization files without linking against
/// VTK or ParaView.
///
/// One instance is owned by StandaloneAdaptor for the lifetime of the
/// simulation.  WriteStep() and WriteDiffusionStep() are called together
/// once per exported time step.
// -----------------------------------------------------------------------------
class StandaloneVtuExporter {
 public:
  /// @param output_dir  Absolute path to the directory where all output files
  ///                    will be written.  Must exist before any Write call.
  explicit StandaloneVtuExporter(const std::string& output_dir);
  ~StandaloneVtuExporter();

  /// Export all agents for the current time step.
  ///
  /// Produces per-thread piece files (agents_{step}_p{p}.vtu) and a
  /// parallel index file (agents_{step}.pvtu).
  ///
  /// Per-agent data written:
  ///   Cell_ID       — unique 64-bit agent identifier
  ///   Diameter      — agent diameter (all agent types)
  ///   Mass          — cell mass (Cell subclass; 0 for other types)
  ///   Volume        — cell volume (Cell subclass; 0 for other types)
  ///   TractionForce — 3-component mechanical force vector (Cell; 0 otherwise)
  ///   <extra>       — any scalar members listed under additional_data_members
  ///                   in bdm.toml, read via ROOT TDataMember reflection
  void WriteStep();

  /// Export diffusion grids for every substance listed under
  /// [[visualize_diffusion]] in bdm.toml.
  ///
  /// For each substance, produces per-thread Z-slab piece files
  /// (diffusion_{name}_{step}_p{p}.vtu) and a parallel index file
  /// (diffusion_{name}_{step}.pvtu).
  ///
  /// Each piece is a VTK_VOXEL (type 11) unstructured grid where:
  ///   • Points  — the (nx+1)×(ny+1)×(nz_slab+1) corner nodes of the voxels.
  ///   • CellData — concentration (scalar) and/or gradient (3-vector) stored
  ///               at voxel centers, one value per diffusion box.
  void WriteDiffusionStep();

 private:
  /// Directory where all .vtu and .pvtu files are written.
  std::string output_dir_;

  /// Monotonically increasing counter used as a filename suffix.
  /// Incremented at the end of each WriteStep() call so that agent and
  /// diffusion files for the same simulation step share the same suffix.
  int step_ = 0;

  /// Write the parallel index file for agents.
  /// Lists all piece files and mirrors the PointData schema so that ParaView
  /// can discover available fields without loading every piece.
  /// @param pieces  Number of .vtu piece files written by WriteStep().
  void WritePvtu(int pieces) const;

  /// Write the parallel index file for one diffusion substance.
  /// @param name              Substance name (used in the filename).
  /// @param has_concentration Whether the concentration array was exported.
  /// @param has_gradient      Whether the gradient array was exported.
  /// @param pieces            Number of Z-slab .vtu piece files written.
  void WriteDiffusionPvtu(const std::string& name,
                          bool has_concentration,
                          bool has_gradient,
                          int pieces = 1) const;
};

}  // namespace bdm

#endif  // BDM_SRC_CORE_VISUALIZATION_STANDALONE_STANDALONE_VTU_EXPORTER_H_
