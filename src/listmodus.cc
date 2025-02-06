/*
 *
 *    Copyright (c) 2015-2024
 *      SMASH Team
 *
 *    GNU General Public License (GPLv3 or later)
 *
 */

#include "smash/listmodus.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <list>
#include <map>
#include <sstream>
#include <utility>
#include <vector>

#include "smash/algorithms.h"
#include "smash/boxmodus.h"
#include "smash/configuration.h"
#include "smash/constants.h"
#include "smash/experimentparameters.h"
#include "smash/fourvector.h"
#include "smash/inputfunctions.h"
#include "smash/logging.h"
#include "smash/particledata.h"
#include "smash/threevector.h"
#include "smash/wallcrossingaction.h"

namespace smash {
static constexpr int LList = LogArea::List::id;

static bool is_list_of_particles_invalid(const Particles &, int);

ListModus::ListModus(Configuration modus_config,
                     const ExperimentParameters &param)
    : file_id_{std::nullopt}, event_id_{0} {
  /*
   * Make some logic on config to understand whether to extract parent or
   * children keys. These have all the same keys but have a different section
   * name (like for instance 'ListBox' instead of 'List'). At the moment there
   * is only one child and this approach is fine enough, although ugly.
   */
  const bool is_list =
      modus_config.has_value(InputKeys::modi_list_fileDirectory);
  const bool is_list_box =
      modus_config.has_value(InputKeys::modi_listBox_fileDirectory);
  if (is_list == is_list_box) {
    throw std::logic_error(
        "Unexpected error in ListModus constructor. Either List or ListBox "
        "sections must be present in configuration.");
  }
  Key<std::string> file_prefix_key = InputKeys::modi_list_filePrefix,
                   file_directory_key = InputKeys::modi_list_fileDirectory,
                   filename_key = InputKeys::modi_list_filename;
  Key<int> shift_id_key = InputKeys::modi_list_shiftId;
  if (is_list_box) {
    file_prefix_key = InputKeys::modi_listBox_filePrefix;
    file_directory_key = InputKeys::modi_listBox_fileDirectory;
    filename_key = InputKeys::modi_listBox_filename;
    shift_id_key = InputKeys::modi_listBox_shiftId;
  }
  // Impose strict requirement on possible keys present in configuration file
  const bool file_prefix_used = modus_config.has_value(file_prefix_key);
  const bool filename_used = modus_config.has_value(filename_key);
  if (file_prefix_used == filename_used) {
    throw std::invalid_argument(
        "Either 'Filename' or 'File_Prefix' key must be used in 'Modi' section "
        "in configuration file. Please, adjust your configuration file.");
  }
  if (file_prefix_used) {
    particle_list_filename_or_prefix_ = modus_config.take(file_prefix_key);
    file_id_ = modus_config.take(shift_id_key);
  } else {
    particle_list_filename_or_prefix_ = modus_config.take(filename_key);
  }
  particle_list_file_directory_ = modus_config.take(file_directory_key);
  if (param.n_ensembles > 1) {
    throw std::runtime_error("ListModus only makes sense with one ensemble");
  }
  validate_list_of_particles_of_all_events_();
}

/* console output on startup of List specific parameters */
std::ostream &operator<<(std::ostream &out, const ListModus &m) {
  out << "-- List Modus\nInput directory for external particle lists:\n"
      << m.particle_list_file_directory_ << "\n";
  return out;
}

void ListModus::backpropagate_to_same_time(Particles &particles) {
  /* (1) If particles are already at the same time - don't touch them
         AND start at the start_time_ from the config. */
  double earliest_formation_time = DBL_MAX;
  double formation_time_difference = 0.0;
  double reference_formation_time = 0.0;  // avoid compiler warning
  bool first_particle = true;
  for (const auto &particle : particles) {
    const double t = particle.position().x0();
    if (t < earliest_formation_time) {
      earliest_formation_time = t;
    }
    if (first_particle) {
      reference_formation_time = t;
      first_particle = false;
    } else {
      formation_time_difference += std::abs(t - reference_formation_time);
    }
  }
  /* (2) If particles are NOT at the same time -> anti-stream them to
         the earliest time (Note: not to the start_time_ set by config) */
  bool anti_streaming_needed = (formation_time_difference > really_small);
  start_time_ = earliest_formation_time;
  if (anti_streaming_needed) {
    for (auto &particle : particles) {
      /* for hydro output where formation time is different */
      const double t = particle.position().x0();
      const double delta_t = t - start_time_;
      const ThreeVector r =
          particle.position().threevec() - delta_t * particle.velocity();
      particle.set_4position(FourVector(start_time_, r));
      particle.set_formation_time(t);
      particle.set_cross_section_scaling_factor(0.0);
    }
  }
}

void ListModus::try_create_particle(Particles &particles, PdgCode pdgcode,
                                    double t, double x, double y, double z,
                                    double mass, double E, double px, double py,
                                    double pz) {
  try {
    ParticleData new_particle =
        create_valid_smash_particle_matching_provided_quantities(
            pdgcode, mass, {t, x, y, z}, {E, px, py, pz}, LList,
            warn_about_mass_discrepancy_, warn_about_off_shell_particles_);
    particles.insert(new_particle);
  } catch (ParticleType::PdgNotFoundFailure &) {
    logg[LList].warn() << "SMASH does not recognize pdg code " << pdgcode
                       << " loaded from file. This particle will be ignored.\n";
  }
}

/* initial_conditions - sets particle data for @particles */
double ListModus::initial_conditions(Particles *particles,
                                     const ExperimentParameters &) {
  read_particles_from_next_event_(*particles);
  if (particles->size() > 0) {
    backpropagate_to_same_time(*particles);
  } else {
    start_time_ = 0.0;
  }
  event_id_++;

  return start_time_;
}

void ListModus::read_particles_from_next_event_(Particles &particles) {
  std::string particle_list = next_event_();
  for (const Line &line : line_parser(particle_list)) {
    std::istringstream lineinput(line.text);
    double t, x, y, z, mass, E, px, py, pz;
    int id, charge;
    std::string pdg_string;
    lineinput >> t >> x >> y >> z >> mass >> E >> px >> py >> pz >>
        pdg_string >> id >> charge;
    if (lineinput.fail()) {
      throw LoadFailure(
          build_error_string("While loading external particle lists data:\n"
                             "Failed to convert the input string to the "
                             "expected data types.",
                             line));
    }
    PdgCode pdgcode(pdg_string);
    logg[LList].debug("Particle ", pdgcode, " (x,y,z)= (", x, ", ", y, ", ", z,
                      ")");

    // Charge consistency check
    if (pdgcode.charge() != charge) {
      if (verbose_) {
        logg[LList].error()
            << "Charge of pdg = " << pdgcode << " != " << charge;
      }
      throw std::invalid_argument("Inconsistent input (charge).");
    }
    try_create_particle(particles, pdgcode, t, x, y, z, mass, E, px, py, pz);
  }
}

std::filesystem::path ListModus::file_path_(std::optional<int> file_id) {
  std::string fname = particle_list_filename_or_prefix_ +
                      ((file_id) ? std::to_string(*file_id) : "");

  const std::filesystem::path default_path =
      std::filesystem::absolute(particle_list_file_directory_);

  const std::filesystem::path fpath = default_path / fname;

  logg[LList].debug() << "File: " << std::filesystem::absolute(fpath) << '\n';

  if (!std::filesystem::exists(fpath)) {
    if (verbose_) {
      logg[LList].fatal()
          << fpath.filename().native() << " does not exist! \n\n"
          << "Usage of smash with external particle lists:\n"
          << "  1. Put the external particle lists in one or more files\n"
          << "     according to the user guide instructions.\n"
          << "  2. Particles info: t x y z mass p0 px py pz pdg ID charge\n"
          << "     in units of: fm fm fm fm GeV GeV GeV GeV GeV none none e\n";
    }
    throw std::runtime_error("External particle list does not exist!");
  }

  return fpath;
}

std::string ListModus::next_event_() {
  const std::filesystem::path fpath = file_path_(file_id_);
  std::ifstream ifs{fpath};
  ifs.seekg(last_read_position_);

  if (!file_has_events_(fpath, last_read_position_)) {
    if (file_id_) {
      // Get next file and call this function recursively
      (*file_id_)++;
      last_read_position_ = 0;
      ifs.close();
      return next_event_();
    } else {
      throw std::runtime_error(
          "Attempt to read in next event in ListModus object but no further "
          "data found in single provided file. Please, check your setup.");
    }
  }

  // read one event. events marked by line # event end i in case of Oscar
  // output. Assume one event per file for all other output formats
  std::string event_string;
  const std::string needle = "end";
  std::string line;
  while (getline(ifs, line)) {
    if (line.find(needle) == std::string::npos) {
      event_string += line + "\n";
    } else {
      break;
    }
  }

  if (!ifs.eof() && (ifs.fail() || ifs.bad())) {
    if (verbose_) {
      logg[LList].fatal() << "Error while reading "
                          << fpath.filename().native();
    }
    throw std::runtime_error("Error while reading external particle list");
  }
  // save position for next event read
  last_read_position_ = ifs.tellg();
  ifs.close();

  return event_string;
}

bool ListModus::file_has_events_(std::filesystem::path filepath,
                                 std::streampos last_position) {
  std::ifstream ifs{filepath};
  std::string line;

  // last event read read at end of file. we know this because errors are
  // handled in next_event
  if (last_position == -1) {
    return false;
  }
  ifs.seekg(last_position);
  // skip over comment lines, assume that a max. of four consecutive comment
  // lines can occur
  int skipped_lines = 0;
  const int max_comment_lines = 4;
  while (std::getline(ifs, line) && line[0] != '#' &&
         skipped_lines++ < max_comment_lines) {
  }

  if (ifs.eof()) {
    return false;
  }

  if (!ifs.good()) {
    if (verbose_) {
      logg[LList].fatal() << "Error while reading "
                          << filepath.filename().native();
    }
    throw std::runtime_error("Error while reading external particle list");
  }

  ifs.close();
  return true;
}

/* In this method, which is called from the constructor only, we "abuse" of the
 * class functionality to read in all events and validate them. In order not to
 * modify the original object we work on an utility copy. Note that the copy
 * constructor provided by the compiler is enough as the class has only STL or
 * builtin members.
 */
void ListModus::validate_list_of_particles_of_all_events_() const {
  ListModus utility_copy{*this};
  utility_copy.verbose_ = false;
  utility_copy.warn_about_mass_discrepancy_ = false;
  utility_copy.warn_about_off_shell_particles_ = false;
  bool are_there_faulty_events = false;
  while (true) {
    try {
      Particles particles{};
      utility_copy.read_particles_from_next_event_(particles);
      if (is_list_of_particles_invalid(particles, utility_copy.event_id_)) {
        are_there_faulty_events = true;
      }
      utility_copy.event_id_++;
    } catch (const std::exception &) {
      break;
    }
  }
  if (are_there_faulty_events) {
    throw InvalidEvents(
        "More than 2 particles with the same 4-position have been found in the "
        "same event.\nPlease, check your particles list file.");
  }
}

ListBoxModus::ListBoxModus(Configuration modus_config,
                           const ExperimentParameters &param)
    : ListModus(), length_(modus_config.take(InputKeys::modi_listBox_length)) {
  /*
   * ATTENTION: In a child class initialization list nothing can be done before
   * calling the base constructor. However, here we cannot hand over the
   * configuration to the base class as there are child-specific keys to be
   * taken before. This cannot be done after having moved the configuration and
   * changing the constructor signature would be a big change as all modus
   * classes should have the same constructor signature to allow Experiment to
   * template on it. Therefore, we abuse C++ here by default-initializing the
   * parent class, then taking the child-specific key(s) and then assigning a
   * parent instance to the child using the parent assignment operator. In
   * general this would be risky as it would open up the possibility to leave
   * part of the children uninitialized, but here we should have under control
   * what exactly happens at initialization time.
   */
  this->ListModus::operator=(ListModus(std::move(modus_config), param));
}

int ListBoxModus::impose_boundary_conditions(Particles *particles,
                                             const OutputsList &output_list) {
  int wraps = 0;
  for (ParticleData &data : *particles) {
    FourVector position = data.position();
    bool wall_hit = enforce_periodic_boundaries(position.begin() + 1,
                                                position.end(), length_);
    if (wall_hit) {
      const ParticleData incoming_particle(data);
      data.set_4position(position);
      ++wraps;
      ActionPtr action =
          std::make_unique<WallcrossingAction>(incoming_particle, data);
      for (const auto &output : output_list) {
        if (!output->is_dilepton_output() && !output->is_photon_output()) {
          output->at_interaction(*action, 0.);
        }
      }
    }
  }

  logg[LList].debug("Moved ", wraps, " particles back into the box.");
  return wraps;
}

/* This function is meant to throw if there are more than two particles at the
 * same position. To be more user-friendly we first check all particles and then
 * report about all faulty groups of particles with their position. Only
 * afterwards the simulation is aborted. */
static bool is_list_of_particles_invalid(const Particles &particles,
                                         int event) {
  /* In order to make the desired check, particles are classified in an std::map
   * using their position as a key. However, to do so, the operator< of the
   * FourVector class is not suitable since in a std::map, by default, two keys
   * a and b are considered equivalent if !(a<b) && !(b<a). Therefore we convert
   * the 4-postion to a string and use this as key. Note that this should also
   * work in the case in which the file contains apparently different positions,
   * i.e. with differences in the decimals beyond double precision. At this
   * point the file has been already read and the 4-positions are stored in
   * double position. */
  auto to_string = [](const FourVector &v) {
    return "(" + std::to_string(v[0]) + ", " + std::to_string(v[1]) + ", " +
           std::to_string(v[2]) + ", " + std::to_string(v[3]) + ")";
  };
  std::map<std::string, int> checker{};
  for (const auto &p : particles) {
    checker[to_string(p.position())]++;
  }
  bool error_found = false;
  for (const auto &[key, value] : checker) {
    if (value > 2) {
      logg[LList].error() << "Event " << event << ": Found " << value
                          << " particles at same position " << key;
      error_found = true;
    }
  }
  return error_found;
}
}  // namespace smash
