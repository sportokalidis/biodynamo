#ifndef CELL_H_
#define CELL_H_

#include <array>
#include <cmath>
#include <type_traits>

#include <Rtypes.h>

#include "backend.h"
#include "cell.h"
#include "default_force.h"
#include "inline_vector.h"
#include "math_util.h"
#include "matrix.h"
#include "param.h"
#include "simulation_object.h"
#include "simulation_object_util.h"

namespace bdm {

using std::array;

template <typename Base = SimulationObject<>>
class CellExt : public Base {
  BDM_CLASS_HEADER(CellExt, position_, mass_location_, tractor_force_,
                   diameter_, volume_, adherence_, density_, x_axis_, y_axis_,
                   z_axis_, neighbors_);

  // Extracted ClassDef for the reason addressed in ROOT-8784
  // clang-format off
 private:
  static atomic_TClass_ptr fgIsA;  // NOLINT

 public:
  static TClass* Class() { throw "This method should not be called."
  " It should rather have been overriden by the ROOT dictionary"; }
  static const char* Class_Name();  // NOLINT
  static Version_t Class_Version() { return 1; }  // NOLINT
  static TClass* Dictionary();
  virtual TClass* IsA() const { return CellExt::Class(); }  // NOLINT
  virtual void ShowMembers(TMemberInspector& insp) const {  // NOLINT
    ::ROOT::Class_ShowMembers(CellExt::Class(), this, insp); }
  virtual void Streamer(TBuffer&) { throw "This method should not be called."
  " It should rather have been overriden by the ROOT dictionary"; }

  void StreamerNVirtual(TBuffer& ClassDef_StreamerNVirtual_b) {  // NOLINT
    CellExt::Streamer(ClassDef_StreamerNVirtual_b);
  }
  static const char* DeclFileName() { return __FILE__; }
  static int ImplFileLine();
  static const char* ImplFileName();
  static int DeclFileLine() { return __LINE__; }
  // clang-format on

 public:
  CellExt() {}
  explicit CellExt(TRootIOCtor* io_ctor) {}  // constructor for ROOT I/O
  explicit CellExt(double diameter) : diameter_(diameter) { UpdateVolume(); }
  explicit CellExt(const array<double, 3>& position)
      : position_(position), mass_location_(position) {}

  virtual ~CellExt() {}

  /// Divide the cell. Of the two daughter cells, one is this one (but smaller,
  /// with half GeneSubstances etc.),
  /// and the other one is given as parameter and initialzed accordingly. Both
  /// cells have more or less the same volume,
  /// the axis of division is random.
  /// @param daughter - second daughter cell = scalar instance which will be
  /// initialized in this method
  void Divide(Self<Scalar>* daughter);

  /// Divide the cell. Of the two daughter cells, one is this one (but smaller,
  /// with half GeneSubstances etc.),
  /// and the other one is given as parameter and initialzed accordingly. The
  /// axis of division is random.
  /// @param daughter second daughter cell = scalar instance which will be
  /// initialized in this method
  /// @param volume_ratio the ratio (Volume daughter 1)/(Volume daughter 2). 1.0
  /// gives equal cells.
  void Divide(Self<Scalar>* daughter, double volume_ratio);

  /// Divide the cell. Of the two daughter cells, one is this one (but smaller,
  /// with half GeneSubstances etc.),
  /// and the other one is is given as parameter and initialzed accordingly.
  /// @param daughter second daughter cell = scalar instance which will be
  /// initialized in this method
  /// @param axis specifies direction of division
  void Divide(Self<Scalar>* daughter, const array<double, 3>& axis);

  /// Divide the cell. Of the two daughter cells, one is this one (but smaller,
  /// with half GeneSubstances etc.),
  /// and the other one is instantiated de novo and is returned.
  /// @param daughter second daughter cell = scalar instance which will be
  /// initialized in this method
  /// @param volume_ratio the ratio (Volume daughter 1)/(Volume daughter 2). 1.0
  /// gives equal cells.
  /// @param axis specifies direction of division
  void Divide(Self<Scalar>* daughter, double volume_ratio,
              const array<double, 3>& axis);

