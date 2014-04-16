/*
 *
 *    Copyright (c) 2014
 *      SMASH Team
 *
 *    GNU General Public License (GPLv3 or later)
 *
 */

#include "include/action.h"

#include "include/outputroutines.h"
#include "include/resonances.h"

namespace Smash {

ScatterAction::ScatterAction(const std::vector<int> &in_part,
			     float time_of_execution)
    : Action(in_part, time_of_execution) {}


void ScatterAction::perform (Particles *particles, size_t &id_process)
{
  FourVector velocity_CM;
  /* relevant particle id's for the collision */
  int id_a = this->in1();
  int id_b = this->in2();
  int interaction_type = this->process_type();
  FourVector initial_momentum(particles->data(id_a).momentum()
    + particles->data(id_b).momentum());
  FourVector final_momentum;

  printd("Process %zu type %i particle %s<->%s colliding %d<->%d time %g\n",
    id_process, interaction_type, particles->type(id_a).name().c_str(),
	  particles->type(id_a).name().c_str(), id_a, id_b,
	  particles->data(id_a).position().x0());
  printd_momenta("particle 1 momenta before", particles->data(id_a));
  printd_momenta("particle 2 momenta before", particles->data(id_b));

  /* 2->2 elastic scattering */
  if (interaction_type == 0) {
    printd("Process: Elastic collision.\n");
    write_oscar(particles->data(id_a), particles->type(id_a), 2, 2);
    write_oscar(particles->data(id_b), particles->type(id_b));

    /* processes computed in the center of momenta */
    boost_CM(particles->data_pointer(id_a), particles->data_pointer(id_b),
	      &velocity_CM);
    momenta_exchange(particles->data_pointer(id_a),
		      particles->data_pointer(id_b));
    boost_back_CM(particles->data_pointer(id_a),
		  particles->data_pointer(id_b), &velocity_CM);

    write_oscar(particles->data(id_a), particles->type(id_a));
    write_oscar(particles->data(id_b), particles->type(id_b));

    printd_momenta("particle 1 momenta after", particles->data(id_a));
    printd_momenta("particle 2 momenta after", particles->data(id_b));

    final_momentum = particles->data(id_a).momentum()
    + particles->data(id_b).momentum();

    /* unset collision time for both particles + keep id + unset partner */
    particles->data_pointer(id_a)->set_collision_past(id_process);
    particles->data_pointer(id_b)->set_collision_past(id_process);

  } else if (interaction_type == 1) {
    /* resonance formation */
    printd("Process: Resonance formation. ");
    size_t new_particles = (this->final_state()).size();
    write_oscar(particles->data(id_a), particles->type(id_a), 2,
		new_particles);
    write_oscar(particles->data(id_b), particles->type(id_b));
    /* processes computed in the center of momenta */
    boost_CM(particles->data_pointer(id_a), particles->data_pointer(id_b),
	      &velocity_CM);

    size_t id_new = resonance_formation(particles, id_a, id_b,
					this->final_state());

    boost_back_CM(particles->data_pointer(id_a),
		  particles->data_pointer(id_b), &velocity_CM);

    /* Boost the new particle to computational frame */
    FourVector neg_velocity_CM;
    neg_velocity_CM.set_FourVector(1.0, -velocity_CM.x1(),
      -velocity_CM.x2(), -velocity_CM.x3());

    for (size_t id_value = id_new; id_value < id_new + new_particles;
	  id_value++) {
      particles->data_pointer(id_value)->set_momentum(
	particles->data(id_value).momentum().LorentzBoost(neg_velocity_CM));
      final_momentum += particles->data(id_value).momentum();

      /* The starting point of resonance is between
	* the two initial particles
	* x_middle = x_a + (x_b - x_a) / 2
	*/
      FourVector middle_point = particles->data(id_a).position()
	+ (particles->data(id_b).position()
	    - particles->data(id_a).position())
	/ 2.0;
      particles->data_pointer(id_value)->set_position(middle_point);
      write_oscar(particles->data(id_value), particles->type(id_value));
      /* unset collision time for particles + keep id + unset partner */
      particles->data_pointer(id_value)->set_collision_past(id_process);

      printd("Resonance %s with ID %zu \n",
	particles->type(id_new).name().c_str(), id_new);
      printd_momenta("momentum in comp frame", particles->data(id_new));
      printd_position("position in comp frame", particles->data(id_new));
    }

    /* Remove the initial particles */
    particles->remove(id_a);
    particles->remove(id_b);

    printd("Particle map has now %zu elements. \n", particles->size());
  } else {
    printf("Warning: ID %i (%s) has unspecified process type %i.\n",
	    id_a, particles->type(id_a).name().c_str(), interaction_type);
  } /* end if (interaction_type == 0) */
  id_process++;

  FourVector momentum_difference;
  momentum_difference += initial_momentum;
  momentum_difference -= final_momentum;
  if (fabs(momentum_difference.x0()) > really_small) {
    printf("Process %zu type %i\n", id_process, interaction_type);
    printf("Warning: Interaction type %i E conservation violation %g\n",
	    interaction_type, momentum_difference.x0());
  }
  if (fabs(momentum_difference.x1()) > really_small)
    printf("Warning: Interaction type %i px conservation violation %g\n",
	    interaction_type, momentum_difference.x1());
  if (fabs(momentum_difference.x2()) > really_small)
    printf("Warning: Interaction type %i py conservation violation %g\n",
	    interaction_type, momentum_difference.x2());
  if (fabs(momentum_difference.x3()) > really_small)
    printf("Warning: Interaction type %i pz conservation violation %g\n",
	    interaction_type, momentum_difference.x3());
}

}