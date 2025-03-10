/*
 *
 *    Copyright (c) 2014-2020,2022-2024
 *      SMASH Team
 *
 *    GNU General Public License (GPLv3 or later)
 *
 */

#include "vir/test.h"  // This include has to be first

#include "setup.h"
#include "smash/boxmodus.h"
#include "smash/collidermodus.h"
#include "smash/modusdefault.h"
#include "smash/spheremodus.h"

using namespace smash;

TEST(init_particle_types) {
  ParticleType::create_type_list("σ 0.4 0.0 + 661\n");
}

TEST(initialize_box) {
  einhard::Logger<> log(einhard::ALL);
  ExperimentParameters par = Test::default_parameters();
  par.box_length = 7.9615;
  Configuration config{R"(
    Modi:
      Box:
        Initial_Condition: "peaked momenta"
        Length: 7.9615
        Temperature: 0.5
        Start_Time: 0.2
        Init_Multiplicities:
          661: 724
  )"};
  BoxModus b(std::move(config), par);
  Particles P;
  // should return START_TIME and set P:
  COMPARE(b.initial_conditions(&P, par), 0.2);
  COMPARE(P.size(), 724u);
  for (const auto &pd : P) {
    COMPARE(pd.pdgcode(), 0x661);
  }
  // we might also look at other properties of Particles, like total
  // momentum and such.
  FourVector momentum(0.0, 0.0, 0.0, 0.0);
  for (auto p : P) {
    momentum += p.momentum();
    VERIFY(p.position().x1() < 7.9615);
    VERIFY(p.position().x1() >= 0.0);
    VERIFY(p.position().x2() < 7.9615);
    VERIFY(p.position().x2() >= 0.0);
    VERIFY(p.position().x3() < 7.9615);
    VERIFY(p.position().x3() >= 0.0);
  }
  COMPARE_ABSOLUTE_ERROR(momentum.x1(), 0.0, 1e-12);
  COMPARE_ABSOLUTE_ERROR(momentum.x2(), 0.0, 1e-12);
  COMPARE_ABSOLUTE_ERROR(momentum.x3(), 0.0, 1e-12);
}

TEST(initialize_collider_normal) {
  Configuration config{R"(
    Modi:
      Collider:
        Sqrtsnn: 1.6
        Projectile:
          Particles: {661: 1}
        Target:
          Particles: {661: 8}
        Initial_Distance: 0
        Impact:
          Value: 0
  )"};
  ColliderModus n(std::move(config), Test::default_parameters());
  Particles P;
  COMPARE(n.initial_conditions(&P, Test::default_parameters()), 0.);
  COMPARE(P.size(), 9u);
  for (auto p : P) {
    // velocity should be +- sqrt(3/4)
    COMPARE_RELATIVE_ERROR(p.velocity().sqr(), 0.75, 1e-6);
    // this is the mass squared
    COMPARE_RELATIVE_ERROR(p.momentum().sqr(), 0.16, 1e-6);
    COMPARE(p.position().x0(), 0.0);
    COMPARE(p.pdgcode(), PdgCode(0x661));
    COMPARE_RELATIVE_ERROR(p.momentum().x0(), 0.8, 1e-6);
    COMPARE_ABSOLUTE_ERROR(p.momentum().x1(), 0.0, 1e-6);
    COMPARE_ABSOLUTE_ERROR(p.momentum().x2(), 0.0, 1e-6);
    COMPARE_RELATIVE_ERROR(std::abs(p.momentum().x3()), std::sqrt(0.48), 1e-6);
  }
  // all other things can only be tested with statistics.
}

TEST_CATCH(initialize_collider_low_energy, ModusDefault::InvalidEnergy) {
  Configuration config{R"(
    Modi:
      Collider:
        Sqrtsnn: 0.5
        Projectile:
          Particles: {661: 1}
        Target:
          Particles: {661: 8}
        Initial_Distance: 0
  )"};
  ColliderModus n(std::move(config), Test::default_parameters());
  Particles P;
  n.initial_conditions(&P, Test::default_parameters());
}

TEST_CATCH(initialize_nucleus_empty_projectile, ColliderModus::ColliderEmpty) {
  Configuration config{R"(
    Modi:
      Collider:
        Sqrtsnn: 1.6
        Projectile:
          Particles: {661: 0}
        Target:
          Particles: {661: 8}
        Initial_Distance: 0
  )"};
  ColliderModus n(std::move(config), Test::default_parameters());
  Particles P;
  n.initial_conditions(&P, Test::default_parameters());
}

TEST_CATCH(initialize_nucleus_empty_target, ColliderModus::ColliderEmpty) {
  Configuration config{R"(
    Modi:
      Collider:
        Sqrtsnn: 1.6
        Projectile:
          Particles: {661: 8}
        Target:
          Particles: {661: 0}
        Initial_Distance: 0
  )"};
  ColliderModus n(std::move(config), Test::default_parameters());
  Particles P;
  n.initial_conditions(&P, Test::default_parameters());
}

TEST(initialize_sphere) {
  Configuration config{R"(
    Modi:
      Sphere:
        Radius: 10
        Start_Time: 0.0
        Init_Multiplicities: {661: 500}
        Temperature: 0.2
  )"};
  SphereModus s(std::move(config), Test::default_parameters());
  Particles P;
  // Is the correct number of particles in the map?
  COMPARE(s.initial_conditions(&P, Test::default_parameters()), 0.);
  COMPARE(P.size(), 500u);
  for (const auto &pd : P) {
    COMPARE(pd.pdgcode(), 0x661);
  }
  // total momentum check
  FourVector momentum(0.0, 0.0, 0.0, 0.0);
  // position less than radius?
  for (auto p : P) {
    momentum += p.momentum();
    const double radius = sqrt(p.position().x1() * p.position().x1() +
                               p.position().x2() * p.position().x2() +
                               p.position().x3() * p.position().x3());
    VERIFY(radius < 10.0);
  }
  COMPARE_ABSOLUTE_ERROR(momentum.x1(), 0.0, 1e-12);
  COMPARE_ABSOLUTE_ERROR(momentum.x2(), 0.0, 1e-12);
  COMPARE_ABSOLUTE_ERROR(momentum.x3(), 0.0, 1e-12);
}
