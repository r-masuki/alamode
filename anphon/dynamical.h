/*
 dynamical.h

 Copyright (c) 2014-2021 Terumasa Tadano

 This file is distributed under the terms of the MIT license.
 Please see the file 'LICENCE.txt' in the root directory 
 or http://opensource.org/licenses/mit-license.php for information.
*/

#pragma once

#include "pointers.h"
#include "fcs_phonon.h"
#include "kpoint.h"
#include "memory.h"
#include <vector>
#include <complex>
#include <string>
#include <Eigen/Core>

namespace PHON_NS {
class DistWithCell {
public:
    int cell;
    double dist;

    DistWithCell();

    DistWithCell(const int n,
                 const double d) : cell(n), dist(d) {};
};

inline bool operator<(const DistWithCell a,
                      const DistWithCell b)
{
    return a.dist < b.dist;
}

class DymatEigenValue {
public:
    DymatEigenValue() : nk(0), ns(0), eval(nullptr), evec(nullptr),
                        is_stored_eigvec(true), is_irreducible_only(false) {};

    DymatEigenValue(const bool stored_eigvec_,
                    const bool store_irreducible_only_,
                    const unsigned int nk_in,
                    const unsigned int ns_in) : nk(nk_in), ns(ns_in),
                                                is_stored_eigvec(stored_eigvec_),
                                                is_irreducible_only(store_irreducible_only_)
    {
        if (eval) deallocate(eval);
        if (evec) deallocate(evec);

        allocate(eval, nk_in, ns_in);
        if (is_stored_eigvec) {
            allocate(evec, nk_in, ns_in, ns_in);
        }
    };

    ~DymatEigenValue()
    {
        if (eval) deallocate(eval);
        if (evec) deallocate(evec);
    };

    void set_eigenvalues(const unsigned int n,
                         double **eval_in);

    void set_eigenvectors(const unsigned int n,
                          std::complex<double> ***evec_in);

    void set_eigenvals_and_eigenvecs(const unsigned int n,
                                     double **eval_in,
                                     std::complex<double> ***evec_in);

    double **get_eigenvalues() const;

    std::complex<double> ***get_eigenvectors() const;


private:
    unsigned int nk, ns;
    double **eval = nullptr;
    std::complex<double> ***evec = nullptr;
    bool is_stored_eigvec = true;
    bool is_irreducible_only = false;
};


class Dynamical : protected Pointers {
public:
    Dynamical(class PHON *);

    ~Dynamical();

    unsigned int neval{};
    bool eigenvectors{};
    bool print_eigenvectors{};
    unsigned int symmetrize_borncharge{};
    unsigned int nonanalytic{};
    bool participation_ratio{};
    unsigned int band_connection{};

    std::string file_born;
    double na_sigma{};

    int **index_bconnect{};
    double dielec[3][3]{};
    double ***borncharge{};

    bool **is_imaginary{};

    DymatEigenValue *dymat_band, *dymat_general;

    void diagonalize_dynamical_all();

    void setup_dynamical();

    void setup_dielectric(const unsigned int verbosity = 1);

    void eval_k(const double *,
                const double *,
                const std::vector<FcsArrayWithCell> &,
                double *,
                std::complex<double> **,
                const bool) const;

    void modify_eigenvectors() const;

    void eval_k_ewald(const double *,
                      const double *,
//                      const std::vector<FcsClassExtent> &,
                      const std::vector<FcsArrayWithCell> &,
                      double *,
                      std::complex<double> **,
                      const bool) const;

    double fold(const double) const;

    double freq(const double) const;

    void calc_participation_ratio_all(const unsigned int nk_in,
                                      const std::complex<double> *const *const *evec_in,
                                      double **ret,
                                      double ***ret_all) const;

    void calc_analytic_k(const double *,
                         const std::vector<FcsClassExtent> &,
                         std::complex<double> **) const;

    void calc_analytic_k(const double *,
                         const std::vector<FcsArrayWithCell> &,
                         std::complex<double> **) const;

    void calc_nonanalytic_k(const double *,
                            const double *,
                            std::complex<double> **) const;

    void calc_nonanalytic_k2(const double *,
                             const double *,
                             std::complex<double> **) const;

    void project_degenerate_eigenvectors(const Eigen::Matrix3d &lavec_p,
                                         const std::vector<FcsArrayWithCell> &fc2_in,
                                         double *xk_in,
                                         const std::vector<std::vector<double>> &project_directions,
                                         std::complex<double> **evec_out) const;

    std::vector<std::vector<double>> get_projection_directions() const;

    void set_projection_directions(const std::vector<std::vector<double>> projections_in);

    void r2q(const double *xk_in,
             const unsigned int nx,
             const unsigned int ny,
             const unsigned int nz,
             const unsigned int ns,
             MinimumDistList ***mindist_list_in,
             std::complex<double> ***dymat_r_in,
             std::complex<double> **dymat_k_out) const;

