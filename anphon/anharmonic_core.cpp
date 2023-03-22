/*
anharmonic_core.cpp

Copyright (c) 2014 Terumasa Tadano

This file is distributed under the terms of the MIT license.
Please see the file 'LICENCE.txt' in the root directory
or http://opensource.org/licenses/mit-license.php for information.
*/

#include "mpi_common.h"
#include "anharmonic_core.h"
#include "constants.h"
#include "dynamical.h"
#include "error.h"
#include "fcs_phonon.h"
#include "integration.h"
#include "kpoint.h"
#include "mathfunctions.h"
#include "memory.h"
#include "mode_analysis.h"
#include "phonon_dos.h"
#include "system.h"
#include "thermodynamics.h"
#include <boost/lexical_cast.hpp>
#include <algorithm>
#include <vector>

#ifdef _OPENMP

#include <omp.h>

#endif

using namespace PHON_NS;

AnharmonicCore::AnharmonicCore(PHON *phon) : Pointers(phon)
{
    set_default_variables();
}

AnharmonicCore::~AnharmonicCore()
{
    deallocate_variables();
};

void AnharmonicCore::set_default_variables()
{
    quartic_mode = 0;
    use_tuned_ver = true;
    use_triplet_symmetry = true;
    use_quartet_symmetry = true;
    relvec_v3 = nullptr;
    relvec_v4 = nullptr;
    invmass_v3 = nullptr;
    invmass_v4 = nullptr;
    evec_index_v3 = nullptr;
    evec_index_v4 = nullptr;
    fcs_group_v3 = nullptr;
    fcs_group_v4 = nullptr;
    phi3_reciprocal = nullptr;
    phi4_reciprocal = nullptr;
    phase_storage_dos = nullptr;
}

void AnharmonicCore::deallocate_variables()
{
    if (relvec_v3) {
        deallocate(relvec_v3);
    }
    if (relvec_v4) {
        deallocate(relvec_v4);
    }
    if (invmass_v3) {
        deallocate(invmass_v3);
    }
    if (invmass_v4) {
        deallocate(invmass_v4);
    }
    if (evec_index_v3) {
        deallocate(evec_index_v3);
    }
    if (evec_index_v4) {
        deallocate(evec_index_v4);
    }
    if (fcs_group_v3) {
        deallocate(fcs_group_v3);
    }
    if (fcs_group_v4) {
        deallocate(fcs_group_v4);
    }
    if (phi3_reciprocal) {
        deallocate(phi3_reciprocal);
    }
    if (phi4_reciprocal) {
        deallocate(phi4_reciprocal);
    }
    if (phase_storage_dos) delete phase_storage_dos;
}

void AnharmonicCore::setup()
{
    sym_permutation = true;
    use_tuned_ver = true;
    MPI_Bcast(&use_tuned_ver, 1, MPI_CXX_BOOL, 0, MPI_COMM_WORLD);

    if (fcs_phonon->maxorder >= 2) setup_cubic();
    if (fcs_phonon->maxorder >= 3) setup_quartic();

    if (!mode_analysis->calc_fstate_k && dos->kmesh_dos) {
        phase_storage_dos = new PhaseFactorStorage(dos->kmesh_dos->nk_i);
        phase_storage_dos->create(use_tuned_ver);
    }
}

void AnharmonicCore::prepare_relative_vector(const std::vector<FcsArrayWithCell> &fcs_in,
                                             const unsigned int N,
                                             const int number_of_groups,
                                             std::vector<double> *fcs_group,
                                             std::vector<RelativeVector> *&vec_out) const
{
    double vecs[3][3];
    unsigned int icount = 0;
    for (auto igroup = 0; igroup < number_of_groups; ++igroup) {

        unsigned int nsize_group = fcs_group[igroup].size();

        for (auto j = 0; j < nsize_group; ++j) {
            for (auto i = 0; i < N - 1; ++i) {
                for (auto k = 0; k < 3; ++k) {
                    // include tpi phase factor here
                    vecs[i][k] = tpi * fcs_in[icount].relvecs[i][k];
                }
            }
            if (N == 3) {
                vec_out[igroup].emplace_back(vecs[0], vecs[1]);
            } else if (N == 4) {
                vec_out[igroup].emplace_back(vecs[0], vecs[1], vecs[2]);
            }
            ++icount;
        }
    }
}

void AnharmonicCore::prepare_group_of_force_constants(const std::vector<FcsArrayWithCell> &fcs_in,
                                                      const unsigned int N,
                                                      int &number_of_groups,
                                                      std::vector<double> *&fcs_group_out) const
{
    // Find the number of groups which has different evecs.

    unsigned int i;
    std::vector<int> arr_old, arr_tmp;

    number_of_groups = 0;

    arr_old.clear();
    for (i = 0; i < N; ++i) {
        arr_old.push_back(-1);
    }

    for (const auto &it: fcs_in) {

        arr_tmp.clear();

        for (i = 0; i < it.pairs.size(); ++i) {
            arr_tmp.push_back(it.pairs[i].index);
        }

        if (arr_tmp != arr_old) {
            ++number_of_groups;
            arr_old.clear();
            arr_old.reserve(arr_tmp.size());
            std::copy(arr_tmp.begin(), arr_tmp.end(), std::back_inserter(arr_old));
        }
    }

    allocate(fcs_group_out, number_of_groups);

    int igroup = -1;

    arr_old.clear();
    for (i = 0; i < N; ++i) {
        arr_old.push_back(-1);
    }

    for (const auto &it: fcs_in) {

        arr_tmp.clear();

        for (i = 0; i < it.pairs.size(); ++i) {
            arr_tmp.push_back(it.pairs[i].index);
        }

        if (arr_tmp != arr_old) {
            ++igroup;
            arr_old.clear();
            arr_old.reserve(arr_tmp.size());
            std::copy(arr_tmp.begin(), arr_tmp.end(), std::back_inserter(arr_old));
        }

        fcs_group_out[igroup].push_back(it.fcs_val);
    }
}

std::complex<double> AnharmonicCore::V3(const unsigned int ks[3])
{
    return V3(ks,
              dos->kmesh_dos->xk,
              dos->dymat_dos->get_eigenvalues(),
              dos->dymat_dos->get_eigenvectors(),
              this->phase_storage_dos);
}

