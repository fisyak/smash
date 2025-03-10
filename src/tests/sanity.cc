/*
 *
 *    Copyright (c) 2014-2015,2017-2020,2022-2024
 *      SMASH Team
 *
 *    GNU General Public License (GPLv3 or later)
 *
 */

#include "vir/test.h"  // This include has to be first

#include <filesystem>

#include "setup.h"
#include "smash/boxmodus.h"
#include "smash/collidermodus.h"
#include "smash/configuration.h"
#include "smash/experimentparameters.h"
#include "smash/modusdefault.h"
#include "smash/spheremodus.h"

using namespace smash;

TEST(init_particle_types) { Test::create_smashon_particletypes(); }

static ParticleData create_smashon_particle(int id = -1) {
  return ParticleData{ParticleType::find(0x661), id};
}

// create a particle list with various interesting particles. We will
// assume a box of 5 fm length and a time step (for propagation) of 1
// fm.
static void create_particle_list(Particles &P) {
  // particle that doesn't move:
  ParticleData particle_stop = create_smashon_particle();
  // particle that moves with speed of light
  ParticleData particle_fast = create_smashon_particle();
  // particle that moves slowly:
  ParticleData particle_slow = create_smashon_particle();
  // particle that will cross a box boundary at high x:
  ParticleData particle_x_hi = create_smashon_particle();
  // particle that will cross a box boundary at low y:
  ParticleData particle_y_lo = create_smashon_particle();
  // particle that will cross a box boundary at low x and high z:
  ParticleData particle_xlzh = create_smashon_particle();

  // set momenta:
  particle_stop.set_4momentum(FourVector(4.0, 0.0, 0.0, 0.0));
  particle_fast.set_4momentum(FourVector(sqrt(0.02), 0.1, -.1, 0.0));
  particle_slow.set_4momentum(FourVector(sqrt(1.13), 0.1, 0.2, -.3));
  particle_x_hi.set_4momentum(FourVector(0.1, 0.1, 0.0, 0.0));
  particle_y_lo.set_4momentum(FourVector(0.1, 0.0, -.1, 0.0));
  particle_xlzh.set_4momentum(FourVector(0.5, -.3, 0.0, 0.4));

  // set positions:
  particle_stop.set_4position(FourVector(0.0, 5.6, 0.7, 0.8));
  particle_fast.set_4position(FourVector(0.5, -.7, 0.8, 8.9));
  particle_slow.set_4position(FourVector(0.7, 0.1, 0.2, 0.3));
  particle_x_hi.set_4position(FourVector(1.2, 4.5, 5.0, 0.0));
  particle_y_lo.set_4position(FourVector(1.8, 0.0, 19., 0.0));
  particle_xlzh.set_4position(FourVector(2.2, 0.2, 0.0, 4.8));

  // add particles (and make sure the particles get the correct ID):
  P.insert(particle_stop);
  COMPARE(P.back().id(), 0);
  P.insert(particle_fast);
  COMPARE(P.back().id(), 1);
  P.insert(particle_slow);
  COMPARE(P.back().id(), 2);
  P.insert(particle_x_hi);
  COMPARE(P.back().id(), 3);
  P.insert(particle_y_lo);
  COMPARE(P.back().id(), 4);
  P.insert(particle_xlzh);
  COMPARE(P.back().id(), 5);

  return;
}

TEST(sanity_default) {
  ModusDefault m;
  Particles P;
  create_particle_list(P);
  COMPARE(m.impose_boundary_conditions(&P), 0);
}

TEST(sanity_box) {
  Configuration conf{R"(
    Modi:
      Box:
        Initial_Condition: "peaked momenta"
        Length:  5.0
        Temperature:  0.13
        Start_Time:  0.2
        Init_Multiplicities:
          2212: 50
          2112: 50
          211:  100
          111:  100
          -211: 100
  )"};
  ExperimentParameters param = smash::Test::default_parameters();
  param.box_length = 5.0;
  BoxModus b(std::move(conf), param);
  Particles P;
  create_particle_list(P);
  COMPARE(b.impose_boundary_conditions(&P), 4);
}

TEST(sanity_collider) {
  Configuration conf{R"(
    Modi:
      Collider:
        Projectile:
            Particles: {661: 1}
        Target:
            Particles: {661: 1}
        E_Kin: 1.0
  )"};
  ExperimentParameters param = smash::Test::default_parameters();
  ColliderModus n(std::move(conf), param);
  Particles P;
  create_particle_list(P);
  COMPARE(n.impose_boundary_conditions(&P), 0);
}

TEST(sanity_sphere) {
  Configuration conf{R"(
    Modi:
      Sphere:
        Radius: 10
        Temperature: 0.2
        Start_Time: 0.0
        Init_Multiplicities: {661: 500}
  )"};
  ExperimentParameters param = smash::Test::default_parameters();
  SphereModus s(std::move(conf), param);
  Particles P;
  create_particle_list(P);
  COMPARE(s.impose_boundary_conditions(&P), 0);
}
