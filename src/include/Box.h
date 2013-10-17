/*
 *    Copyright (c) 2012-2013
 *      maximilian attems <attems@fias.uni-frankfurt.de>
 *      Jussi Auvinen <auvinen@fias.uni-frankfurt.de>
 *    GNU General Public License (GPLv3)
 */
#ifndef SRC_INCLUDE_BOX_H_
#define SRC_INCLUDE_BOX_H_

#include <stdint.h>
#include <time.h>
#include <cmath>

#include "../include/Laboratory.h"
#include "../include/time.h"

class Box : public Laboratory {
  public:
    /* default constructor with probable values */
    Box(): initial_condition_(1), length_(10.0f), temperature_(0.1f),
      energy_initial_(0.0f), number_density_initial_(0.0f),
      time_start_(set_timer_start()) {}
    /* useful constructor with explicit values for laboratory */
    explicit Box(Laboratory lab): Laboratory(lab), initial_condition_(1),
      length_(10.0f), temperature_(0.1f), energy_initial_(0.0f),
      number_density_initial_(0.0f), time_start_(set_timer_start()) {}
    /* member funtions */
    inline int initial_condition() const;
    inline void set_initial_condition(const int &INITIAL_CONDITION);
    float inline length() const;
    void inline set_length(float LENGTH);
    float inline energy_initial() const;
    void inline set_energy_initial(float energy);
    float inline number_density_initial() const;
    void inline set_number_density_inital(float number_density);
    float inline temperature() const;
    void inline set_temperature(float T);
    timespec inline time_start() const;
    timespec inline set_timer_start();

  private:
    /* initial condition */
    int initial_condition_;
    /* Cube edge length */
    float length_;
    /* Temperature of the Boltzmann distribution for thermal initialization */
    float temperature_;
    /* initial total energy of the box */
    float energy_initial_;
    /* initial number density of the box */
    float number_density_initial_;
    /* starting time of the simulation */
    timespec time_start_;
};

/* return the used initial condition */
inline int Box::initial_condition(void) const {
  return initial_condition_;
}

/* set the initial condition */
inline void Box::set_initial_condition(const int &INITIAL_CONDITION) {
  initial_condition_ = INITIAL_CONDITION;
}

/* return the edge length */
float inline Box::length(void) const {
  return length_;
}

/* set the edge length */
void inline Box::set_length(float LENGTH) {
  length_ = LENGTH;
}

/* return the used IC temperature */
float inline Box::temperature(void) const {
  return temperature_;
}

/* set the IC temperature */
void inline Box::set_temperature(float T) {
  temperature_ = T;
}

/* return the IC total energy */
float inline Box::energy_initial(void) const {
  return energy_initial_;
}

/* set the IC total energy */
void inline Box::set_energy_initial(float energy) {
  energy_initial_ = energy;
}

/* return the IC number density */
float inline Box::number_density_initial(void) const {
  return number_density_initial_;
}

/* set the IC number density */
void inline Box::set_number_density_inital(float number_density) {
  number_density_initial_ = number_density;
}

/* return when the timer started */
timespec inline Box::time_start(void) const {
  return time_start_;
}

/* set the timer to the actual time in nanoseconds precision */
timespec inline Box::set_timer_start(void) {
  timespec time;
  clock_gettime(&time);
  return time;
}

#endif  // SRC_INCLUDE_BOX_H_
