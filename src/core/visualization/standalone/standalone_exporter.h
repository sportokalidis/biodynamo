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
// StandaloneExporter: writes VTK XML visualization files (.vtu/.pvtu for
// agents; .vti/.pvti for diffusion) using only the C++ standard library —
// no VTK, no ParaView dependency.
//
// Two kinds of output are produced per exported time step:
//
//  Agents    →  agents_{step}_p{p}.vtu  +  agents_{step}.pvtu
//  Diffusion →  diffusion_{name}_{step}_p{p}.vti  +
//               diffusion_{name}_{step}.pvti
//
// Agent files use the VTK "appended raw binary" layout (UnstructuredGrid):
//   • An XML header declares data arrays and their byte offsets.
//   • A binary AppendedData block holds the raw array bytes.
//   • Each array is preceded by a 4-byte UInt32 giving its byte length.
//
// Diffusion files use VTK ImageData (.vti / .pvti):
//   • The uniform Cartesian grid is described by three numbers: Origin,
//     Spacing, and Extent — no explicit geometry arrays.
//   • Concentration and gradient are stored as CellData at box centres
//     (one value per diffusion box), using the same appended binary layout.
//   • This eliminates the corner-node and connectivity overhead that a
//     VTK_VOXEL UnstructuredGrid would require, reducing diffusion file
//     sizes by ~70 % compared to the VTU approach.
//
// Parallelism strategy:
//   Agent export   — agents are partitioned into equal slices; each OpenMP
//                    thread writes one slice to its own .vtu piece file.
//   Diffusion export — the 3-D grid is sliced into horizontal Z-slabs; each
//                    thread writes one slab to its own .vti piece file.
//   Parallel index files (.pvtu / .pvti) are always written on the main
//   thread after all piece files are complete.
//
// -----------------------------------------------------------------------------

#ifndef BDM_SRC_CORE_VISUALIZATION_STANDALONE_STANDALONE_EXPORTER_H_
#define BDM_SRC_CORE_VISUALIZATION_STANDALONE_STANDALONE_EXPORTER_H_

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace bdm {

// -----------------------------------------------------------------------------
/// Writes VTK XML visualization files without linking against VTK or ParaView.
///
/// One instance is owned by StandaloneAdaptor for the lifetime of the
/// simulation.  WriteStep() and WriteDiffusionStep() are called together
/// once per exported time step.
// -----------------------------------------------------------------------------
class StandaloneExporter {
 public:
  /// @param output_dir  Absolute path to the directory where all output files
  ///                    will be written.  Must exist before any Write call.
  explicit StandaloneExporter(const std::string& output_dir);
  ~StandaloneExporter();

  /// Export all agents for the current time step.
  ///
  /// Produces per-thread piece files (agents_{step}_p{p}.vtu) and a
  /// parallel index file (agents_{step}.pvtu).
  ///
  /// Per-agent data written:
  ///   Cell_ID   — unique 64-bit agent identifier (always)
  ///   Diameter  — agent diameter (always)
  ///   <extra>   — any scalar members listed under additional_data_members
  ///               in bdm.toml, read via ROOT TDataMember reflection
  void WriteStep();

  /// Export diffusion grids for every substance listed under
  /// [[visualize_diffusion]] in bdm.toml.
  ///
  /// Delegates to WriteDiffusionStepVti(), which produces per-thread Z-slab
  /// VTI piece files (diffusion_{name}_{step}_p{p}.vti) and a parallel index
  /// file (diffusion_{name}_{step}.pvti).
  ///
  /// The uniform grid is encoded as VTK ImageData (Origin, Spacing, Extent —
  /// no explicit geometry arrays).  PointData stores concentration and/or
  /// gradient at box centres, one value per diffusion box.
  ///
  /// Also increments the internal step counter so that agent and diffusion
  /// files for the same exported step always share the same filename suffix.
  void WriteDiffusionStep();

 private:
  /// Directory where all output files are written.
  std::string output_dir_;

  /// Monotonically increasing counter used as the filename suffix.
  /// Incremented at the end of WriteDiffusionStep() (after both agent and
  /// diffusion files are written) so both file families share the same N.
  int step_ = 0;

  /// Scalar member names read from additional_data_members in bdm.toml.
  /// Loaded once in the constructor; bdm.toml does not change at runtime.
  std::vector<std::string> extra_members_;

  /// Write the parallel agent index file (agents_{step}.pvtu).
  void WritePvtu(int pieces,
                 const std::vector<std::pair<std::string, std::string>>&
                     extra_type_info) const;

  /// Write VTI piece files + PVTI index for all diffusion substances.
  /// Uses VTK ImageData (PointData at box centres, no explicit geometry).
  void WriteDiffusionStepVti();

  /// Write VTU piece files + PVTU index for all diffusion substances.
  /// Uses VTK UnstructuredGrid with explicit VTK_VOXEL cells (CellData).
  void WriteDiffusionStepVtu();

  /// Write the VTI parallel index file for one diffusion substance.
  void WriteDiffusionPvti(const std::string& name,
                          bool has_concentration,
                          bool has_gradient,
                          std::size_t nx, std::size_t ny, std::size_t nz,
                          double ox, double oy, double oz,
                          double spacing,
                          int pieces,
                          std::size_t boxes_per_piece) const;

  /// Write the VTU parallel index file for one diffusion substance.
  void WriteDiffusionVtuIndex(const std::string& name,
                              bool has_concentration,
                              bool has_gradient,
                              int pieces) const;
};

}  // namespace bdm

#endif  // BDM_SRC_CORE_VISUALIZATION_STANDALONE_STANDALONE_EXPORTER_H_