std::complex<double> AnharmonicCore::V3(const unsigned int ks[3],
                                        const double *const *xk_in,
                                        const double *const *eval_in,
                                        const std::complex<double> *const *const *evec_in)
{
    return V3(ks,
              xk_in,
              eval_in,
              evec_in,
              this->phase_storage_dos);
}

std::complex<double> AnharmonicCore::V4(const unsigned int ks[4])
{
    return V4(ks,
              dos->kmesh_dos->xk,
              dos->dymat_dos->get_eigenvalues(),
              dos->dymat_dos->get_eigenvectors(),
              this->phase_storage_dos);
}

std::complex<double> AnharmonicCore::Phi3(const unsigned int ks[3])
{
    return Phi3(ks,
                dos->kmesh_dos->xk,
                dos->dymat_dos->get_eigenvalues(),
                dos->dymat_dos->get_eigenvectors(),
                this->phase_storage_dos);
}

std::complex<double> AnharmonicCore::Phi4(const unsigned int ks[4])
{
    return Phi4(ks,
                dos->kmesh_dos->xk,
                dos->dymat_dos->get_eigenvalues(),
                dos->dymat_dos->get_eigenvectors(),
                this->phase_storage_dos);
}

std::complex<double> AnharmonicCore::V3(const unsigned int ks[3],
                                        const double *const *xk_in,
                                        const double *const *eval_in,
                                        const std::complex<double> *const *const *evec_in,
                                        const PhaseFactorStorage *phase_storage_in)
{
    int i;
    unsigned int kn[3], sn[3];
    const int ns = dynamical->neval;

    double omega[3];
    auto ret = std::complex<double>(0.0, 0.0);
    auto ret_re = 0.0;
    auto ret_im = 0.0;

    for (i = 0; i < 3; ++i) {
        kn[i] = ks[i] / ns;
        sn[i] = ks[i] % ns;
        omega[i] = eval_in[kn[i]][sn[i]];
    }

    // Return zero if any of the involving phonon has imaginary frequency
    if (omega[0] < eps8 || omega[1] < eps8 || omega[2] < eps8) return 0.0;

    if (kn[1] != kindex_phi3_stored[0] || kn[2] != kindex_phi3_stored[1]) {
        calc_phi3_reciprocal(xk_in[kn[1]],
                             xk_in[kn[2]],
                             phase_storage_in,
                             phi3_reciprocal);
        kindex_phi3_stored[0] = kn[1];
        kindex_phi3_stored[1] = kn[2];
    }
#ifdef _OPENMP
#pragma omp parallel for private(ret), reduction(+: ret_re, ret_im)
#endif
    for (i = 0; i < ngroup_v3; ++i) {
        ret = evec_in[kn[0]][sn[0]][evec_index_v3[i][0]]
              * evec_in[kn[1]][sn[1]][evec_index_v3[i][1]]
              * evec_in[kn[2]][sn[2]][evec_index_v3[i][2]]
              * invmass_v3[i] * phi3_reciprocal[i];
        ret_re += ret.real();
        ret_im += ret.imag();
    }

    return std::complex<double>(ret_re, ret_im)
           / std::sqrt(omega[0] * omega[1] * omega[2]);
}

std::complex<double> AnharmonicCore::Phi3(const unsigned int ks[3],
                                          const double *const *xk_in,
                                          const double *const *eval_in,
                                          const std::complex<double> *const *const *evec_in,
                                          const PhaseFactorStorage *phase_storage_in)
{
    int i;
    unsigned int kn[3], sn[3];
    const auto ns = dynamical->neval;

    double omega[3];
    std::complex<double> ret = std::complex<double>(0.0, 0.0);
    double ret_re = 0.0;
    double ret_im = 0.0;

    for (i = 0; i < 3; ++i) {
        kn[i] = ks[i] / ns;
        sn[i] = ks[i] % ns;
        omega[i] = eval_in[kn[i]][sn[i]];
    }

    if (kn[1] != kindex_phi3_stored[0] || kn[2] != kindex_phi3_stored[1]) {
        calc_phi3_reciprocal(xk_in[kn[1]],
                             xk_in[kn[2]],
                             phase_storage_in,
                             phi3_reciprocal);
        kindex_phi3_stored[0] = kn[1];
        kindex_phi3_stored[1] = kn[2];
    }
#ifdef _OPENMP
#pragma omp parallel for private(ret), reduction(+: ret_re, ret_im)
#endif
    for (i = 0; i < ngroup_v3; ++i) {
        ret = evec_in[kn[0]][sn[0]][evec_index_v3[i][0]]
              * evec_in[kn[1]][sn[1]][evec_index_v3[i][1]]
              * evec_in[kn[2]][sn[2]][evec_index_v3[i][2]]
              * invmass_v3[i] * phi3_reciprocal[i];
        ret_re += ret.real();
        ret_im += ret.imag();
    }

    return std::complex<double>(ret_re, ret_im);
}

void AnharmonicCore::calc_phi3_reciprocal(const double *xk1,
                                          const double *xk2,
                                          const PhaseFactorStorage *phase_storage_in,
                                          std::complex<double> *ret)
{
    int i, j;
    double phase;
    std::complex<double> ret_in;
    unsigned int nsize_group;

    const auto tune_type_now = phase_storage_in->get_tune_type();

    if (tune_type_now == 1) {

#pragma omp parallel for private(ret_in, nsize_group, j, phase)
        for (i = 0; i < ngroup_v3; ++i) {

            ret_in = std::complex<double>(0.0, 0.0);
            nsize_group = fcs_group_v3[i].size();

            for (j = 0; j < nsize_group; ++j) {
                phase = relvec_v3[i][j].vecs[0][0] * xk1[0]
                        + relvec_v3[i][j].vecs[0][1] * xk1[1]
                        + relvec_v3[i][j].vecs[0][2] * xk1[2]
                        + relvec_v3[i][j].vecs[1][0] * xk2[0]
                        + relvec_v3[i][j].vecs[1][1] * xk2[1]
                        + relvec_v3[i][j].vecs[1][2] * xk2[2];

                ret_in += fcs_group_v3[i][j] * phase_storage_in->get_exp_type1(phase);
            }
            ret[i] = ret_in;
        }

    } else if (tune_type_now == 2) {

        // Tuned version is used when nk1=nk2=nk3 doesn't hold.

        double phase3[3];

#pragma omp parallel for private(ret_in, nsize_group, j, phase3)
        for (i = 0; i < ngroup_v3; ++i) {

            ret_in = std::complex<double>(0.0, 0.0);
            nsize_group = fcs_group_v3[i].size();

            for (j = 0; j < nsize_group; ++j) {
                for (auto ii = 0; ii < 3; ++ii) {
                    phase3[ii]
                            = relvec_v3[i][j].vecs[0][ii] * xk1[ii]
                              + relvec_v3[i][j].vecs[1][ii] * xk2[ii];
                }
                ret_in += fcs_group_v3[i][j] * phase_storage_in->get_exp_type2(phase3);
            }
            ret[i] = ret_in;
        }
    } else {
        // Original version
#pragma omp parallel for private(ret_in, nsize_group, phase, j)
        for (i = 0; i < ngroup_v3; ++i) {

            ret_in = std::complex<double>(0.0, 0.0);
            nsize_group = fcs_group_v3[i].size();

            for (j = 0; j < nsize_group; ++j) {
                phase
                        = relvec_v3[i][j].vecs[0][0] * xk1[0]
                          + relvec_v3[i][j].vecs[0][1] * xk1[1]
                          + relvec_v3[i][j].vecs[0][2] * xk1[2]
                          + relvec_v3[i][j].vecs[1][0] * xk2[0]
                          + relvec_v3[i][j].vecs[1][1] * xk2[1]
                          + relvec_v3[i][j].vecs[1][2] * xk2[2];
                ret_in += fcs_group_v3[i][j] * std::exp(im * phase);
            }
            ret[i] = ret_in;
        }
    }
}

