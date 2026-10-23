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
#ifdef USE_STANDALONE_VISUALIZATION

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include "core/agent/cell.h"
#include "core/model_initializer.h"
#include "core/param/param.h"
#include "core/resource_manager.h"
#include "core/scheduler.h"
#include "core/simulation.h"
#include "core/util/io.h"
#include "core/util/string.h"
#include "core/visualization/standalone/adaptor.h"
#include "core/visualization/standalone/standalone_exporter.h"
#include "unit/core/visualization/standalone/standalone_exporter_test.h"
#include "unit/test_util/test_util.h"

namespace bdm {

using standalone_exporter_test_internal::StandaloneTestCell;

namespace {

// Prepares `dir` as an empty directory, removing any leftovers from a
// previous run of the same test (gtest reuses TEST_NAME's output directory
// across runs, it does not create a fresh one per invocation).
std::string FreshVizDir(Simulation* sim) {
  std::filesystem::remove_all(sim->GetOutputDir());
  std::string viz_dir = Concat(sim->GetOutputDir(), "/viz");
  std::filesystem::create_directories(viz_dir);
  return viz_dir;
}

// Counts regular files in `dir` whose name starts with `prefix`.
std::size_t CountFilesWithPrefix(const std::string& dir,
                                 const std::string& prefix) {
  std::size_t count = 0;
  for (const auto& entry : std::filesystem::directory_iterator(dir)) {
    if (entry.path().filename().string().rfind(prefix, 0) == 0) {
      count++;
    }
  }
  return count;
}

// Counts "agents_<N>.pvtu" index files in `dir`: one per successful
// StandaloneExporter::WriteStep() call, regardless of how many per-thread
// "agents_<N>_p<P>.vtu" piece files that call also produced.
std::size_t CountAgentsPvtuFiles(const std::string& dir) {
  std::size_t count = 0;
  for (const auto& entry : std::filesystem::directory_iterator(dir)) {
    const std::string name = entry.path().filename().string();
    if (name.rfind("agents_", 0) == 0 && entry.path().extension() == ".pvtu") {
      count++;
    }
  }
  return count;
}

std::string ReadFile(const std::string& path) {
  std::ifstream ifs(path);
  std::ostringstream ss;
  ss << ifs.rdbuf();
  return ss.str();
}

// Sums the `NumberOfPoints="N"` attribute across every *.vtu piece file
// whose name starts with `prefix` inside `dir`.
std::size_t SumNumberOfPoints(const std::string& dir,
                              const std::string& prefix) {
  std::size_t total = 0;
  std::regex re("NumberOfPoints=\"(\\d+)\"");
  for (const auto& entry : std::filesystem::directory_iterator(dir)) {
    const std::string name = entry.path().filename().string();
    if (name.rfind(prefix, 0) != 0) {
      continue;
    }
    std::smatch m;
    std::string content = ReadFile(entry.path().string());
    if (std::regex_search(content, m, re)) {
      total += std::stoul(m[1].str());
    }
  }
  return total;
}

}  // namespace

// -----------------------------------------------------------------------------
TEST(StandaloneExporterTest, WriteStepCreatesExpectedFiles) {
  Simulation sim(TEST_NAME);
  std::string viz_dir = FreshVizDir(&sim);
  auto* rm = sim.GetResourceManager();

  rm->AddAgent(new Cell({0, 0, 0}));
  rm->AddAgent(new Cell({1, 2, 3}));
  rm->AddAgent(new Cell({-5, 5, 0.9}));

  StandaloneExporter exporter(viz_dir);
  exporter.WriteStep();

  std::size_t pieces = CountFilesWithPrefix(viz_dir, "agents_0_p");
  EXPECT_GT(pieces, 0u);
  EXPECT_TRUE(FileExists(Concat(viz_dir, "/agents_0.pvtu")));

  // One <Piece Source=.../> line per on-disk piece file.
  std::string pvtu = ReadFile(Concat(viz_dir, "/agents_0.pvtu"));
  std::size_t piece_refs = 0;
  std::string::size_type pos = 0;
  while ((pos = pvtu.find("<Piece Source=", pos)) != std::string::npos) {
    piece_refs++;
    pos += 1;
  }
  EXPECT_EQ(pieces, piece_refs);

  // All three agents must show up across the piece files, however many
  // OpenMP threads happened to split the work.
  EXPECT_EQ(3u, SumNumberOfPoints(viz_dir, "agents_0_p"));
}

// -----------------------------------------------------------------------------
TEST(StandaloneExporterTest, WriteStepExportsAdditionalDataMembers) {
  Simulation sim(TEST_NAME);
  std::string viz_dir = FreshVizDir(&sim);
  auto* rm = sim.GetResourceManager();

  auto* cell = new StandaloneTestCell();
  cell->SetPosition({0, 0, 0});
  cell->extra_field_ = 42;
  rm->AddAgent(cell);

  // StandaloneExporter reads additional_data_members from "bdm.toml" in the
  // current working directory once, in its constructor. Write it just
  // before construction and remove it right after, to keep the window where
  // this process-wide file exists as short as possible.
  {
    std::ofstream toml("bdm.toml");
    toml << "additional_data_members = [ \"extra_field_\" ]\n";
  }
  StandaloneExporter exporter(viz_dir);
  std::remove("bdm.toml");

  exporter.WriteStep();

  EXPECT_TRUE(FileExists(Concat(viz_dir, "/agents_0.pvtu")));
  std::string pvtu = ReadFile(Concat(viz_dir, "/agents_0.pvtu"));
  EXPECT_NE(std::string::npos, pvtu.find("Name=\"extra_field_\""));

  bool found_in_piece = false;
  for (const auto& entry : std::filesystem::directory_iterator(viz_dir)) {
    const std::string name = entry.path().filename().string();
    if (name.rfind("agents_0_p", 0) == 0 &&
        ReadFile(entry.path().string()).find("Name=\"extra_field_\"") !=
            std::string::npos) {
      found_in_piece = true;
    }
  }
  EXPECT_TRUE(found_in_piece);
}

// -----------------------------------------------------------------------------
TEST(StandaloneExporterTest, WriteDiffusionStepCreatesExpectedFiles) {
  // Param is only mutable through the pointer the Simulation constructor
  // hands to this callback; GetParam() returns a const Param* afterwards.
  auto set_param = [](auto* param) {
    param->bound_space = Param::BoundSpaceMode::kClosed;
    param->min_bound = -100;
    param->max_bound = 100;

    Param::VisualizeDiffusion vd;
    vd.name = "Na";
    vd.concentration = true;
    vd.gradient = true;
    param->visualize_diffusion.push_back(vd);
  };
  Simulation sim(TEST_NAME, set_param);
  std::string viz_dir = FreshVizDir(&sim);
  auto* rm = sim.GetResourceManager();

  enum Substances { kNa };
  ModelInitializer::DefineSubstance(kNa, "Na", 0.05, 0, 5);
  rm->GetDiffusionGrid(kNa)->Initialize();

  StandaloneExporter exporter(viz_dir);
  exporter.WriteDiffusionStep();

  std::size_t pieces = CountFilesWithPrefix(viz_dir, "diffusion_Na_0_p");
  EXPECT_GT(pieces, 0u);
  EXPECT_TRUE(FileExists(Concat(viz_dir, "/diffusion_Na_0.pvti")));

  std::string pvti = ReadFile(Concat(viz_dir, "/diffusion_Na_0.pvti"));
  EXPECT_NE(std::string::npos, pvti.find("Concentration"));
  EXPECT_NE(std::string::npos, pvti.find("Gradient"));
}

// -----------------------------------------------------------------------------
/// Exercises StandaloneAdaptor::Visualize()'s two gates: param->
/// export_visualization must be on, and the current step must be a multiple
/// of param->visualization_interval. The scheduler is stepped with
/// export_visualization off so VisualizationOp's own plugin-loading path is
/// never triggered; only this test's own StandaloneAdaptor instance writes.
TEST(StandaloneAdaptorTest, VisualizeHonorsExportGate) {
  // Capture the mutable Param* the constructor hands to this callback;
  // GetParam() returns a const Param* once the Simulation is built, but
  // this test needs to flip export_visualization and visualization_interval
  // partway through.
  Param* param = nullptr;
  Simulation sim(TEST_NAME, [&](Param* p) {
    param = p;
    param->export_visualization = false;
  });
  std::string viz_dir = FreshVizDir(&sim);
  auto* rm = sim.GetResourceManager();
  rm->AddAgent(new Cell({0, 0, 0}));

  StandaloneAdaptor adaptor;
  adaptor.Visualize();  // lazy-init only; export disabled -> no write
  EXPECT_TRUE(std::filesystem::exists(viz_dir));
  EXPECT_EQ(0u, CountAgentsPvtuFiles(viz_dir));

  sim.GetScheduler()->Simulate(1);  // GetSimulatedSteps() -> 1
  sim.GetScheduler()->Simulate(1);  // GetSimulatedSteps() -> 2

  param->export_visualization = true;
  param->visualization_interval = 2;

  adaptor.Visualize();  // step 2, 2 % 2 == 0 -> writes
  EXPECT_EQ(1u, CountAgentsPvtuFiles(viz_dir));

  sim.GetScheduler()->Simulate(1);  // -> 3
  adaptor.Visualize();              // 3 % 2 != 0 -> no write
  EXPECT_EQ(1u, CountAgentsPvtuFiles(viz_dir));

  sim.GetScheduler()->Simulate(1);  // -> 4
  adaptor.Visualize();              // 4 % 2 == 0 -> writes
  EXPECT_EQ(2u, CountAgentsPvtuFiles(viz_dir));
}

}  // namespace bdm

#endif  // USE_STANDALONE_VISUALIZATION
