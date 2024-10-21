/*
 *
 *    Copyright (c) 2023
 *      SMASH Team
 *
 *    GNU General Public License (GPLv3 or later)
 *
 */

#ifndef SRC_INCLUDE_SMASH_DYNAMICFLUIDFINDER_H_
#define SRC_INCLUDE_SMASH_DYNAMICFLUIDFINDER_H_

#include <map>
#include <vector>

#include "actionfinderfactory.h"
#include "icparameters.h"
#include "input_keys.h"

namespace smash {

/**
 * \ingroup action
 * Finder for dynamic fluidizations.
 * Loops through all particles and checks if they reach the energy density
 * threshold. This happens at the end of every time step for all hadrons that
 * originate in a decay or string fragmentation. For the latter process,
 * fluidization happens only after the formation time of the particle, but it
 * will happen if it passes the threshold at any point in the propagation.
 */
class DynamicFluidizationFinder : public ActionFinderInterface {
 public:
  /**
   * Construct finder for fluidization action.
   * \param[in] lattice Lattice used for the energy density interpolation
   * \param[in] background Background map between particle indices and the
   * corresponding background energy density
   * \param[in] ic_params Parameters for dynamic fluidization
   *
   * \note \c energy_density_lattice and \c energy_density_background are both
   * "in" parameters because the class stores references, but the values are non
   * constant.
   */
  DynamicFluidizationFinder(RectangularLattice<EnergyMomentumTensor> &lattice,
                            std::map<int32_t, double> &background,
                            const InitialConditionParameters &ic_params)
      : energy_density_lattice_{lattice},
        background_{background},
        energy_density_threshold_{ic_params.energy_density_threshold.value()},
        min_time_{ic_params.min_time.value()},
        max_time_{ic_params.max_time.value()},
        formation_time_fraction_{ic_params.formation_time_fraction.value()},
        fluid_cells_{ic_params.num_fluid_cells.value()},
        fluidizable_processes_{ic_params.fluidizable_processes} {};

  /**
   * Find particles to fluidize, depending on the energy density around them.
   * \param[in] search_list List of candidate particles to fluidize.
   * \param[in] dt Time step duration \unit{in fm}.
   * \param[in] gcell_vol Volume of the grid cell \unit{in fm³},
   * \param[in] beam_momentum Unused.
   * \return List of fluidization actions that will happen at the corresponding
   * formation time.
   */
  ActionList find_actions_in_cell(
      const ParticleList &search_list, double dt, double gcell_vol,
      const std::vector<FourVector> &beam_momentum) const override;

  /// Ignore the neighbor search for fluidization
  ActionList find_actions_with_neighbors(
      const ParticleList &, const ParticleList &, const double,
      const std::vector<FourVector> &) const override {
    return {};
  }

  /// Ignore the surrounding searches for fluidization
  ActionList find_actions_with_surrounding_particles(
      const ParticleList &, const Particles &, double,
      const std::vector<FourVector> &) const override {
    return {};
  }

  /// No final actions after fluidizing
  ActionList find_final_actions(const Particles &, bool) const override {
    return {};
  }

  /**
   * Determine fluidization
   * \param[in] pdata Particle to be checked for fluidization.
   * \return Whether energy density around pdata is high enough.
   */
  bool above_threshold(const ParticleData &pdata) const;

 private:
  /**
   * Lattice where energy momentum tensor is computed
   * \note It must be a reference so that it can be updated outside the class,
   * without creating a new Finder object.
   */
  RectangularLattice<EnergyMomentumTensor> &energy_density_lattice_;
  /**
   *  Background energy density at positions of particles, using the id as key
   * \note It is a reference so that it can be updated outside the class, e.g.
   * by an external manager using SMASH as a library.
   */
  std::map<int32_t, double> &background_;
  /**
   * Queue for future fluidizations, which will take place after the formation
   * time of particles. Keys are particle indices and values are absolute
   * formation time in the lab frame. \note It must be \c mutable so that
   * \c finder_actions_in_cell, overriden as a \c const method from the parent
   * class, can modify it.
   */
  mutable std::map<int32_t, double> queue_{};
  /// Minimum energy density surrounding the particle to fluidize it
  const double energy_density_threshold_ =
      InputKeys::modi_collider_initialConditions_eDenThreshold.default_value();
  /// Minimum time (in lab frame) in fm to allow fluidization
  const double min_time_ =
      InputKeys::modi_collider_initialConditions_minTime.default_value();
  /// Maximum time (in lab frame) in fm to allow fluidization
  const double max_time_ =
      InputKeys::modi_collider_initialConditions_maxTime.default_value();
  /// Fraction of formation time after which a particles can fluidize
  const double formation_time_fraction_ =
      InputKeys::modi_collider_initialConditions_formTimeFraction
          .default_value();
  /// Number of cells to interpolate the energy density
  const int fluid_cells_ =
      InputKeys::modi_collider_initialConditions_fluidCells.default_value();
  /// Processes that create a fluidizable particle
  const FluidizableProcessesBitSet fluidizable_processes_;

  bool is_process_fluidizable(const ProcessType &type) const;
};

/**
 * Build lattice of energy momentum tensor. If enough time has passed
 * (t>20\unit{fm}), the lattice grows linearly at each time step to accomodate
 * for the system expansion.
 *
 * \param[inout] energy_density_lattice Lattice where the energy momentum tensor
 * is to be computed.
 * \param[in] t Current time.
 * \param[in] ensembles Only the first Particles element is actually used.
 * \param[in] dens_par Contains parameters for density smearing.
 */
void build_fluidization_lattice(
    RectangularLattice<EnergyMomentumTensor> *energy_density_lattice, double t,
    const std::vector<Particles> &ensembles, const DensityParameters &dens_par);

}  // namespace smash

#endif  // SRC_INCLUDE_SMASH_DYNAMICFLUIDFINDER_H_