std::complex<double> AnharmonicCore::V4(const unsigned int ks[4],
                                        const double *const *xk_in,
                                        const double *const *eval_in,
                                        const std::complex<double> *const *const *evec_in,
                                        const PhaseFactorStorage *phase_storage_in)
{
    int i;
    const int ns = dynamical->neval;
    unsigned int kn[4], sn[4];
    double omega[4];
    auto ret_re = 0.0;
    auto ret_im = 0.0;
    auto ret = std::complex<double>(0.0, 0.0);

    for (i = 0; i < 4; ++i) {
        kn[i] = ks[i] / ns;
        sn[i] = ks[i] % ns;
        omega[i] = eval_in[kn[i]][sn[i]];
    }
    // Return zero if any of the involving phonon has imaginary frequency
    if (omega[0] < eps8 || omega[1] < eps8 || omega[2] < eps8 || omega[3] < eps8) return 0.0;

    if (kn[1] != kindex_phi4_stored[0]
        || kn[2] != kindex_phi4_stored[1]
        || kn[3] != kindex_phi4_stored[2]) {

        calc_phi4_reciprocal(xk_in[kn[1]],
                             xk_in[kn[2]],
                             xk_in[kn[3]],
                             phase_storage_in,
                             phi4_reciprocal);

        kindex_phi4_stored[0] = kn[1];
        kindex_phi4_stored[1] = kn[2];
        kindex_phi4_stored[2] = kn[3];
    }

#ifdef _OPENMP
#pragma omp parallel for private(ret), reduction(+: ret_re, ret_im)
#endif
    for (i = 0; i < ngroup_v4; ++i) {
        ret = evec_in[kn[0]][sn[0]][evec_index_v4[i][0]]
              * evec_in[kn[1]][sn[1]][evec_index_v4[i][1]]
              * evec_in[kn[2]][sn[2]][evec_index_v4[i][2]]
              * evec_in[kn[3]][sn[3]][evec_index_v4[i][3]]
              * invmass_v4[i] * phi4_reciprocal[i];
        ret_re += ret.real();
        ret_im += ret.imag();
    }

    return std::complex<double>(ret_re, ret_im)
           / std::sqrt(omega[0] * omega[1] * omega[2] * omega[3]);
}

std::complex<double> AnharmonicCore::Phi4(const unsigned int ks[4],
                                          const double *const *xk_in,
                                          const double *const *eval_in,
                                          const std::complex<double> *const *const *evec_in,
                                          const PhaseFactorStorage *phase_storage_in)
{
    int i;
    int ns = dynamical->neval;
    unsigned int kn[4], sn[4];
    double omega[4];
    double ret_re = 0.0;
    double ret_im = 0.0;
    std::complex<double> ret = std::complex<double>(0.0, 0.0);

    for (i = 0; i < 4; ++i) {
        kn[i] = ks[i] / ns;
        sn[i] = ks[i] % ns;
        omega[i] = eval_in[kn[i]][sn[i]];
    }

    if (kn[1] != kindex_phi4_stored[0]
        || kn[2] != kindex_phi4_stored[1]
        || kn[3] != kindex_phi4_stored[2]) {

        calc_phi4_reciprocal(xk_in[kn[1]],
                             xk_in[kn[2]],
                             xk_in[kn[3]],
                             phase_storage_in,
                             phi4_reciprocal);

        kindex_phi4_stored[0] = kn[1];
        kindex_phi4_stored[1] = kn[2];
        kindex_phi4_stored[2] = kn[3];
    }

#ifdef _OPENMP
#pragma omp parallel for private(ret), reduction(+: ret_re, ret_im)
#endif
    for (i = 0; i < ngroup_v4; ++i) {
        ret = evec_in[kn[0]][sn[0]][evec_index_v4[i][0]]
              * evec_in[kn[1]][sn[1]][evec_index_v4[i][1]]
              * evec_in[kn[2]][sn[2]][evec_index_v4[i][2]]
              * evec_in[kn[3]][sn[3]][evec_index_v4[i][3]]
              * invmass_v4[i] * phi4_reciprocal[i];
        ret_re += ret.real();
        ret_im += ret.imag();
    }

    return std::complex<double>(ret_re, ret_im);
}

