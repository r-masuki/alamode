/*
 scph.h

 Copyright (c) 2015 Terumasa Tadano

 This file is distributed under the terms of the MIT license.
 Please see the file 'LICENCE.txt' in the root directory 
 or http://opensource.org/licenses/mit-license.php for information.
*/

#pragma once

#include "pointers.h"
#include "kpoint.h"
#include "anharmonic_core.h"
#include "gruneisen.h"
#include "dynamical.h"
#include <complex>
#include <Eigen/Dense>

namespace PHON_NS {
class DistList {
public:
    unsigned int cell_s;
    double dist;

    DistList();

    DistList(const unsigned int cell_s_,
             const double dist_) : cell_s(cell_s_), dist(dist_) {};

    bool operator<(const DistList &obj) const
    {
        return dist < obj.dist;
    }
};


class Scph : protected Pointers {
public:

    Scph(class PHON *phon);

    ~Scph();

    unsigned int kmesh_scph[3];
    unsigned int kmesh_interpolate[3];
    unsigned int ialgo;
    unsigned int bubble;

    bool restart_scph;
    bool warmstart_scph;
    bool lower_temp;
    double tolerance_scph;

    void exec_scph();

    void setup_scph();

    double mixalpha;
    unsigned int maxiter;
    bool print_self_consistent_fc2;
    bool selfenergy_offdiagonal;
    //int relax_str;

    // variables related to structural optimization
//    int relax_algo;
//    int max_str_iter;
//    double coord_conv_tol;
//    double mixbeta_coord;
//    double alpha_steepest_decent;
//    double cell_conv_tol;
//    double mixbeta_cell;

//    int set_init_str;
//    int cooling_u0_index; // used if set_init_str is 3
//    double cooling_u0_thr; // used if set_init_str is 3
//    double add_hess_diag;
//    double stat_pressure;

    // optimization scheme used in QHA
    //int qha_scheme;

    // options of IFC renormalization
//    int renorm_3to2nd;
//    int renorm_2to1st;
//    int renorm_34to1st;
//    std::string strain_IFC_dir;

    // initial strain and displacement
//    double **init_u_tensor = nullptr;
//    std::vector<double> init_u0;

    // zero-th order term of the potential energy surface
//    double *V0 = nullptr;

    void zerofill_harmonic_dymat_renormalize(std::complex<double> ****,
                                             unsigned int);


    void write_anharmonic_correction_fc2(std::complex<double> ****delta_dymat,
                                         const unsigned int NT,
                                         const int type = 0);

    void load_scph_dymat_from_file(std::complex<double> ****);

    void load_scph_dymat_from_file(std::complex<double> ****,
                                   std::string);

    void store_scph_dymat_to_file(const std::complex<double> *const *const *const *,
                                  std::string);

    void compute_V3_elements_for_given_IFCs(std::complex<double> ***v3_out,
                                            const int ngroup_v3_in,
                                            std::vector<double> *fcs_group_v3_in,
                                            std::vector<RelativeVector> *relvec_v3_in,
                                            double *invmass_v3_in,
                                            int **evec_index_v3_in,
                                            const std::complex<double> *const *const *evec_in,
                                            const bool self_offdiag);


    void compute_V4_elements_mpi_over_kpoint(std::complex<double> ***v4_out,
                                             std::complex<double> ***evec_in,
                                             const bool self_offdiag,
                                             const bool relax,
                                             const KpointMeshUniform *kmesh_coarse_in,
                                             const KpointMeshUniform *kmesh_dense_in,
                                             const std::vector<int> &kmap_coarse_to_dense,
                                             const PhaseFactorStorage *phase_storage_in,
                                             std::complex<double> *phi4_reciprocal_inout);

    void compute_V4_elements_mpi_over_band(std::complex<double> ***v4_out,
                                           std::complex<double> ***evec_in,
                                           const bool self_offdiag,
                                           const KpointMeshUniform *kmesh_coarse_in,
                                           const KpointMeshUniform *kmesh_dense_in,
                                           const std::vector<int> &kmap_coarse_to_scph,
                                           const PhaseFactorStorage *phase_storage_in,
                                           std::complex<double> *phi4_reciprocal_inout);


    void compute_V3_elements_mpi_over_kpoint(std::complex<double> ***v3_out,
                                             const std::complex<double> *const *const *evec_in,
                                             const bool self_offdiag,
                                             const KpointMeshUniform *kmesh_coarse_in,
                                             const KpointMeshUniform *kmesh_dense_in,
                                             const std::vector<int> &kmap_coarse_to_scph,
                                             const PhaseFactorStorage *phase_storage_in,
                                             std::complex<double> *phi4_reciprocal_inout);

    void compute_anharmonic_del_v0_del_umn(std::complex<double> *del_v0_del_umn_SCP,
                                           std::complex<double> *del_v0_del_umn_renorm,
                                           std::complex<double> ***del_v2_del_umn,
                                           std::complex<double> ***del2_v2_del_umn2,
                                           std::complex<double> ****del_v3_del_umn,
                                           double **u_tensor,
                                           double *q0,
                                           std::complex<double> ***cmat_convert,
                                           double **omega2_anharm_T,
                                           const double T_in,
                                           const KpointMeshUniform *kmesh_dense_in);

    void compute_anharmonic_v1_array(std::complex<double> *v1_SCP,
                                     std::complex<double> *v1_renorm,
                                     std::complex<double> ***v3_renorm,
                                     std::complex<double> ***cmat_convert,
                                     double **omega2_anharm_T,
                                     const double T_in,
                                     const KpointMeshUniform *kmesh_dense_in);

