/* -*- c++ -*- ----------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   http://lammps.sandia.gov, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

#ifdef PAIR_CLASS

PairStyle(ls,PairLS)

#else

#ifndef LMP_PAIR_LS_H
#define LMP_PAIR_LS_H

#include "pair.h"

namespace LAMMPS_NS {


class PairLS : public Pair {
 public:

  PairLS(class LAMMPS *);
  virtual ~PairLS();
  virtual void compute(int, int);
  void settings(int, char **);
  virtual void coeff(int, char **);
  void init_style();
  double init_one(int, int);
  // double single(int, int, int, int, double, double, double, double &);
  // virtual void *extract(const char *, int &);

  // virtual int pack_forward_comm(int, int *, double *, int, int *);
  // virtual void unpack_forward_comm(int, int, double *);
  // int pack_reverse_comm(int, int, double *);
  // void unpack_reverse_comm(int, int *, double *);
  // double memory_usage();
  // void swap_ls(double *, double **);



 protected:
 
  // Begin max_at.h
  const int  n_mark_at=10;
  const long  max_at=100000;
  const int  max_at_def=100;
  const int  max_neighb=200;
  const long  max_pair_at=max_at*max_neighb;
  const long  max_pair_at_def=max_at_def*max_neighb;
  const long  max_list=max_pair_at+max_at;
  const long  max_cell=100000;
  const long  max_at_group=max_at/5;
  const int  max_group=10000;
  const int  max_p=1;
  const int  max_hole=max_p/100;
  const long  max_seat=max_at;
  // End max_at.h

  // Begin pot_ls.h
  const int mf3=4;            // max_f3
  const int mfi=30;           // max_sp_fi
  const int mro=25;           // max_sp_ro
  const int memb=10;          // max_sp_emb
  const int mf=15;            // max_sp_f
  const int mg=15;            // max_sp_g
  const int mi=6;             // max_sort_at



 
 // common/abcd_sp/
 // pointers to arrays with spline coefficients and data about steps 
 
  double **shag_sp_fi, **shag_sp_ro, *shag_sp_emb, **shag_sp_f, shag_sp_g; 
  double ***R_sp_fi, ***R_sp_ro, **R_sp_emb, ***R_sp_f, *R_sp_g; 
  // spline coefficients determining functions for pair interactions: the first term in the equation for total energy fi(R_ij) 
  double ***a_sp_fi, ***b_sp_fi, ***c_sp_fi, ***d_sp_fi;     
  // spline coefficients determining basis functions rho(R_ij)
  double ***a_sp_ro, ***b_sp_ro, ***c_sp_ro, ***d_sp_ro;     
  // spline coefficients determining functions for manybody interactions within the centro-symmetric approximation (CSA): the last term in the equation for total energy F(sum_i!=j(rho_ij)) 
  double **a_sp_emb, **b_sp_emb, **c_sp_emb, **d_sp_emb; 
  // spline coefficients determining basis functions f3(R_ij) in the second term describing three-body interactions in the equation for total energy
  double ****a_sp_f3, ****b_sp_f3, ****c_sp_f3, ****d_sp_f3;  
  // spline coefficients determining expansion coefficients g3(cos(theta_ijk)) for basis functions f3(R_ij) in the third term describing three-body interactions in the equation for total energy    
  double ****a_sp_g3, ****b_sp_g3, ****c_sp_g3, ****d_sp_g3;   
  // spline coefficients determining basis functions f4(R_ij) in the third term describing four-body interactions in the equation for total energy   
  double ***a_sp_f4, ***b_sp_f4, ***c_sp_f4, ***d_sp_f4;
  // spline coefficients determining expansion coefficients g4(cos(theta_ijk),cos(theta_jkl),cos(theta_ilj)) for basis functions f4(R_ij) in the third term describing four-body interactions in the equation for total energy    
  double **a_sp_g4, **b_sp_g4, **c_sp_g4, **d_sp_g4;    
  double **fip_rmin;
  double *z_ion, *c_ZBL, *d_ZBL, **zz_ZBL, **a_ZBL, **e0_ZBL;
  double **Rmin_fi_ZBL, ***c_fi_ZBL;
  double Rc_fi, Rc_f;


 // common/abcd_i/

  bool if_g3_pot;  // if two- and three-body interactions are accurately described by the potential
  bool if_g4_pot;  // if two-, three- and four-body interactions are accurately described by the potential
  bool if_F2_pot;  // if two-body interactions are accurately described by the potential (like within the EAM potential)
  bool *if_gp0_pot;
  // bool *if_diag;   // probably deprecated variable
  int n_sort;     // number of sorts of atoms
  int **n_sp_fi, **n_sp_ro, **n_sp_emb, **n_sp_f, **n_sp_g; // numbers of spline nodes  for differen 
  int *n_f3;    // array with numbers of basis functions for each sort of atom

  // arrays in stack as they stored in common blocks
  // double shag_sp_fi[mi][mi], shag_sp_ro[mi][mi], shag_sp_emb[mi], shag_sp_f[mi][mi], shag_sp_g;  
  // double R_sp_fi[mfi][mi][mi], R_sp_ro[mfi][mi][mi], R_sp_emb[memb][mi], R_sp_f[mf][mi][mi], R_sp_g[mg]; 
  // double a_sp_fi[mfi][mi][mi], b_sp_fi[mfi][mi][mi], c_sp_fi[mfi][mi][mi], d_sp_fi[mfi][mi][mi];
  // double a_sp_ro[mro][mi][mi], b_sp_ro[mro][mi][mi], c_sp_ro[mro][mi][mi], d_sp_ro[mro][mi][mi];
  // double a_sp_emb[memb][mi], b_sp_emb[memb][mi], c_sp_emb[memb][mi], d_sp_emb[memb][mi];
  // double a_sp_f3[mf][mf3][mi][mi], b_sp_f3[mf][mf3][mi][mi], c_sp_f3[mf][mf3][mi][mi], d_sp_f3[mf][mf3][mi][mi];
  // double a_sp_g3[mg][mf3][mf3][mi], b_sp_g3[mg][mf3][mf3][mi], c_sp_g3[mg][mf3][mf3][mi], d_sp_g3[mg][mf3][mf3][mi]; 
  // double a_sp_f4[mf][mi][mi], b_sp_f4[mf][mi][mi], c_sp_f4[mf][mi][mi], d_sp_f4[mf][mi][mi];
  // double a_sp_g4[mi][mi], b_sp_g4[mi][mi], c_sp_g4[mi][mi], d_sp_g4[mi][mi];
  // double fip_rmin[mi][mi];
  // double z_ion[mi], c_ZBL[4], d_ZBL[4], zz_ZBL[mi][mi], a_ZBL[mi][mi], e0_ZBL[mi][mi];
  // double Rmin_fi_ZBL[mi][mi], c_fi_ZBL[6][mi][mi];
  // double Rc_fi, Rc_f;

    // inverted arrays in stack in comparison as they declared in common blocks in pot_ls.h
  // double shag_sp_fi[mi][mi], shag_sp_ro[mi][mi], shag_sp_emb[mi], shag_sp_f[mi][mi], shag_sp_g;  
  // double R_sp_fi[mi][mi][mfi], R_sp_ro[mi][mi][mro], R_sp_emb[mi][memb], R_sp_f[mi][mi][mf], R_sp_g[mg]; 
  // double a_sp_fi[mi][mi][mfi], b_sp_fi[mi][mi][mfi], c_sp_fi[mi][mi][mfi], d_sp_fi[mi][mi][mfi];
  // double a_sp_ro[mi][mi][mro], b_sp_ro[mi][mi][mro], c_sp_ro[mi][mi][mro], d_sp_ro[mi][mi][mro];
  // double a_sp_emb[mi][memb], b_sp_emb[mi][memb], c_sp_emb[mi][memb], d_sp_emb[mi][memb];
  // double a_sp_f3[mi][mi][mf3][mf], b_sp_f3[mi][mi][mf3][mf], c_sp_f3[mi][mi][mf3][mf], d_sp_f3[mi][mi][mf3][mf];
  // double a_sp_g3[mi][mf3][mf3][mg], b_sp_g3[mi][mf3][mf3][mg], c_sp_g3[mi][mf3][mf3][mg], d_sp_g3[mi][mf3][mf3][mg]; 
  // double a_sp_f4[mi][mi][mf], b_sp_f4[mi][mi][mf], c_sp_f4[mi][mi][mf], d_sp_f4[mi][mi][mf];
  // double a_sp_g4[mi][mi], b_sp_g4[mi][mi], c_sp_g4[mi][mi], d_sp_g4[mi][mi];
  // double fip_rmin[mi][mi];
  // double z_ion[mi], c_ZBL[4], d_ZBL[4], zz_ZBL[mi][mi], a_ZBL[mi][mi], e0_ZBL[mi][mi];
  // double Rmin_fi_ZBL[mi][mi], c_fi_ZBL[mi][mi][6];
  // double Rc_fi, Rc_f;

  // End pot_ls.h

//   // Begin external fortan subroutines
//   extern void e_force_(double *e_sum, double *pressure, double *sxx, double *syy, double *szz, double *e_at, double *f_at[], double *px_at, double *py_at, double pz_at,
//   double *r_at[], int *i_sort_at, int *n_at, double *sizex, double *sizey, double *sizez);

//   extern void read_pot_(char *name_pot_is, char *name_pot_is1_is2, char *r_pot)

//   // End external fortan subroutines

  /* 
   Fortran subroutines translated into void functions (class methods) in C++ 
   In the original program md1_temp, all these subrotines use the variables stored in common blocks declared in file pot_ls.h 
   In the LAMMPS implementation, the variables from Fortran common blocks are declared as protected members of PairLS class
   The protected members of PairLS class can be inherited by any of its child classes   
  */

  // subroutines for reading potential files
  // void read_pot_ls(char *name_pot_is, char *name_pot_is1_is2, double *r_pot); // implemented in coeff method
  void r_pot_ls_is(char *, int);
  void r_pot_ls_is1_is2(char *, int, int);
  void allocate();

  // void par2pot_is(int);
  // void par2pot_is1_is2(int, int);

  // External Fortran subroutines that does not use the Fotran common block 
  // These subrotines are called by par2pot_is and par2pot_is1_is2 subrotines and may should be declared in the corresponding voids
  // These subroutines should be compiled separately with fortran compiler 
  // extern void smooth_zero_22_(double *B);
  // extern void SPL_(int n, );

   // // External fortan subrotines called from SPL and smooth_zero_22 subroutines
   // extern void LA30();
   // extern void dgesv_(int *n, int *nrhs,  double *a,  int  *lda,  
   //                    int *ipivot, double *b, int *ldb, int *info)

  // subroutines for calculating energies and forces
  //  void e_force_ls(double *e_sum, double *pressure, 
  //                  double *sxx, double *syy, double *szz, 
  //                  double *e_at, double *f_at[], 
  //                  double *px_at, double *py_at, double pz_at,
  //                  double *r_at[], int *i_sort_at, int *n_at, 
  //                  double *sizex, double *sizey, double *sizez);

  //  void e_force_fi_emb(e_at,f_at,px_at,py_at,pz_at,
  //            	        r_at,i_sort_at,n_at,sizex,sizey,sizez);

  //  void e_force_g3(e_at_g3,f_at_g3,px_at_g3,py_at_g3,pz_at_g3,
  //    :		       r_at,i_sort_at,n_at,sizex,sizey,sizez);



//   virtual void read_file(char *);
//   virtual void file2array();

};

}

#endif
#endif

/* ERROR/WARNING messages:

E: Illegal ... command

Self-explanatory.  Check the input script syntax and compare to the
documentation for the command.  You can use -echo screen as a
command-line option when running LAMMPS to see the offending line.

E: Incorrect args for pair coefficients

Self-explanatory.  Check the input script or data file.

E: Cannot open LS potential file %s

The specified LS potential file cannot be opened.  Check that the
path and name are correct.

E: Invalid LS potential file

UNDOCUMENTED

*/
