#ifndef BDM_SRC_CORE_VISUALIZATION_STANDALONE_STANDALONE_VTU_EXPORTER_H_
#define BDM_SRC_CORE_VISUALIZATION_STANDALONE_STANDALONE_VTU_EXPORTER_H_

#include <string>
#include <vector>

namespace bdm {

class StandaloneVtuExporter {
 public:
  explicit StandaloneVtuExporter(const std::string& output_dir);
  ~StandaloneVtuExporter();

  void WriteStep();
  void WriteDiffusionStep();

 private:
  std::string output_dir_;
  int step_ = 0;

  void WritePvtu(int pieces) const;
  void WriteDiffusionPvtu(const std::string& name,
                          bool has_concentration,
                          bool has_gradient,
                          int pieces = 1) const;
};

}  // namespace bdm

#endif  // BDM_SRC_CORE_VISUALIZATION_STANDALONE_STANDALONE_VTU_EXPORTER_H_