    void calculate_del_v0_del_umn_renorm(std::complex<double> *del_v0_del_umn_renorm,
                                         double *C1_array,
                                         double **C2_array,
                                         double ***C3_array,
                                         double **eta_tensor,
                                         double **u_tensor,
                                         std::complex<double> **del_v1_del_umn,
                                         std::complex<double> **del2_v1_del_umn2,
                                         std::complex<double> **del3_v1_del_umn3,
                                         std::complex<double> ***del_v2_del_umn,
                                         std::complex<double> ***del2_v2_del_umn2,
                                         std::complex<double> ****del_v3_del_umn,
                                         double *q0,
                                         double pvcell,
                                         const KpointMeshUniform *kmesh_dense_in);


private:

    // Information of kmesh for SCPH calculation
    KpointMeshUniform *kmesh_coarse = nullptr;
    KpointMeshUniform *kmesh_dense = nullptr;
    std::vector<int> kmap_interpolate_to_scph;

    // Information for calculating the ph-ph interaction coefficients
    std::complex<double> *phi3_reciprocal, *phi4_reciprocal;

    // Phase shift
    PhaseFactorStorage *phase_factor_scph;

    // Information of harmonic dynamical matrix
    double **omega2_harmonic;
    std::complex<double> ***evec_harmonic;
    MinimumDistList ***mindist_list_scph;

    // Local variables for handling symmetry of dynamical matrix
    std::complex<double> ****mat_transform_sym;
    std::vector<int> *symop_minus_at_k;
    KpointSymmetry *kpoint_map_symmetry;

    std::vector<Eigen::MatrixXcd> dymat_harm_short, dymat_harm_long;

    int compute_Cv_anharmonic;

    void set_default_variables();

    void deallocate_variables();

    void setup_kmesh();

    void setup_eigvecs();

    void setup_pp_interaction();

    void setup_transform_ifc();

    void setup_transform_symmetry();


    void exec_scph_main(std::complex<double> ****);

    void exec_scph_relax_cell_coordinate_main(std::complex<double> ****,
                                              std::complex<double> ****);

//    void exec_QHA_relax_main(std::complex<double> ****,
//                             std::complex<double> ****);
//
//    void exec_perturbative_QHA(std::complex<double> ****,
//                               std::complex<double> ****);

//
//    void precompute_dymat_harm(const unsigned int nk_in,
//                               double **xk_in,
//                               double **kvec_in);
//
//    void read_cell_opt_input(double &,
//                             double &,
//                             double &,
//                             double &);
//
//
//    void calculate_force_in_real_space(const std::complex<double> *const,
//                                       double *);


    void postprocess(std::complex<double> ****,
                     std::complex<double> ****,
                     std::complex<double> ****);


    void zerofill_elements_acoustic_at_gamma(double **,
                                             std::complex<double> ***,
                                             const int) const;


    FcsClassExtent from_FcsArrayWithCell_to_FcsClassExtent(const FcsArrayWithCell &);


    void compute_anharmonic_frequency(std::complex<double> ***,
                                      double **,
                                      std::complex<double> ***,
                                      double,
                                      bool &,
                                      std::complex<double> ***,
                                      bool,
                                      std::complex<double> **,
                                      const unsigned int verbosity);


    void compute_anharmonic_frequency2(std::complex<double> ***,
                                       double **,
                                       std::complex<double> ***,
                                       double,
                                       bool &,
                                       std::complex<double> ***,
                                       bool,
                                       const unsigned int verbosity);

    void update_frequency(const double temperature_in,
                          const Eigen::MatrixXd &omega_in,
                          const std::vector<Eigen::MatrixXcd> &Fmat0,
                          const std::vector<Eigen::MatrixXcd> &evec0,
                          std::complex<double> ***dymat0,
                          std::complex<double> ***v4_array_all,
                          std::complex<double> ***cmat_convert,
                          std::vector<Eigen::MatrixXcd> &dmat,
                          std::complex<double> ***dymat_out,
                          std::complex<double> ***evec_out,
                          const double alpha,
                          const bool offdiag,
                          Eigen::MatrixXd &omega_out);

    void get_permutation_matrix(const int ns,
                                std::complex<double> **cmat_in,
                                Eigen::MatrixXd &permutation_matrix) const;



//    void r2q(const double *,
//             unsigned int,
//             unsigned int,
//             unsigned int,
//             unsigned int,
//             std::complex<double> ***,
//             std::complex<double> **) const;



    void find_degeneracy(std::vector<int> *degeneracy_out,
                         unsigned int nk_in,
                         double **eval_in) const;

    static double distance(double *,
                           double *);

    void symmetrize_dynamical_matrix(unsigned int,
                                     Eigen::MatrixXcd &) const;

    void replicate_dymat_for_all_kpoints(std::complex<double> ***) const;


    static void mpi_bcast_complex(std::complex<double> ****data,
                                  const unsigned int NT,
                                  const unsigned int nk,
                                  const unsigned int ns);

    void get_derivative_central_diff(const double delta_t,
                                     const unsigned int nk,
                                     double **omega0,
                                     double **omega2,
                                     double **domega_dt);

    void compute_free_energy_bubble_SCPH(const unsigned int [3],
                                         std::complex<double> ****);

    void bubble_correction(std::complex<double> ****,
                           std::complex<double> ****);

    std::vector<std::complex<double>> get_bubble_selfenergy(const KpointMeshUniform *kmesh_in,
                                                            const unsigned int ns_in,
                                                            const double *const *eval_in,
                                                            const std::complex<double> *const *const *evec_in,
                                                            const unsigned int knum,
                                                            const unsigned int snum,
                                                            const double temp_in,
                                                            const std::vector<std::complex<double>> &omegalist);


};

extern "C" {
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