  /// Forwards call to DivideImpl @see DivideImpl
  void Divide(Self<Scalar>* daughter, double volume_ratio, double phi,
              double theta);

  /// Divide mother cell in two daughter cells\n
  /// When mother cell divides, by definition:\n
  /// 1) the mother cell becomes the 1st daughter cell\n
  /// 2) the new cell becomes the 2nd daughter cell and inherits a equal or
  /// bigger volume than the 1st
  ///    daughter cell, which means that this cell will eventually inherit more
  ///    differentiating factors
  ///    and will be recorded in the left side of the lineage tree.
  ///
  /// @param daughter second daughter cell = scalar instance which will be
  /// initialized in this method
  /// @param volume_ratio the ratio (Volume daughter 1)/(Volume daughter 2). 1.0
  /// gives equal cells.
  /// @param phi azimuthal angle (polar coordinate)
  /// @param theta polar angle (polar coordinate)
  /// @see \link simulation_object_util.h Divide \endlink
  virtual void DivideImpl(Self<Scalar>* daughter, double volume_ratio,
                          double phi, double theta);

  double GetAdherence() const { return adherence_[kIdx]; }

  double GetDiameter() const { return diameter_[kIdx]; }

  double GetMass() const { return density_[kIdx] * volume_[kIdx]; }

  double GetDensity() const { return density_[kIdx]; }

  const array<double, 3>& GetMassLocation() const {
    return mass_location_[kIdx];
  }

  const array<double, 3>& GetPosition() const { return position_[kIdx]; }

  const array<double, 3>& GetTractorForce() const {
    return tractor_force_[kIdx];
  }

  double GetVolume() const { return volume_[kIdx]; }

  const InlineVector<int, 8>& GetNeighbors() const { return neighbors_[kIdx]; }

  void SetAdherence(double adherence) { adherence_[kIdx] = adherence; }

  void SetDiameter(double diameter) {
    diameter_[kIdx] = diameter;
    UpdateVolume();
  }

  void SetMass(double mass) { density_[kIdx] = mass / volume_[kIdx]; }

  void SetDensity(double density) { density_[kIdx] = density; }

  void SetMassLocation(const array<double, 3>& mass_location) {
    mass_location_[kIdx] = mass_location;
  }

  void SetPosition(const array<double, 3>& position) {
    position_[kIdx] = position;
  }

  void SetTractorForce(const array<double, 3>& tractor_force) {
    tractor_force_[kIdx] = tractor_force;
  }

  void SetNeighbors(const InlineVector<int, 8>& neighbors) {
    neighbors_[kIdx] = neighbors;
  }

  void ChangeVolume(double speed) {
    // scaling for integration step
    double delta = speed * Param::kSimulationTimeStep;
    volume_[kIdx] += delta;
    if (volume_[kIdx] < 5.2359877E-7) {
      volume_[kIdx] = 5.2359877E-7;
    }
    UpdateDiameter();
  }

  void UpdateDiameter() {
    // V = (4/3)*pi*r^3 = (pi/6)*diameter^3
    diameter_[kIdx] = std::cbrt(volume_[kIdx] * 6 / Math::kPi);
  }

  void UpdateVolume() {
    // V = (4/3)*pi*r^3 = (pi/6)*diameter^3
    volume_[kIdx] = Math::kPi / 6 * std::pow(diameter_[kIdx], 3);
  }

  void UpdateMassLocation(const array<double, 3>& delta) {
    mass_location_[kIdx][0] += delta[0];
    mass_location_[kIdx][1] += delta[1];
    mass_location_[kIdx][2] += delta[2];
  }

