/*
 *
 *    Copyright (c) 2015-2018,2020,2022,2024
 *      SMASH Team
 *
 *    GNU General Public License (GPLv3 or later)
 *
 */

#include "vir/test.h"  // This include has to be first

#include "smash/pauliblocking.h"

#include <filesystem>

#include "setup.h"
#include "smash/boxmodus.h"
#include "smash/configuration.h"
#include "smash/experiment.h"
#include "smash/nucleus.h"
#include "smash/potentials.h"

using namespace smash;

TEST(init_particle_types) {
  ParticleType::create_type_list(
      "# NAME MASS[GEV] WIDTH[GEV] PARITY PDG\n"
      "N+ 0.938 0.0 + 2212\n"
      "N0 0.938 0.0 + 2112\n");
}

static Configuration get_pauli_blocking_conf() {
  return Configuration{R"(
    Collision_Term:
      Pauli_Blocking:
        Spatial_Averaging_Radius: 1.86
        Momentum_Averaging_Radius: 0.08
        Gaussian_Cutoff: 2.2
  )"};
}

/* Checks if phase space density gives correct result
   for a particular simple case: one particle in the phase-space sphere.
*/
TEST(phase_space_density) {
  ExperimentParameters param = smash::Test::default_parameters();
  std::unique_ptr<PauliBlocker> pb =
      std::make_unique<PauliBlocker>(get_pauli_blocking_conf(), param);
  std::vector<Particles> part(1);
  PdgCode pdg = 0x2112;
  ParticleData one_particle{ParticleType::find(pdg)};
  one_particle.set_4position(FourVector(0.0, 0.0, 0.0, 0.0));
  one_particle.set_4momentum(0.0, 0.0, 0.0, 0.0);
  part[0].insert(one_particle);
  COMPARE(part[0].size(), 1u);
  ThreeVector r(1.218, 0.0, 0.0), p(0.0, 0.0, 0.0);
  ParticleList disregard;
  const double f = pb->phasespace_dens(r, p, part, pdg, disregard);
  const double f_expected = 9.93318;
  COMPARE_RELATIVE_ERROR(f, f_expected, 1.e-3) << f << " ?= " << f_expected;
}

/*TEST(phase_space_density_box) {
  Configuration conf(TEST_CONFIG_PATH);
  conf["Modi"]["Box"]["Initial_Condition"] = 1;
  conf["Modi"]["Box"]["Length"] = 5.0;
  conf["Modi"]["Box"]["Temperature"] = 1.0;
  conf["Modi"]["Box"]["Start_Time"] = 0.0;
  conf.take({"Modi", "Box", "Init_Multiplicities"});
  conf["Modi"]["Box"]["Init_Multiplicities"]["2114"] = 10000;
  conf["Collision_Term"]["Pauli_Blocking"]["Spatial_Averaging_Radius"] = 1.86;
  conf["Collision_Term"]["Pauli_Blocking"]["Momentum_Averaging_Radius"] = 0.32;
  conf["Collision_Term"]["Pauli_Blocking"]["Gaussian_Cutoff"] = 2.2;
  // Number of test-particles
  const size_t ntest = 10;
  ExperimentParameters param = smash::Test::default_parameters(ntest);
  PauliBlocker *pb = new PauliBlocker(conf["Collision_Term"]["Pauli_Blocking"],
param);
  BoxModus *b = new BoxModus(conf["Modi"], param);
  Particles Pbox;
  b->initial_conditions(&Pbox, param);
  ThreeVector r(0.0, 0.0, 0.0);
  PdgCode pdg = 0x2114;
  ParticleList disregard;
  for (int i = 1; i < 100; i++) {
    const ThreeVector p = ThreeVector(0.0, 0.0, 5.0/100*i);
    const double f = pb->phasespace_dens(r, p, Pbox, pdg, disregard);
   // std::cout << 5.0/100*i << "  " << f << std::endl;
  }
}*/

TEST(phase_space_density_nucleus) {
  // Gold nuclei with 1000 test-particles
  std::map<PdgCode, int> list = {{0x2212, 79}, {0x2112, 118}};
  int Ntest = 100;
  Nucleus Au(list, Ntest);
  Au.set_parameters_automatic();
  Au.arrange_nucleons();
  Au.generate_fermi_momenta();

  std::vector<Particles> part_Au(1);
  Au.copy_particles(&part_Au[0]);

  ExperimentParameters param = smash::Test::default_parameters(Ntest);
  std::unique_ptr<PauliBlocker> pb =
      std::make_unique<PauliBlocker>(get_pauli_blocking_conf(), param);

  ThreeVector r(0.0, 0.0, 0.0);
  ThreeVector p;
  PdgCode pdg = 0x2212;
  ParticleList disregard;
  for (int i = 1; i < 100; i++) {
    p = ThreeVector(0.0, 0.0, 0.5 / 100 * i);
    const double f = pb->phasespace_dens(r, p, part_Au, pdg, disregard);
    std::cout << 0.5 / 100 * i << "  " << f << std::endl;
  }
}
