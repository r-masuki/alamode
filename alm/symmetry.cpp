/*
 symmetry.cpp

 Copyright (c) 2014-2018 Terumasa Tadano

 This file is distributed under the terms of the MIT license.
 Please see the file 'LICENCE.txt' in the root directory
 or http://opensource.org/licenses/mit-license.php for information.
*/

#include "symmetry.h"
#include "error.h"
#include "cluster.h"
#include "memory.h"
#include "system.h"
#include "timer.h"
#include <cmath>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <Eigen/Core>
#include <Eigen/LU>

extern "C" {
#include "spglib.h"
}

using namespace ALM_NS;

Symmetry::Symmetry()
{
    set_default_variables();
}

Symmetry::~Symmetry()
{
    deallocate_variables();
}

double Symmetry::get_tolerance() const
{
    return tolerance;
}

void Symmetry::set_tolerance(const double tolerance_in)
{
    tolerance = tolerance_in;
}

int Symmetry::get_print_symmetry() const
{
    return printsymmetry;
}

void Symmetry::set_print_symmetry(const int printsymmetry_in)
{
    printsymmetry = printsymmetry_in;
}

const std::vector<Maps> &Symmetry::get_map_super_to_trueprim() const
{
    return map_super_to_trueprim;
}

const std::vector<std::vector<int>> &Symmetry::get_map_trueprim_to_super() const
{
    return map_trueprim_to_super;
}

const std::vector<SymmetryOperation> &Symmetry::get_symmetry_data(const std::string cell) const
{
    if (cell == "prim" || cell == "primitive") return symmetry_data_prim;
    return symmetry_data_super;
}

const std::vector<std::vector<int>> &Symmetry::get_map_sym() const
{
    return map_sym;
}

const std::vector<int> &Symmetry::get_symnum_tran(const std::string cell) const
{
    if (cell == "prim" || cell == "primitive") return symnum_tran_prim;
    return symnum_tran_super;
}

size_t Symmetry::get_nsym(const std::string cell) const
{
    if (cell == "prim" || cell == "primitive") return nsym_prim;
    return nsym_super;
}

size_t Symmetry::get_ntran(const std::string cell) const
{
    if (cell == "prim" || cell == "primitive") return ntran_prim;
    return ntran_super;
}

size_t Symmetry::get_nat_prim() const
{
    return nat_trueprim;
}

void Symmetry::init(const std::unique_ptr<System> &system,
                    const int verbosity,
                    std::unique_ptr<Timer> &timer)
{
    timer->start_clock("symmetry");

    if (verbosity > 0) {
        std::cout << " ==========\n";
        std::cout << "  SYMMETRY \n";
        std::cout << " ==========\n\n";
    }

    // nat_trueprim, ntran_super, nsym_super, symmetry_data_super, symnum_tran_super are set here.
    // Symmdata[nsym_super], symnum_tran_super[ntran_super]
    setup_symmetry_operation(system->get_primcell(),
                             system->get_spin("primitive"),
                             system->get_atomtype_group("primitive"),
                             system->get_supercell(),
                             system->get_spin("super"),
                             system->get_atomtype_group("super"),
                             system->get_periodicity(),
                             verbosity);


    map_sym.clear();
    map_sym.shrink_to_fit();
    map_sym.resize(system->get_supercell().number_of_atoms, std::vector<int>(nsym_super));

    map_trueprim_to_super.clear();
    map_trueprim_to_super.shrink_to_fit();
    map_trueprim_to_super.resize(nat_trueprim, std::vector<int>(ntran_super));

    // symmetry_data_super is updated here.
    update_symmetry_operations_supercell(system->get_primcell(),
                                         symmetry_data_prim,
                                         system->get_supercell(),
                                         symmetry_data_super,
                                         atomgroup_super);

    symnum_tran_prim.clear();
    symnum_tran_super.clear();
    for (auto i = 0; i < nsym_prim; ++i) {
        if (symmetry_data_prim[i].is_translation) symnum_tran_prim.push_back(i);
    }
    for (auto i = 0; i < nsym_super; ++i) {
        if (symmetry_data_super[i].is_translation) symnum_tran_super.push_back(i);
    }

    gen_mapping_information(system->get_supercell(),
                            system->get_atomtype_group(),
                            symmetry_data_super,
                            system->get_primcell());

    if (printsymmetry) {
        print_symmetry_infomation(verbosity);
    }

    if (verbosity > 0) {
        print_symminfo_stdout();
        timer->print_elapsed();
        std::cout << " -------------------------------------------------------------------" << std::endl;
        std::cout << std::endl;
    }



    timer->stop_clock("symmetry");
}

void Symmetry::set_default_variables()
{
    // Default values
    nsym_super = 0;
    printsymmetry = false;
    ntran_super = 0;
    nat_trueprim = 0;
    tolerance = 1e-3;
}

void Symmetry::deallocate_variables() {}