  void GetForceOn(const array<double, 3>& ref_mass_location,
                  double ref_diameter, array<double, 3>* force) const {
    DefaultForce default_force;
    double iof_coefficient = Param::kSphereDefaultInterObjectCoefficient;

    default_force.ForceBetweenSpheres(ref_mass_location, ref_diameter,
                                      iof_coefficient, mass_location_[kIdx],
                                      diameter_[kIdx], iof_coefficient, force);
  }

 protected:
  /// Returns the position in the polar coordinate system (cylindrical or
  /// spherical) of a point expressed in global cartesian coordinates
  /// ([1,0,0],[0,1,0],[0,0,1]).
  /// @param coord: position in absolute coordinates - [x,y,z] cartesian values
  /// @return the position in local coordinates
  array<double, 3> TransformCoordinatesGlobalToPolar(
      const array<double, 3>& coord) const;

  vec<array<double, 3>> position_;
  vec<array<double, 3>> mass_location_;
  vec<array<double, 3>> tractor_force_;
  vec<double> diameter_;
  vec<double> volume_;
  vec<double> adherence_;
  vec<double> density_;

  /// First axis of the local coordinate system.
  vec<array<double, 3>> x_axis_ = {array<double, 3>{1.0, 0.0, 0.0}};
  /// Second axis of the local coordinate system.
  vec<array<double, 3>> y_axis_ = {array<double, 3>{0.0, 1.0, 0.0}};
  /// Third axis of the local coordinate system.
  vec<array<double, 3>> z_axis_ = {array<double, 3>{0.0, 0.0, 1.0}};

