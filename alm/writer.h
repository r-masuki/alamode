/*
 writer.h

 Copyright (c) 2014, 2015, 2016 Terumasa Tadano

 This file is distributed under the terms of the MIT license.
 Please see the file 'LICENCE.txt' in the root directory
 or http://opensource.org/licenses/mit-license.php for information.
*/

#pragma once

#include <string>
#include <vector>
#include <map>
#include "system.h"
#include "symmetry.h"
#include "cluster.h"
#include "patterndisp.h"
#include "fcs.h"
#include "constraint.h"
#include "optimize.h"
#include "files.h"


namespace ALM_NS {
class AtomProperty {
public:
    double x, y, z;
    int kind;
    size_t atom, tran;

    AtomProperty() = default;;

    AtomProperty(const AtomProperty &other) = default;

    AtomProperty(const double *pos,
                 const int kind_in,
                 const int atom_in,
                 const int tran_in)
    {
        x = pos[0];
        y = pos[1];
        z = pos[2];
        kind = kind_in;
        atom = atom_in;
        tran = tran_in;
    }
};

class SystemInfo {
public:
    double lattice_vector[3][3];
    std::vector<AtomProperty> atoms;
    size_t nat, natmin, ntran;
    size_t nspecies;

    SystemInfo() = default;;
};

class Writer {
public:
    Writer();

    ~Writer();

    void writeall(const std::unique_ptr<System> &system,
                  const std::unique_ptr<Symmetry> &symmetry,
                  const std::unique_ptr<Cluster> &cluster,
                  const std::unique_ptr<Constraint> &constraint,
                  const std::unique_ptr<Fcs> &fcs,
                  const std::unique_ptr<Optimize> &optimize,
                  const std::unique_ptr<Files> &files,
                  const int verbosity) const;

    void write_input_vars(const std::unique_ptr<System> &system,
                          const std::unique_ptr<Symmetry> &symmetry,
                          const std::unique_ptr<Cluster> &cluster,
                          const std::unique_ptr<Displace> &displace,
                          const std::unique_ptr<Fcs> &fcs,
                          const std::unique_ptr<Constraint> &constraint,
                          const std::unique_ptr<Optimize> &optimize,
                          const std::unique_ptr<Files> &files,
                          const std::string run_mode) const;

    void write_displacement_pattern(const std::unique_ptr<Cluster> &cluster,
                                    const std::unique_ptr<Displace> &displace,
                                    const std::string prefix,
                                    const int verbosity) const;

    void save_fcs_with_specific_format(const std::string fcs_format,
                                       const std::unique_ptr<System> &system,
                                       const std::unique_ptr<Symmetry> &symmetry,
                                       const std::unique_ptr<Cluster> &cluster,
                                       const std::unique_ptr<Constraint> &constraint,
                                       const std::unique_ptr<Fcs> &fcs,
                                       const std::unique_ptr<Optimize> &optimize,
                                       const std::unique_ptr<Files> &files,
                                       const int verbosity) const;

    void set_fcs_save_flag(const std::string key_str, const int val);

    int get_fcs_save_flag(const std::string key_str);

    void set_filename_fcs(const std::string filename_in);

    std::string get_filename_fcs() const;

    void set_output_maxorder(const int maxorder);

    int get_output_maxorder() const;

    void set_input_vars(const std::map<std::string, std::string> &input_var_dict);

    std::string get_input_var(const std::string &key) const;

private:
    void write_force_constants(const std::unique_ptr<Cluster> &cluster,
                               const std::unique_ptr<Fcs> &fcs,
                               const std::unique_ptr<Symmetry> &symmetry,
                               const double *fcs_vals,
                               const int verbosity,
                               const std::string fname_save) const;

    void save_fcs_alamode_oldformat(const std::unique_ptr<System> &system,
                                    const std::unique_ptr<Symmetry> &symmetry,
                                    const std::unique_ptr<Cluster> &cluster,
                                    const std::unique_ptr<Fcs> &fcs,
                                    const std::unique_ptr<Constraint> &constraint,
                                    const double *fcs_vals,
                                    const std::string fname_dfset,
                                    const std::string fname_fcs,
                                    const int verbosity) const;

    void save_fcs_alamode(const std::unique_ptr<System> &system,
                          const std::unique_ptr<Symmetry> &symmetry,
                          const std::unique_ptr<Cluster> &cluster,
                          const std::unique_ptr<Fcs> &fcs,
                          const std::unique_ptr<Constraint> &constraint,
                          const double *fcs_vals,
                          const std::string fname_dfset,
                          const std::string fname_fcs,
                          const int verbosity) const;

    void write_hessian(const std::unique_ptr<System> &system,
                       const std::unique_ptr<Symmetry> &symmetry,
                       const std::unique_ptr<Fcs> &fcs,
                       const std::string fname_out,
                       const int verbosity) const;

    void save_fc2_QEfc_format(const std::unique_ptr<System> &system,
                              const std::unique_ptr<Symmetry> &symmetry,
                              const std::unique_ptr<Fcs> &fcs,
                              const std::string fname_out,
                              const int verbosity) const;

    void save_fc3_thirdorderpy_format(const std::unique_ptr<System> &system,
                                      const std::unique_ptr<Symmetry> &symmetry,
                                      const std::unique_ptr<Cluster> &cluster,
                                      const std::unique_ptr<Constraint> &constraint,
                                      const std::unique_ptr<Fcs> &fcs,
                                      const std::string fname_out,
                                      const int verbosity) const;

    std::string easyvizint(int) const;

    std::string double2string(double,
                              int nprec = 15) const;

    std::map<std::string, int> save_format_flags;
    int output_maxorder;
    std::string file_fcs, file_hes;
    std::string filename_fcs;

    std::map<std::string, std::string> input_variables;

};
}
