/*
 *    Copyright (c) 2014-2018
 *      SMASH Team
 *
 *    GNU General Public License (GPLv3 or later)
 */
#include <cmath>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "smash/constants.h"
#include "smash/customnucleus.h"
#include "smash/particledata.h"
#include "smash/particletype.h"
#include "smash/pdgcode.h"
#include "smash/pdgcode_constants.h"
#include "smash/random.h"

namespace smash {

/*!\Userguide
 * \page projectile_and_target Projectile and Target
 * \n
 * Example: Configuring custom nuclei from external file
 * --------------
 * The following example illustrates how to configure a center-of-mass
 * heavy-ion collision with nuclei generated from an external file.
 * The nucleon positions are not sampled by smash but read in from an
 * external file. The given path and name
 * of the external file are made up and should be defined by the user
 * according to the used file.
 *\verbatim
 Modi:
     Collider:
         Projectile:
             Particles:    {2212: 79, 2112: 118}
             Custom:
                 File_Directory: "/home/username/custom_lists"
                 File_Name: "Au197_custom.txt"
         Target:
             Particles:    {2212: 79, 2112: 118}
             Custom:
                 File_Directory: "/home/username/custom_lists"
                 File_Name: "Au197_custom.txt"
         Sqrtsnn: 7.7
 \endverbatim
 *
 * The following example shows how an input file should be formatted:
 * <div class="fragment">
 * <div class="line"><span class="preprocessor"> 0.20100624   0.11402423
 * -2.40964466   0   0</span></div>
 * <div class="line"><span class="preprocessor"> 1.69072087  -3.21471918
 *  1.06050693   0   1</span></div>
 * <div class="line"><span class="preprocessor">-1.95791109  -3.51483782
 *  2.47294656   1   1</span></div>
 * <div class="line"><span class="preprocessor"> 0.43554894   4.35250733
 *  0.13331011   1   0</span></div>
 * </div>
 * The input file contains 5 columns (x, y, z, s, c). The first three
 * columns specify the spatial cordinates in fm. The fourth column denotes
 * the spin projection. The fifth contains the charge with 1 and 0 for
 * protons and neutrons respectively. In the example given the first line
 * defines a neutron and the second one a proton. Please make sure that
 * your file contains as many particles as you specified in the configuration.
 * For the example configuration your file needs to contain 79 protons and
 * 118 neutrons in the first 197 lines. And the same number in the following
 * 197 lines. The read in nuclei are randomly rotated and recentered. Therefore
 * you can run smash even if your file does not contain enough nuclei for the
 * number of events you want to simulate as the missing nuclei are generated by
 * rotation of the given configurations.
 *
 * \note
 * SMASH is shipped with an example configuration file to set up a collision
 * with externally generated nucleon positions. This requires a particle list
 * to be read in. Both, the configuration file and the particle list, are
 * located in /input/custom_nucleus. To run SMASH with the provided example
 * configuration and particle list, execute \n
 * \n
 * \verbatim
    ./smash -i INPUT_DIR/custom_nucleus/config.yaml
 \endverbatim
 * \n
 * Where 'INPUT_DIR' needs to be replaced by the path to the input directory
 * ('../input', if the build directory is located in the smash
 * folder).
 */

std::unique_ptr<std::ifstream> CustomNucleus::filestream_shared_ = nullptr;

CustomNucleus::CustomNucleus(Configuration& config, int testparticles,
                             bool same_file) {
  // Read in file directory from config
  std::string particle_list_file_directory =
      config.take({"Custom", "File_Directory"});
  // Read in file name from config
  std::string particle_list_file_name = config.take({"Custom", "File_Name"});

  if (particles_.size() != 0) {
    throw std::runtime_error(
        "Your Particle List is already filled before reading in from the "
        "external file."
        "Something went wrong. Please check your config.");
  }
  /*
   * Counts number of nucleons in one nucleus as it is specialized
   * by the user in the config file.
   * It is needed to read in the proper number of nucleons for one
   * nucleus and to restart at the listreading for the following
   * nucleus as one does not want to read configurations twice.
   */
  std::map<PdgCode, int> particle_list = config.take({"Particles"});
  for (const auto& particle : particle_list)
    number_of_nucleons_ += particle.second * testparticles;
  /*
   * "if" statement makes sure the streams to the file are initialized
   * properly.
   */
  const std::string path =
      file_path(particle_list_file_directory, particle_list_file_name);
  if (same_file && !filestream_shared_) {
    filestream_shared_ = make_unique<std::ifstream>(path);
    used_filestream_ = &filestream_shared_;
  } else if (!same_file) {
    filestream_ = make_unique<std::ifstream>(path);
    used_filestream_ = &filestream_;
  } else {
    used_filestream_ = &filestream_shared_;
  }

  custom_nucleus_ = readfile(**used_filestream_, number_of_nucleons_);
  fill_from_list(custom_nucleus_);
  // Inherited from nucleus class (see nucleus.h)
  set_parameters_automatic();
}

void CustomNucleus::fill_from_list(const std::vector<Nucleoncustom>& vec) {
  particles_.clear();
  index = 0;
  // checking if particle is proton or neutron
  for (const auto& it : vec) {
    PdgCode pdgcode;
    if (it.isospin == 1) {
      pdgcode = pdg::p;
    } else if (it.isospin == 0) {
      pdgcode = pdg::n;
    } else {
      throw std::runtime_error(
          "Your particles charges are not 1 = proton or 0 = neutron."
          "Check whether your list is correct or there is an error.");
    }
    // setting parameters for the particles in the particlelist in smash
    const ParticleType& current_type = ParticleType::find(pdgcode);
    double current_mass = current_type.mass();
    particles_.emplace_back(current_type);
    particles_.back().set_4momentum(current_mass, 0.0, 0.0, 0.0);
  }
}

ThreeVector CustomNucleus::distribute_nucleon() {
  /*
   * As only arrange_nucleons is called at the beginning of every
   * event it is important to have readfile and fill from list
   * called again when a new event starts. The constructor is only
   * called twice to initialize the first target and projectile.
   * Therefore this if statement is implemented.
   */
  if (index >= custom_nucleus_.size()) {
    custom_nucleus_ = readfile(**used_filestream_, number_of_nucleons_);
    fill_from_list(custom_nucleus_);
  }
  const auto& pos = custom_nucleus_.at(index);
  index++;
  ThreeVector nucleon_position(pos.x, pos.y, pos.z);
  // rotate nucleon about euler angle
  nucleon_position.rotate(phi_, theta_, psi_);

  return nucleon_position;
}

void CustomNucleus::arrange_nucleons() {
  /* Randomly generate Euler angles for rotation everytime a new
   * custom nucleus is initialiezed. Therefore this is done 2 times per
   * event.
   */
  random_euler_angles();

  for (auto i = begin(); i != end(); i++) {
    // Initialize momentum
    i->set_4momentum(i->pole_mass(), 0.0, 0.0, 0.0);
    /* Sampling the Woods-Saxon, get the radial
     * position and solid angle for the nucleon. */
    ThreeVector pos = distribute_nucleon();
    // Set the position of the nucleon.
    i->set_4position(FourVector(0.0, pos));
  }
  // Recenter
  align_center();
}

std::string CustomNucleus::file_path(const std::string& file_directory,
                                     const std::string& file_name) {
  if (file_directory.back() == '/') {
    return file_directory + file_name;
  } else {
    return file_directory + '/' + file_name;
  }
}

std::vector<Nucleoncustom> CustomNucleus::readfile(std::ifstream& infile,
                                                   int particle_number) const {
  int A = particle_number;
  std::string line;
  std::vector<Nucleoncustom> custom_nucleus;
  // read in only A particles for one nucleus
  for (int i = 0; i < A; ++i) {
    std::getline(infile, line);
    // make sure the stream goes back to the beginning when it hits end of file
    if (infile.eof()) {
      infile.clear();
      infile.seekg(0, infile.beg);
      std::getline(infile, line);
    }
    Nucleoncustom nucleon;
    std::istringstream iss(line);
    if (!(iss >> nucleon.x >> nucleon.y >> nucleon.z >>
          nucleon.spinprojection >> nucleon.isospin)) {
      throw std::runtime_error(
          "SMASH could not read in a line from your initial nuclei input file."
          "Check if your file has the following format: x y z spinprojection "
          "isospin");
      break;
    }
    custom_nucleus.push_back(nucleon);
  }
  return custom_nucleus;
}

void CustomNucleus::random_euler_angles() {
  // theta_ has to be sampled that way as cos(theta) should be uniform
  phi_ = twopi * random::uniform(0., 1.);
  theta_ = std::acos(2 * random::uniform(0., 1.) - 1);
  psi_ = twopi * random::uniform(0., 1.);
}

}  // namespace smash
