/*
 system.h

 Copyright (c) 2014 Terumasa Tadano

 This file is distributed under the terms of the MIT license.
 Please see the file 'LICENCE.txt' in the root directory
 or http://opensource.org/licenses/mit-license.php for information.
*/

#pragma once

#include <string>
#include <vector>
#include "timer.h"
#include <Eigen/Core>

namespace ALM_NS {
class AtomType {
public:
    int element;
    double magmom;

    bool operator<(const AtomType &a) const
    {
        if (this->element < a.element) {
            return true;
        }
        if (this->element == a.element) {
            return this->magmom < a.magmom;
        }
        return false;
    }
};

class Cell {
public:
    Eigen::Matrix3d lattice_vector;
    Eigen::Matrix3d reciprocal_lattice_vector;
    double volume;
    size_t number_of_atoms;
    size_t number_of_elems;
    std::vector<int> kind;
    Eigen::MatrixXd x_fractional;
    Eigen::MatrixXd x_cartesian;
};

class Spin {
public:
    bool lspin;
    int time_reversal_symm;
    int noncollinear;
    std::vector<std::vector<double>> magmom;
};

class System {
public:
    System();

    ~System();

    void init(const int,
              Timer *);

    void set_basecell(const double lavec_in[3][3],
                      const size_t nat_in,
                      const int *kind_in,
                      const double xf_in[][3]);

    void set_kdname(const std::string *);

    void set_periodicity(const int [3]);

    void set_spin_variables(const size_t nat,
                            const bool,
                            const int,
                            const int,
                            const double (*)[3]);

    void set_str_magmom(std::string);

    void set_transformation_matrices(const double transmat_to_super_in[3][3],
                                     const double transmat_to_prim_in[3][3]);

    const Cell &get_supercell() const;

    const Cell &get_primcell() const;

    const Cell &get_inputcell() const;

    double ***get_x_image() const;

    int *get_exist_image() const;

    std::string *get_kdname() const;

    int *get_periodicity() const;

    const Spin &get_spin() const;

    const std::string &get_str_magmom() const;

    const std::vector<std::vector<unsigned int>> &get_atomtype_group() const;

private:
    // Variables for geometric structure
    Cell supercell, primcell, inputcell;

    // Transformation matrices
    Eigen::Matrix3d transmat_to_super, transmat_to_prim;

    std::string *kdname;
    int *is_periodic; // is_periodic[3];
    double ***x_image;
    int *exist_image;

    // Variables for spins
    Spin spin_input, spin_super, spin_prim;
    std::string str_magmom;

    // concatenate atomic kind and magmom (only for collinear case)
    std::vector<std::vector<unsigned int>> atomtype_group;

    enum LatticeType {
        Direct, Reciprocal
    };

    void build_cells();

    void build_supercell();

    void build_primcell();

    void set_reciprocal_latt(const Eigen::Matrix3d &lavec_in,
                             Eigen::Matrix3d &rlavec_out) const;

    void set_default_variables();

    void deallocate_variables();

    double volume(const Eigen::Matrix3d &mat_in,
                  const LatticeType latttype_in) const;

    void set_atomtype_group();

    void generate_coordinate_of_periodic_images();

    void print_structure_stdout(const int verbosity);

    void print_magmom_stdout() const;
};
}