    void precompute_dymat_harm(const unsigned int nk_in,
                               double **xk_in,
                               double **kvec_in,
                               std::vector<Eigen::MatrixXcd> &dymat_short,
                               std::vector<Eigen::MatrixXcd> &dymat_long) const;


    void compute_renormalized_harmonic_frequency(double **omega2_out,
                                                 std::complex<double> ***evec_harm_renormalized,
                                                 std::complex<double> **delta_v2_renorm,
                                                 const double *const *omega2_harmonic,
                                                 const std::complex<double> *const *const *evec_harmonic,
                                                 const KpointMeshUniform *kmesh_coarse,
                                                 const KpointMeshUniform *kmesh_dense,
                                                 const std::vector<int> &kmap_interpolate_to_scph,
                                                 std::complex<double> ****mat_transform_sym,
                                                 MinimumDistList ***mindist_list,
                                                 const unsigned int verbosity);

    void symmetrize_dynamical_matrix(const unsigned int ik,
                                     const KpointMeshUniform *kmesh_coarse,
                                     std::complex<double> ****mat_transform_sym,
                                     Eigen::MatrixXcd &dymat) const;

    void replicate_dymat_for_all_kpoints(const KpointMeshUniform *kmesh_coarse,
                                         std::complex<double> ****mat_transform_sym,
                                         std::complex<double> ***dymat_inout) const;

    void diagonalize_interpolated_matrix(std::complex<double> **,
                                         double *,
                                         std::complex<double> **,
                                         bool) const;
    double **get_xrs_image() const;

    void exec_interpolation(const unsigned int kmesh_orig[3],
                            std::complex<double> ***dymat_r,
                            const unsigned int nk_dense,
                            double **xk_dense,
                            double **kvec_dense,
                            double **eval_out,
                            std::complex<double> ***evec_out,
                            const std::vector<Eigen::MatrixXcd> &dymat_short,
                            const std::vector<Eigen::MatrixXcd> &dymat_long,
                            MinimumDistList ***mindist_list_in,
                            const bool use_precomputed_dymat = false,
                            const bool return_sqrt = true);


    void calc_new_dymat_with_evec(std::complex<double> ***dymat_out,
                                  double **omega2_in,
                                  std::complex<double> ***evec_in,
                                  const KpointMeshUniform *kmesh_coarse,
                                  const std::vector<int> &kmap_interpolate_to_scph);


    void get_symmetry_gamma_dynamical(KpointMeshUniform *kmesh_in,
                                      const unsigned int natmin_in,
                                      const Eigen::MatrixXd &x_fractional_in,
                                      const std::vector<std::vector<unsigned int>> &map_p2s_in,
                                      const std::vector<SymmetryOperationWithMapping> &symmlist,
                                      std::complex<double> ****&mat_transform_sym) const;

private:
    void set_default_variables();

    void deallocate_variables();

    void load_born(const unsigned int flag_symmborn,
                   const unsigned int verbosity = 1);

    void prepare_mindist_list(std::vector<int> **) const;

    void calc_atomic_participation_ratio(const std::complex<double> *evec_in,
                                         double *ret) const;

    double distance(double *,
                    double *) const;

    void connect_band_by_eigen_similarity(const unsigned int nk_in,
                                          std::complex<double> ***evec,
                                          int **index_sorted) const;

    void detect_imaginary_branches(const KpointMeshUniform &kmesh_in,
                                   double **eval_in);

    void get_eigenvalues_dymat(const unsigned int nk_in,
                               const double *const *xk_in,
                               const double *const *kvec_na_in,
                               const std::vector<FcsArrayWithCell> &fc2,
//                               const std::vector<FcsClassExtent> &fc2_without_dipole_in,
                               const std::vector<FcsArrayWithCell> &fc2_without_dipole_in,
                               const bool require_evec,
                               double **eval_ret,
                               std::complex<double> ***evec_ret);

    std::vector<std::vector<double>> projection_directions;

    int transform_eigenvectors(double *xk_in,
                               std::vector<double> perturb_direction,
                               const double dk,
                               Eigen::MatrixXcd &evec_sub) const;


    void duplicate_xk_boundary(double *,
                               std::vector<std::vector<double>> &);


    double **xshift_s;
    char UPLO{};
    std::complex<double> ***dymat{};
    std::vector<int> **mindist_list{};
};

extern "C" {
void zheev_(const char *jobz,
            const char *uplo,
            int *n,
            std::complex<double> *a,
            int *lda,
            double *w,
            std::complex<double> *work,
            int *lwork,
            double *rwork,
            int *info);

void zgemm_(const char *transa,
            const char *transb,
            int *m,
            int *n,
            int *k,
            std::complex<double> *alpha,
            std::complex<double> *a,
            int *lda,
            std::complex<double> *b,
            int *ldb,
            std::complex<double> *beta,
            std::complex<double> *c,
            int *ldc);
}
}