void AnharmonicCore::calc_phi4_reciprocal(const double *xk1,
                                          const double *xk2,
                                          const double *xk3,
                                          const PhaseFactorStorage *phase_storage_in,
                                          std::complex<double> *ret)
{
    int i, j;
    double phase;
    std::complex<double> ret_in;
    unsigned int nsize_group;

    const auto tune_type_now = phase_storage_in->get_tune_type();

    if (tune_type_now == 1) {

#pragma omp parallel for private(ret_in, nsize_group, j, phase)
        for (i = 0; i < ngroup_v4; ++i) {

            ret_in = std::complex<double>(0.0, 0.0);
            nsize_group = fcs_group_v4[i].size();

            for (j = 0; j < nsize_group; ++j) {
                phase = relvec_v4[i][j].vecs[0][0] * xk1[0]
                        + relvec_v4[i][j].vecs[0][1] * xk1[1]
                        + relvec_v4[i][j].vecs[0][2] * xk1[2]
                        + relvec_v4[i][j].vecs[1][0] * xk2[0]
                        + relvec_v4[i][j].vecs[1][1] * xk2[1]
                        + relvec_v4[i][j].vecs[1][2] * xk2[2]
                        + relvec_v4[i][j].vecs[2][0] * xk3[0]
                        + relvec_v4[i][j].vecs[2][1] * xk3[1]
                        + relvec_v4[i][j].vecs[2][2] * xk3[2];

                ret_in += fcs_group_v4[i][j] * phase_storage_in->get_exp_type1(phase);
            }
            ret[i] = ret_in;
        }

    } else if (tune_type_now == 2) {

        // Tuned version is used when nk1=nk2=nk3 doesn't hold.

        double phase3[3];

#pragma omp parallel for private(ret_in, nsize_group, j, phase3)
        for (i = 0; i < ngroup_v4; ++i) {

            ret_in = std::complex<double>(0.0, 0.0);
            nsize_group = fcs_group_v4[i].size();

            for (j = 0; j < nsize_group; ++j) {
                for (auto ii = 0; ii < 3; ++ii) {
                    phase3[ii]
                            = relvec_v4[i][j].vecs[0][ii] * xk1[ii]
                              + relvec_v4[i][j].vecs[1][ii] * xk2[ii]
                              + relvec_v4[i][j].vecs[2][ii] * xk3[ii];
                }
                ret_in += fcs_group_v4[i][j] * phase_storage_in->get_exp_type2(phase3);
            }
            ret[i] = ret_in;
        }
    } else {
        // Original version
#pragma omp parallel for private(ret_in, nsize_group, phase)
        for (i = 0; i < ngroup_v4; ++i) {

            ret_in = std::complex<double>(0.0, 0.0);
            nsize_group = fcs_group_v4[i].size();

            for (j = 0; j < nsize_group; ++j) {
                phase = relvec_v4[i][j].vecs[0][0] * xk1[0]
                        + relvec_v4[i][j].vecs[0][1] * xk1[1]
                        + relvec_v4[i][j].vecs[0][2] * xk1[2]
                        + relvec_v4[i][j].vecs[1][0] * xk2[0]
                        + relvec_v4[i][j].vecs[1][1] * xk2[1]
                        + relvec_v4[i][j].vecs[1][2] * xk2[2]
                        + relvec_v4[i][j].vecs[2][0] * xk3[0]
                        + relvec_v4[i][j].vecs[2][1] * xk3[1]
                        + relvec_v4[i][j].vecs[2][2] * xk3[2];

                ret_in += fcs_group_v4[i][j] * std::exp(im * phase);
            }
            ret[i] = ret_in;
        }
    }
}

std::complex<double> AnharmonicCore::V3_mode(int mode,
                                             const double *xk2,
                                             const double *xk3,
                                             int is,
                                             int js,
                                             double **eval,
                                             std::complex<double> ***evec) const
{
    std::complex<double> ctmp = std::complex<double>(0.0, 0.0);

    // Return zero if any of the involving phonon has imaginary frequency
    if (eval[0][mode] < eps8 || eval[1][is] < eps8 || eval[2][js] < eps8) return 0.0;

    for (int i = 0; i < ngroup_v3; ++i) {

        auto vec_tmp = evec[0][mode][evec_index_v3[i][0]]
                       * evec[1][is][evec_index_v3[i][1]]
                       * evec[2][js][evec_index_v3[i][2]]
                       * invmass_v3[i];

        auto ret_in = std::complex<double>(0.0, 0.0);

        const int nsize_group = fcs_group_v3[i].size();

        for (auto j = 0; j < nsize_group; ++j) {

            auto phase = relvec_v3[i][j].vecs[0][0] * xk2[0]
                         + relvec_v3[i][j].vecs[0][1] * xk2[1]
                         + relvec_v3[i][j].vecs[0][2] * xk2[2]
                         + relvec_v3[i][j].vecs[1][0] * xk3[0]
                         + relvec_v3[i][j].vecs[1][1] * xk3[1]
                         + relvec_v3[i][j].vecs[1][2] * xk3[2];

            ret_in += fcs_group_v3[i][j] * std::exp(im * phase);
        }
        ctmp += ret_in * vec_tmp;
    }

    return ctmp / std::sqrt(eval[0][mode] * eval[1][is] * eval[2][js]);
}