void Symmetry::setup_symmetry_operation(const Cell &pcell,
                                        const Spin &spin_prim,
                                        const std::vector<std::vector<unsigned int>> &atomtype_prim,
                                        const Cell &scell,
                                        const Spin &spin_super,
                                        const std::vector<std::vector<unsigned int>> &atomtype_super,
                                        const int is_periodic[3],
                                        const int verbosity)
{
    size_t i, j;

    symmetry_data_super.clear();
    symmetry_data_prim.clear();

    // First, generate space group operations using the primitive cell.
    // Please be noted that the input pcell might not be a true primitive cell
    // because one can give PRIMCELL value whic does not necessary transform the
    // input cell into a true primitive cell.
    if (spin_prim.lspin && spin_prim.noncollinear) {
        if (verbosity > 0) {
            std::cout << "  Switch to the internal symmetry finder from spglib when NONCOLLINEAR = 1.\n";
        }
        findsym_alm(pcell, atomtype_prim, spin_prim, symmetry_data_prim);
        findsym_alm(scell, atomtype_super, spin_super, symmetry_data_super);
    } else {
        std::string spgsymbol;
        const auto spgnum = findsym_spglib(pcell,
                                           atomtype_prim,
                                           spin_prim,
                                           spgsymbol,
                                           symmetry_data_prim);

        if (verbosity > 0) {
            std::cout << "  Space group: " << spgsymbol << " ("
                      << std::setw(3) << spgnum << ")" << std::endl;
        }

        const auto spgnum2 = findsym_spglib(scell,
                                            atomtype_super,
                                            spin_super,
                                            spgsymbol,
                                            symmetry_data_super);
        if (spgnum != spgnum2) {
            exit("setup_symmetry_operation",
                 "The space group of the primitive and super cells are different. Something is wrong.");
        }
    }

    // The order in symmetry_data_prim changes for each run because it was generated
    // with OpenMP. Therefore, we sort the list here to have the same result at all time.
    std::sort(symmetry_data_prim.begin() + 1, symmetry_data_prim.end());
    nsym_prim = symmetry_data_prim.size();
    nsym_super = symmetry_data_super.size();
    ntran_prim = 0;
    for (i = 0; i < nsym_prim; ++i) {
        if (symmetry_data_prim[i].is_translation) ++ntran_prim;
    }
    ntran_super = 0;
    for (i = 0; i < nsym_super; ++i) {
        if (symmetry_data_super[i].is_translation) ++ntran_super;
    }

    if (ntran_prim > 1) {
        warn("setup_symmetry_operation",
             "The input primitive cell is NOT a true primitive cell.\n"
             " The calculation continues, but please check again if the input PRIMCELL values are\n"
             " correct.\n");
    }
    if (pcell.number_of_atoms % ntran_prim) {
        exit("setup_symmetry_operation",
             "nat_primitive != nat_trueprim * ntran_prim. Something is wrong with the primitive cell structure.");
    }
    if (scell.number_of_atoms % ntran_super) {
        exit("setup_symmetry_operation",
             "nat_super != nat_trueprim * ntran_super. Something is wrong with the supercell structure.");
    }

    nat_trueprim = pcell.number_of_atoms / ntran_prim;
    const auto nat_trueprim2 = scell.number_of_atoms / ntran_super;

    if (nat_trueprim != nat_trueprim2) {
        exit("setup_symmetry_operation",
             "The number of atoms included in a true primitive cell is different\n"
             " between the SUPERCELL and PRIMCELL. Something is wrong.");
    }
}

void Symmetry::findsym_alm(const Cell &cell,
                           const std::vector<std::vector<unsigned int>> &atomtype_group,
                           const Spin &spin,
                           std::vector<SymmetryOperation> &symm_out) const
{
    std::vector<RotationMatrix> LatticeSymmList;

    // Generate rotational matrices that don't change the metric tensor
    LatticeSymmList.clear();
    find_lattice_symmetry(cell.lattice_vector, LatticeSymmList);

    // Generate all the space group operations with translational vectors
    // The data is stored in symmetry_data_super.
    symm_out.clear();
    find_crystal_symmetry(cell,
                          atomtype_group,
                          spin,
                          LatticeSymmList,
                          symm_out);

    LatticeSymmList.clear();
}

