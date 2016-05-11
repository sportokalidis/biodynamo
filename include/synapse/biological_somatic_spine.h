#ifndef SYNAPSE_BIOLOGICAL_SOMATIC_SPINE_H_
#define SYNAPSE_BIOLOGICAL_SOMATIC_SPINE_H_

#include <memory>
#include <exception>
#include <string>

#include "sim_state_serializable.h"

namespace cx3d {
namespace synapse {

class PhysicalSomaticSpine;

class BiologicalSomaticSpine : public SimStateSerializable {
 public:
  using UPtr = std::unique_ptr<BiologicalSomaticSpine>;

  BiologicalSomaticSpine();

  virtual ~BiologicalSomaticSpine();

  virtual StringBuilder& simStateToJson(StringBuilder& sb) const override;

  virtual std::shared_ptr<PhysicalSomaticSpine> getPhysicalSomaticSpine() const;

  virtual void setPhysicalSomaticSpine(const std::shared_ptr<PhysicalSomaticSpine>& ps);

 private:
  BiologicalSomaticSpine(const BiologicalSomaticSpine&) = delete;
  BiologicalSomaticSpine& operator=(const BiologicalSomaticSpine&) = delete;

  std::shared_ptr<PhysicalSomaticSpine> physical_somatic_spine_;
};

}  // namespace synapse
}  // namespace cx3d

#endif  // SYNAPSE_BIOLOGICAL_SOMATIC_SPINE_H_