void AnharmonicCore::calc_damping_smearing(const unsigned int ntemp,
                                           const double *temp_in,
                                           const double omega_in,
                                           const unsigned int ik_in,
                                           const unsigned int is_in,
                                           const KpointMeshUniform *kmesh_in,
                                           const double *const *eval_in,
                                           const std::complex<double> *const *const *evec_in,
                                           double *ret)
{
    // This function returns the imaginary part of phonon self-energy 
    // for the given frequency omega_in.
    // Lorentzian or Gaussian smearing will be used.
    // This version employs the crystal symmetry to reduce the computational cost

    const auto nk = kmesh_in->nk;
    const auto ns = dynamical->neval;
    const auto ns2 = ns * ns;
    unsigned int i;
    int ik;
    unsigned int is, js;
    unsigned int arr[3];

    int k1, k2;

    double T_tmp;
    double n1, n2;
    double omega_inner[2];

    double multi;

    for (i = 0; i < ntemp; ++i) ret[i] = 0.0;

    double **v3_arr;
    double ***delta_arr;
    double ret_tmp;

    double f1, f2;

    const auto epsilon = integration->epsilon;

    std::vector<KsListGroup> triplet;

    kmesh_in->get_unique_triplet_k(ik_in,
                                   symmetry->SymmList,
                                   false,
                                   false,
                                   triplet);

    const auto npair_uniq = triplet.size();

    allocate(v3_arr, npair_uniq, ns * ns);
    allocate(delta_arr, npair_uniq, ns * ns, 2);

    const auto knum = kmesh_in->kpoint_irred_all[ik_in][0].knum;
    const auto knum_minus = kmesh_in->kindex_minus_xk[knum];
#ifdef _OPENMP
#pragma omp parallel for private(multi, arr, k1, k2, is, js, omega_inner)
#endif
    for (ik = 0; ik < npair_uniq; ++ik) {
        multi = static_cast<double>(triplet[ik].group.size());

        arr[0] = ns * knum_minus + is_in;

        k1 = triplet[ik].group[0].ks[0];
        k2 = triplet[ik].group[0].ks[1];

        for (is = 0; is < ns; ++is) {
            arr[1] = ns * k1 + is;
            omega_inner[0] = eval_in[k1][is];

            for (js = 0; js < ns; ++js) {
                arr[2] = ns * k2 + js;
                omega_inner[1] = eval_in[k2][js];

                if (integration->ismear == 0) {
                    delta_arr[ik][ns * is + js][0]
                            = delta_lorentz(omega_in - omega_inner[0] - omega_inner[1], epsilon)
                              - delta_lorentz(omega_in + omega_inner[0] + omega_inner[1], epsilon);
                    delta_arr[ik][ns * is + js][1]
                            = delta_lorentz(omega_in - omega_inner[0] + omega_inner[1], epsilon)
                              - delta_lorentz(omega_in + omega_inner[0] - omega_inner[1], epsilon);
                } else if (integration->ismear == 1) {
                    delta_arr[ik][ns * is + js][0]
                            = delta_gauss(omega_in - omega_inner[0] - omega_inner[1], epsilon)
                              - delta_gauss(omega_in + omega_inner[0] + omega_inner[1], epsilon);
                    delta_arr[ik][ns * is + js][1]
                            = delta_gauss(omega_in - omega_inner[0] + omega_inner[1], epsilon)
                              - delta_gauss(omega_in + omega_inner[0] - omega_inner[1], epsilon);
                }
            }
        }
    }

    for (ik = 0; ik < npair_uniq; ++ik) {

        k1 = triplet[ik].group[0].ks[0];
        k2 = triplet[ik].group[0].ks[1];

        multi = static_cast<double>(triplet[ik].group.size());

        for (int ib = 0; ib < ns2; ++ib) {
            is = ib / ns;
            js = ib % ns;

            arr[0] = ns * knum_minus + is_in;
            arr[1] = ns * k1 + is;
            arr[2] = ns * k2 + js;

            v3_arr[ik][ib] = std::norm(V3(arr,
                                          kmesh_in->xk,
                                          eval_in,
                                          evec_in,
                                          phase_storage_dos)) * multi;
        }
    }

    for (i = 0; i < ntemp; ++i) {
        T_tmp = temp_in[i];
        ret_tmp = 0.0;
#ifdef _OPENMP
#pragma omp parallel for private(k1, k2, is, js, omega_inner, n1, n2, f1, f2), reduction(+:ret_tmp)
#endif
        for (ik = 0; ik < npair_uniq; ++ik) {

            k1 = triplet[ik].group[0].ks[0];
            k2 = triplet[ik].group[0].ks[1];

            for (is = 0; is < ns; ++is) {

                omega_inner[0] = eval_in[k1][is];

                for (js = 0; js < ns; ++js) {

                    omega_inner[1] = eval_in[k2][js];

                    if (thermodynamics->classical) {
                        f1 = thermodynamics->fC(omega_inner[0], T_tmp);
                        f2 = thermodynamics->fC(omega_inner[1], T_tmp);

                        n1 = f1 + f2;
                        n2 = f1 - f2;
                    } else {
                        f1 = thermodynamics->fB(omega_inner[0], T_tmp);
                        f2 = thermodynamics->fB(omega_inner[1], T_tmp);

                        n1 = f1 + f2 + 1.0;
                        n2 = f1 - f2;
                    }

                    ret_tmp += v3_arr[ik][ns * is + js]
                               * (n1 * delta_arr[ik][ns * is + js][0]
                                  - n2 * delta_arr[ik][ns * is + js][1]);
                }
            }
        }
        ret[i] = ret_tmp;
    }

    deallocate(v3_arr);
    deallocate(delta_arr);
    triplet.clear();

    for (i = 0; i < ntemp; ++i) ret[i] *= pi * std::pow(0.5, 4) / static_cast<double>(nk);
}