void Symmetry::find_lattice_symmetry(const Eigen::Matrix3d &aa,
                                     std::vector<RotationMatrix> &LatticeSymmList) const
{
    /*
    Find the rotational matrices that leave the metric tensor invariant.

    Metric tensor G = (g)_{ij} = a_{i} * a_{j} is invariant under crystal symmetry operations T,
    i.e. T^{t}GT = G. Since G can be written as G = A^{t}A, the invariance condition is given by
    (AT)^{t}(AT) = G0 (original).
    */

    int i, j, k;
    int m11, m12, m13, m21, m22, m23, m31, m32, m33;

    auto nsym_tmp = 0;
    int mat_tmp[3][3];
    double det, res;
    Eigen::Matrix3d rot_tmp;
    Eigen::Matrix3d aa_rot;

    double metric_tensor[3][3];
    double metric_tensor_rot[3][3];


    for (i = 0; i < 3; ++i) {
        for (j = 0; j < 3; ++j) {
            metric_tensor[i][j] = 0.0;
            for (k = 0; k < 3; ++k) {
                metric_tensor[i][j] += aa(k, i) * aa(k, j);
            }
        }
    }

    for (i = 0; i < 3; ++i) {
        for (j = 0; j < 3; ++j) {
            if (i == j) {
                mat_tmp[i][i] = 1;
            } else {
                mat_tmp[i][j] = 0;
            }
        }
    }

    // Identity matrix should be the first entry.
    LatticeSymmList.emplace_back(mat_tmp);

    for (m11 = -1; m11 <= 1; ++m11) {
        for (m12 = -1; m12 <= 1; ++m12) {
            for (m13 = -1; m13 <= 1; ++m13) {
                for (m21 = -1; m21 <= 1; ++m21) {
                    for (m22 = -1; m22 <= 1; ++m22) {
                        for (m23 = -1; m23 <= 1; ++m23) {
                            for (m31 = -1; m31 <= 1; ++m31) {
                                for (m32 = -1; m32 <= 1; ++m32) {
                                    for (m33 = -1; m33 <= 1; ++m33) {

                                        if (m11 == 1 && m12 == 0 && m13 == 0 &&
                                            m21 == 0 && m22 == 1 && m23 == 0 &&
                                            m31 == 0 && m32 == 0 && m33 == 1)
                                            continue;

                                        det = m11 * (m22 * m33 - m32 * m23)
                                              - m21 * (m12 * m33 - m32 * m13)
                                              + m31 * (m12 * m23 - m22 * m13);

                                        if (det != 1 && det != -1) continue;

                                        rot_tmp(0, 0) = m11;
                                        rot_tmp(0, 1) = m12;
                                        rot_tmp(0, 2) = m13;
                                        rot_tmp(1, 0) = m21;
                                        rot_tmp(1, 1) = m22;
                                        rot_tmp(1, 2) = m23;
                                        rot_tmp(2, 0) = m31;
                                        rot_tmp(2, 1) = m32;
                                        rot_tmp(2, 2) = m33;

                                        // Here, aa_rot = aa * rot_tmp is correct.
                                        aa_rot = aa * rot_tmp;

                                        for (i = 0; i < 3; ++i) {
                                            for (j = 0; j < 3; ++j) {
                                                metric_tensor_rot[i][j] = 0.0;
                                                for (k = 0; k < 3; ++k) {
                                                    metric_tensor_rot[i][j] += aa_rot(k, i) * aa_rot(k, j);
                                                }
                                            }
                                        }

                                        res = 0.0;
                                        for (i = 0; i < 3; ++i) {
                                            for (j = 0; j < 3; ++j) {
                                                res += std::pow(metric_tensor[i][j] - metric_tensor_rot[i][j], 2.0);
                                            }
                                        }

                                        // Metric tensor is invariant under symmetry operations.

                                        if (res < tolerance * tolerance) {
                                            ++nsym_tmp;
                                            for (i = 0; i < 3; ++i) {
                                                for (j = 0; j < 3; ++j) {
                                                    mat_tmp[i][j] = static_cast<int>(rot_tmp(i, j));
                                                }
                                            }
                                            LatticeSymmList.emplace_back(mat_tmp);
                                        }

                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (LatticeSymmList.size() > 48) {
        exit("find_lattice_symmetry", "Number of lattice symmetry is larger than 48.");
    }
}

void Symmetry::find_crystal_symmetry(const Cell &cell,
                                     const std::vector<std::vector<unsigned int>> &atomtype_group,
                                     const Spin &spin,
                                     const std::vector<RotationMatrix> &LatticeSymmList,
                                     std::vector<SymmetryOperation> &symm_out) const
{
    unsigned int i, j;
    unsigned int iat, jat, kat, lat;
    Eigen::Vector3d x_rot, x_tmp;
    Eigen::Matrix3d rot;
    Eigen::Matrix3i rot_int;
    Eigen::Matrix3d rot_tmp, rot_cart;
    Eigen::Vector3d mag, mag_rot;
    Eigen::Vector3d tran;
    Eigen::Vector3d x_rot_tmp;
    double tmp[3];
    double diff;
    const auto nclass = atomtype_group.size();

    int ii;
    size_t jj, kk;
    unsigned int itype;

    bool is_found;
    bool isok;
    bool mag_sym1, mag_sym2;
    bool is_identity_matrix;

    const Eigen::Matrix3d mat_identity = Eigen::Matrix3d::Identity();

    // Add identity matrix first.
    for (i = 0; i < 3; ++i) {
        for (j = 0; j < 3; ++j) {
            if (i == j) {
                rot_int(i, j) = 1;
                rot_cart(i, j) = 1.0;
            } else {
                rot_int(i, j) = 0;
                rot_cart(i, j) = 0.0;
            }
        }
        tran[i] = 0.0;
    }

    symm_out.emplace_back(rot_int,
                          tran,
                          rot_cart,
                          is_compatible(rot_int),
                          is_compatible(rot_cart),
                          is_translation(rot_int));

    for (auto &it_latsym: LatticeSymmList) {

        iat = atomtype_group[0][0];
        for (i = 0; i < 3; ++i) {
            for (j = 0; j < 3; ++j) {
                rot(i, j) = static_cast<double>(it_latsym.mat(i, j));
            }
        }

        for (i = 0; i < 3; ++i) x_tmp[i] = cell.x_fractional(iat, i);
        //rotvec(x_rot, x_tmp, rot);
        x_rot = rot * x_tmp;

#ifdef _OPENMP
#pragma omp parallel for private(jat, tran, isok, kat, x_tmp, x_rot_tmp, is_found, lat, tmp, diff, \
    i, j, itype, jj, kk, is_identity_matrix, mag, mag_rot, rot_tmp, rot_cart, mag_sym1, mag_sym2)
#endif
        for (ii = 0; ii < atomtype_group[0].size(); ++ii) {
            jat = atomtype_group[0][ii];

            for (i = 0; i < 3; ++i) {
                tran[i] = cell.x_fractional(jat, i) - x_rot[i];
                tran[i] = tran[i] - nint(tran[i]);
            }

//            if ((std::abs(tran[0]) > eps12 && !is_periodic[0]) ||
//                (std::abs(tran[1]) > eps12 && !is_periodic[1]) ||
//                (std::abs(tran[2]) > eps12 && !is_periodic[2]))
//                continue;

            is_identity_matrix = (rot - mat_identity).norm() < eps6;
            if (is_identity_matrix) continue;

            isok = true;

            for (itype = 0; itype < nclass; ++itype) {

                for (jj = 0; jj < atomtype_group[itype].size(); ++jj) {

                    kat = atomtype_group[itype][jj];

                    for (i = 0; i < 3; ++i) x_tmp[i] = cell.x_fractional(kat, i);
                    x_rot_tmp = rot * x_tmp;

                    for (i = 0; i < 3; ++i) {
                        x_rot_tmp[i] += tran[i];
                    }

                    is_found = false;

                    for (kk = 0; kk < atomtype_group[itype].size(); ++kk) {

                        lat = atomtype_group[itype][kk];

                        for (i = 0; i < 3; ++i) {
                            tmp[i] = std::fmod(std::abs(cell.x_fractional(lat, i) - x_rot_tmp[i]), 1.0);
                            tmp[i] = std::min<double>(tmp[i], 1.0 - tmp[i]);
                        }
                        diff = tmp[0] * tmp[0] + tmp[1] * tmp[1] + tmp[2] * tmp[2];
                        if (diff < tolerance * tolerance) {
                            is_found = true;
                            break;
                        }
                    }

                    if (!is_found) isok = false;
                }
            }

            if (isok) {
                rot_tmp = rot * cell.reciprocal_lattice_vector;
                rot_cart = cell.lattice_vector * rot_tmp * inv_tpi;

                if (spin.lspin && spin.noncollinear) {

                    for (i = 0; i < 3; ++i) {
                        mag[i] = spin.magmom[jat][i];
                        mag_rot[i] = spin.magmom[iat][i];
                    }

                    mag_rot = rot_cart * mag_rot;

                    // In the case of improper rotation, the factor -1 should be multiplied
                    // because the inversion operation doesn't flip the spin.
                    if (!is_proper(rot_cart)) {
                        for (i = 0; i < 3; ++i) {
                            mag_rot[i] = -mag_rot[i];
                        }
                    }

                    mag_sym1 = (std::pow(mag[0] - mag_rot[0], 2.0)
                                + std::pow(mag[1] - mag_rot[1], 2.0)
                                + std::pow(mag[2] - mag_rot[2], 2.0)) < eps6;

                    mag_sym2 = (std::pow(mag[0] + mag_rot[0], 2.0)
                                + std::pow(mag[1] + mag_rot[1], 2.0)
                                + std::pow(mag[2] + mag_rot[2], 2.0)) < eps6;

                    if (!mag_sym1 && !mag_sym2) {
                        isok = false;
                    } else if (!mag_sym1 && mag_sym2 && !spin.time_reversal_symm) {
                        isok = false;
                    }
                }
            }

            if (isok) {
#ifdef _OPENMP
#pragma omp critical
#endif
                symm_out.emplace_back(it_latsym.mat,
                                      tran,
                                      rot_cart,
                                      is_compatible(it_latsym.mat),
                                      is_compatible(rot_cart),
                                      is_translation(it_latsym.mat));
            }
        }
    }
}

int Symmetry::findsym_spglib(const Cell &cell,
                             const std::vector<std::vector<unsigned int>> &atomtype_group,
                             const Spin &spin,
                             std::string &spgsymbol,
                             std::vector<SymmetryOperation> &symm_out) const
{
    size_t i, j;
    double (*position)[3];
    double (*translation)[3];
    int (*rotation)[3][3];
    char symbol[11];
    double aa_tmp[3][3];
    int *types_tmp;

    const auto nat = cell.number_of_atoms;

    allocate(position, nat);
    allocate(types_tmp, nat);

    for (i = 0; i < 3; ++i) {
        for (j = 0; j < 3; ++j) {
            aa_tmp[i][j] = cell.lattice_vector(i, j);
        }
    }
    for (i = 0; i < nat; ++i) {
        for (j = 0; j < 3; ++j) {
            position[i][j] = cell.x_fractional(i, j);
        }
    }

    if (spin.lspin) {
        for (i = 0; i < atomtype_group.size(); ++i) {
            for (j = 0; j < atomtype_group[i].size(); ++j) {
                types_tmp[atomtype_group[i][j]] = i;
            }
        }
    } else {
        for (i = 0; i < nat; ++i) {
            types_tmp[i] = cell.kind[i];
        }
    }

    // First find the number of symmetry operations
    auto nsym_out = spg_get_multiplicity(aa_tmp, position, types_tmp, nat, tolerance);

    if (nsym_out == 0) exit("findsym_spglib", "Error occured in spg_get_multiplicity");

    allocate(translation, nsym_out);
    allocate(rotation, nsym_out);

    // Store symmetry operations
    nsym_out = spg_get_symmetry(rotation, translation, nsym_out,
                                aa_tmp, position, types_tmp, nat, tolerance);

    const auto spgnum = spg_get_international(symbol, aa_tmp, position, types_tmp, nat, tolerance);
    spgsymbol = std::string(symbol);

    // Copy symmetry information
    symm_out.clear();
    Eigen::Matrix3d rot_cartesian;
    Eigen::Matrix3i rot_int;
    Eigen::Vector3d trans_tmp;

    for (i = 0; i < nsym_out; ++i) {

        for (j = 0; j < 3; ++j) {
            for (auto k = 0; k < 3; ++k) {
                rot_int(j, k) = rotation[i][j][k];
            }
            trans_tmp(j) = translation[i][j];
        }

        symop_in_cart(rot_cartesian,
                      rot_int,
                      cell.lattice_vector,
                      cell.reciprocal_lattice_vector);

        symm_out.emplace_back(rot_int,
                              trans_tmp,
                              rot_cartesian,
                              is_compatible(rotation[i]),
                              is_compatible(rot_cartesian),
                              is_translation(rotation[i]));

    }

    deallocate(rotation);
    deallocate(translation);
    deallocate(types_tmp);
    deallocate(position);

    return spgnum;
}


void Symmetry::symop_in_cart(Eigen::Matrix3d &rot_cart,
                             const Eigen::Matrix3i &rot_lattice,
                             const Eigen::Matrix3d &lavec,
                             const Eigen::Matrix3d &rlavec) const
{
    int i, j;
    Eigen::Matrix3d sym_tmp, tmp;

    for (i = 0; i < 3; ++i) {
        for (j = 0; j < 3; ++j) {
            sym_tmp(i, j) = static_cast<double>(rot_lattice(i, j));
        }
    }
    rot_cart = lavec * sym_tmp * rlavec * inv_tpi;
}


void Symmetry::print_symminfo_stdout() const
{
    std::cout << "  Number of symmetry operations of the primitive cell = "
                << symmetry_data_prim.size() << std::endl;
    std::cout << std::endl;
    if (ntran_prim > 1) {
        std::cout << "  The user defined primitive cell is NOT a true primitive cell." << std::endl;
        std::cout << "  It is composed of " << std::setw(5)
                  << ntran_prim << " true primitive cells." << std::endl;
    }

    std::cout << std::endl;
    std::cout << "  ---------------------------------------------------------\n";
    std::cout << "         List of primitive cell translation vectors        \n";
    std::cout << "  ---------------------------------------------------------\n\n";
    std::cout << "  ┌────────┬───────────────────┬──────────────────────────┐\n";
    std::cout << "  │ Cell # │   Shift  vector   │        Atom index        │\n";
    std::cout << "  │        │ (Primitive basis) │(Numbers in the supercell)│\n";
    std::cout << "  ├────────┼───────────────────┼──────────────────────────┤\n";

    auto natom_per_primitive = atomgroup_super[0].map_index_p2s.size();
    constexpr int nentry_per_line = 5;
    auto nlines = natom_per_primitive / nentry_per_line;

    if (natom_per_primitive > nlines * nentry_per_line) ++nlines;

    for (auto i = 0; i < atomgroup_super.size(); ++i) {

        auto counter = 0;

        for (auto j = 0; j < nlines; ++j) {
            if (j == 0) {
                std::cout << "  │" << std::setw(7) << i + 1 << " │";
                std::cout.width(4);
                std::cout << "(" << std::setw(2)
                << atomgroup_super[i].shift_vector.transpose()
                << ")";
                std::cout.width(6);
                std::cout << "│";
            } else {
                std::cout << "  │";
                std::cout.width(11);
                std::cout << "│";
                std::cout.width(22);
                std::cout << "│";
            }

            for (auto k = 0; k < nentry_per_line; ++k) {
                counter = nentry_per_line * j + k;
                if (counter < natom_per_primitive) {
                    std::cout << std::setw(5) << atomgroup_super[i].map_index_p2s.at(counter) + 1;
                } else {
                    std::cout << std::setw(5) << "";
                }
            }
            std::cout << " │\n";
        }
        if (i == (atomgroup_super.size() - 1)) {
            std::cout << "  └────────┴───────────────────┴──────────────────────────┘\n\n";
        } else {
            std::cout << "  ├────────┼───────────────────┼──────────────────────────┤\n";
        }
    }
}

void Symmetry::update_symmetry_operations_supercell(const ALM_NS::Cell &cell_prim,
                                                    const std::vector<SymmetryOperation> &symm_prim,
                                                    const ALM_NS::Cell &cell_super,
                                                    std::vector<SymmetryOperation> &symm_super,
                                                    std::vector<PrimitiveGroup> &atomgroup_out) const
{
    // Create the symm_super by replicating the symmetry operations generated for
    // the primitive cell (symm_prim) and the shift vectors.
    // This operation is performed to keep the order of the input atom indices as much as possible.

    Eigen::Vector3d xtmp, xtmp2, xdiff, tran_d;
    Eigen::Vector3i tran;
    Eigen::MatrixXd x_super;
    std::vector<int> atom_num_prim;
    std::vector<std::vector<int>> trans_vecs;
    const Eigen::Matrix3d invlavec_p = cell_prim.lattice_vector.transpose().inverse();
    const Eigen::Matrix3d transform_basis_primitive_to_super
    = cell_super.lattice_vector.inverse() * cell_prim.lattice_vector;

    x_super = cell_super.x_cartesian * invlavec_p;

    for (auto i = 0; i < cell_super.number_of_atoms; ++i) {
        xtmp = x_super.row(i);

        auto iloc = -1;
        for (auto j = 0; j < cell_prim.number_of_atoms; ++j) {
            xtmp2 = cell_prim.x_fractional.row(j);
            xdiff = (xtmp - xtmp2).unaryExpr([](const double x) { return x-nint(x); });

            if (xdiff.norm() < eps6) {
                tran_d = (xtmp - xtmp2).unaryExpr([](const double x) {return static_cast<double>(nint(x));});
                // First move back to the fractional coordinate of the supercell and
                // make sure that the shift vectors are in the 0<=x<1 region in that basis.
                tran_d = transform_basis_primitive_to_super * tran_d;
                tran_d = tran_d.unaryExpr([](const double x) { return std::fmod(x, 1.0); });
                for (auto k = 0; k < 3; ++k) {
                    if (tran_d[k] < -eps6) tran_d[k] += 1.0;
                }
                // Then, transform it back to the components in the primitive cell basis.
                // All components should be integer.
                tran_d = transform_basis_primitive_to_super.inverse() * tran_d;
                tran = tran_d.unaryExpr([](const double x) {return nint(x);});
                iloc = j;
                break;
            }
        }

        if (iloc == -1) {
            exit("update_symmetry_operations_supercell",
                 "An equivalent atom not found.");
        } else {
            atom_num_prim.emplace_back(iloc);
            std::vector<int> vtmp(&tran[0], tran.data()+tran.cols()*tran.rows());
            trans_vecs.emplace_back(vtmp);
        }
    }

    std::set<std::vector<int>> unique_shifts_set;
    std::vector<std::vector<int>> unique_shifts_vec;

    for (auto i = 0; i < cell_super.number_of_atoms; ++i) {
        if (unique_shifts_set.find(trans_vecs[i]) == unique_shifts_set.end()) {
            unique_shifts_set.insert(trans_vecs[i]);
            unique_shifts_vec.emplace_back(trans_vecs[i]);
        }
    }

    if (unique_shifts_vec.size() != (cell_super.number_of_atoms / cell_prim.number_of_atoms)) {
        exit("update_symmetry_operations_supercell",
             "The number of primitive translations is inconsistent.");
    }

    // Create atomgroup_out for later use
    atomgroup_out.clear();

    std::map<int, int> map_index;

    for (auto i = 0; i < unique_shifts_vec.size(); ++i){
        map_index.clear();

        for (auto j = 0; j < 3; ++j) tran[j] = unique_shifts_vec[i][j];

        for (auto j = 0; j < cell_super.number_of_atoms; ++j) {
            if (unique_shifts_vec[i] == trans_vecs[j]) {
                map_index.insert({atom_num_prim[j], j});
            }
        }
        atomgroup_out.emplace_back(map_index, tran);
    }

    // Finally, update symm_super.
    symm_super.clear();

    Eigen::Matrix3d rot_cart, rot_latt;
    Eigen::Matrix3i rot_latt_int;

    for (const auto &it_tran : unique_shifts_vec) {
        for (const auto &it_symm : symm_prim) {
            tran_d = it_symm.tran; // lattice basis of the primitive cell
            for (auto k = 0; k < 3; ++k) tran_d[k] += static_cast<double>(it_tran[k]); // add primitive lattice translation

            rot_cart = it_symm.rotation_cart; // Common to the primitive cell and supercell.

            // Rotation operation in the lattice basis of the supercell
            rot_latt = cell_super.lattice_vector.inverse() * rot_cart * cell_super.lattice_vector;
            for (auto k = 0; k < 3; ++k) {
                for (auto m = 0; m < 3; ++m) {
                    rot_latt_int(k,m) = nint(rot_latt(k,m));
                    if (std::abs(rot_latt(k,m)-static_cast<double>(rot_latt_int(k,m))) > eps6) {
                        exit("update_symmetry_operations_supercell",
                             "The components of the rotation matrix in "
                             "the supercell lattice basis must be integer.");
                    }
                }
            }

            // Translation in the supercell lattice basis
            tran_d = transform_basis_primitive_to_super * tran_d;
            tran_d = tran_d.unaryExpr([](const double x) { return std::fmod(x, 1.0); });
            for (auto k = 0; k < 3; ++k) {
                if (tran_d[k] < -eps6) tran_d[k] += 1.0;
            }
            symm_super.emplace_back(rot_latt_int,
                                  tran_d,
                                  rot_cart,
                                  is_compatible(rot_latt_int),
                                  is_compatible(rot_cart),
                                  is_translation(rot_latt_int));
        }
    }
}

void Symmetry::gen_mapping_information(const Cell &scell,
                                       const std::vector<std::vector<unsigned int>> &atomtype_group_super,
                                       const std::vector<SymmetryOperation> &symm_super,
                                       const Cell &pcell)
{
    int isym;
    size_t iat, jat;
    size_t i, j;
    size_t itype;
    size_t ii, jj;
    double xnew[3], x_tmp[3];
    double tmp[3], diff;
    double rot_double[3][3];

    auto nsym_tmp = symm_super.size();

    for (iat = 0; iat < scell.number_of_atoms; ++iat) {
        for (isym = 0; isym < symm_super.size(); ++isym) {
            map_sym[iat][isym] = -1;
        }
    }

    const auto natomtypes = atomtype_group_super.size();

#ifdef _OPENMP
#pragma omp parallel for private(i, j, rot_double, itype, ii, iat, x_tmp, xnew, jj, jat, tmp, diff, isym)
#endif
    for (isym = 0; isym < nsym_tmp; ++isym) {

        for (i = 0; i < 3; ++i) {
            for (j = 0; j < 3; ++j) {
                rot_double[i][j] = static_cast<double>(symm_super[isym].rotation(i, j));
            }
        }

        for (itype = 0; itype < natomtypes; ++itype) {

            for (ii = 0; ii < atomtype_group_super[itype].size(); ++ii) {

                iat = atomtype_group_super[itype][ii];

                for (i = 0; i < 3; ++i) x_tmp[i] = scell.x_fractional(iat, i);
                rotvec(xnew, x_tmp, rot_double);

                for (i = 0; i < 3; ++i) xnew[i] += symm_super[isym].tran[i];

                for (jj = 0; jj < atomtype_group_super[itype].size(); ++jj) {

                    jat = atomtype_group_super[itype][jj];

                    for (i = 0; i < 3; ++i) {
                        tmp[i] = std::fmod(std::abs(scell.x_fractional(jat, i) - xnew[i]), 1.0);
                        tmp[i] = std::min<double>(tmp[i], 1.0 - tmp[i]);
                    }
                    diff = tmp[0] * tmp[0] + tmp[1] * tmp[1] + tmp[2] * tmp[2];
                    if (diff < tolerance * tolerance) {
                        map_sym[iat][isym] = jat;
                        break;
                    }
                }
                if (map_sym[iat][isym] == -1) {
                    exit("gen_mapping_information",
                         "cannot find symmetry for operation # ",
                         isym + 1);
                }
            }
        }
    }

    // Generate map_trueprim_to_super (true primitive --> super)

    bool *is_checked;
    allocate(is_checked, scell.number_of_atoms);

    for (i = 0; i < scell.number_of_atoms; ++i) is_checked[i] = false;

    jat = 0;
    int atomnum_translated;
    for (iat = 0; iat < scell.number_of_atoms; ++iat) {

        if (is_checked[iat]) continue;
        for (i = 0; i < ntran_super; ++i) {
            atomnum_translated = map_sym[iat][symnum_tran_super[i]];
            map_trueprim_to_super[jat][i] = atomnum_translated;
            is_checked[atomnum_translated] = true;
        }
        ++jat;
    }

    deallocate(is_checked);

    // Generate map_super_to_trueprim (super --> true primitive)

    map_super_to_trueprim.clear();
    map_super_to_trueprim.resize(scell.number_of_atoms);

    for (iat = 0; iat < nat_trueprim; ++iat) {
        for (i = 0; i < ntran_super; ++i) {
            atomnum_translated = map_trueprim_to_super[iat][i];
            map_super_to_trueprim[atomnum_translated].atom_num = iat;
            map_super_to_trueprim[atomnum_translated].tran_num = i;
        }
    }
}

bool Symmetry::is_translation(const int rot[3][3]) const
{
    const auto ret =
            rot[0][0] == 1 && rot[0][1] == 0 && rot[0][2] == 0 &&
            rot[1][0] == 0 && rot[1][1] == 1 && rot[1][2] == 0 &&
            rot[2][0] == 0 && rot[2][1] == 0 && rot[2][2] == 1;

    return ret;
}

bool Symmetry::is_translation(const Eigen::Matrix3i &rot) const
{
    const Eigen::Matrix3i identity = Eigen::Matrix3i::Identity();
    return (rot == identity);
}

template<typename T>
bool Symmetry::is_compatible(const T rot[3][3],
                             const double tolerance_zero) const
{
    auto nfinite = 0;
    double rot_double[3][3];

    for (auto i = 0; i < 3; ++i) {
        for (auto j = 0; j < 3; ++j) {
            rot_double[i][j] = static_cast<double>(rot[i][j]);
            if (std::abs(rot_double[i][j]) > tolerance_zero) ++nfinite;
        }
    }

    return (nfinite == 3);
}

template<typename T>
bool Symmetry::is_compatible(const Eigen::MatrixBase<T> &mat,
                             double tolerance_zero) const
{

    if (mat.rows() != 3 || mat.cols() != 3) return 0;

    auto nfinite = 0;

    for (auto it: mat.reshaped()) {
        if (std::abs(it) > tolerance_zero) ++nfinite;
    }
    return (nfinite == 3);
}

bool Symmetry::is_proper(const Eigen::Matrix3d &rot) const
{
    const auto det = rot.determinant();

    if (std::abs(det - 1.0) < eps12) {
        return true;
    }
    if (std::abs(det + 1.0) < eps12) {
        return false;
    }
    exit("is_proper", "This cannot happen.");
    return false; // dummy to avoid compiler warning
}



//
//void Symmetry::set_primitive_lattice(const double aa[3][3],
//                                     const size_t nat,
//                                     const int *kd,
//                                     double **x,
//                                     double aa_prim[3][3],
//                                     size_t &nat_prim_out,
//                                     int *kd_prim,
//                                     double **x_prim,
//                                     const double symprec) const
//{
//    size_t i, j;
//    int *types_tmp;
//    double (*position)[3];
//
//    for (i = 0; i < 3; ++i) {
//        for (j = 0; j < 3; ++j) {
//            aa_prim[i][j] = aa[i][j];
//        }
//    }
//
//    allocate(position, nat);
//    allocate(types_tmp, nat);
//
//    for (i = 0; i < nat; ++i) {
//        for (j = 0; j < 3; ++j) {
//            position[i][j] = x[i][j];
//        }
//        types_tmp[i] = kd[i];
//    }
//
//    //    nat_trueprim = spg_find_primitive(aa_prim, position, types_tmp, nat_base, symprec);
//    nat_prim_out = spg_standardize_cell(aa_prim,
//                                        position,
//                                        types_tmp,
//                                        nat, 1, 0,
//                                        symprec);
//
//    for (i = 0; i < nat_prim_out; ++i) {
//        for (j = 0; j < 3; ++j) {
//            x_prim[i][j] = position[i][j];
//        }
//        kd_prim[i] = types_tmp[i];
//    }
//
//    deallocate(position);
//    deallocate(types_tmp);
//}

void Symmetry::print_symmetry_infomation(const int verbosity) const
{
    std::string file_sym = "SYMM_INFO";
    std::ofstream ofs_sym;
    if (verbosity > 0) {
        std::cout << "  PRINTSYM = 1: Symmetry information will be stored in SYMM_INFO file."
                  << std::endl << std::endl;
    }

    ofs_sym.open(file_sym.c_str(), std::ios::out);
    ofs_sym << nsym_prim << std::endl;

    for (auto &p: symmetry_data_prim) {
        for (auto i = 0; i < 3; ++i) {
            for (auto j = 0; j < 3; ++j) {
                ofs_sym << std::setw(4) << p.rotation(i, j);
            }
        }
        ofs_sym << "  ";
        for (auto i = 0; i < 3; ++i) {
            ofs_sym << std::setprecision(15) << std::setw(21) << p.tran[i];
        }
        ofs_sym << std::endl;
    }
    ofs_sym.close();
}
