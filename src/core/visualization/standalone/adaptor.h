// Minimal standalone visualization adaptor implementing VisualizationAdaptor
#ifndef BDM_SRC_CORE_VISUALIZATION_STANDALONE_ADAPTOR_H_
#define BDM_SRC_CORE_VISUALIZATION_STANDALONE_ADAPTOR_H_

#include "core/util/root.h"
#include "core/visualization/visualization_adaptor.h"

namespace bdm {

class StandaloneVtuExporter;

class StandaloneAdaptor : public VisualizationAdaptor {
 public:
  StandaloneAdaptor();
  ~StandaloneAdaptor() override;

  void Visualize() override;

 private:
  bool initialized_ = false;
  StandaloneVtuExporter* exporter_ = nullptr;

  BDM_CLASS_DEF_NV(StandaloneAdaptor, 1);
};

}  // namespace bdm

#endif  // BDM_SRC_CORE_VISUALIZATION_STANDALONE_ADAPTOR_H_