void AnharmonicCore::calc_damping_tetrahedron(const unsigned int ntemp,
                                              const double *temp_in,
                                              const double omega_in,
                                              const unsigned int ik_in,
                                              const unsigned int is_in,
                                              const KpointMeshUniform *kmesh_in,
                                              const double *const *eval_in,
                                              const std::complex<double> *const *const *evec_in,
                                              double *ret)
{
    // This function returns the imaginary part of phonon self-energy 
    // for the given frequency omega_in.
    // Tetrahedron method will be used.
    // This version employs the crystal symmetry to reduce the computational cost

    const int nk = kmesh_in->nk;
    const int ns = dynamical->neval;

    int ik, ib;
    const auto ns2 = ns * ns;

    unsigned int i;
    unsigned int jk;
    unsigned int is, js;
    unsigned int k1, k2;
    unsigned int arr[3];

    double T_tmp;
    double n1, n2;
    double f1, f2;
    double multi;

    double xk_tmp[3];
    double omega_inner[2];

    double ret_tmp;

    unsigned int *kmap_identity;
    double **energy_tmp;
    double **weight_tetra;
    double **v3_arr;
    double ***delta_arr;

    std::vector<KsListGroup> triplet;

    for (i = 0; i < ntemp; ++i) ret[i] = 0.0;

    kmesh_in->get_unique_triplet_k(ik_in,
                                   symmetry->SymmList,
                                   use_triplet_symmetry,
                                   sym_permutation,
                                   triplet);

    const auto npair_uniq = triplet.size();

    allocate(v3_arr, npair_uniq, ns2);
    allocate(delta_arr, npair_uniq, ns2, 2);

    const auto knum = kmesh_in->kpoint_irred_all[ik_in][0].knum;
    const auto knum_minus = kmesh_in->kindex_minus_xk[knum];
    const auto xk = kmesh_in->xk;

    allocate(kmap_identity, nk);

    for (i = 0; i < nk; ++i) kmap_identity[i] = i;

#ifdef _OPENMP
#pragma omp parallel private(is, js, k1, k2, xk_tmp, energy_tmp, i, weight_tetra, ik, jk, arr)
#endif
    {
        allocate(energy_tmp, 3, nk);
        allocate(weight_tetra, 3, nk);

#ifdef _OPENMP
#pragma omp for
#endif
        for (ib = 0; ib < ns2; ++ib) {
            is = ib / ns;
            js = ib % ns;

            for (k1 = 0; k1 < nk; ++k1) {

                // Prepare two-phonon frequency for the tetrahedron method

                for (i = 0; i < 3; ++i) xk_tmp[i] = xk[knum][i] - xk[k1][i];

                k2 = kmesh_in->get_knum(xk_tmp);

                energy_tmp[0][k1] = eval_in[k1][is] + eval_in[k2][js];
                energy_tmp[1][k1] = eval_in[k1][is] - eval_in[k2][js];
                energy_tmp[2][k1] = -energy_tmp[1][k1];
            }

            for (i = 0; i < 3; ++i) {
                integration->calc_weight_tetrahedron(nk, kmap_identity,
                                                     energy_tmp[i], omega_in,
                                                     dos->tetra_nodes_dos->get_ntetra(),
                                                     dos->tetra_nodes_dos->get_tetras(),
                                                     weight_tetra[i]);
            }

            for (ik = 0; ik < npair_uniq; ++ik) {
                jk = triplet[ik].group[0].ks[0];
                delta_arr[ik][ib][0] = weight_tetra[0][jk];
                delta_arr[ik][ib][1] = weight_tetra[1][jk] - weight_tetra[2][jk];
            }
        }

        deallocate(energy_tmp);
        deallocate(weight_tetra);
    }

    for (ik = 0; ik < npair_uniq; ++ik) {

        k1 = triplet[ik].group[0].ks[0];
        k2 = triplet[ik].group[0].ks[1];

        multi = static_cast<double>(triplet[ik].group.size());

        for (ib = 0; ib < ns2; ++ib) {
            is = ib / ns;
            js = ib % ns;

            if (delta_arr[ik][ib][0] > 0.0 || std::abs(delta_arr[ik][ib][1]) > 0.0) {

                arr[0] = ns * knum_minus + is_in;
                arr[1] = ns * k1 + is;
                arr[2] = ns * k2 + js;

                v3_arr[ik][ib] = std::norm(V3(arr,
                                              kmesh_in->xk,
                                              eval_in,
                                              evec_in,
                                              phase_storage_dos)) * multi;

            } else {
                v3_arr[ik][ib] = 0.0;
            }
        }
    }

    for (i = 0; i < ntemp; ++i) {
        T_tmp = temp_in[i];
        ret_tmp = 0.0;
#ifdef _OPENMP
#pragma omp parallel for private(k1, k2, is, js, omega_inner, n1, n2, f1, f2), reduction(+:ret_tmp)
#endif
        for (ik = 0; ik < npair_uniq; ++ik) {

            k1 = triplet[ik].group[0].ks[0];
            k2 = triplet[ik].group[0].ks[1];

            for (is = 0; is < ns; ++is) {

                omega_inner[0] = eval_in[k1][is];

                for (js = 0; js < ns; ++js) {

                    omega_inner[1] = eval_in[k2][js];

                    if (thermodynamics->classical) {
                        f1 = thermodynamics->fC(omega_inner[0], T_tmp);
                        f2 = thermodynamics->fC(omega_inner[1], T_tmp);

                        n1 = f1 + f2;
                        n2 = f1 - f2;
                    } else {
                        f1 = thermodynamics->fB(omega_inner[0], T_tmp);
                        f2 = thermodynamics->fB(omega_inner[1], T_tmp);

                        n1 = f1 + f2 + 1.0;
                        n2 = f1 - f2;
                    }

                    ret_tmp += v3_arr[ik][ns * is + js]
                               * (n1 * delta_arr[ik][ns * is + js][0]
                                  - n2 * delta_arr[ik][ns * is + js][1]);
                }
            }
        }
        ret[i] = ret_tmp;
    }

    deallocate(v3_arr);
    deallocate(delta_arr);
    deallocate(kmap_identity);

    for (i = 0; i < ntemp; ++i) ret[i] *= pi * std::pow(0.5, 4);
}

void AnharmonicCore::setup_cubic()
{
    // Sort force_constant[1] using the operator defined in fcs_phonons.h
    // This sorting is necessary.
    std::sort(fcs_phonon->force_constant_with_cell[1].begin(),
              fcs_phonon->force_constant_with_cell[1].end());
    prepare_group_of_force_constants(fcs_phonon->force_constant_with_cell[1],
                                     3, ngroup_v3, fcs_group_v3);

    allocate(invmass_v3, ngroup_v3);
    allocate(evec_index_v3, ngroup_v3, 3);
    allocate(relvec_v3, ngroup_v3);
    allocate(phi3_reciprocal, ngroup_v3);

    prepare_relative_vector(fcs_phonon->force_constant_with_cell[1],
                            3,
                            ngroup_v3,
                            fcs_group_v3,
                            relvec_v3);

    const auto invsqrt_mass_p = system->get_invsqrt_mass();

    int k = 0;
    for (auto i = 0; i < ngroup_v3; ++i) {
        for (int j = 0; j < 3; ++j) {
            evec_index_v3[i][j] = fcs_phonon->force_constant_with_cell[1][k].pairs[j].index;
        }
        invmass_v3[i]
                = invsqrt_mass_p[evec_index_v3[i][0] / 3]
                  * invsqrt_mass_p[evec_index_v3[i][1] / 3]
                  * invsqrt_mass_p[evec_index_v3[i][2] / 3];
        k += fcs_group_v3[i].size();
    }
}

void AnharmonicCore::setup_quartic()
{
    std::sort(fcs_phonon->force_constant_with_cell[2].begin(),
              fcs_phonon->force_constant_with_cell[2].end());
    prepare_group_of_force_constants(fcs_phonon->force_constant_with_cell[2],
                                     4, ngroup_v4, fcs_group_v4);

    allocate(invmass_v4, ngroup_v4);
    allocate(evec_index_v4, ngroup_v4, 4);
    allocate(relvec_v4, ngroup_v4);
    allocate(phi4_reciprocal, ngroup_v4);

    prepare_relative_vector(fcs_phonon->force_constant_with_cell[2],
                            4,
                            ngroup_v4,
                            fcs_group_v4,
                            relvec_v4);

    const auto invsqrt_mass_p = system->get_invsqrt_mass();

    int k = 0;
    for (auto i = 0; i < ngroup_v4; ++i) {
        for (int j = 0; j < 4; ++j) {
            evec_index_v4[i][j] = fcs_phonon->force_constant_with_cell[2][k].pairs[j].index;
        }
        invmass_v4[i]
                = invsqrt_mass_p[evec_index_v4[i][0] / 3]
                  * invsqrt_mass_p[evec_index_v4[i][1] / 3]
                  * invsqrt_mass_p[evec_index_v4[i][2] / 3]
                  * invsqrt_mass_p[evec_index_v4[i][3] / 3];
        k += fcs_group_v4[i].size();
    }
}

