#include "core/visualization/standalone/adaptor.h"
#include "core/simulation.h"
#include <filesystem>
#include "core/visualization/standalone/standalone_vtu_exporter.h"

namespace bdm {

StandaloneAdaptor::StandaloneAdaptor() = default;
StandaloneAdaptor::~StandaloneAdaptor() {
  delete exporter_;
}

void StandaloneAdaptor::Visualize() {
  auto* sim = Simulation::GetActive();
  if (!initialized_) {
    std::string out_dir = sim->GetOutputDir() + "/viz";
    std::filesystem::create_directories(out_dir);
    exporter_ = new StandaloneVtuExporter(out_dir);
    initialized_ = true;
  }
  exporter_->WriteStep();
}

}  // namespace bdm
