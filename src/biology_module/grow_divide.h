#ifndef BIOLOGY_MODULE_GROW_DIVIDE_H_
#define BIOLOGY_MODULE_GROW_DIVIDE_H_

#include <Rtypes.h>
#include "biology_module_util.h"

namespace bdm {

/// This biology module grows the simulation object until the diameter reaches
/// the specified threshold and divides the object afterwards.
struct GrowDivide {
  GrowDivide() {}
  GrowDivide(double threshold, double growth_rate)
      : threshold_(threshold), growth_rate_(growth_rate) {}

  template <typename T>
  void Run(T* cell) {
    if (cell->GetDiameter() <= threshold_) {
      cell->ChangeVolume(growth_rate_);
    } else {
      Divide(*cell);
    }
  }

  bool IsCopied(Event event) const { return true; }

 private:
  double threshold_ = 40;
  double growth_rate_ = 300;
  ClassDefNV(GrowDivide, 1);
};

}  // namespace bdm

#endif  // BIOLOGY_MODULE_GROW_DIVIDE_H_