void PhaseFactorStorage::create(const bool use_tuned_ver,
                                const bool switch_to_type2)
{
    // For accelerating function V3 and V4 by avoiding continual call of std::exp.

    if (use_tuned_ver) {

        const auto inv2pi = 1.0 / (2.0 * pi);

        for (auto i = 0; i < 3; ++i) dnk[i] = static_cast<double>(nk_grid[i]) * inv2pi;

        tune_type = 1;

        if (nk_grid[0] == nk_grid[1] && nk_grid[1] == nk_grid[2]) {
            nk_represent = nk_grid[0];
        } else if (nk_grid[0] == nk_grid[1] && nk_grid[2] == 1) {
            nk_represent = nk_grid[0];
        } else if (nk_grid[1] == nk_grid[2] && nk_grid[0] == 1) {
            nk_represent = nk_grid[1];
        } else if (nk_grid[2] == nk_grid[0] && nk_grid[1] == 1) {
            nk_represent = nk_grid[2];
        } else if (nk_grid[0] == 1 && nk_grid[1] == 1) {
            nk_represent = nk_grid[2];
        } else if (nk_grid[1] == 1 && nk_grid[2] == 1) {
            nk_represent = nk_grid[0];
        } else if (nk_grid[2] == 1 && nk_grid[0] == 1) {
            nk_represent = nk_grid[1];
        } else {
            tune_type = 2;
        }

        // Force using tune_type == 2 version
        if (switch_to_type2) tune_type = 2;

        int ii, jj, kk;

        if (tune_type == 1) {

            double phase;
            dnk_represent = static_cast<double>(nk_represent) * inv2pi;
            const auto inv_dnk_represent = 1.0 / dnk_represent;

            // Pre-calculate the phase factor exp[i 2pi * phase]
            // for different phase angles ranging from [-2pi + 2pi/nk_represent: 2pi*(nk_represent-1)/nk_represent].
            // The redundancy of the data here is intentional and helpful for accepting
            // both positive and negative modulo.
            allocate(exp_phase, 2 * nk_represent - 1);
#ifdef _OPENMP
#pragma omp parallel for private(phase)
#endif
            for (ii = 0; ii < 2 * nk_represent - 1; ++ii) {
                phase = static_cast<double>(ii - nk_represent + 1) * inv_dnk_represent;
                exp_phase[ii] = std::exp(im * phase);
            }

        } else if (tune_type == 2) {

            double phase[3];
            double inv_dnk[3];

            for (auto i = 0; i < 3; ++i) inv_dnk[i] = 1.0 / dnk[i];

            allocate(exp_phase3,
                     2 * nk_grid[0] - 1,
                     2 * nk_grid[1] - 1,
                     2 * nk_grid[2] - 1);
#ifdef _OPENMP
#pragma omp parallel for private(phase, jj, kk)
#endif
            for (ii = 0; ii < 2 * nk_grid[0] - 1; ++ii) {
                phase[0] = static_cast<double>(ii - nk_grid[0] + 1) * inv_dnk[0];
                for (jj = 0; jj < 2 * nk_grid[1] - 1; ++jj) {
                    phase[1] = static_cast<double>(jj - nk_grid[1] + 1) * inv_dnk[1];
                    for (kk = 0; kk < 2 * nk_grid[2] - 1; ++kk) {
                        phase[2] = static_cast<double>(kk - nk_grid[2] + 1) * inv_dnk[2];
                        exp_phase3[ii][jj][kk] = std::exp(im * (phase[0] + phase[1] + phase[2]));
                    }
                }
            }
        }
    } else {
        tune_type = 0;
    }
}

unsigned int PhaseFactorStorage::get_tune_type() const
{
    return tune_type;
}

std::complex<double> PhaseFactorStorage::get_exp_type1(const double phase_in) const
{
    int iloc = nint(phase_in * dnk_represent) % nk_represent + nk_represent - 1;
    return exp_phase[iloc];
}

std::complex<double> PhaseFactorStorage::get_exp_type2(const double *phase3_in) const
{
    int loc[3];
    for (auto i = 0; i < 3; ++i) {
        loc[i] = nint(phase3_in[i] * dnk[i]) % nk_grid[i] + nk_grid[i] - 1;
    }
    return exp_phase3[loc[0]][loc[1]][loc[2]];
}

