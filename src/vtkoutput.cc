/*
 *
 *    Copyright (c) 2014-2022,2024
 *      SMASH Team
 *
 *    GNU General Public License (GPLv3 or later)
 *
 */

#include "smash/vtkoutput.h"

#include <fstream>
#include <memory>
#include <utility>

#include "smash/clock.h"
#include "smash/config.h"
#include "smash/file.h"
#include "smash/forwarddeclarations.h"
#include "smash/particles.h"

namespace smash {

VtkOutput::VtkOutput(const std::filesystem::path &path, const std::string &name,
                     const OutputParameters &out_par)
    : OutputInterface(name),
      base_path_(std::move(path)),
      is_thermodynamics_output_(name == "Thermodynamics"),
      is_fields_output_(name == "Fields") {
  if (out_par.part_extended) {
    logg[LOutput].warn()
        << "Creating VTK output: There is no extended VTK format.";
  }
}

VtkOutput::~VtkOutput() {}

/*!\Userguide
 * \page doxypage_output_vtk
 * In general VTK is a very versatile format, which allows many possible
 * structures. For generic VTK format one can see http://vtk.org. Here only
 * SMASH-specific VTK format is described.
 *
 * SMASH VTK files contain a snapshot of simulation at one moment of time.
 * VTK output files are written at initialization at event start and
 * every period of time \f$ \Delta t \f$, where \f$ \Delta t \f$ is regulated
 * by option (see \ref doxypage_input_conf_general). For every new output moment
 * a separate VTK file is written. File names are constructed as follows:
 * `pos_ev<event>_ens<ensemble>_tstep<timestep_counter>.vtk`.
 *
 * Files contain particle coordinates, momenta, PDG codes, cross-section
 * scaling factors, ID, number of collisions baryon number, strangeness and
 * masses. VTK output is known to work with paraview, a free visualization and
 * data analysis software. Files of this format are supposed to be used as a
 * black box and opened with paraview, but at the same time they are
 * human-readable text files.
 *
 * There is also a possibility to print a lattice with thermodynamical
 * quantities to vtk files, see \ref doxypage_output_vtk_lattice.
 **/

void VtkOutput::at_eventstart(const Particles &particles,
                              const EventLabel &event_label,
                              const EventInfo &) {
  vtk_density_output_counter_ = 0;
  vtk_tmn_output_counter_ = 0;
  vtk_tmn_landau_output_counter_ = 0;
  vtk_v_landau_output_counter_ = 0;
  vtk_fluidization_counter_ = 0;

  current_event_ = event_label.event_number;
  current_ensemble_ = event_label.ensemble_number;
  vtk_output_counter_[counter_key()] = 0;
  if (!is_thermodynamics_output_ && !is_fields_output_) {
    write(particles);
    vtk_output_counter_[counter_key()]++;
  }
}

void VtkOutput::at_eventend(const Particles & /*particles*/,
                            const EventLabel & /*event_number*/,
                            const EventInfo &) {}

void VtkOutput::at_intermediate_time(const Particles &particles,
                                     const std::unique_ptr<Clock> &,
                                     const DensityParameters &,
                                     const EventLabel &event_label,
                                     const EventInfo &) {
  current_event_ = event_label.event_number;
  current_ensemble_ = event_label.ensemble_number;
  if (!is_thermodynamics_output_ && !is_fields_output_) {
    write(particles);
    vtk_output_counter_[counter_key()]++;
  }
}

void VtkOutput::write(const Particles &particles) {
  char filename[64];
  snprintf(filename, sizeof(filename), "pos_ev%05i_ens%05i_tstep%05i.vtk",
           current_event_, current_ensemble_,
           vtk_output_counter_[counter_key()]);
  FilePtr file_{std::fopen((base_path_ / filename).native().c_str(), "w")};

  /* Legacy VTK file format */
  std::fprintf(file_.get(), "# vtk DataFile Version 2.0\n");
  std::fprintf(file_.get(), "Generated from molecular-offset data %s\n",
               SMASH_VERSION);
  std::fprintf(file_.get(), "ASCII\n");

  /* Unstructured data sets are composed of points, lines, polygons, .. */
  std::fprintf(file_.get(), "DATASET UNSTRUCTURED_GRID\n");
  std::fprintf(file_.get(), "POINTS %zu double\n", particles.size());
  for (const auto &p : particles) {
    std::fprintf(file_.get(), "%g %g %g\n", p.position().x1(),
                 p.position().x2(), p.position().x3());
  }
  std::fprintf(file_.get(), "CELLS %zu %zu\n", particles.size(),
               particles.size() * 2);
  for (size_t point_index = 0; point_index < particles.size(); point_index++) {
    std::fprintf(file_.get(), "1 %zu\n", point_index);
  }
  std::fprintf(file_.get(), "CELL_TYPES %zu\n", particles.size());
  for (size_t point_index = 0; point_index < particles.size(); point_index++) {
    std::fprintf(file_.get(), "1\n");
  }
  std::fprintf(file_.get(), "POINT_DATA %zu\n", particles.size());
  std::fprintf(file_.get(), "SCALARS pdg_codes int 1\n");
  std::fprintf(file_.get(), "LOOKUP_TABLE default\n");
  for (const auto &p : particles) {
    std::fprintf(file_.get(), "%s\n", p.pdgcode().string().c_str());
  }
  std::fprintf(file_.get(), "SCALARS is_formed int 1\n");
  std::fprintf(file_.get(), "LOOKUP_TABLE default\n");
  double current_time = particles.time();
  for (const auto &p : particles) {
    std::fprintf(file_.get(), "%s\n",
                 (p.formation_time() > current_time) ? "0" : "1");
  }
  std::fprintf(file_.get(), "SCALARS cross_section_scaling_factor double 1\n");
  std::fprintf(file_.get(), "LOOKUP_TABLE default\n");
  for (const auto &p : particles) {
    std::fprintf(file_.get(), "%g\n", p.xsec_scaling_factor());
  }
  std::fprintf(file_.get(), "SCALARS mass double 1\n");
  std::fprintf(file_.get(), "LOOKUP_TABLE default\n");
  for (const auto &p : particles) {
    std::fprintf(file_.get(), "%g\n", p.effective_mass());
  }
  std::fprintf(file_.get(), "SCALARS N_coll int 1\n");
  std::fprintf(file_.get(), "LOOKUP_TABLE default\n");
  for (const auto &p : particles) {
    std::fprintf(file_.get(), "%i\n", p.get_history().collisions_per_particle);
  }
  std::fprintf(file_.get(), "SCALARS particle_ID int 1\n");
  std::fprintf(file_.get(), "LOOKUP_TABLE default\n");
  for (const auto &p : particles) {
    std::fprintf(file_.get(), "%i\n", p.id());
  }
  std::fprintf(file_.get(), "SCALARS baryon_number int 1\n");
  std::fprintf(file_.get(), "LOOKUP_TABLE default\n");
  for (const auto &p : particles) {
    std::fprintf(file_.get(), "%i\n", p.pdgcode().baryon_number());
  }
  std::fprintf(file_.get(), "SCALARS strangeness int 1\n");
  std::fprintf(file_.get(), "LOOKUP_TABLE default\n");
  for (const auto &p : particles) {
    std::fprintf(file_.get(), "%i\n", p.pdgcode().strangeness());
  }
  std::fprintf(file_.get(), "VECTORS momentum double\n");
  for (const auto &p : particles) {
    std::fprintf(file_.get(), "%g %g %g\n", p.momentum().x1(),
                 p.momentum().x2(), p.momentum().x3());
  }
}

/*!\Userguide
 * \page doxypage_output_vtk_lattice
 * Density on the lattice can be printed out in the VTK format of structured
 * grid. At every output moment a new vtk file is created. The name format is
 * `<density_type>_<density_name>_<event_number>_tstep<timestep_counter>.vtk`.
 * Files can be opened directly with <a href="http://paraview.org">ParaView</a>.
 */

template <typename T>
void VtkOutput::write_vtk_header(std::ofstream &file,
                                 RectangularLattice<T> &lattice,
                                 const std::string &description) {
  const auto dim = lattice.n_cells();
  const auto cs = lattice.cell_sizes();
  const auto orig = lattice.origin();
  file << "# vtk DataFile Version 2.0\n"
       << description << "\n"
       << "ASCII\n"
       << "DATASET STRUCTURED_POINTS\n"
       << "DIMENSIONS " << dim[0] << " " << dim[1] << " " << dim[2] << "\n"
       << "SPACING " << cs[0] << " " << cs[1] << " " << cs[2] << "\n"
       << "ORIGIN " << orig[0] << " " << orig[1] << " " << orig[2] << "\n"
       << "POINT_DATA " << lattice.size() << "\n";
}

template <typename T, typename F>
void VtkOutput::write_vtk_scalar(std::ofstream &file,
                                 RectangularLattice<T> &lattice,
                                 const std::string &varname, F &&get_quantity) {
  file << "SCALARS " << varname << " double 1\n"
       << "LOOKUP_TABLE default\n";
  file << std::setprecision(3);
  file << std::fixed;
  const auto dim = lattice.n_cells();
  lattice.iterate_sublattice({0, 0, 0}, dim, [&](T &node, int ix, int, int) {
    const double f_from_node = get_quantity(node);
    file << f_from_node << " ";
    if (ix == dim[0] - 1) {
      file << "\n";
    }
  });
}

template <typename T, typename F>
void VtkOutput::write_vtk_vector(std::ofstream &file,
                                 RectangularLattice<T> &lattice,
                                 const std::string &varname, F &&get_quantity) {
  file << "VECTORS " << varname << " double\n";
  file << std::setprecision(3);
  file << std::fixed;
  const auto dim = lattice.n_cells();
  lattice.iterate_sublattice({0, 0, 0}, dim, [&](T &node, int, int, int) {
    const ThreeVector v = get_quantity(node);
    file << v.x1() << " " << v.x2() << " " << v.x3() << "\n";
  });
}

std::string VtkOutput::make_filename(const std::string &descr, int counter) {
  char suffix[22];
  snprintf(suffix, sizeof(suffix), "_%05i_tstep%05i.vtk", current_event_,
           counter);
  return base_path_.string() + std::string("/") + descr + std::string(suffix);
}

std::string VtkOutput::make_varname(const ThermodynamicQuantity tq,
                                    const DensityType dens_type) {
  return std::string(to_string(dens_type)) + std::string("_") +
         std::string(to_string(tq));
}

void VtkOutput::thermodynamics_output(
    const ThermodynamicQuantity tq, const DensityType dens_type,
    RectangularLattice<DensityOnLattice> &lattice) {
  if (!is_thermodynamics_output_) {
    return;
  }
  std::ofstream file;
  const std::string varname = make_varname(tq, dens_type);
  file.open(make_filename(varname, vtk_density_output_counter_), std::ios::out);
  write_vtk_header(file, lattice, varname);
  write_vtk_scalar(file, lattice, varname,
                   [&](DensityOnLattice &node) { return node.rho(); });
  vtk_density_output_counter_++;
}

/*!\Userguide
 * \page doxypage_output_vtk_lattice
 * Additionally to density, energy-momentum tensor \f$T^{\mu\nu} \f$,
 * energy-momentum tensor in Landau rest frame \f$T^{\mu\nu}_L \f$ and
 * velocity of Landau rest frame \f$v_L\f$ on the lattice can be printed out
 * in the VTK format of structured grid. At every output moment a new vtk file
 * is created. The name format is
 * `<density_type>_<quantity>_<event_number>_tstep<timestep_counter>.vtk`. Files
 * can be opened directly with <a href="http://paraview.org">ParaView</a>.
 *
 * For configuring the output see \ref input_output_content_specific_
 * "content-specific output options".
 */

void VtkOutput::thermodynamics_output(
    const ThermodynamicQuantity tq, const DensityType dens_type,
    RectangularLattice<EnergyMomentumTensor> &Tmn_lattice) {
  if (!is_thermodynamics_output_) {
    return;
  }
  std::ofstream file;
  const std::string varname = make_varname(tq, dens_type);

  if (tq == ThermodynamicQuantity::Tmn) {
    file.open(make_filename(varname, vtk_tmn_output_counter_++), std::ios::out);
    write_vtk_header(file, Tmn_lattice, varname);
    for (int i = 0; i < 4; i++) {
      for (int j = i; j < 4; j++) {
        write_vtk_scalar(file, Tmn_lattice,
                         varname + std::to_string(i) + std::to_string(j),
                         [&](EnergyMomentumTensor &node) {
                           return node[EnergyMomentumTensor::tmn_index(i, j)];
                         });
      }
    }
  } else if (tq == ThermodynamicQuantity::TmnLandau) {
    file.open(make_filename(varname, vtk_tmn_landau_output_counter_++),
              std::ios::out);
    write_vtk_header(file, Tmn_lattice, varname);
    for (int i = 0; i < 4; i++) {
      for (int j = i; j < 4; j++) {
        write_vtk_scalar(file, Tmn_lattice,
                         varname + std::to_string(i) + std::to_string(j),
                         [&](EnergyMomentumTensor &node) {
                           const FourVector u = node.landau_frame_4velocity();
                           const EnergyMomentumTensor Tmn_L = node.boosted(u);
                           return Tmn_L[EnergyMomentumTensor::tmn_index(i, j)];
                         });
      }
    }
  } else {
    file.open(make_filename(varname, vtk_v_landau_output_counter_++),
              std::ios::out);
    write_vtk_header(file, Tmn_lattice, varname);
    write_vtk_vector(file, Tmn_lattice, varname,
                     [&](EnergyMomentumTensor &node) {
                       const FourVector u = node.landau_frame_4velocity();
                       return -u.velocity();
                     });
  }
}

void VtkOutput::fields_output(
    const std::string name1, const std::string name2,
    RectangularLattice<std::pair<ThreeVector, ThreeVector>> &lat) {
  if (!is_fields_output_) {
    return;
  }
  std::ofstream file1;
  file1.open(make_filename(name1, vtk_fields_output_counter_), std::ios::out);
  write_vtk_header(file1, lat, name1);
  write_vtk_vector(
      file1, lat, name1,
      [&](std::pair<ThreeVector, ThreeVector> &node) { return node.first; });
  std::ofstream file2;
  file2.open(make_filename(name2, vtk_fields_output_counter_), std::ios::out);
  write_vtk_header(file2, lat, name2);
  write_vtk_vector(
      file2, lat, name2,
      [&](std::pair<ThreeVector, ThreeVector> &node) { return node.second; });
  vtk_fields_output_counter_++;
}

void VtkOutput::thermodynamics_output(const GrandCanThermalizer &gct) {
  if (!is_thermodynamics_output_) {
    return;
  }
  std::ofstream file;
  file.open(make_filename("fluidization_td", vtk_fluidization_counter_++),
            std::ios::out);
  write_vtk_header(file, gct.lattice(), "fluidization_td");
  write_vtk_scalar(file, gct.lattice(), "e",
                   [&](ThermLatticeNode &node) { return node.e(); });
  write_vtk_scalar(file, gct.lattice(), "p",
                   [&](ThermLatticeNode &node) { return node.p(); });
  write_vtk_vector(file, gct.lattice(), "v",
                   [&](ThermLatticeNode &node) { return node.v(); });
  write_vtk_scalar(file, gct.lattice(), "T",
                   [&](ThermLatticeNode &node) { return node.T(); });
  write_vtk_scalar(file, gct.lattice(), "mub",
                   [&](ThermLatticeNode &node) { return node.mub(); });
  write_vtk_scalar(file, gct.lattice(), "mus",
                   [&](ThermLatticeNode &node) { return node.mus(); });
}

}  // namespace smash
