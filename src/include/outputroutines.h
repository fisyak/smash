/*
 *    Copyright (c) 2012-2013
 *      maximilian attems <attems@fias.uni-frankfurt.de>
 *      Jussi Auvinen <auvinen@fias.uni-frankfurt.de>
 *    GNU General Public License (GPLv3)
 */
#ifndef SRC_INCLUDE_OUTPUTROUTINES_H_
#define SRC_INCLUDE_OUTPUTROUTINES_H_

/* forward declarations */
class box;
class ParticleData;

/* console output */
void print_startup(box box);

/* data directory */
void mkdir_data(void);

/* Compile time debug info */
#ifdef DEBUG
# define printd printf
#else
# define printd(...) ((void)0)
#endif

/* console debug output */
void printd_position(ParticleData particle);
void printd_momenta(ParticleData particle);

/* output data files */
void write_particles(ParticleData *particles, const int number);
void write_oscar_header(void);
void write_oscar(const ParticleData &particle1, const ParticleData &particle2);

#endif  // SRC_INCLUDE_OUTPUTROUTINES_H_
