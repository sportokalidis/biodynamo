#include "core/visualization/standalone/adaptor.h"
#include "core/simulation.h"
#include "core/scheduler.h"
#include "core/param/param.h"
#include <filesystem>
#include "core/visualization/standalone/standalone_vtu_exporter.h"

namespace bdm {

StandaloneAdaptor* StandaloneAdaptor::Factory() { return new StandaloneAdaptor(); }

StandaloneAdaptor::StandaloneAdaptor() = default;
StandaloneAdaptor::~StandaloneAdaptor() {
  delete exporter_;
}

void StandaloneAdaptor::Visualize() {
  auto* sim = Simulation::GetActive();
  auto* param = sim->GetParam();
  if (!initialized_) {
    std::string out_dir = sim->GetOutputDir() + "/viz";
    std::filesystem::create_directories(out_dir);
    exporter_ = new StandaloneVtuExporter(out_dir);
    initialized_ = true;
  }
  // Respect export flag and visualization interval
  uint64_t total_steps = sim->GetScheduler()->GetSimulatedSteps();
  if (param->export_visualization &&
      (total_steps % param->visualization_interval == 0)) {
    exporter_->WriteStep();
    exporter_->WriteDiffusionStep();
  }
}

}  // namespace bdm