void AnharmonicCore::calc_self3omega_tetrahedron(const double Temp,
                                                 const KpointMeshUniform *kmesh_in,
                                                 const double *const *eval_in,
                                                 const std::complex<double> *const *const *evec_in,
                                                 const unsigned int ik_in,
                                                 const unsigned int snum,
                                                 const unsigned int nomega,
                                                 const double *omega,
                                                 double *ret)
{
    // This function returns the imaginary part of phonon self-energy 
    // for the given frequency range of omega, phonon frequency (eval) and phonon eigenvectors (evec).
    // The tetrahedron method will be used.
    // This version employs the crystal symmetry to reduce the computational cost
    // In addition, both MPI and OpenMP parallelization are used in a hybrid way inside this function.

    const auto nk = kmesh_in->nk;
    const int ns = dynamical->neval;

    int ik, ib, iomega;
    const auto ns2 = ns * ns;

    unsigned int i;
    unsigned int is, js;
    unsigned int k1, k2;
    unsigned int arr[3];
    unsigned int nk_tmp;

    double n1, n2;
    double f1, f2;
    double omega_inner[2];

    unsigned int *kmap_identity;
    int **kpairs;
    double **energy_tmp;
    double **weight_tetra;
    double **v3_arr, *v3_arr_loc;
    double *ret_private;

    std::vector<KsListGroup> triplet;
    std::vector<int> vk_l;

    const int knum = kmesh_in->kpoint_irred_all[ik_in][0].knum;
    const int knum_minus = kmesh_in->kindex_minus_xk[knum];

    kmesh_in->get_unique_triplet_k(ik_in,
                                   symmetry->SymmList,
                                   false,
                                   false,
                                   triplet);

    const auto npair_uniq = triplet.size();

    if (npair_uniq != nk) {
        exit("calc_self3omega_tetrahedron", "Something is wrong.");
    }

    allocate(kpairs, nk, 2);
    allocate(kmap_identity, nk);

    for (i = 0; i < nk; ++i) kmap_identity[i] = i;

    for (iomega = 0; iomega < nomega; ++iomega) ret[iomega] = 0.0;

    for (ik = 0; ik < npair_uniq; ++ik) {
        kpairs[ik][0] = triplet[ik].group[0].ks[0];
        kpairs[ik][1] = triplet[ik].group[0].ks[1];
    }

    if (nk % mympi->nprocs != 0) {
        nk_tmp = nk / mympi->nprocs + 1;
    } else {
        nk_tmp = nk / mympi->nprocs;
    }

    vk_l.clear();

    for (ik = 0; ik < nk; ++ik) {
        if (ik % mympi->nprocs == mympi->my_rank) {
            vk_l.push_back(ik);
        }
    }

    if (vk_l.size() < nk_tmp) {
        vk_l.push_back(-1);
    }

    allocate(v3_arr_loc, ns2);
    allocate(v3_arr, nk_tmp * mympi->nprocs, ns2);

    for (ik = 0; ik < nk_tmp; ++ik) {

        int ik_now = vk_l[ik];

        if (ik_now == -1) {

            for (ib = 0; ib < ns2; ++ib) v3_arr_loc[ib] = 0.0; // do nothing

        } else {
#ifdef _OPENMP
#pragma omp parallel for private(is, js, arr)
#endif
            for (ib = 0; ib < ns2; ++ib) {

                is = ib / ns;
                js = ib % ns;

                arr[0] = ns * knum_minus + snum;
                arr[1] = ns * kpairs[ik_now][0] + is;
                arr[2] = ns * kpairs[ik_now][1] + js;

                v3_arr_loc[ib] = std::norm(V3(arr,
                                              kmesh_in->xk,
                                              eval_in,
                                              evec_in,
                                              phase_storage_dos));
            }
        }
        MPI_Gather(&v3_arr_loc[0], ns2, MPI_DOUBLE,
                   v3_arr[ik * mympi->nprocs], ns2,
                   MPI_DOUBLE, 0, MPI_COMM_WORLD);
    }
    deallocate(v3_arr_loc);

    if (mympi->my_rank == 0) {

#ifdef _OPENMP
#pragma omp parallel private(is, js, k1, k2, energy_tmp, i, \
                             iomega, weight_tetra, ik, \
                             omega_inner, f1, f2, n1, n2)
#endif
        {
            allocate(energy_tmp, 2, nk);
            allocate(weight_tetra, 2, nk);
#ifdef _OPENMP
            const int nthreads = omp_get_num_threads();
            const int ithread = omp_get_thread_num();
#else
            const int nthreads = 1;
            const int ithread = 0;
#endif

#ifdef _OPENMP
#pragma omp single
#endif
            {
                allocate(ret_private, nthreads * nomega);
                for (i = 0; i < nthreads * nomega; ++i) ret_private[i] = 0.0;
            }
#ifdef _OPENMP
#pragma omp for
#endif
            for (ib = 0; ib < ns2; ++ib) {

                is = ib / ns;
                js = ib % ns;

                for (ik = 0; ik < nk; ++ik) {
                    k1 = kpairs[ik][0];
                    k2 = kpairs[ik][1];

                    energy_tmp[0][ik] = eval_in[k1][is] + eval_in[k2][js];
                    energy_tmp[1][ik] = eval_in[k1][is] - eval_in[k2][js];
                }
                for (iomega = 0; iomega < nomega; ++iomega) {
                    for (i = 0; i < 2; ++i) {
                        integration->calc_weight_tetrahedron(nk, kmap_identity,
                                                             energy_tmp[i], omega[iomega],
                                                             dos->tetra_nodes_dos->get_ntetra(),
                                                             dos->tetra_nodes_dos->get_tetras(),
                                                             weight_tetra[i]);
                    }

                    for (ik = 0; ik < nk; ++ik) {
                        k1 = kpairs[ik][0];
                        k2 = kpairs[ik][1];

                        omega_inner[0] = eval_in[k1][is];
                        omega_inner[1] = eval_in[k2][js];
                        if (thermodynamics->classical) {
                            f1 = thermodynamics->fC(omega_inner[0], Temp);
                            f2 = thermodynamics->fC(omega_inner[1], Temp);
                            n1 = f1 + f2;
                            n2 = f1 - f2;
                        } else {
                            f1 = thermodynamics->fB(omega_inner[0], Temp);
                            f2 = thermodynamics->fB(omega_inner[1], Temp);
                            n1 = f1 + f2 + 1.0;
                            n2 = f1 - f2;
                        }

                        //#pragma omp critical
                        ret_private[nomega * ithread + iomega]
                                += v3_arr[ik][ib] * (n1 * weight_tetra[0][ik] - 2.0 * n2 * weight_tetra[1][ik]);
                    }
                }
            }
#ifdef _OPENMP
#pragma omp for
#endif
            for (iomega = 0; iomega < nomega; ++iomega) {
                for (int t = 0; t < nthreads; t++) {
                    ret[iomega] += ret_private[nomega * t + iomega];
                }
            }
            deallocate(energy_tmp);
            deallocate(weight_tetra);
        }
#ifdef _OPENMP
#pragma omp parallel for
#endif
        for (iomega = 0; iomega < nomega; ++iomega) {
            ret[iomega] *= pi * std::pow(0.5, 4);
        }
        deallocate(ret_private);
    }

    deallocate(v3_arr);
    deallocate(kmap_identity);
    deallocate(kpairs);
}

int AnharmonicCore::get_ngroup_fcs(const unsigned int order) const
{
    if (order == 3) return ngroup_v3;
    if (order == 4) return ngroup_v4;
    return 0;
}

std::vector<double> *AnharmonicCore::get_fcs_group(const unsigned int order) const
{
    if (order == 3) return fcs_group_v3;
    if (order == 4) return fcs_group_v4;
    return nullptr;
}

double *AnharmonicCore::get_invmass_factor(const unsigned int order) const
{
    if (order == 3) return invmass_v3;
    if (order == 4) return invmass_v4;
    return nullptr;
}

int **AnharmonicCore::get_evec_index(const unsigned int order) const
{
    if (order == 3) return evec_index_v3;
    if (order == 4) return evec_index_v4;
    return nullptr;
}
