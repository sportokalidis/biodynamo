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

#ifndef CORE_BEHAVIOR_REGULATORY_NETWORK_H_
#define CORE_BEHAVIOR_REGULATORY_NETWORK_H_

#include <functional>
#include <vector>

#include "core/behavior/behavior.h"

#include "boost/numeric/odeint.hpp"
#include "boost/phoenix/core.hpp"
#include "boost/phoenix/operator.hpp"

typedef boost::numeric::ublas::vector<double> boost_vector_t;
typedef boost::numeric::ublas::matrix<double> boost_matrix_t;

namespace bdm {

/// Enum for selecting the ODE solver method used by RegulatoryNetwork.
enum class ODE_solver {
  Rosenbrock,  ///< Rosenbrock implicit solver (requires Jacobian)
  RungeKutta   ///< Runge-Kutta-Dormand-Prince 5th order explicit solver
};

/// This behavior integrates systems of ODEs using Boost.Odeint, providing
/// both implicit (Rosenbrock) and explicit (Runge-Kutta-Dormand-Prince)
/// solvers. It is suited for stiff and non-stiff regulatory network models
/// where multiple species interact through differential equations.
///
/// Unlike GeneRegulation (which uses simple Euler/RK4 on scalar ODEs),
/// RegulatoryNetwork accepts a full system RHS function, an optional
/// Jacobian, and an observer, operating on boost::numeric::ublas vectors.
///
/// Usage example:
/// \code
///   auto rhs = [](const boost_vector_t& x, boost_vector_t& dxdt, real_t t) {
///     dxdt[0] = -0.1 * x[0];
///     dxdt[1] =  0.1 * x[0] - 0.05 * x[1];
///   };
///   auto jacob = [](const boost_vector_t& x, boost_matrix_t& J,
///                    real_t t, boost_vector_t& dfdt) {
///     J(0,0) = -0.1; J(0,1) = 0.0;
///     J(1,0) =  0.1; J(1,1) = -0.05;
///     dfdt[0] = 0.0; dfdt[1] = 0.0;
///   };
///   auto obs = [](const boost_vector_t& x, real_t t) { /* observer */ };
///
///   auto* rn = new RegulatoryNetwork(0.01, {1.0, 0.0},
///                                    ODE_solver::Rosenbrock,
///                                    rhs, jacob, obs);
///   cell->AddBehavior(rn);
/// \endcode
class RegulatoryNetwork : public Behavior {
  BDM_BEHAVIOR_HEADER(RegulatoryNetwork, Behavior, 1);

 public:
  RegulatoryNetwork() { AlwaysCopyToNew(); }

  RegulatoryNetwork(
      real_t dt, const std::vector<real_t>& x, ODE_solver m,
      const std::function<void(const boost_vector_t&, boost_vector_t&, real_t)>&
          rhs,
      const std::function<void(const boost_vector_t&, boost_matrix_t&, real_t,
                                boost_vector_t&)>& jacob,
      const std::function<void(const boost_vector_t&, real_t)>& out) {
    AlwaysCopyToNew();
    SetInitialSpecies(x);
    time_step_ = dt;
    rhs_ = rhs;
    jacob_ = jacob;
    out_ = out;
    method_ = m;
  }

  virtual ~RegulatoryNetwork() = default;

  void Initialize(const NewAgentEvent& event) override {
    Base::Initialize(event);

    if (auto* r =
            dynamic_cast<RegulatoryNetwork*>(event.existing_behavior)) {
      current_time_ = r->current_time_;
      time_step_ = r->time_step_;
      current_species_ = r->current_species_;
      previous_species_ = r->previous_species_;
      rhs_ = r->rhs_;
      jacob_ = r->jacob_;
      out_ = r->out_;
      method_ = r->method_;
    } else {
      Log::Fatal("RegulatoryNetwork::EventConstructor",
                 "other was not of type RegulatoryNetwork");
    }
  }

  const size_t GetNumberOfSpecies() const { return current_species_.size(); }
  const boost_vector_t& GetSpecies() const { return current_species_; }
  const real_t& GetSpecie(size_t i) const { return current_species_[i]; }

  /// Performs one time step of ODE integration using the selected solver.
  void Run(Agent* agent) override {
    // update the previous solution
    previous_species_ = current_species_;
    // initialize the time-integration scheme
    if (ODE_solver::Rosenbrock == method_) {
      auto stepper = boost::numeric::odeint::make_dense_output<
          boost::numeric::odeint::rosenbrock4<double>>(1.e-6, 1.e-6);
      // perform time integration
      integrate_const(stepper, std::make_pair(rhs_, jacob_), current_species_,
                      current_time_, current_time_ + time_step_,
                      time_step_ / 1000, out_);
    } else if (ODE_solver::RungeKutta == method_) {
      auto stepper = boost::numeric::odeint::make_dense_output<
          boost::numeric::odeint::runge_kutta_dopri5<boost_vector_t>>(1.e-6,
                                                                      1.e-6);
      // perform time integration
      integrate_const(stepper, rhs_, current_species_, current_time_,
                      current_time_ + time_step_, time_step_ / 1000, out_);
    } else {
      Log::Fatal("RegulatoryNetwork::Run",
                 "invalid type of ODE solution method indicated");
    }
    // update the time of the regulatory network
    current_time_ += time_step_;
  }

 protected:
  void SetInitialSpecies(const std::vector<real_t>& x) {
    const size_t n_species = x.size();
    current_species_.resize(n_species);
    previous_species_.resize(n_species);
    for (size_t i = 0; i < n_species; i++) {
      current_species_[i] = previous_species_[i] = x[i];
    }
  }

 private:
  /// Pseudo-time for ODE(s) time integration
  real_t current_time_ = 0.0;
  /// Time-step for ODE(s) time integration
  real_t time_step_ = 1.0;
  /// Current solution of the species concentration
  boost_vector_t current_species_ = {};
  /// Previous solution of the species concentration
  boost_vector_t previous_species_ = {};
  /// Method used for the ODE(s) numerical solution
  ODE_solver method_;

  std::function<void(const boost_vector_t&, boost_vector_t&, real_t)> rhs_;
  std::function<void(const boost_vector_t&, boost_matrix_t&, real_t,
                      boost_vector_t&)>
      jacob_;
  std::function<void(const boost_vector_t&, real_t)> out_;
};

}  // namespace bdm

#endif  // CORE_BEHAVIOR_REGULATORY_NETWORK_H_