  // stores a list of neighbor ids for each scalar cell
  vec<InlineVector<int, 8>> neighbors_;
};

template <typename Backend = Scalar>
using Cell = CellExt<SimulationObject<Backend>>;

template <typename T>
inline void CellExt<T>::Divide(Self<Scalar>* daughter) {
  Divide(daughter, 0.9 + 0.2 * gRandom.NextDouble());
}

template <typename T>
inline void CellExt<T>::Divide(Self<Scalar>* daughter, double volume_ratio) {
  // find random point on sphere (based on :
  // http://mathworld.wolfram.com/SpherePointPicking.html)
  double theta = 2 * Math::kPi * gRandom.NextDouble();
  double phi = std::acos(2 * gRandom.NextDouble() - 1);
  DivideImpl(daughter, volume_ratio, phi, theta);
}

template <typename T>
inline void CellExt<T>::Divide(Self<Scalar>* daughter,
                               const array<double, 3>& axis) {
  auto polarcoord = TransformCoordinatesGlobalToPolar(
      Matrix::Add(axis, mass_location_[kIdx]));
  DivideImpl(daughter, 0.9 + 0.2 * gRandom.NextDouble(), polarcoord[1],
             polarcoord[2]);
}

template <typename T>
inline void CellExt<T>::Divide(Self<Scalar>* daughter, double volume_ratio,
                               const array<double, 3>& axis) {
  auto polarcoord = TransformCoordinatesGlobalToPolar(
      Matrix::Add(axis, mass_location_[kIdx]));
  DivideImpl(daughter, volume_ratio, polarcoord[1], polarcoord[2]);
}

template <typename T>
inline void CellExt<T>::Divide(Self<Scalar>* daughter, double volume_ratio,
                               double phi, double theta) {
  DivideImpl(daughter, volume_ratio, phi, theta);
}

template <typename T>
inline void CellExt<T>::DivideImpl(Self<Scalar>* daughter, double volume_ratio,
                                   double phi, double theta) {
  // A) Defining some values
  // ..................................................................
  // defining the two radii s.t total volume is conserved
  // * radius^3 = r1^3 + r2^3 ;
  // * volume_ratio = r2^3 / r1^3
  double radius = diameter_[kIdx] * 0.5;
  double r1 = radius / std::pow(1.0 + volume_ratio, 1.0 / 3.0);
  double r2 = radius / std::pow(1.0 + 1 / volume_ratio, 1.0 / 3.0);

  // define an axis for division (along which the nuclei will move)
  double x_coord = std::cos(theta) * std::sin(phi);
  double y_coord = std::sin(theta) * std::sin(phi);
  double z_coord = std::cos(phi);
  double total_length_of_displacement = radius / 4.0;
  array<double, 3> axis_of_division{
      total_length_of_displacement *
          (x_coord * x_axis_[kIdx][0] + y_coord * y_axis_[kIdx][0] +
           z_coord * z_axis_[kIdx][0]),
      total_length_of_displacement *
          (x_coord * x_axis_[kIdx][1] + y_coord * y_axis_[kIdx][1] +
           z_coord * z_axis_[kIdx][1]),
      total_length_of_displacement *
          (x_coord * x_axis_[kIdx][2] + y_coord * y_axis_[kIdx][2] +
           z_coord * z_axis_[kIdx][2])};

  // two equations for the center displacement :
  //  1) d2/d1= v2/v1 = volume_ratio (each sphere is shifted inver.
  //  proportionally to its volume)
  //  2) d1 + d2 = TOTAL_LENGTH_OF_DISPLACEMENT
  double d_2 = total_length_of_displacement / (volume_ratio + 1);
  double d_1 = total_length_of_displacement - d_2;

  // B) Instantiating a new sphere = 2nd daughter
  daughter->x_axis_[0] = x_axis_[kIdx];
  daughter->y_axis_[0] = y_axis_[kIdx];
  daughter->z_axis_[0] = z_axis_[kIdx];
  daughter->adherence_[0] = adherence_[kIdx];
  daughter->density_[0] = density_[kIdx];

  daughter->diameter_[0] = r2 * 2;
  daughter->UpdateVolume();

  // Mass Location
  array<double, 3> new_mass_location{
      mass_location_[kIdx][0] + d_2 * axis_of_division[0],
      mass_location_[kIdx][1] + d_2 * axis_of_division[1],
      mass_location_[kIdx][2] + d_2 * axis_of_division[2]};
  daughter->mass_location_[0] = new_mass_location;
  daughter->position_[kIdx][0] = daughter->mass_location_[kIdx][0];
  daughter->position_[kIdx][1] = daughter->mass_location_[kIdx][1];
  daughter->position_[kIdx][2] = daughter->mass_location_[kIdx][2];

  // E) This sphere becomes the 1st daughter
  // move these cells on opposite direction
  position_[kIdx][0] -= d_1 * axis_of_division[0];
  position_[kIdx][1] -= d_1 * axis_of_division[1];
  position_[kIdx][2] -= d_1 * axis_of_division[2];
  mass_location_[kIdx][0] = position_[kIdx][0];
  mass_location_[kIdx][1] = position_[kIdx][1];
  mass_location_[kIdx][2] = position_[kIdx][2];

  // F) change properties of this cell
  diameter_[kIdx] = r1 * 2;
  UpdateVolume();

  // G) TODO(lukas) Copy the intracellular and membrane bound Substances
}

template <typename T>
array<double, 3> CellExt<T>::TransformCoordinatesGlobalToPolar(
    const array<double, 3>& pos) const {
  auto vector_to_point = Matrix::Subtract(pos, mass_location_[kIdx]);
  array<double, 3> local_cartesian{Matrix::Dot(x_axis_[kIdx], vector_to_point),
                                   Matrix::Dot(y_axis_[kIdx], vector_to_point),
                                   Matrix::Dot(z_axis_[kIdx], vector_to_point)};
  return {std::sqrt(local_cartesian[0] * local_cartesian[0] +
                    local_cartesian[1] * local_cartesian[1] +
                    local_cartesian[2] * local_cartesian[2]),
          std::atan2(std::sqrt(local_cartesian[0] * local_cartesian[0] +
                               local_cartesian[1] * local_cartesian[1]),
                     local_cartesian[2]),
          std::atan2(local_cartesian[1], local_cartesian[0])};
}

}  // namespace bdm

#endif  // CELL_H_
