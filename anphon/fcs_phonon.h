/*
 fcs_phonon.h

 Copyright (c) 2014 Terumasa Tadano

 This file is distributed under the terms of the MIT license.
 Please see the file 'LICENCE.txt' in the root directory 
 or http://opensource.org/licenses/mit-license.php for information.
*/

#pragma once

#include "pointers.h"
#include "mathfunctions.h"
#include <string>
#include <vector>
#include <set>

namespace PHON_NS {

class FcsClassExtent {
public:
    unsigned int atm1, atm2;
    unsigned int xyz1, xyz2;
    unsigned int cell_s;
    double fcs_val;

    FcsClassExtent() {};

    FcsClassExtent(const FcsClassExtent &obj)
    {
        atm1 = obj.atm1;
        atm2 = obj.atm2;
        xyz1 = obj.xyz1;
        xyz2 = obj.xyz2;
        cell_s = obj.cell_s;
        fcs_val = obj.fcs_val;
    }

    bool operator==(const FcsClassExtent &a) const
    {
        return (this->atm1 == a.atm1) & (this->atm2 == a.atm2)
               & (this->xyz1 == a.xyz1) & (this->xyz2 == a.xyz2)
               & (this->cell_s == a.cell_s);
    }
};

struct AtomCellSuper {
    unsigned int index;// flattened array
    unsigned int tran;
    unsigned int cell_s;
};

inline bool operator<(const AtomCellSuper &a,
                      const AtomCellSuper &b)
{
    return a.index < b.index;
}

class FcsArrayWithCell {
public:
    std::vector<AtomCellSuper> pairs;
    //std::vector<unsigned int> atoms_p; // atom index in the primitive cell (not used?)
    std::vector<unsigned int> atoms_s; // atom index in the supercell
    std::vector<unsigned int> coords; // xyz components
    double fcs_val;
    std::vector<Eigen::Vector3d> relvecs; // For computing phase factor in exp
    std::vector<Eigen::Vector3d> relvecs_velocity; // For computing group velocity matrix

    FcsArrayWithCell() {};

    FcsArrayWithCell(const double fcs_in,
                     const std::vector<AtomCellSuper> &pairs_in,
                     const std::vector<unsigned int> &atoms_s_in) : pairs(pairs_in),
                                                           fcs_val(fcs_in),
                                                           atoms_s(atoms_s_in)
                    {
                        coords.clear();
                        for (const auto &it : pairs_in) {
                            coords.push_back(it.index%3);
                        }
                    };

    FcsArrayWithCell(const double fcs_in,
                     const std::vector<AtomCellSuper> &pairs_in,
                     const std::vector<unsigned int> &atoms_s_in,
                     const std::vector<Eigen::Vector3d> &relvecs_vel_in) : pairs(pairs_in),
                                                                       fcs_val(fcs_in),
                                                                       atoms_s(atoms_s_in),
                                                                       relvecs_velocity(relvecs_vel_in)
                     {
                         coords.clear();
                         for (const auto &it : pairs_in) {
                             coords.push_back(it.index%3);
                         }
                     };

    FcsArrayWithCell(const double fcs_in,
                     const std::vector<AtomCellSuper> &pairs_in,
                     const std::vector<unsigned int> &atoms_s_in,
                     const std::vector<Eigen::Vector3d> &relvecs_in,
                     const std::vector<Eigen::Vector3d> &relvecs_vel_in) : pairs(pairs_in),
                     fcs_val(fcs_in),
                     atoms_s(atoms_s_in),
                     relvecs(relvecs_in),
                     relvecs_velocity(relvecs_vel_in)
                     {
                        coords.clear();
                        for (const auto &it : pairs_in) {
                            coords.push_back(it.index%3);
                        }
                     };

    bool operator<(const FcsArrayWithCell &obj) const
    {
        std::vector<unsigned int> index_a, index_b;
        index_a.clear();
        index_b.clear();
        for (int i = 0; i < pairs.size(); ++i) {
            index_a.push_back(pairs[i].index);
            index_b.push_back(obj.pairs[i].index);
        }
        for (int i = 0; i < pairs.size(); ++i) {
            index_a.push_back(pairs[i].tran);
            index_a.push_back(pairs[i].cell_s);
            index_b.push_back(obj.pairs[i].tran);
            index_b.push_back(obj.pairs[i].cell_s);
        }
        return lexicographical_compare(index_a.begin(), index_a.end(),
                                       index_b.begin(), index_b.end());
    }




};

class Fcs_phonon : protected Pointers {
public:
    Fcs_phonon(class PHON *);

    ~Fcs_phonon();

    void setup(std::string);

    unsigned int maxorder;
    std::string file_fcs, file_fc2, file_fc3, file_fc4;

    std::vector<FcsArrayWithCell> *force_constant_with_cell;
//    std::vector<FcsClassExtent> fc2_ext;

    bool update_fc2;

private:
    bool require_cubic;
    bool require_quartic;

    void set_default_variables();

    void deallocate_variables();

    void load_fc2_xml();

    void load_fcs_xml(const std::string fname_fcs,
                      const int order,
                      std::vector<FcsArrayWithCell> &fcs_out) const;

    void parse_fcs_from_h5(const std::string fname_fcs,
                           const int order,
                           std::vector<FcsArrayWithCell> &fcs_out) const;

    void load_fcs_from_file(const int maxorder_in) const;


    double examine_translational_invariance(const int order,
                                            const unsigned int nat,
                                            const unsigned int natmin,
                                            const std::vector<std::vector<unsigned int>> &map_p2s_in,
                                            const std::vector<FcsArrayWithCell> &fc_in) const;

    void replicate_force_constants(const int maxorder_in);

    void MPI_Bcast_fc_class(unsigned int) const;

    void MPI_Bcast_fcs_array(unsigned int) const;

    void MPI_Bcast_fc2_ext();
};
}
