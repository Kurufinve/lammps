/* ----------------------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   http://lammps.sandia.gov, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
   Contributing authors: Alexey Lipnitskiy (BSU), Daniil Poletaev (BSU) 
------------------------------------------------------------------------- */

#include "pair_ls.h"

#include <cmath>

#include <cstring>
#include <iostream>
#include <fstream>
#include <stdio.h>
#include "atom.h"
#include "force.h"
#include "comm.h"
#include "neighbor.h"
#include "neigh_list.h"
#include "neigh_request.h"
#include "domain.h"
#include "memory.h"
#include "error.h"
#include "update.h"

#include "tokenizer.h"
#include "potential_file_reader.h"

// #include <Accelerate/Accelerate.h>
// #include <lapacke.h>

using namespace LAMMPS_NS;

#define MAXLINE 1024

/* ---------------------------------------------------------------------- */

PairLS::PairLS(LAMMPS *lmp) : Pair(lmp)
{
  // !std::cout << "!!!!! PairLS debug mode !!!!! " << "PairLS constructor started working"  << std::endl;

  restartinfo = 0;
  single_enable = 0;
  one_coeff = 1;
  manybody_flag = 1;
  
  allocated = 0;

  rosum = nullptr;

  shag_sp_fi  = nullptr;
  shag_sp_ro  = nullptr;
  shag_sp_emb = nullptr;
  shag_sp_f   = nullptr;

  R_sp_fi  = nullptr; 
  R_sp_ro  = nullptr;
  R_sp_emb = nullptr;
  R_sp_f   = nullptr;
  R_sp_g   = nullptr;
  
  a_sp_fi = nullptr;
  b_sp_fi = nullptr;
  c_sp_fi = nullptr;
  d_sp_fi = nullptr;

  a_sp_ro = nullptr;
  b_sp_ro = nullptr;
  c_sp_ro = nullptr;
  d_sp_ro = nullptr;

  a_sp_emb = nullptr;
  b_sp_emb = nullptr;
  c_sp_emb = nullptr;
  d_sp_emb = nullptr;

  a_sp_f3 = nullptr;
  b_sp_f3 = nullptr;
  c_sp_f3 = nullptr;
  d_sp_f3 = nullptr;

  // a_sp_g3 = nullptr;
  // b_sp_g3 = nullptr;
  // c_sp_g3 = nullptr;
  // d_sp_g3 = nullptr; 

  a_sp_f4 = nullptr;
  b_sp_f4 = nullptr;
  c_sp_f4 = nullptr;
  d_sp_f4 = nullptr;

  a_sp_g4 = nullptr;
  b_sp_g4 = nullptr;
  c_sp_g4 = nullptr;
  d_sp_g4 = nullptr;

  fip_rmin = nullptr;

  z_ion  = nullptr;
  c_ZBL  = nullptr;
  d_ZBL  = nullptr;
  zz_ZBL = nullptr;
  a_ZBL  = nullptr;
  e0_ZBL = nullptr;

  n_sp_fi  = nullptr;
  n_sp_ro  = nullptr;
  n_sp_emb = nullptr;
  n_sp_f   = nullptr;
  n_sp_g   = nullptr;

  cutsq = nullptr;

  n_sort = atom->ntypes;

  // comm_forward = 1;
  // comm_reverse = 1;

  periodic[0] = domain->xperiodic;
  periodic[1] = domain->yperiodic;
  periodic[2] = domain->zperiodic;
  
  // !std::cout << "!!!!! PairLS debug mode !!!!! " << "PairLS constructor end working"  << std::endl;


}

/* ----------------------------------------------------------------------
   check if allocated, since class can be destructed when incomplete
------------------------------------------------------------------------- */

PairLS::~PairLS()
{
  if (allocated) {
    memory->destroy(setflag);
    memory->destroy(cutsq);

    memory->destroy(shag_sp_fi);
    memory->destroy(shag_sp_ro);
    memory->destroy(shag_sp_emb);
    memory->destroy(shag_sp_f);

    memory->destroy(R_sp_fi);
    memory->destroy(R_sp_ro);
    memory->destroy(R_sp_emb);
    memory->destroy(R_sp_f);
    memory->destroy(R_sp_g);

    memory->destroy(a_sp_fi);
    memory->destroy(b_sp_fi);
    memory->destroy(c_sp_fi);
    memory->destroy(d_sp_fi);

    memory->destroy(a_sp_ro);
    memory->destroy(b_sp_ro);
    memory->destroy(c_sp_ro);
    memory->destroy(d_sp_ro);

    memory->destroy(a_sp_emb);
    memory->destroy(b_sp_emb);
    memory->destroy(c_sp_emb);
    memory->destroy(d_sp_emb);

    memory->destroy(a_sp_f3);
    memory->destroy(b_sp_f3);
    memory->destroy(c_sp_f3);
    memory->destroy(d_sp_f3);

    // memory->destroy(a_sp_g3);
    // memory->destroy(b_sp_g3);
    // memory->destroy(c_sp_g3);
    // memory->destroy(d_sp_g3);

    memory->destroy(a_sp_f4);
    memory->destroy(b_sp_f4);
    memory->destroy(c_sp_f4);
    memory->destroy(d_sp_f4);

    memory->destroy(a_sp_g4);
    memory->destroy(b_sp_g4);
    memory->destroy(c_sp_g4);
    memory->destroy(d_sp_g4);

    memory->destroy(fip_rmin);

    memory->destroy(z_ion);
    memory->destroy(c_ZBL);
    memory->destroy(d_ZBL);
    memory->destroy(zz_ZBL);
    memory->destroy(a_ZBL);
    memory->destroy(e0_ZBL);

    memory->destroy(Rmin_fi_ZBL);
    memory->destroy(c_fi_ZBL);

    memory->destroy(n_sp_fi);
    memory->destroy(n_sp_ro);
    memory->destroy(n_sp_emb);
    memory->destroy(n_sp_f);
    memory->destroy(n_sp_g);
    memory->destroy(n_f3);
  }


}

/* ---------------------------------------------------------------------- */

void PairLS::compute(int eflag, int vflag)
{
  // setting up eatom and vatom arrays with method of the parent Pair class
/* ----------------------------------------------------------------------
   eflag = 0 = no energy computation
   eflag = 1 = global energy only
   eflag = 2 = per-atom energy only
   eflag = 3 = both global and per-atom energy
   vflag = 0 = no virial computation (pressure)
   vflag = 1 = global virial with pair portion via sum of pairwise interactions
   vflag = 2 = global virial with pair portion via F dot r including ghosts
   vflag = 4 = per-atom virial only
   vflag = 5 or 6 = both global and per-atom virial
   vflag = 8 = per-atom centroid virial only
   vflag = 9 or 10 = both global and per-atom centroid virial
   vflag = 12 = both per-atom virial and per-atom centroid virial
   vflag = 13 or 15 = global, per-atom virial and per-atom centroid virial
------------------------------------------------------------------------- */  
  // eflag = 3; // = both global and per-atom energy

  // if (comm->me == 0) 
  // {
  //   std::cout << "timestep = " << update->ntimestep << std::endl;
  //   std::cout << "before ev_init(" << eflag <<","<<vflag<<")"<<std::endl;
  //   std::cout << "eng_vdwl = " << eng_vdwl << "  eng_coul = " << eng_coul << std::endl;
  //   for (int i = 0; i < 6; i++) std::cout << "virial["<<i<<"] = " << virial[i] << std::endl;
  // }

  ev_init(eflag,vflag);

  double **x = atom->x;
  double **f = atom->f;
  double **v = atom->v;
  double *e_at;
  int *type = atom->type;

  int nlocal = atom->nlocal;
  int nall = nlocal + atom->nghost;
  int newton_pair = force->newton_pair;

  atom->map_init(1);
  atom->map_set();

  memory->create(e_at,nlocal,"PairLS:e_at");

  // if (comm->me == 0) 
  // {
  //   // std::cout << "timestep = " << update->ntimestep << std::endl;
  //   std::cout << "after ev_init(" << eflag <<","<<vflag<<")"<<std::endl;
  //   std::cout << "eng_vdwl = " << eng_vdwl << "  eng_coul = " << eng_coul << std::endl;
  //   for (int i = 0; i < 6; i++) std::cout << "virial["<<i<<"] = " << virial[i] << std::endl;
  // }

  eng_vdwl = eng_coul = 0.0;
  for (int i = 0; i < 6; i++) virial[i] = 0.0;

  // if (comm->me == 0) 
  // {
  //   // std::cout << "timestep = " << update->ntimestep << std::endl;
  //   std::cout << "after zeroing eng_vdwl"<<std::endl;
  //   std::cout << "eng_vdwl = " << eng_vdwl << "  eng_coul = " << eng_coul << std::endl;
  //   for (int i = 0; i < 6; i++) std::cout << "virial["<<i<<"] = " << virial[i] << std::endl;
  // }


  e_force_fi_emb(eflag, vflag, e_at, f, x);

  if (if_g3_pot) e_force_g3(eflag, vflag, e_at, f, x);

  if (vflag_fdotr) virial_fdotr_compute();

  memory->destroy(e_at);
}

/* ----------------------------------------------------------------------
   allocate all arrays
------------------------------------------------------------------------- */

void PairLS::allocate()
{
  int n = atom->ntypes;

  memory->create(setflag,n+1,n+1,"pair:setflag");
  for (int i = 1; i <= n; i++)
    for (int j = i; j <= n; j++)
      setflag[i][j] = 0;

  memory->create(cutsq,n+1,n+1,"pair:cutsq");

  map = new int[n+1];
  for (int i = 1; i <= n; i++) map[i] = -1;

  memory->create(shag_sp_fi,mi,mi,"PairLS:shag_sp_fi");
  memory->create(shag_sp_ro,mi,mi,"PairLS:shag_sp_ro");
  memory->create(shag_sp_emb,mi,"PairLS:shag_sp_emb");
  memory->create(shag_sp_f,mi,mi,"PairLS:shag_sp_f");

  memory->create(R_sp_fi,mi,mi,mfi,"PairLS:R_sp_fi");
  memory->create(R_sp_ro,mi,mi,mfi,"PairLS:R_sp_ro");
  memory->create(R_sp_emb,mi,memb,"PairLS:R_sp_emb");
  memory->create(R_sp_f,mi,mi,mf,"PairLS:R_sp_f");
  memory->create(R_sp_g,mg,"PairLS:R_sp_g");

  memory->create(a_sp_fi,mi,mi,mfi,"PairLS:a_sp_fi");
  memory->create(b_sp_fi,mi,mi,mfi,"PairLS:b_sp_fi");
  memory->create(c_sp_fi,mi,mi,mfi,"PairLS:c_sp_fi");
  memory->create(d_sp_fi,mi,mi,mfi,"PairLS:d_sp_fi");

  memory->create(a_sp_ro,mi,mi,mro,"PairLS:a_sp_ro");
  memory->create(b_sp_ro,mi,mi,mro,"PairLS:b_sp_ro");
  memory->create(c_sp_ro,mi,mi,mro,"PairLS:c_sp_ro");
  memory->create(d_sp_ro,mi,mi,mro,"PairLS:d_sp_ro");

  memory->create(a_sp_emb,mi,memb,"PairLS:a_sp_emb");
  memory->create(b_sp_emb,mi,memb,"PairLS:b_sp_emb");
  memory->create(c_sp_emb,mi,memb,"PairLS:c_sp_emb");
  memory->create(d_sp_emb,mi,memb,"PairLS:d_sp_emb");

  memory->create(a_sp_f3,mi,mi,mf3,mf,"PairLS:a_sp_f3");
  memory->create(b_sp_f3,mi,mi,mf3,mf,"PairLS:b_sp_f3");
  memory->create(c_sp_f3,mi,mi,mf3,mf,"PairLS:c_sp_f3");
  memory->create(d_sp_f3,mi,mi,mf3,mf,"PairLS:d_sp_f3");

  // memory->create(a_sp_g3,mi,mf3,mf3,mg,"PairLS:a_sp_g3");
  // memory->create(b_sp_g3,mi,mf3,mf3,mg,"PairLS:b_sp_g3");
  // memory->create(c_sp_g3,mi,mf3,mf3,mg,"PairLS:c_sp_g3");
  // memory->create(d_sp_g3,mi,mf3,mf3,mg,"PairLS:d_sp_g3");

  memory->create(a_sp_f4,mi,mi,mf,"PairLS:a_sp_f4");
  memory->create(b_sp_f4,mi,mi,mf,"PairLS:b_sp_f4");
  memory->create(c_sp_f4,mi,mi,mf,"PairLS:c_sp_f4");
  memory->create(d_sp_f4,mi,mi,mf,"PairLS:d_sp_f4");

  memory->create(a_sp_g4,mi,mi,"PairLS:a_sp_g4");
  memory->create(b_sp_g4,mi,mi,"PairLS:b_sp_g4");
  memory->create(c_sp_g4,mi,mi,"PairLS:c_sp_g4");
  memory->create(d_sp_g4,mi,mi,"PairLS:d_sp_g4");

  memory->create(fip_rmin,mi,mi,"PairLS:fip_rmin");

  memory->create(z_ion,mi,"PairLS:z_ion");
  memory->create(c_ZBL,4,"PairLS:c_ZBL");
  memory->create(d_ZBL,4,"PairLS:d_ZBL");
  memory->create(zz_ZBL,mi,mi,"PairLS:zz_ZBL");
  memory->create(a_ZBL,mi,mi,"PairLS:a_ZBL");
  memory->create(e0_ZBL,mi,mi,"PairLS:e0_ZBL");

  memory->create(Rmin_fi_ZBL,mi,mi,"PairLS:Rmin_fi_ZBL");
  memory->create(c_fi_ZBL,6,mi,mi,"PairLS:c_fi_ZBL");

  memory->create(n_sp_fi,mi,mi,"PairLS:n_sp_fi");
  memory->create(n_sp_ro,mi,mi,"PairLS:n_sp_ro");
  memory->create(n_sp_emb,mi,mi,"PairLS:n_sp_emb");
  memory->create(n_sp_f,mi,mi,"PairLS:n_sp_f");
  memory->create(n_sp_g,mi,mi,"PairLS:n_sp_g");
  memory->create(n_f3,mi,"PairLS:n_f3");

  allocated = 1;
  // !std::cout << "!!!!! PairLS debug mode !!!!! " << " End allocating memory"  << std::endl;

}

/* ----------------------------------------------------------------------
   global settings
------------------------------------------------------------------------- */

void PairLS::settings(int narg, char **/*arg*/)
{
  if (narg > 0) error->all(FLERR,"Illegal pair_style command");
}

/* ----------------------------------------------------------------------
  Pair coeffs for each pair of atoms are the names of files with potential functions.
  They should be written in a row in the following order (example for 3-component system):
  pair_coeff pot_1 pot_2 pot_3 pot_1_2 pot_1_3 pot_2_3

  Here pot_i are the names of files containing potential functions for one sort of atom i,
  pot_i_j are the names of files containing cross potential functions for pair of atoms i and j.
   
  The total number of potential files should be equal to N(N+1)/2 where N is a number of the atom types in the system
------------------------------------------------------------------------- */

void PairLS::coeff(int narg, char **arg)
{
  if (!allocated) allocate();

  // int n_sort = atom->ntypes;
  int n_pot = n_sort*(n_sort+1)/2;
  double r_pot, w;
  int i, j;
  char name_32;

  // !std::cout << "!!!!! PairLS debug mode !!!!! " << " Number of atomtypes is " << n_sort << std::endl;

  // spelling that the number of arguments (files with potentials) is correspond to the number of atom types in the system
  if (narg != 2 + n_pot) error->all(FLERR,"Incorrect number of args for pair coefficients");

  // insure I,J args are * *

  if (strcmp(arg[0],"*") != 0 || strcmp(arg[1],"*") != 0)
    error->all(FLERR,"Incorrect args for pair coefficients");

  // Start reading monoatomic potentials

  // read_pot_ls.f (14-18):
  // do i=1,n_sort
  //   name_32=name_pot_is(i)
  //   call wr_pot_ls_is(-1,name_32,i,info_pot)
  //   call par2pot_is(i)
  // enddo
  // !std::cout << "!!!!! PairLS debug mode !!!!! " << " Number of potentials is " << n_pot << std::endl;

  // setting length and energy conversion factors
  double lcf = 1.0; // default length conversion factor
  double ecf = 1.0; // default energy conversion factor
  if (lmp->update->unit_style == (char *)"metal")
  {
    // !std::cout << "Unit style is " << lmp->update->unit_style << std::endl;
    lcf = 1.0;
    ecf = 1.0;
  }
  else if (lmp->update->unit_style == (char *)"si")
  {
    // !std::cout << "Unit style is " << lmp->update->unit_style << std::endl;
    lcf = 10000000000.0;
    ecf = 96000.0;
  }
  else if (lmp->update->unit_style == (char *)"cgs")
  {
    // !std::cout << "Unit style is " << lmp->update->unit_style << std::endl;
    lcf = 100000000.0;
    ecf = 0.0000000000016;
  }
  else if (lmp->update->unit_style == (char *)"electron")
  {
    // !std::cout << "Unit style is " << lmp->update->unit_style << std::endl;
    lcf = 1.88973;
    ecf = 0.0367493;
  }    
  // else if (lmp->update->unit_style == "micro")
  // {
  //   // !std::cout << "Unit style is micro" << std::endl;
  //   lcf = 0.0001;
  //   ecf = 0.0367493;
  // }    
  // else if (lmp->update->unit_style == "nano")
  // {
  //   // !std::cout << "Unit style is nano" << std::endl;
  //   lcf = 0.1;
  //   ecf = 0.0367493;
  // }     
  else
  {
    // lmp->error->one(FLERR, fmt::format("{} unit style is not supported by this type of potential ", lmp->update->unit_style));
    // error->all(FLERR, fmt::format("{} unit style is not supported by this type of potential ", lmp->update->unit_style));
    // !std::cout << "Unit style is " << lmp->update->unit_style << " but no conversion was done!"<< std::endl;
  }


  for (i = 1; i <= (n_sort); i++)
    {
      r_pot_ls_is(arg[i+1], i, lcf, ecf);
      par2pot_is(i);
      setflag[i][i] = 1;
    }

  // read_pot_ls.f (20-25):
  // w=R_sp_fi(n_sp_fi,1,1)
  // do i=1,n_sort
  //  if(R_sp_fi(n_sp_fi,i,i) > w) w=R_sp_fi(n_sp_fi,i,i)
  // enddo
  // Rc_fi=w
  // r_pot=Rc_fi

  w = R_sp_fi[1][1][n_sp_fi[1][1]-1];
  for (i = 1; i <= n_sort; i++)
    {
      if (R_sp_fi[i][i][n_sp_fi[i][i]-1] > w) w = R_sp_fi[i][i][n_sp_f[i][i]-1];
    }
  Rc_fi = w;
  r_pot = Rc_fi;

  // read_pot_ls.f (27-32):
  // w=R_sp_f(n_sp_f,1,1)
  // do i=1,n_sort
  //  if(R_sp_f(n_sp_f,i,i) > w) w=R_sp_f(n_sp_f,i,i)
  // enddo
  // Rc_f=w

  w = R_sp_f[1][1][n_sp_f[1][1]-1];
  for (i = 1; i <= n_sort; i++)
    {
      if (R_sp_f[i][i][n_sp_f[i][i]-1] > w) w = R_sp_f[i][i][n_sp_f[i][i]-1];
    }
  Rc_f = w;

  // End reading monoatomic potentials

  // Start reading cross-potentials

  // read_pot_ls.f (35-44):
  // if(n_sort>1) then
  //  do i=1,n_sort-1
  //    do j=i+1,n_sort
  //      name_32=name_pot_is1_is2(i,j)
  //      call wr_pot_ls_is1_is2(-1,name_32,i,j,info_pot)
  //      call par2pot_is1_is2(i,j)
  //      call par2pot_is1_is2(j,i)
  //    enddo
  //  enddo
  // endif


  if (n_sort > 1)
    {
      int ij = n_sort+1;
      for (i = 1; i <= n_sort-1; i++)
      {
        for (j = i + 1; j <= n_sort; j++)
        {
          r_pot_ls_is1_is2(arg[ij+1], i, j, lcf, ecf);
          par2pot_is1_is2(i,j);
          par2pot_is1_is2(j,i);          
          setflag[i][j] = 1;
          ij++;
        }
      }
    }


}

 

/* ----------------------------------------------------------------------
   init specific to this pair style
------------------------------------------------------------------------- */

void PairLS::init_style()
{
  // need full neighbor list
  // force->newton_pair = 0;

  int irequest_full = neighbor->request(this,instance_me);
  neighbor->requests[irequest_full]->id = 1;
  neighbor->requests[irequest_full]->half = 0;
  neighbor->requests[irequest_full]->full = 1;
  // int irequest_half = neighbor->request(this,instance_me);
  // neighbor->requests[irequest_half]->id = 2;

}

/* ----------------------------------------------------------------------
   neighbor callback to inform pair style of neighbor list to use
   half or full
------------------------------------------------------------------------- */

void PairLS::init_list(int id, NeighList *ptr)
{
  if (id == 1) listfull = ptr;
  else if (id == 2) listhalf = ptr;
}


/* ----------------------------------------------------------------------
   init for one type pair i,j and corresponding j,i
------------------------------------------------------------------------- */

double PairLS::init_one(int i, int j)
{
  // single global cutoff = max of cut from all files read in
    cutsq[i][j] = Rc_fi*Rc_fi;
    cutsq[j][i] = cutsq[i][j];
    return Rc_fi;
}

/* ---------------------------------------------------------------------- */

int PairLS::pack_reverse_comm(int n, int first, double *buf)
{
  int i,m,last;

  m = 0;
  last = first + n;
  for (i = first; i < last; i++) buf[m++] = rosum[i];
  return m;
}

/* ---------------------------------------------------------------------- */

void PairLS::unpack_reverse_comm(int n, int *list, double *buf)
{
  int i,j,m;

  m = 0;
  for (i = 0; i < n; i++) {
    j = list[i];
    rosum[j] += buf[m++];
  }
}


// Specific functions for this pair style

void PairLS::r_pot_ls_is(char *name_32, int is, double lcf, double ecf)
{


  if(comm->me == 0) 
  {
    PotentialFileReader reader(lmp, name_32, "ls");

    try {

      // wr_pot_ls_is.f (75):
      // read(23,'(A128)') info_pot

      reader.skip_line();  // the unnessessary information in the 1st line

      // wr_pot_ls_is.f (76-78):
      // read(23,*) if_g3_pot
      // read(23,*) if_g4_pot
      // read(23,*) if_gp0_pot(is)


      if_g3_pot = strcmp(reader.next_string().c_str(),".true.") == 0;
      if_g4_pot = strcmp(reader.next_string().c_str(),".true.") == 0;
      reader.skip_line(); // the unnessessary information in the 4th line

      // wr_pot_ls_is.f (80-83):
      // read(23,*) n_sp_fi
      // do i=1,n_sp_fi
      // read(23,*) R_sp_fi(i,is,is),a_sp_fi(i,is,is)
      // enddo

      n_sp_fi[is][is] = reader.next_int();
      for (int n = 0; n < n_sp_fi[is][is]; n++)
      {
        ValueTokenizer sp_fi_values = reader.next_values(2);
        R_sp_fi[is][is][n] = lcf*sp_fi_values.next_double();
        a_sp_fi[is][is][n] = ecf*sp_fi_values.next_double();
      }

      // wr_pot_ls_is.f (84-86):
      // read(23,*) fip_Rmin(is,is)
      // read(23,*) Rmin_fi_ZBL(is,is)
      // read(23,*) e0_ZBL(is,is)

      fip_rmin[is][is] = reader.next_double();
      Rmin_fi_ZBL[is][is] = lcf*reader.next_double();
      e0_ZBL[is][is] = ecf*reader.next_double();


      // wr_pot_ls_is.f (88-91):
      // read(23,*) n_sp_ro
      // do i=1,n_sp_ro
      // read(23,*) R_sp_ro(i,is,is),a_sp_ro(i,is,is)
      // enddo

      n_sp_ro[is][is] = reader.next_int();
      for (int n = 0; n < n_sp_ro[is][is]; n++)
      {
        ValueTokenizer sp_ro_values = reader.next_values(2);
        R_sp_ro[is][is][n] = lcf*sp_ro_values.next_double();
        a_sp_ro[is][is][n] = ecf*sp_ro_values.next_double();
      }    

      // wr_pot_ls_is.f (92-95):
      // read(23,*) n_sp_emb
      // do i=1,n_sp_emb
      // read(23,*) R_sp_emb(i,is),a_sp_emb(i,is)
      // enddo

      n_sp_emb[is][is] = reader.next_int();
      for (int n = 0; n < n_sp_emb[is][is]; n++)
      {
        ValueTokenizer sp_emb_values = reader.next_values(2);
        R_sp_emb[is][n] = lcf*sp_emb_values.next_double();
        a_sp_emb[is][n] = ecf*sp_emb_values.next_double();
      }         

      // wr_pot_ls_is.f (97-101):
      // read(23,*) n_sp_f,n_f3(is)
      // do i=1,n_sp_f
      // read(23,*) R_sp_f(i,is,is),(a_sp_f3(i,i1,is,is),i1=1,n_f3(is))
      // enddo
      if (if_g3_pot) 
      {
        ValueTokenizer values = reader.next_values(2);
        n_sp_f[is][is] = values.next_int();
        n_f3[is] = values.next_int();
        for (int n = 0; n < n_sp_f[is][is]; n++)
        {
          ValueTokenizer sp_f_values = reader.next_values(n_f3[is]+1);
          R_sp_f[is][is][n] = lcf*sp_f_values.next_double();
          for (int n1 = 0; n1 < n_f3[is]; n1++)
          {
            a_sp_f3[is][is][n1][n] = ecf*sp_f_values.next_double();
          }

        }

        // wr_pot_ls_is.f (102-108):
        // read(23,*) n_sp_g
        // read(23,*) (R_sp_g(i),i=1,n_sp_g)
        // do i1=1,n_f3(is)
        // do i2=1,i1
        // read(23,*) (a_sp_g3(i,i1,i2,is),i=1,n_sp_g)
        // enddo
        // enddo

        n_sp_g[is][is] = reader.next_int();
        values = reader.next_values(n_sp_g[is][is]);
        for (int n = 0; n < n_sp_g[is][is]; n++)
        {
          R_sp_g[n] = lcf*values.next_double();  // R_sp_g actually are the values of cos(ijk) from -1 (180 grad) to 1 (0 grad)
        }
        
        for (int n = 0; n < n_f3[is]; n++)
        {
          for (int n1 = 0; n1 <= n; n1++)
          {
            ValueTokenizer sp_g_values = reader.next_values(n_sp_g[is][is]);
            for (int n2 = 0; n2 < n_sp_g[is][is]; n2++)
            {
              a_sp_g3[is][n2][n][n1] = ecf*sp_g_values.next_double();
            }
          }
        }
      }
      // wr_pot_ls_is.f (120-126):
      // read(23,*) z_ion(is)
      // do i=1,4
      // read(23,*) c_ZBL(i)
      // enddo
      // do i=1,4
      // read(23,*) d_ZBL(i)
      // enddo

      z_ion[is] = reader.next_double();
      for (int n = 0; n < 4; n++)
      {
        c_ZBL[n] = ecf*reader.next_double();
      }

      for (int n = 0; n < 4; n++)
      {
        d_ZBL[n] = ecf*reader.next_double();
      }
    }
    catch (TokenizerException &e) {
      error->one(FLERR, e.what());
    }

  }
 
 
  // Start broadcasting unary potential information to other procs using cycles
  MPI_Bcast(&if_g3_pot, 1, MPI_INT, 0, world);
  MPI_Bcast(&n_sp_fi[is][is], 1, MPI_INT, 0, world);
  for (int n = 0; n < n_sp_fi[is][is]; n++)
  {
    MPI_Bcast(&R_sp_fi[is][is][n], 1, MPI_DOUBLE, 0, world);
    MPI_Bcast(&a_sp_fi[is][is][n], 1, MPI_DOUBLE, 0, world);
  }

  MPI_Bcast(&fip_rmin[is][is], 1, MPI_DOUBLE, 0, world);
  MPI_Bcast(&Rmin_fi_ZBL[is][is], 1, MPI_DOUBLE, 0, world);
  MPI_Bcast(&e0_ZBL[is][is], 1, MPI_DOUBLE, 0, world);

  MPI_Bcast(&n_sp_ro[is][is], 1, MPI_INT, 0, world);
  for (int n = 0; n < n_sp_ro[is][is]; n++)
  {
    MPI_Bcast(&R_sp_ro[is][is][n], 1, MPI_DOUBLE, 0, world);
    MPI_Bcast(&a_sp_ro[is][is][n], 1, MPI_DOUBLE, 0, world);
  }

  MPI_Bcast(&n_sp_emb[is][is], 1, MPI_INT, 0, world);
  for (int n = 0; n < n_sp_emb[is][is]; n++)
  {  
    MPI_Bcast(&R_sp_emb[is][n], 1, MPI_DOUBLE, 0, world);
    MPI_Bcast(&a_sp_emb[is][n], 1, MPI_DOUBLE, 0, world);  
  }

  MPI_Bcast(&n_sp_f[is][is], 1, MPI_INT, 0, world);
  MPI_Bcast(&n_f3[is], 1, MPI_INT, 0, world);
  for (int n = 0; n < n_sp_f[is][is]; n++)
  {
    MPI_Bcast(&R_sp_f[is][is][n], 1, MPI_DOUBLE, 0, world);
    for (int n1 = 0; n1 < n_f3[is]; n1++)
    {
      MPI_Bcast(&a_sp_f3[is][is][n1][n], 1, MPI_DOUBLE, 0, world);
    }    
  }

  MPI_Bcast(&n_sp_g[is][is], 1, MPI_INT, 0, world);
  for (int n = 0; n < n_sp_g[is][is]; n++)
  {
    MPI_Bcast(&R_sp_g[n], 1, MPI_DOUBLE, 0, world);
  }

  for (int n = 0; n < n_f3[is]; n++)
  {
    for (int n1 = 0; n1 <= n; n1++)
    {
      for (int n2 = 0; n2 < n_sp_g[is][is]; n2++)
      {
        MPI_Bcast(&a_sp_g3[is][n2][n][n1], 1, MPI_DOUBLE, 0, world);
      }
    }
  }  
  // End broadcasting unary potential information to other procs using cycles

}



void PairLS::r_pot_ls_is1_is2(char *name_32, int is1, int is2, double lcf, double ecf)
{

  if(comm->me == 0) 
  {
    PotentialFileReader reader(lmp, name_32, "ls");

    try {

      // wr_pot_ls_is1_is2.f (54):
      // read(23,'(A128)') info_pot

      reader.skip_line();  // the unnessessary information in the 1st line

      // wr_pot_ls_is1_is2.f (57-60):
      // read(23,*) n_sp_fi
      // do i=1,n_sp_fi
      // read(23,*) R_sp_fi(i,is1,is2),a_sp_fi(i,is1,is2)
      // enddo

      n_sp_fi[is1][is2] = reader.next_int();
      for (int n = 0; n < n_sp_fi[is1][is2]; n++)
      {
        ValueTokenizer sp_fi_values = reader.next_values(2);
        R_sp_fi[is1][is2][n] = lcf*sp_fi_values.next_double();
        a_sp_fi[is1][is2][n] = ecf*sp_fi_values.next_double();
      }

      // wr_pot_ls_is1_is2.f (61-63):
      // read(23,*) fip_Rmin(is1,is2)
      // read(23,*) Rmin_fi_ZBL(is1,is2)
      // read(23,*) e0_ZBL(is1,is2)

      fip_rmin[is1][is2] = reader.next_double();
      Rmin_fi_ZBL[is1][is2] = lcf*reader.next_double();
      e0_ZBL[is1][is2] = ecf*reader.next_double();

      // wr_pot_ls_is1_is2.f (65-73):
      // do i=1,n_sp_fi
      // R_sp_fi(i,is2,is1)=R_sp_fi(i,is1,is2)
      // enddo
      // do i=1,n_sp_fi
      // a_sp_fi(i,is2,is1)=a_sp_fi(i,is1,is2)
      // enddo
      // fip_Rmin(is2,is1)=fip_Rmin(is1,is2)
      // Rmin_fi_ZBL(is2,is1)=Rmin_fi_ZBL(is1,is2)
      // e0_ZBL(is2,is1)=e0_ZBL(is1,is2)

      n_sp_fi[is2][is1] = n_sp_fi[is1][is2];
      for (int n = 0; n < n_sp_fi[is1][is2]; n++)
      {
        R_sp_fi[is2][is1][n] = lcf*R_sp_fi[is1][is2][n];
        a_sp_fi[is2][is1][n] = ecf*a_sp_fi[is1][is2][n];
      }

      fip_rmin[is2][is1] = fip_rmin[is1][is2];
      Rmin_fi_ZBL[is2][is1] = lcf*Rmin_fi_ZBL[is1][is2];
      e0_ZBL[is2][is1] = ecf*e0_ZBL[is1][is2];

      // wr_pot_ls_is1_is2.f (76-82):
      // read(23,*) n_sp_ro
      // do i=1,n_sp_ro
      // read(23,*) R_sp_ro(i,is1,is2),a_sp_ro(i,is1,is2)
      // enddo
      // do i=1,n_sp_ro
      // read(23,*) R_sp_ro(i,is2,is1),a_sp_ro(i,is2,is1)
      // enddo

      n_sp_ro[is1][is2] = reader.next_int();
      n_sp_ro[is2][is1] = n_sp_ro[is1][is2];
      for (int n = 0; n < n_sp_ro[is1][is2]; n++)
      {
        ValueTokenizer sp_ro_is1_values = reader.next_values(2);
        R_sp_ro[is1][is2][n] = lcf*sp_ro_is1_values.next_double();
        a_sp_ro[is1][is2][n] = ecf*sp_ro_is1_values.next_double();
      }    

      for (int n = 0; n < n_sp_ro[is2][is1]; n++)
      {
        ValueTokenizer sp_ro_is2_values = reader.next_values(2);
        R_sp_ro[is2][is1][n] = lcf*sp_ro_is2_values.next_double();
        a_sp_ro[is2][is1][n] = ecf*sp_ro_is2_values.next_double();
      }   

      // wr_pot_ls_is1_is2.f (85-88):
      // read(23,*) n_sp_f,n_f3(is1)
      // do i=1,n_sp_f
      // read(23,*) R_sp_f(i,is2,is1),(a_sp_f3(i,i1,is2,is1),i1=1,n_f3(is1))
      // enddo

      ValueTokenizer n_sp_f_is1_values = reader.next_values(2);
      n_sp_f[is2][is1] = n_sp_f_is1_values.next_int();
      n_f3[is1] = n_sp_f_is1_values.next_int();
      for (int n = 0; n < n_sp_f[is2][is1]; n++)
      {
        ValueTokenizer sp_f_is1_values = reader.next_values(n_f3[is1]+1);
        R_sp_f[is2][is1][n] = lcf*sp_f_is1_values.next_double();
        for (int n1 = 0; n1 < n_f3[is1]; n1++)
        {
          a_sp_f3[is2][is1][n1][n] = ecf*sp_f_is1_values.next_double();
        }

      }

      // wr_pot_ls_is1_is2.f (89-92):
      // read(23,*) n_sp_f,n_f3(is2)
      // do i=1,n_sp_f
      // read(23,*) R_sp_f(i,is1,is2),(a_sp_f3(i,i1,is1,is2),i1=1,n_f3(is2))
      // enddo

      ValueTokenizer n_sp_f_is2_values = reader.next_values(2);
      n_sp_f[is1][is2] = n_sp_f_is2_values.next_int();
      n_f3[is2] = n_sp_f_is2_values.next_int();
      for (int n = 0; n < n_sp_f[is1][is2]; n++)
      {
        ValueTokenizer sp_f_is2_values = reader.next_values(n_f3[is2]+1);
        R_sp_f[is1][is2][n] = lcf*sp_f_is2_values.next_double();
        for (int n1 = 0; n1 < n_f3[is2]; n1++)
        {
          a_sp_f3[is1][is2][n1][n] = ecf*sp_f_is2_values.next_double();
        }

      }

    }
    catch (TokenizerException &e) {
      error->one(FLERR, e.what());
    }
  }

  // Start broadcasting binary potential information to other procs using cycles
  MPI_Bcast(&if_g3_pot, 1, MPI_INT, 0, world);
  MPI_Bcast(&n_sp_fi[is1][is2], 1, MPI_INT, 0, world);
  for (int n = 0; n < n_sp_fi[is1][is2]; n++)
  {
    MPI_Bcast(&R_sp_fi[is1][is2][n], 1, MPI_DOUBLE, 0, world);
    MPI_Bcast(&a_sp_fi[is1][is2][n], 1, MPI_DOUBLE, 0, world);
  }

  MPI_Bcast(&fip_rmin[is1][is2], 1, MPI_DOUBLE, 0, world);
  MPI_Bcast(&Rmin_fi_ZBL[is1][is2], 1, MPI_DOUBLE, 0, world);
  MPI_Bcast(&e0_ZBL[is1][is2], 1, MPI_DOUBLE, 0, world);

  MPI_Bcast(&n_sp_fi[is2][is1], 1, MPI_INT, 0, world);
  for (int n = 0; n < n_sp_fi[is2][is1]; n++)
  {
    MPI_Bcast(&R_sp_fi[is2][is1][n], 1, MPI_DOUBLE, 0, world);
    MPI_Bcast(&a_sp_fi[is2][is1][n], 1, MPI_DOUBLE, 0, world);
  }

  MPI_Bcast(&fip_rmin[is2][is1], 1, MPI_DOUBLE, 0, world);
  MPI_Bcast(&Rmin_fi_ZBL[is2][is1], 1, MPI_DOUBLE, 0, world);
  MPI_Bcast(&e0_ZBL[is2][is1], 1, MPI_DOUBLE, 0, world);

  MPI_Bcast(&n_sp_ro[is1][is2], 1, MPI_INT, 0, world);
  for (int n = 0; n < n_sp_ro[is1][is2]; n++)
  {
    MPI_Bcast(&R_sp_ro[is1][is2][n], 1, MPI_DOUBLE, 0, world);
    MPI_Bcast(&a_sp_ro[is1][is2][n], 1, MPI_DOUBLE, 0, world);
  }

  MPI_Bcast(&n_sp_ro[is2][is1], 1, MPI_INT, 0, world);
  for (int n = 0; n < n_sp_ro[is2][is1]; n++)
  {
    MPI_Bcast(&R_sp_ro[is2][is1][n], 1, MPI_DOUBLE, 0, world);
    MPI_Bcast(&a_sp_ro[is2][is1][n], 1, MPI_DOUBLE, 0, world);
  }

  MPI_Bcast(&n_sp_f[is2][is1], 1, MPI_INT, 0, world);
  MPI_Bcast(&n_f3[is1], 1, MPI_INT, 0, world);
  for (int n = 0; n < n_sp_f[is2][is1]; n++)
  {
    MPI_Bcast(&R_sp_f[is2][is1][n], 1, MPI_DOUBLE, 0, world);
    for (int n1 = 0; n1 < n_f3[is1]; n1++)
    {
      MPI_Bcast(&a_sp_f3[is2][is1][n1][n], 1, MPI_DOUBLE, 0, world);
    }    
  }

  MPI_Bcast(&n_sp_f[is1][is2], 1, MPI_INT, 0, world);
  MPI_Bcast(&n_f3[is2], 1, MPI_INT, 0, world);
  for (int n = 0; n < n_sp_f[is1][is2]; n++)
  {
    MPI_Bcast(&R_sp_f[is1][is2][n], 1, MPI_DOUBLE, 0, world);
    for (int n1 = 0; n1 < n_f3[is2]; n1++)
    {
      MPI_Bcast(&a_sp_f3[is1][is2][n1][n], 1, MPI_DOUBLE, 0, world);
    }    
  }

  // End broadcasting potential information to other procs using cycles  
}



void PairLS::par2pot_is(int is)
{
  // Start pot_ls_a_sp.h
  int n_sp;
  // double R_sp[mfi];
  // double a_sp[mfi], b_sp[mfi], c_sp[mfi], d_sp[mfi], e_sp[mfi];
  double *R_sp;
  double *a_sp, *b_sp, *c_sp, *d_sp, *e_sp; 
  // End pot_ls_a_sp.h

  int i, j, i1, i2, n;
  double p1, p2, pn;
  // double B6[6][1];
  double *B6;
  double r1, r2, f1, fp1, fpp1, f2, fp2, fpp2;
  // !std::cout << "!!!!! PairLS debug mode !!!!! " << " entering par2pot_is" << std::endl;
  
  memory->destroy(R_sp);
  memory->destroy(a_sp);
  memory->destroy(b_sp);
  memory->destroy(c_sp);
  memory->destroy(d_sp);
  memory->destroy(e_sp);
  memory->destroy(B6);
  // // !std::cout << "!!!!! PairLS debug mode !!!!! " << " destroyed memory for R_sp, a_sp, etc" << std::endl;

  memory->create(R_sp, mfi, "PairLS:a_sp_w");
  memory->create(a_sp, mfi, "PairLS:a_sp_w");
  memory->create(b_sp, mfi, "PairLS:b_sp_w");
  memory->create(c_sp, mfi, "PairLS:c_sp_w");
  memory->create(d_sp, mfi, "PairLS:d_sp_w");
  memory->create(e_sp, mfi, "PairLS:e_sp_w");
  memory->create(B6, 6, "PairLS:B6_w");

  // par2pot_is.f(15-18):
  //   zz_ZBL(is,is)=z_ion(is)*z_ion(is)*(3.795D0**2)
  //   a_ZBL(is,is)=0.8853D0
  //  : *0.5291772083D0/(z_ion(is)**0.23D0 + z_ion(is)**0.23D0)
  // // !std::cout << "!!!!! PairLS debug mode !!!!! " << " created memory for R_sp, a_sp, etc" << std::endl;

  zz_ZBL[is][is] = z_ion[is]*z_ion[is]*pow(3.795,2);
  a_ZBL[is][is] = 0.8853*0.5291772083/(pow(z_ion[is],0.23) + pow(z_ion[is],0.23));

  // // !std::cout << "!!!!! PairLS debug mode !!!!! " << " zz_ZBL[is][is] and a_ZBL[is][is] assigned" << std::endl;

  // par2pot_is.f(21-28):
  // n_sp=n_sp_fi
  // do i=1,n_sp
  // R_sp(i)=R_sp_fi(i,is,is)
  // enddo
  // do i=1,n_sp-1
  // a_sp(i)=a_sp_fi(i,is,is)
  // enddo
  // a_sp(n_sp)=0.0D0

  n_sp = n_sp_fi[is][is];
  for (i = 0; i < n_sp; i++)
  {
    R_sp[i]=R_sp_fi[is][is][i];
  }
  for (i = 0; i < n_sp-1; i++)
  {
    a_sp[i]=a_sp_fi[is][is][i];
  }  
  a_sp[n_sp-1] = 0.0;

  // par2pot_is.f(29-36):
  // call SPL(n_sp, R_sp, a_sp, 1, fip_Rmin(is,is),0.0D0, b_sp,c_sp,d_sp)
    // do i=1,n_sp_fi
    // a_sp_fi(i,is,is)=a_sp(i)
    // b_sp_fi(i,is,is)=b_sp(i)
    // c_sp_fi(i,is,is)=c_sp(i)
    // d_sp_fi(i,is,is)=d_sp(i)
    // enddo
  // shag_sp_fi(is,is)=1.0D0/((R_sp_fi(n_sp,is,is)-R_sp_fi(1,is,is))/dfloat(n_sp-1))

  // SPL(n_sp, &R_sp, &a_sp, 1, fip_rmin[is][is], 0.0, &b_sp, &c_sp, &d_sp);
  // // !std::cout << "!!!!! PairLS debug mode !!!!! " << " now SPL function will be called" << std::endl;

  SPL(n_sp, R_sp, a_sp, 1, fip_rmin[is][is], 0.0, b_sp, c_sp, d_sp);

  // // !std::cout << "!!!!! PairLS debug mode !!!!! " << " SPL function worked well" << std::endl;

  // !std::cout << "!!!!!!!!!!!! Fi" << std::endl;
  for (i = 0; i < n_sp; i++)
  {
    a_sp_fi[is][is][i] = a_sp[i];
    b_sp_fi[is][is][i] = b_sp[i];
    c_sp_fi[is][is][i] = c_sp[i];
    d_sp_fi[is][is][i] = d_sp[i];
    // !std::cout << " R_sp_fi[" << i << "][" << is << "][" << is << "] = " << R_sp_fi[is][is][i] << "   ";
    // !std::cout << " a_sp_fi[" << i << "][" << is << "][" << is << "] = " << a_sp_fi[is][is][i] << "   ";    
    // !std::cout << " b_sp_fi[" << i << "][" << is << "][" << is << "] = " << b_sp_fi[is][is][i] << "   ";    
    // !std::cout << " c_sp_fi[" << i << "][" << is << "][" << is << "] = " << c_sp_fi[is][is][i] << "   ";    
    // !std::cout << " d_sp_fi[" << i << "][" << is << "][" << is << "] = " << d_sp_fi[is][is][i] << std::endl;    
  }
  
  shag_sp_fi[is][is] = 1.0/((R_sp_fi[is][is][n_sp-1]-R_sp_fi[is][is][0])/(n_sp-1));

  // par2pot_is.f(39-49):
  // c fi_ZBL
  // r1=Rmin_fi_ZBL(is,is)
  // f1=v_ZBL(r1,is,is)+e0_ZBL(is,is)
  // fp1=vp_ZBL(r1,is,is)
  // fpp1=vpp_ZBL(r1,is,is)
  //     r2=R_sp_fi(1,is,is)
  //     f2=a_sp_fi(1,is,is)
  //     fp2=b_sp_fi(1,is,is)
  //     fpp2=2.0D0*c_sp_fi(1,is,is)  
  // call smooth_zero_22 (B6,r1,r2,f1,fp1,fpp1,
  //    :                         f2,fp2,fpp2)
  // c_fi_ZBL(1:6,is,is)=B6(1:6)

  r1 = Rmin_fi_ZBL[is][is];
  f1 = v_ZBL(r1, is, is) + e0_ZBL[is][is];
  fp1 = vp_ZBL(r1, is, is);
  fpp1 = vpp_ZBL(r1, is, is);
  r2 = R_sp_fi[is][is][0];
  f2 = a_sp_fi[is][is][0];
  fp2 = b_sp_fi[is][is][0];
  fpp2 = 2.0*c_sp_fi[is][is][0];  
  
  // // !std::cout << "!!!!! PairLS debug mode !!!!! " << " now smooth_zero_22 function will be called" << std::endl;

  smooth_zero_22(B6, r1, r2, f1, fp1, fpp1, f2, fp2, fpp2);
  // smooth_zero_22(B6, &r1, &r2, &f1, &fp1, &fpp1, &f2, &fp2, &fpp2);
  // // !std::cout << "!!!!! PairLS debug mode !!!!! " << " smooth_zero_22 function worked well" << std::endl;

  // !std::cout << "!!!!!!!!!!!! c_fi_ZBL" << std::endl;
  // !std::cout << " r1 = " << r1 <<std::endl;
  // !std::cout << " r2 = " << r2 <<std::endl;
  // !std::cout << " f1 = " << f1 <<std::endl;
  // !std::cout << " fp1 = " << fp1 <<std::endl;
  // !std::cout << " fpp1 = " << fpp1 <<std::endl;
  // !std::cout << " f2 = " << f2 <<std::endl;
  // !std::cout << " fp2 = " << fp2 <<std::endl;
  // !std::cout << " fpp2 = " << fpp2 <<std::endl;  

  for (i = 0; i < 6; i++)
  {
    c_fi_ZBL[is][is][i] = B6[i];
    // !std::cout << c_fi_ZBL[is][is][i] << "  ";
  }
  // !std::cout << std::endl;
  // // !std::cout << "!!!!! PairLS debug mode !!!!! " << " for (i = 0; i < 6; i++) worked well" << std::endl;

  // par2pot_is.f(51-68):
  // ! ro
  // n_sp=n_sp_ro
  // do i=1,n_sp
  // R_sp(i)=R_sp_ro(i,is,is)
  // enddo
  // do i=1,n_sp
  // a_sp(i)=a_sp_ro(i,is,is)
  // enddo
  // p1=0.0D0
  // c  p1=(a_sp(2)-a_sp(1))/(R_sp(2)-R_sp(1))
  // call SPL(n_sp, R_sp, a_sp, 1, p1,0.0D0, b_sp,c_sp,d_sp)
  // do i=1,n_sp
  // a_sp_ro(i,is,is)=a_sp(i)
  // b_sp_ro(i,is,is)=b_sp(i)
  // c_sp_ro(i,is,is)=c_sp(i)
  // d_sp_ro(i,is,is)=d_sp(i)
  // enddo
  // shag_sp_ro(is,is)=1.0D0/((R_sp_ro(n_sp,is,is)-R_sp_ro(1,is,is))/dfloat(n_sp-1))

  n_sp = n_sp_ro[is][is];
  // !std::cout << " n_sp = " << n_sp_ro[is][is] << std::endl;
  for (i = 0; i < n_sp; i++)
  {
    // !std::cout << " i = " << i << " R_sp_ro[is][is][i] = " << R_sp_ro[is][is][i] << std::endl;
    R_sp[i] = R_sp_ro[is][is][i];
    a_sp[i] = a_sp_ro[is][is][i];
  }
  p1=0.0;

  // // !std::cout << "!!!!! PairLS debug mode !!!!! " << " for (i = 0; i < n_sp; i++) worked well" << std::endl;


  // SPL(n_sp, &R_sp, &a_sp, 1, p1, 0.0, &b_sp, &c_sp, &d_sp);
  SPL(n_sp, R_sp, a_sp, 1, p1, 0.0, b_sp, c_sp, d_sp);
  // // !std::cout << "!!!!! PairLS debug mode !!!!! " << " SPL(n_sp, R_sp, a_sp, 1, p1, 0.0, b_sp, c_sp, d_sp) worked well" << std::endl;

  // !std::cout << "!!!!!!!!!!!! Rho" << std::endl;
  for (i = 0; i < n_sp; i++)
  {  
    a_sp_ro[is][is][i] = a_sp[i];
    b_sp_ro[is][is][i] = b_sp[i];
    c_sp_ro[is][is][i] = c_sp[i];
    d_sp_ro[is][is][i] = d_sp[i];
    // !std::cout << " R_sp_ro[" << i << "][" << is << "][" << is << "] = " << R_sp_ro[is][is][i] << "   ";
    // !std::cout << " a_sp_ro[" << i << "][" << is << "][" << is << "] = " << a_sp_ro[is][is][i] << "   ";    
    // !std::cout << " b_sp_ro[" << i << "][" << is << "][" << is << "] = " << b_sp_ro[is][is][i] << "   ";    
    // !std::cout << " c_sp_ro[" << i << "][" << is << "][" << is << "] = " << c_sp_ro[is][is][i] << "   ";    
    // !std::cout << " d_sp_ro[" << i << "][" << is << "][" << is << "] = " << d_sp_ro[is][is][i] << std::endl;     
  }
  shag_sp_ro[is][is] = 1.0/((R_sp_ro[is][is][n_sp-1]-R_sp_ro[is][is][0])/(n_sp-1));

  // // !std::cout << "!!!!! PairLS debug mode !!!!! " << " for (i = 0; i < n_sp; i++) worked well" << std::endl;

  // par2pot_is.f(70-91):
  // ! emb
  //  n_sp=n_sp_emb
  //  n=n_sp_emb
  //  do i=1,n_sp
  //  R_sp(i)=R_sp_emb(i,is)
  //  enddo
  //  do i=1,n_sp
  //  a_sp(i)=a_sp_emb(i,is)
  //  enddo
  //  a_sp(1)=0.0D0
  //  p1=(a_sp(2)-a_sp(1))/(R_sp(2)-R_sp(1))
  // c  pn=(a_sp(n)-a_sp(n-1))/(R_sp(n)-R_sp(n-1))
  //  pn=0.0D0
  //  call SPL(n_sp, R_sp, a_sp, 1, p1,pn, b_sp,c_sp,d_sp)
  //    do i=1,n_sp
  //    a_sp_emb(i,is)=a_sp(i)
  //    b_sp_emb(i,is)=b_sp(i)
  //    c_sp_emb(i,is)=c_sp(i)
  //    d_sp_emb(i,is)=d_sp(i)
  //    enddo
  //  shag_sp_emb(is)=1.0D0/((R_sp_emb(n_sp,is)-R_sp_emb(1,is))/dfloat(n_sp-1))

  n_sp = n_sp_emb[is][is];
  n = n_sp_emb[is][is];
  for (i = 0; i < n_sp; i++)
  {
    R_sp[i] = R_sp_emb[is][i];
    a_sp[i] = a_sp_emb[is][i];
  }  
  a_sp[0] = 0.0;

  p1 = (a_sp[1]-a_sp[0])/(R_sp[1]-R_sp[0]);
  pn = 0.0;
  // SPL(n_sp, &R_sp, &a_sp, 1, p1, pn, &b_sp, &c_sp, &d_sp);
  SPL(n_sp, R_sp, a_sp, 1, p1, pn, b_sp, c_sp, d_sp);
  // // !std::cout << "!!!!! PairLS debug mode !!!!! " << " SPL(n_sp, R_sp, a_sp, 1, p1, pn, b_sp, c_sp, d_sp) worked well" << std::endl;
  // !std::cout << "!!!!!!!!!!!! Emb" << std::endl;
  for (i = 0; i < n_sp; i++)
  {  
    a_sp_emb[is][i] = a_sp[i];
    b_sp_emb[is][i] = b_sp[i];
    c_sp_emb[is][i] = c_sp[i];
    d_sp_emb[is][i] = d_sp[i];
    // !std::cout << " R_sp_emb[" << i << "][" << is << "] = " << R_sp_emb[i][is] << "   ";
    // !std::cout << " a_sp_emb[" << i << "][" << is << "] = " << a_sp_emb[i][is] << "   ";    
    // !std::cout << " b_sp_emb[" << i << "][" << is << "] = " << b_sp_emb[i][is] << "   ";    
    // !std::cout << " c_sp_emb[" << i << "][" << is << "] = " << c_sp_emb[i][is] << "   ";    
    // !std::cout << " d_sp_emb[" << i << "][" << is << "] = " << d_sp_emb[i][is] << std::endl;     
  }

  shag_sp_emb[is] = 1.0/((R_sp_emb[is][n_sp-1] - R_sp_emb[is][0])/(n_sp - 1)); 

  // par2pot_is.f(97-115):
  // ! f3
  //  n_sp=n_sp_f
  //  do i=1,n_sp
  //  R_sp(i)=R_sp_f(i,is,is)
  //  enddo
  //  do i1=1,n_f3(is)
  //      do i=1,n_sp
  //      a_sp(i)=a_sp_f3(i,i1,is,is)
  //      enddo
  //      p1=0.0D0
  // c      p1=(a_sp(2)-a_sp(1))/(R_sp(2)-R_sp(1))
  //  call SPL(n_sp, R_sp, a_sp, 1, p1,0.0D0, b_sp,c_sp,d_sp)
  //    do i=1,n_sp
  //    a_sp_f3(i,i1,is,is)=a_sp(i)
  //    b_sp_f3(i,i1,is,is)=b_sp(i)
  //    c_sp_f3(i,i1,is,is)=c_sp(i)
  //    d_sp_f3(i,i1,is,is)=d_sp(i)
  //    enddo
  //  enddo
  //  shag_sp_f(is,is)=1.0D0/((R_sp_f(n_sp,is,is)-R_sp_f(1,is,is))/dfloat(n_sp-1))

  if (if_g3_pot)
  {
    n_sp = n_sp_f[is][is];
    for (i = 0; i < n_sp; i++)
    {
      R_sp[i] = R_sp_f[is][is][i];
    }

    // !std::cout << "!!!!!!!!!!!! f3" << std::endl;
    for (i1 = 0; i1 < n_f3[is]; i1++)
    {
      for (i = 0; i < n_sp; i++)
      {
        a_sp[i] = a_sp_f3[is][is][i1][i];
      }
      p1 = 0.0;
      // SPL(n_sp, &R_sp, &a_sp, 1, p1, 0.0, &b_sp, &c_sp, &d_sp);
      SPL(n_sp, R_sp, a_sp, 1, p1, 0.0, b_sp, c_sp, d_sp);
      // // !std::cout << "!!!!! PairLS debug mode !!!!! " << " SPL(n_sp, R_sp, a_sp, 1, p1, 0.0, b_sp, c_sp, d_sp) worked well" << std::endl;
      for (i = 0; i < n_sp; i++)
      { 
        a_sp_f3[is][is][i1][i] = a_sp[i];
        b_sp_f3[is][is][i1][i] = b_sp[i];
        c_sp_f3[is][is][i1][i] = c_sp[i];
        d_sp_f3[is][is][i1][i] = d_sp[i];
        // !std::cout << " R_sp_f3[" << i << "][" << is << "][" << is << "] = " << R_sp_f[is][is][i] << "   ";
        // !std::cout << " a_sp_f3[" << i << "][" << i1 << "][" << is << "][" << is << "] = " << a_sp_f3[is][is][i1][i] << "   ";    
        // !std::cout << " b_sp_f3[" << i << "][" << i1 << "][" << is << "][" << is << "] = " << b_sp_f3[is][is][i1][i] << "   ";    
        // !std::cout << " c_sp_f3[" << i << "][" << i1 << "][" << is << "][" << is << "] = " << c_sp_f3[is][is][i1][i] << "   ";    
        // !std::cout << " d_sp_f3[" << i << "][" << i1 << "][" << is << "][" << is << "] = " << d_sp_f3[is][is][i1][i] << std::endl;      
      }
    }
    shag_sp_f[is][is] = 1.0/((R_sp_f[is][is][n_sp-1]-R_sp_f[is][is][0])/(n_sp-1));

    // par2pot_is.f(117-150):
    // ! g3
    //  n_sp=n_sp_g
    //  do i=1,n_sp
    //  R_sp(i)=R_sp_g(i)
    //  enddo
    // c                if_diag(is)=.false.
    //  do i1=1,n_f3(is)
    //  do i2=1,i1
    //      do i=1,n_sp
    //      a_sp(i)=a_sp_g3(i,i1,i2,is)
    //      enddo
    // c      if(i2.NE.i1) then
    // c      if(abs(a_sp(2))<0.00000001.AND.abs(a_sp(7))<0.00000001) if_diag(is)=.true.
    // c      endif
    //  p1=0.0D0
    //  p2=0.0D0
    //      if(.NOT.if_gp0_pot(is)) then
    //      p1=(a_sp(2)-a_sp(1))/(R_sp(2)-R_sp(1))
    //      p2=(a_sp(n_sp)-a_sp(n_sp-1))/(R_sp(n_sp)-R_sp(n_sp-1))
    //      endif
    //  call SPL(n_sp, R_sp, a_sp, 1, p1,p2, b_sp,c_sp,d_sp)
    //    do i=1,n_sp
    //    a_sp_g3(i,i1,i2,is)=a_sp(i)
    //    a_sp_g3(i,i2,i1,is)=a_sp(i)
    //    b_sp_g3(i,i1,i2,is)=b_sp(i)
    //    b_sp_g3(i,i2,i1,is)=b_sp(i)
    //    c_sp_g3(i,i1,i2,is)=c_sp(i)
    //    c_sp_g3(i,i2,i1,is)=c_sp(i)
    //    d_sp_g3(i,i1,i2,is)=d_sp(i)
    //    d_sp_g3(i,i2,i1,is)=d_sp(i)
    //    enddo
    //  enddo
    //  enddo
    //  shag_sp_g=1.0D0/((R_sp_g(n_sp)-R_sp_g(1))/dfloat(n_sp-1))

    // !std::cout << "!!!!!!!!!!!! g3" << std::endl;
    n_sp = n_sp_g[is][is];
    for (i = 0; i < n_sp; i++)
    {
      R_sp[i] = R_sp_g[i];
    }
    for (i1 = 0; i1 < n_f3[is]; i1++)
    {
      for (i2 = 0; i2 <= i1; i2++)
      {
        for (i = 0; i < n_sp; i++)
        {
          a_sp[i] = a_sp_g3[is][i][i1][i2];
          // a_sp[i] = a_sp_g3[is][i1][i2][i];
        }
        p1 = 0.0;
        p2 = 0.0;
        // SPL(n_sp, &R_sp, &a_sp, 1, p1, p2, &b_sp, &c_sp, &d_sp);
        SPL(n_sp, R_sp, a_sp, 1, p1, p2, b_sp, c_sp, d_sp);
        // // !std::cout << "!!!!! PairLS debug mode !!!!! " << " SPL(n_sp, R_sp, a_sp, 1, p1, p2, b_sp, c_sp, d_sp) worked well" << std::endl;
        for (i = 0; i < n_sp; i++)
        { 
          a_sp_g3[is][i][i1][i2] = a_sp[i];
          a_sp_g3[is][i][i2][i1] = a_sp[i];
          b_sp_g3[is][i][i1][i2] = b_sp[i];
          b_sp_g3[is][i][i2][i1] = b_sp[i];
          c_sp_g3[is][i][i1][i2] = c_sp[i];
          c_sp_g3[is][i][i2][i1] = c_sp[i];
          d_sp_g3[is][i][i1][i2] = d_sp[i];
          d_sp_g3[is][i][i2][i1] = d_sp[i];
          // !std::cout << " R_sp_g3[" << i <<  "] = " << R_sp_g[i] << "   ";
          // !std::cout << " a_sp_g3[" << i << "][" << i1 << "][" << i2 << "][" << is << "] = " << a_sp_g3[is][i][i1][i2] << "   ";    
          // !std::cout << " b_sp_g3[" << i << "][" << i1 << "][" << i2 << "][" << is << "] = " << b_sp_g3[is][i][i1][i2] << "   ";    
          // !std::cout << " c_sp_g3[" << i << "][" << i1 << "][" << i2 << "][" << is << "] = " << c_sp_g3[is][i][i1][i2] << "   ";    
          // !std::cout << " d_sp_g3[" << i << "][" << i1 << "][" << i2 << "][" << is << "] = " << d_sp_g3[is][i][i1][i2] << std::endl;   
          // 
          // !std::cout << " R_sp_g3[" << i <<  "] = " << R_sp_g[i] << "   ";
          // !std::cout << " a_sp_g3[" << i << "][" << i2 << "][" << i1 << "][" << is << "] = " << a_sp_g3[is][i][i2][i1] << "   ";    
          // !std::cout << " b_sp_g3[" << i << "][" << i2 << "][" << i1 << "][" << is << "] = " << b_sp_g3[is][i][i2][i1] << "   ";    
          // !std::cout << " c_sp_g3[" << i << "][" << i2 << "][" << i1 << "][" << is << "] = " << c_sp_g3[is][i][i2][i1] << "   ";    
          // !std::cout << " d_sp_g3[" << i << "][" << i2 << "][" << i1 << "][" << is << "] = " << d_sp_g3[is][i][i2][i1] << std::endl;     
        }
      }  
    }
    shag_sp_g = 1.0/((R_sp_g[n_sp-1]-R_sp_g[0])/(n_sp-1));
  } // end if_g3_pot

  memory->destroy(R_sp);
  memory->destroy(a_sp);
  memory->destroy(b_sp);
  memory->destroy(c_sp);
  memory->destroy(d_sp);
  memory->destroy(e_sp);
  memory->destroy(B6);
}



void PairLS::par2pot_is1_is2(int is1, int is2)
{
  // Start pot_ls_a_sp.h
  int n_sp;
  // double R_sp[mfi];
  // double a_sp[mfi], b_sp[mfi], c_sp[mfi], d_sp[mfi], e_sp[mfi];
  double *R_sp;
  double *a_sp, *b_sp, *c_sp, *d_sp, *e_sp; 
  // End pot_ls_a_sp.h

  int i, j, i1, i2, n;
  double p1, p2, pn;
  // double B6[6][1];
  double *B6;
  double r1, r2, f1, fp1, fpp1, f2, fp2, fpp2;
  // !std::cout << "!!!!! PairLS debug mode !!!!! " << " entering par2pot_is1_is2" << std::endl;
  
  memory->destroy(R_sp);
  memory->destroy(a_sp);
  memory->destroy(b_sp);
  memory->destroy(c_sp);
  memory->destroy(d_sp);
  memory->destroy(e_sp);
  memory->destroy(B6);
  // // !std::cout << "!!!!! PairLS debug mode !!!!! " << " destroyed memory for R_sp, a_sp, etc" << std::endl;

  memory->create(R_sp, mfi, "PairLS:a_sp_w");
  memory->create(a_sp, mfi, "PairLS:a_sp_w");
  memory->create(b_sp, mfi, "PairLS:b_sp_w");
  memory->create(c_sp, mfi, "PairLS:c_sp_w");
  memory->create(d_sp, mfi, "PairLS:d_sp_w");
  memory->create(e_sp, mfi, "PairLS:e_sp_w");
  memory->create(B6, 6, "PairLS:B6_w");  

  // // !std::cout << "!!!!! PairLS debug mode !!!!! " << " created memory for R_sp, a_sp, etc" << std::endl;

  zz_ZBL[is1][is2] = z_ion[is1]*z_ion[is2]*pow(3.795,2);
  a_ZBL[is1][is2] = 0.8853*0.5291772083/(pow(z_ion[is1],0.23) + pow(z_ion[is2],0.23));

  // // !std::cout << "!!!!! PairLS debug mode !!!!! " << " zz_ZBL[is][is] and a_ZBL[is][is] assigned" << std::endl;

  n_sp = n_sp_fi[is1][is2];
  for (i = 0; i < n_sp; i++)
  {
    R_sp[i]=R_sp_fi[is1][is2][i];
  }
  for (i = 0; i < n_sp-1; i++)
  {
    a_sp[i]=a_sp_fi[is1][is2][i];
  }  
  a_sp[n_sp-1] = 0.0;

  SPL(n_sp, R_sp, a_sp, 1, fip_rmin[is1][is2], 0.0, b_sp, c_sp, d_sp);

  // // !std::cout << "!!!!! PairLS debug mode !!!!! " << " SPL function worked well" << std::endl;

  // !std::cout << "!!!!!!!!!!!! Fi" << std::endl;
  for (i = 0; i < n_sp; i++)
  {
    a_sp_fi[is1][is2][i] = a_sp[i];
    b_sp_fi[is1][is2][i] = b_sp[i];
    c_sp_fi[is1][is2][i] = c_sp[i];
    d_sp_fi[is1][is2][i] = d_sp[i];
    // !std::cout << " R_sp_fi[" << i << "][" << is1 << "][" << is2 << "] = " << R_sp_fi[is1][is2][i] << "   ";
    // !std::cout << " a_sp_fi[" << i << "][" << is1 << "][" << is2 << "] = " << a_sp_fi[is1][is2][i] << "   ";    
    // !std::cout << " b_sp_fi[" << i << "][" << is1 << "][" << is2 << "] = " << b_sp_fi[is1][is2][i] << "   ";    
    // !std::cout << " c_sp_fi[" << i << "][" << is1 << "][" << is2 << "] = " << c_sp_fi[is1][is2][i] << "   ";    
    // !std::cout << " d_sp_fi[" << i << "][" << is1 << "][" << is2 << "] = " << d_sp_fi[is1][is2][i] << std::endl;    
  }
  
  shag_sp_fi[is1][is2] = 1.0/((R_sp_fi[is1][is2][n_sp-1]-R_sp_fi[is1][is2][0])/(n_sp-1));


  r1 = Rmin_fi_ZBL[is1][is2];
  f1 = v_ZBL(r1, is1, is2) + e0_ZBL[is1][is2];
  fp1 = vp_ZBL(r1, is1, is2);
  fpp1 = vpp_ZBL(r1, is1, is2);
  r2 = R_sp_fi[is1][is2][0];
  f2 = a_sp_fi[is1][is2][0];
  fp2 = b_sp_fi[is1][is2][0];
  fpp2 = 2.0*c_sp_fi[is1][is2][0];  
  
  // // !std::cout << "!!!!! PairLS debug mode !!!!! " << " now smooth_zero_22 function will be called" << std::endl;

  smooth_zero_22(B6, r1, r2, f1, fp1, fpp1, f2, fp2, fpp2);
  // smooth_zero_22(B6, &r1, &r2, &f1, &fp1, &fpp1, &f2, &fp2, &fpp2);
  // // !std::cout << "!!!!! PairLS debug mode !!!!! " << " smooth_zero_22 function worked well" << std::endl;

  // !std::cout << "!!!!!!!!!!!! c_fi_ZBL" << std::endl;
  // !std::cout << " r1 = " << r1 <<std::endl;
  // !std::cout << " r2 = " << r2 <<std::endl;
  // !std::cout << " f1 = " << f1 <<std::endl;
  // !std::cout << " fp1 = " << fp1 <<std::endl;
  // !std::cout << " fpp1 = " << fpp1 <<std::endl;
  // !std::cout << " f2 = " << f2 <<std::endl;
  // !std::cout << " fp2 = " << fp2 <<std::endl;
  // !std::cout << " fpp2 = " << fpp2 <<std::endl;

  for (i = 0; i < 6; i++)
  {
    c_fi_ZBL[is1][is2][i] = B6[i];
    // !std::cout << c_fi_ZBL[is1][is2][i] << "  ";
  }
  // !std::cout << std::endl;
  // // !std::cout << "!!!!! PairLS debug mode !!!!! " << " for (i = 0; i < 6; i++) worked well" << std::endl;


  // ro
  n_sp = n_sp_ro[is1][is2];
  // !std::cout << " n_sp = " << n_sp_ro[is1][is2] << std::endl;
  for (i = 0; i < n_sp; i++)
  {
    // // !std::cout << " i = " << i << " R_sp_ro[is][is][i] = " << R_sp_ro[is1][is2][i] << std::endl;
    R_sp[i] = R_sp_ro[is1][is2][i];
    a_sp[i] = a_sp_ro[is1][is2][i];
  }
  p1=0.0;

  // // !std::cout << "!!!!! PairLS debug mode !!!!! " << " for (i = 0; i < n_sp; i++) worked well" << std::endl;


  // SPL(n_sp, &R_sp, &a_sp, 1, p1, 0.0, &b_sp, &c_sp, &d_sp);
  SPL(n_sp, R_sp, a_sp, 1, p1, 0.0, b_sp, c_sp, d_sp);
  // // !std::cout << "!!!!! PairLS debug mode !!!!! " << " SPL(n_sp, R_sp, a_sp, 1, p1, 0.0, b_sp, c_sp, d_sp) worked well" << std::endl;

  // !std::cout << "!!!!!!!!!!!! Rho" << std::endl;
  for (i = 0; i < n_sp; i++)
  {  
    a_sp_ro[is1][is2][i] = a_sp[i];
    b_sp_ro[is1][is2][i] = b_sp[i];
    c_sp_ro[is1][is2][i] = c_sp[i];
    d_sp_ro[is1][is2][i] = d_sp[i];
    // !std::cout << " R_sp_ro[" << i << "][" << is1 << "][" << is2 << "] = " << R_sp_ro[is1][is2][i] << "   ";
    // !std::cout << " a_sp_ro[" << i << "][" << is1 << "][" << is2 << "] = " << a_sp_ro[is1][is2][i] << "   ";    
    // !std::cout << " b_sp_ro[" << i << "][" << is1 << "][" << is2 << "] = " << b_sp_ro[is1][is2][i] << "   ";    
    // !std::cout << " c_sp_ro[" << i << "][" << is1 << "][" << is2 << "] = " << c_sp_ro[is1][is2][i] << "   ";    
    // !std::cout << " d_sp_ro[" << i << "][" << is1 << "][" << is2 << "] = " << d_sp_ro[is1][is2][i] << std::endl;     
  }
  shag_sp_ro[is1][is2] = 1.0/((R_sp_ro[is1][is2][n_sp-1]-R_sp_ro[is1][is2][0])/(n_sp-1));


  // f3

  n_sp = n_sp_f[is1][is2];
  for (i = 0; i < n_sp; i++)
  {
    R_sp[i] = R_sp_f[is1][is2][i];
  }

  // !std::cout << "!!!!!!!!!!!! f3" << std::endl;
  for (i1 = 0; i1 < n_f3[is2]; i1++)
  {
    for (i = 0; i < n_sp; i++)
    {
      a_sp[i] = a_sp_f3[is1][is2][i1][i];
    }
    p1 = 0.0;
    // SPL(n_sp, &R_sp, &a_sp, 1, p1, 0.0, &b_sp, &c_sp, &d_sp);
    SPL(n_sp, R_sp, a_sp, 1, p1, 0.0, b_sp, c_sp, d_sp);
    // // !std::cout << "!!!!! PairLS debug mode !!!!! " << " SPL(n_sp, R_sp, a_sp, 1, p1, 0.0, b_sp, c_sp, d_sp) worked well" << std::endl;
    for (i = 0; i < n_sp; i++)
    { 
      a_sp_f3[is1][is2][i1][i] = a_sp[i];
      b_sp_f3[is1][is2][i1][i] = b_sp[i];
      c_sp_f3[is1][is2][i1][i] = c_sp[i];
      d_sp_f3[is1][is2][i1][i] = d_sp[i];
      // !std::cout << " R_sp_f3[" << i << "][" << is1 << "][" << is2 << "] = " << R_sp_f[is1][is2][i] << "   ";
      // !std::cout << " a_sp_f3[" << i << "][" << i1 << "][" << is1 << "][" << is2 << "] = " << a_sp_f3[is1][is2][i1][i] << "   ";    
      // !std::cout << " b_sp_f3[" << i << "][" << i1 << "][" << is1 << "][" << is2 << "] = " << b_sp_f3[is1][is2][i1][i] << "   ";    
      // !std::cout << " c_sp_f3[" << i << "][" << i1 << "][" << is1 << "][" << is2 << "] = " << c_sp_f3[is1][is2][i1][i] << "   ";    
      // !std::cout << " d_sp_f3[" << i << "][" << i1 << "][" << is1 << "][" << is2 << "] = " << d_sp_f3[is1][is2][i1][i] << std::endl;      
    }
  }
  shag_sp_f[is1][is2] = 1.0/((R_sp_f[is1][is2][n_sp-1]-R_sp_f[is1][is2][0])/(n_sp-1));

}


// Subroutines for spline creation written by A.G. Lipnitskii and translated from Fortran to C++ 

// smooth_zero_22.f
void PairLS::smooth_zero_22(double *B, double R1, double R2, double f1, double fp1, double fpp1, double f2, double fp2, double fpp2)
{
  //c == calc sqear delta  ==>
  int N = 6, NRHS = 1, LDA = 6, LDB = 7, INFO = 1;
  // int IPIV[N];
  // double A[LDA][N];
  int *IPIV;
  double *A;
  memory->create(IPIV, N, "PairLS:smooth_zero_22_IPIV_w");
  // memory->create(A, LDA, N, "PairLS:smooth_zero_22_A_w");
  memory->create(A, LDA*N, "PairLS:smooth_zero_22_A_w");

  // double B[6][1];
  // double R1, R2, f1, fp1, fpp1, f2, fp2, fpp2;

  A[0] = 1;            // A[0][0] = 1;
  A[1] = 0;            // A[1][0] = 0;
  A[2] = 0;            // A[2][0] = 0;
  A[3] = 1;            // A[3][0] = 1;
  A[4] = 0;            // A[4][0] = 0;
  A[5] = 0;            // A[5][0] = 0;

  A[6] = R1;           // A[0][1] = R1;
  A[7] = 1;            // A[1][1] = 1;
  A[8] = 0;            // A[2][1] = 0;
  A[9] = R2;           // A[3][1] = R2;
  A[10] = 1;            // A[4][1] = 1;
  A[11] = 0;            // A[5][1] = 0;

  A[12] = pow(R1,2);    // A[0][2] = pow(R1,2);
  A[13] = 2*R1;         // A[1][2] = 2*R1;
  A[14] = 2;            // A[2][2] = 2;
  A[15] = pow(R2,2);    // A[3][2] = pow(R2,2);
  A[16] = 2*R2;         // A[4][2] = 2*R2;
  A[17] = 2;            // A[5][2] = 2;

  A[18] = pow(R1,3);    // A[0][3] = pow(R1,3);
  A[19] = 3*pow(R1,2);  // A[1][3] = 3*pow(R1,2);
  A[20] = 6*R1;         // A[2][3] = 6*R1;
  A[21] = pow(R2,3);    // A[3][3] = pow(R2,3);
  A[22] = 3*pow(R2,2);  // A[4][3] = 3*pow(R2,2);
  A[23] = 6*R2;         // A[5][3] = 6*R2;

  A[24] = pow(R1,4);    // A[0][4] = pow(R1,4);
  A[25] = 4*pow(R1,3);  // A[1][4] = 4*pow(R1,3);
  A[26] = 12*pow(R1,2); // A[2][4] = 12*pow(R1,2);
  A[27] = pow(R2,4);    // A[3][4] = pow(R2,4);
  A[28] = 4*pow(R2,3);  // A[4][4] = 4*pow(R2,3);
  A[29] = 12*pow(R2,2); // A[5][4] = 12*pow(R2,2);

  A[30] = pow(R1,5);    // A[0][5] = pow(R1,5);
  A[31] = 5*pow(R1,4);  // A[1][5] = 5*pow(R1,4);
  A[32] = 20*pow(R1,3); // A[2][5] = 20*pow(R1,3);
  A[33] = pow(R2,5);    // A[3][5] = pow(R2,5);
  A[34] = 5*pow(R2,4);  // A[4][5] = 5*pow(R2,4);
  A[35] = 20*pow(R2,3); // A[5][5] = 20*pow(R2,3);

  B[0] = f1;
  B[1] = fp1;
  B[2] = fpp1;
  B[3] = f2;
  B[4] = fp2;
  B[5] = fpp2;

  // // !std::cout << "!!!!! PairLS debug mode !!!!! " << " entering dgesv" << std::endl;

  dgesv_(&N, &NRHS, A, &LDA, IPIV, B, &LDB, &INFO);
  // dgesv_(LAPACK_COL_MAJOR, N, NRHS, A, LDA, IPIV, B, LDB);
  // // !std::cout << "!!!!! PairLS debug mode !!!!! " << " _dgesv worked well" << std::endl;

  memory->destroy(IPIV);
  memory->destroy(A);
};


// SPL.f90
void PairLS::SPL(int n, double *X, double *Y, int ib, double D1, double DN, double *B, double *C, double *D)
{
  // int ib, n;         // intent(in)
  // double X[n], Y[n]; // intent(in)
  // double D1, DN;     // intent(in)
  // double B[n], C[n], D[n];
  double *A, *S;
  double t1, t2, t3;
  int i, n1, nn, Err;
  // Err = 0;
  // std::cout << "!!!!! PairLS debug mode !!!!! " << " entering SPL" << std::endl;
  // std::cout << "!!!!! PairLS debug mode !!!!! " << " n = " << n << std::endl;
  // for (int k = 0; k < n; k++)
  // {
  //   std::cout << " X ["<<k<<"]= " << X[k]  << "  Y ["<<k<<"]= " << Y[k] << std::endl; 
  // }   
  //begin
  // n = size(X)
  // // !std::cout << "!!!!! PairLS debug mode !!!!! " << " entering SPL" << std::endl;
  if (n == 1) 
  {
      B[0] = 0.0; C[0] = 0.0; D[0] = 0.0;
      return;
  } else
  if (n == 2) 
  {
      B[0] = (Y[1] - Y[0])/(X[1] - X[0]);
      C[0] = 0.0; D[0] = 0.0;
      B[1] = 0.0; C[1] = 0.0; D[1] = 0.0;
      return;
  }
  // std::cout << "!!!!! PairLS debug mode !!!!! " << " first if block passed" << std::endl;
  n1 = n - 1;
  B[0] = X[1] - X[0]; B[n-1] = 0.0;
  C[0] = 0.0; C[1] = B[0];
  D[0] = (Y[1] - Y[0])/B[0]; D[1] = D[0];
  memory->create(A, n, "PairLS:SPL_A_w");
  memory->create(S, n, "PairLS:SPL_S_w");  
  // std::cout << "!!!!! PairLS debug mode !!!!! " << " A and S arrays created" << std::endl;

  // SPL.f90(27-32):
  // do i=2, n1
  //   B(i)=X(i+1)-X(i); C(i+1)=B(i)
  //   A(i)=2.0*(X(i+1)-X(i-1))
  //   D(i+1)=(Y(i+1)-Y(i))/B(i)
  //   D(i)=D(i+1)-D(i)
  // end do

  for (i = 1; i < n1; i++)
  {
    // // !std::cout << i << std::endl;
    B[i] = X[i + 1] - X[i]; 
    C[i + 1] = B[i];
    A[i] = 2.0*(X[i + 1] - X[i - 1]);
    D[i + 1] = (Y[i + 1] - Y[i])/B[i];
    D[i] = D[i + 1] - D[i];    
  }

  // std::cout << "!!!!! PairLS debug mode !!!!! " << " Cycle: for (i = 1; i < n1; i++) done" << std::endl;

  switch (ib)
  {
    case 1:
      A[0] = 2.0*B[0]; A[n-1] = 2.0*B[n1-1];
      D[0] = D[0] - D1; D[n-1] = DN - D[n-1];
      nn = n;
      break;
    case 2:
      A[0] = 6.0; A[n-1] = 6.0; B[0] = 0.0; C[n-1] = 0.0;
      D[0] = D1; D[n-1] = DN;
      nn = n;
      break;
    case 3:
      D[0] = D[0] - D[n-1];   
      if (n == 3) 
      {
        A[0] = X[2] - X[0]; 
        A[1] = A[0]; 
        A[2] = A[0]; 
        D[2] = D[0];
        B[0] = 0.0; 
        B[1] = 0.0; 
        C[1] = 0.0; 
        C[2] = 0.0;
        nn = n;   // maybe it should be nn = n - 1
      } 
      else
      {
        A[0] = 2.0*(B[0] + B[n1-1]); C[0] = B[n1-1];
        nn = n1;  // maybe it should be nn = n1 - 1
      }     
      break;
    default:
      A[0] = -B[0]; A[n] = -B[n1-1];
      if (n == 3) 
      {
        D[0] = 0.0; D[2] = 0.0;
      } 
      else
      {
        D[0] = D[2]/(X[3] - X[1]) - D[1]/(X[2] - X[0]);
        D[n-1] = D[n1-1]/(X[n-1] - X[n-3]) - D[n-3]/(X[n1-1] - X[n-4]);
        D[0] = -D[0]*B[0]*B[0]/(X[3] - X[0]);
        D[n-1] = D[n-1]*B[n1-1]*B[n1-1]/(X[n-1] - X[n-4]);
      }
      nn = n;
      break;
  }    

  // std::cout << "!!!!! PairLS debug mode !!!!! " << " now LA30 function will be called" << std::endl;
  LA30(nn, A, B, C, D, S, &Err);
  // LA30(nn, A[0:nn-1], B[0:nn-1], C[0:nn-1], D[0:nn-1], S[0:nn-1], Err);
  // std::cout << "!!!!! PairLS debug mode !!!!! " << " LA30 worked well" << std::endl;

  B[0] = X[1] - X[0];
  if (ib == 3) 
  {
    S[n-1] = S[0]; B[1] = X[2] - X[1];
  }
  for (i = 0; i < n1; i++)
  {
    D[i] = (S[i + 1] - S[i])/B[i];
    C[i] = 3.0*S[i];
    B[i] = (Y[i + 1] - Y[i])/B[i] - B[i]*(S[i + 1] + 2.0*S[i]);
  }
  D[n-1] = D[n1-1]; C[n-1] = 3.0*S[n-1]; B[n-1] = B[n1-1];
  // std::cout << "!!!!! PairLS debug mode !!!!! " << " last if and for block passed" << std::endl;

  memory->destroy(A);
  memory->destroy(S);
}

// LA30.f
void PairLS::LA30(int n, double *A, double *B, double *C, double *D, double *X, int *Error)
{
  //
  // int(4), intent(in):: n;
  // float(8), intent(in):: A[n], B[n], C[n], D[n];
  // float(8):: X[n];
  // int(4):: Error;
  // float(8), allocatable:: P[:], Q[:], R[:], S[:], T[:];
  double *P, *Q, *R, *S, *T;
  double W;
  int i, ii;
  //begin
  // n = size(A)
  if (n < 3) 
  {
    // *Error = 1; 
    return;
  }

  memory->create(P, n, "PairLS:LA30_P_w");
  memory->create(Q, n, "PairLS:LA30_Q_w");
  memory->create(R, n, "PairLS:LA30_R_w");
  memory->create(S, n, "PairLS:LA30_S_w");
  memory->create(T, n, "PairLS:LA30_T_w");
  P[0] = 0.0; 
  Q[0] = 0.0; 
  R[0] = 1.0;

  for (i = 0; i < n-1; i++)
  {
    ii = i + 1;
    W = A[i] + Q[i]*C[i];
    if (1.0 + W == 1.0) 
    {
      memory->destroy(P);
      memory->destroy(Q);
      memory->destroy(R);
      memory->destroy(S);
      memory->destroy(T);
      // *Error = 65; 
      return;
    }
    P[ii] = (D[i] - P[i]*C[i])/W;
    Q[ii] = -B[i]/W;
    R[ii] = -R[i]*C[i]/W;
  }

  S[n-1] = 1.0; 
  T[n-1] = 0.0;
  for (i = n-2; i >= 0; i--) // check for consistency with LA30.f in testing
  {
    ii = i + 1;
    S[i] = Q[ii]*S[ii] + R[ii];
    T[i] = Q[ii]*T[ii] + P[ii];
  }


  W = A[n-1] + B[n-1]*S[0] + C[n-1]*S[n-2];

  if (1.0 + W == 1.0) 
  {
    memory->destroy(P);
    memory->destroy(Q);
    memory->destroy(R);
    memory->destroy(S);
    memory->destroy(T);
    // *Error = 65; 
    return;
  }

  X[n-1] = (D[n-1] - B[n-1]*T[0] - C[n-1]*T[n-2])/W;
  for (i = 0; i < n-1; i++)
  {
    X[i] = S[i]*X[n-1] + T[i];
  }

  memory->destroy(P);
  memory->destroy(Q);
  memory->destroy(R);
  memory->destroy(S);
  memory->destroy(T);
  // *Error = 0; 
  return;
}

// functions for calculating energies and forces

// e_force_fi_emb.f
// void PairLS::e_force_fi_emb(double *e_at, double **f_at, double *px_at, double *py_at, double *pz_at,  double **r_at, int *type, int nlocal, double *sizex, double *sizey, double *sizez)
// void PairLS::e_force_fi_emb(int eflag, double *e_at, double **f_at, double *px_at, double *py_at, double *pz_at,  double **r_at, int *type, int nlocal, double sizex, double sizey, double sizez)
void PairLS::e_force_fi_emb(int eflag, int vflag, double *e_at, double **f_at, double **r_at)
{

  // Local Scalars
  int i, j, ii, jj, jnum, is, js, li, lj;
  int n_i, in_list;
  double rr_pot[10][10];
  double sizex05, sizey05, sizez05, x, y, z, xx, yy, zz, rr, r, r1;
  double roi, roj, w, ropi, ropj, w1, w2, w3;
  int i1, i2, i3, iw;

  // Local Arrays
  double *rosum_global, *rosum_global_proc;

  // pointers to LAMMPS arrays 

  // full neighbor list
  int *ilist,*jlist,*numneigh,**firstneigh;
  int inum = listfull->inum; // # of I atoms neighbors are stored for (the length of the neighborlists list)
  int gnum = listfull->gnum; // # of ghost atoms neighbors are stored for

  ilist = listfull->ilist;           // local indices of I atoms (list of "i" atoms for which neighbor lists exist)
  numneigh = listfull->numneigh;     // # of J neighbors for each I atom (the length of each these neigbor list)
  firstneigh = listfull->firstneigh; // ptr to 1st J int value of each I atom (the pointer to the list of neighbors of "i")

  int nlocal = atom->nlocal;         // nlocal == nlocal for this function
  int nall = nlocal + atom->nghost;
  int newton_pair = force->newton_pair;
  tagint *tag = atom->tag;
  bigint nglobal = atom->natoms;
  int *type = atom->type;

  // arrays for virial computation
  double fi[3], deli[3];

  memory->create(rosum, nall, "PairLS:rosum");
  memory->create(rosum_global_proc, nglobal, "PairLS:rosum_global_proc");
  memory->create(rosum_global, nglobal, "PairLS:rosum_global");
 
  for (is = 1; is <= n_sort; is++)
  {
    for (js = 1; js <= n_sort; js++)
    {
      rr_pot[js][is] = R_sp_fi[js][is][n_sp_fi[js][is]-1]*R_sp_fi[js][is][n_sp_fi[js][is]-1];
      // rr_pot[js][is] = cutsq[js][is];
    }
  }
  
  for (i = 0; i < nlocal; i++)
  {
    e_at[i] = 0.0;
    rosum[i] = 0.0;
  } 

  for (i = 0; i < nglobal; i++) 
  {
    rosum_global_proc[i] = 0.0;
    rosum_global[i] = 0.0;
  }

  // == calc rosum(nlocal) =========================>

  // Loop over the LAMMPS full neighbour list 
  for (ii = 0; ii < inum; ii++) // inum is the # of I atoms neighbors are stored for
  {
    i = ilist[ii];  // local index of atom i
    x = r_at[i][0];
    y = r_at[i][1];
    z = r_at[i][2];
    is = type[i];
    jnum = numneigh[i];    // # of J neighbors for the atom i
    jlist = firstneigh[i]; // ptr to 1st J int value of the atom i
    int n_neighb = 0;
    for (jj = 0; jj < jnum; jj++) 
    {
      j = jlist[jj];  // local or ghost index of atom j
      j &= NEIGHMASK;  // from pair_eam.cpp 
      js = type[j];
      xx = r_at[j][0] - x;
      yy = r_at[j][1] - y;
      zz = r_at[j][2] - z;
      rr = xx*xx + yy*yy + zz*zz;
      if (rr < rr_pot[js][is]) 
      {
        r = sqrt(rr);
        roj = fun_ro(r, js, is);
        rosum[i] += roj;
        w = fun_fi(r, is, js);
        // if (eflag_atom) e_at[i] += w;
        e_at[i] += w;
        n_neighb += 1;
      }
    }
  }

  // == calc energies: e_at(nlocal) ==>
  for (i = 0; i < nlocal; i++)
  {
    w = fun_emb(rosum[i], type[i]) + 0.5*e_at[i];
    e_at[i] = w;
    eng_vdwl += w;        // adding fi_emb energy to the accumulated energy
    if (eflag_atom) eatom[i] = e_at[i];  // adding fi_emb energy to the per-atom energy
  }

  // == calc funp_emb(nlocal) and put one into rosum(nlocal) ==>
  for (i = 0; i < nlocal; i++)
  {
      rosum_global_proc[tag[i]-1] = funp_emb(rosum[i], type[i]); // set rosum for global atom id's on this proc
      rosum[i] = funp_emb(rosum[i], type[i]); // set rosum for local atom id's 
  }

  MPI_Barrier(world); // waiting all MPI processes at this point
  MPI_Allreduce(rosum_global_proc,rosum_global,nglobal,MPI_DOUBLE,MPI_SUM,world); // reducing global array with rosum

  //c == calc forces: f_at(3,nlocal) ==============================>
  // may be this is an unnessessarily cycle as these arrays should be zeroed earlier
  for (i = 0; i < nlocal; i++)
  {
    f_at[i][0] = 0.0; 
    f_at[i][1] = 0.0; 
    f_at[i][2] = 0.0; 
  }

  // Loop over the Verlet"s list
  for (ii = 0; ii < inum; ii++) // inum is the # of I atoms neighbors are stored for
  {
    i = ilist[ii];  // local index of atom i
    // li = atom->map(tag[i]);    // mapped local index of atom i
    x = r_at[i][0];
    y = r_at[i][1];
    z = r_at[i][2];
    is = type[i];
    jnum = numneigh[i];    // # of J neighbors for the atom i
    jlist = firstneigh[i]; // ptr to 1st J int value of the atom i
    for (jj = 0; jj < jnum; jj++) 
    {
      j = jlist[jj];  // index of atom j (maybe local or ghost)
      // j &= NEIGHMASK;  // from pair_eam.cpp 
      lj = atom->map(tag[j]); // mapped local index of atom j
      js = type[j];
      xx = r_at[j][0] - x;
      yy = r_at[j][1] - y;
      zz = r_at[j][2] - z;
      rr = xx*xx + yy*yy + zz*zz;
      if (rr < rr_pot[js][is]) 
      {
        r = sqrt(rr);
        r1 = 1.0/r;
        ropi = funp_ro(r, is, js);
        if (js == is) 
        {
          ropj = ropi;
        } 
        else
        {
          ropj = funp_ro(r, js, is);
        }
        w = ((rosum[i]*ropj + rosum_global[tag[j]-1]*ropi) + funp_fi(r, is, js))*r1; // if j is a ghost atom atom which correspond to atom from other proc we take its rosum from the rosum_global array
        w1 = w*xx; 
        w2 = w*yy; 
        w3 = w*zz;
        f_at[i][0] += w1;
        f_at[i][1] += w2; // add the fi force j = >i;
        f_at[i][2] += w3;

        // virial computation
        fi[0] = w1;
        fi[1] = w2;
        fi[2] = w3;
        deli[0] = xx;
        deli[1] = yy;
        deli[2] = zz;
        // if (evflag) ev_tally_xyz_full(i, 0.0, 0.0, w1, w2, w3, xx, yy, zz); // calculating fi_emb per-atom virial
        if (vflag_atom) v_tally(i, (&fi)[3], (&deli)[3]); // calculating global and per-atom virial
      }
    }
  }

  memory->destroy(rosum);
  memory->destroy(rosum_global);
  memory->destroy(rosum_global_proc);
  return;
}

// void PairLS::e_force_g3(int eflag, double *e_at, double **f_at, double **r_at, int *type, int nlocal, double sizex, double sizey, double sizez)
void PairLS::e_force_g3(int eflag, int vflag, double *e_at, double **f_at, double **r_at)
{
  int i,j,k,ii,jj,kk,l,i1,i2,i3, li, lj;
  int n_i,in_list,n_neighb;
  double w,ww,funfi,funfip;
  double e_angle,Gmn_i,eta,fung,fungp,Gmn[3],Dmn;
  double x,y,z,xx,yy,zz,rop;
  double xx0,yy0,zz0,rr,r,r1,w1,w2,w3,cosl,wp;
  double p_Gmn[3],fx,fy,fz;
  int is,js,ks,iw,i_bas,i1_bas,i2_bas,jnum;

  // Local Arrays
  double rr_pot[10][10];
  double funf[max_neighb][mf3], funfp[max_neighb][mf3];
  double evek_x[max_neighb], evek_y[max_neighb], evek_z[max_neighb];
  double vek_x[max_neighb], vek_y[max_neighb], vek_z[max_neighb], r_1[max_neighb];
  int nom_j[max_neighb];

  tagint *tag = atom->tag;
  bigint nglobal = atom->natoms;

  // arrays for virial computation
  double fi[3], deli[3];

  // Arrays for parallel computing
  double *f_at_g3_x, *f_at_g3_proc_x;
  double *f_at_g3_y, *f_at_g3_proc_y;
  double *f_at_g3_z, *f_at_g3_proc_z;

  // pointers to LAMMPS arrays 
  int *ilist,*jlist,*numneigh,**firstneigh;
  int inum = listfull->inum; // # of I atoms neighbors are stored for (the length of the neighborlists list)
  int gnum = listfull->gnum; // # of ghost atoms neighbors are stored for
  int *type = atom->type;


  ilist = listfull->ilist;           // local indices of I atoms (list of "i" atoms for which neighbor lists exist)
  numneigh = listfull->numneigh;     // # of J neighbors for each I atom (the length of each these neigbor list)
  firstneigh = listfull->firstneigh; // ptr to 1st J int value of each I atom (the pointer to the list of neighbors of "i")

  int nlocal = atom->nlocal;         // nlocal == nlocal for this function
  int nall = nlocal + atom->nghost;
  int newton_pair = force->newton_pair;

  memory->create(f_at_g3_proc_x, nglobal, "PairLS:f_at_g3_proc_x");
  memory->create(f_at_g3_proc_y, nglobal, "PairLS:f_at_g3_proc_y");
  memory->create(f_at_g3_proc_z, nglobal, "PairLS:f_at_g3_proc_z");
  memory->create(f_at_g3_x, nglobal, "PairLS:f_at_g3_x");
  memory->create(f_at_g3_y, nglobal, "PairLS:f_at_g3_y");
  memory->create(f_at_g3_z, nglobal, "PairLS:f_at_g3_z");

  for (i = 0; i < nglobal; i++)
  {
    f_at_g3_x[i] = 0.0;
    f_at_g3_y[i] = 0.0;
    f_at_g3_z[i] = 0.0;
    f_at_g3_proc_x[i] = 0.0;
    f_at_g3_proc_y[i] = 0.0;
    f_at_g3_proc_z[i] = 0.0;    
  }

  for (is = 1; is <= n_sort; is++)
  {
    for (js = 1; js <= n_sort; js++)
    {
      rr_pot[js][is] = R_sp_f[js][is][n_sp_f[js][is]-1]*R_sp_f[js][is][n_sp_f[js][is]-1];
      // rr_pot[js][is] = cutsq[js][is];
    }
  }

  //c == angle 3 ========================================================

  // Loop over the Verlet"s list (j>i and j<i neighbours are included)
  for (ii = 0; ii < inum; ii++) // inum is the # of I atoms neighbors are stored for
  {
    i = ilist[ii];  // local index of atom i
    jnum = numneigh[i];    // # of J neighbors for the atom i
    if (jnum < 2) continue;
    x = r_at[i][0];
    y = r_at[i][1];
    z = r_at[i][2];
    is = type[i];
    jlist = firstneigh[i]; // ptr to 1st J int value of the atom i
    n_neighb = 0;

    for (jj = 0; jj < jnum; jj++) 
    {
      j = jlist[jj];  // local or ghost index of atom j
      js = type[j]; 
      lj = atom->map(tag[j]);  // mapped local index of atom j
      xx = r_at[j][0] - x;
      yy = r_at[j][1] - y;
      zz = r_at[j][2] - z;
      rr = xx*xx + yy*yy + zz*zz;
      if (rr < rr_pot[js][is])
      {
        r = sqrt(rr);
        r1 = 1.0/r;
      
        // calc the parameters for the angle part
        for (i_bas = 0; i_bas < n_f3[is]; i_bas++)
        {
          funf[n_neighb][i_bas] = fun_f3(r, i_bas, js, is);
          funfp[n_neighb][i_bas] = funp_f3(r, i_bas, js, is);
        }

        evek_x[n_neighb] = xx*r1;
        evek_y[n_neighb] = yy*r1;
        evek_z[n_neighb] = zz*r1;
        r_1[n_neighb] = r1;
        vek_x[n_neighb] = xx;
        vek_y[n_neighb] = yy;
        vek_z[n_neighb] = zz;
        nom_j[n_neighb] = j;

        n_neighb += 1;
      }
    }  // end loop over jj

    // angle // angle // angle // angle // angle // angle // angle // angle // angle // angle // angle // angle // angle // angle

    e_angle = 0.0;
    if (n_neighb > 1) 
    {
      for (jj = 0; jj < n_neighb; jj++)
      {
        j = nom_j[jj]; // j is n in formul for Dmn and j is m in formul for Gmn;
        lj = atom->map(tag[j]);  // mapped local index of atom j
        js = type[j];
        
        xx = r_at[j][0] - x;
        yy = r_at[j][1] - y;
        zz = r_at[j][2] - z;

        Dmn = 0.0;
        Gmn[0] = 0.0;
        Gmn[1] = 0.0;
        Gmn[2] = 0.0;
        p_Gmn[0] = 0.0;
        p_Gmn[1] = 0.0;
        p_Gmn[2] = 0.0;
        for (kk = 0; kk < n_neighb; kk++)
        {
          if (kk == jj) continue;
          k = nom_j[kk];
          ks = type[k];
          Gmn_i = 0.0;
          eta = evek_x[jj]*evek_x[kk] + evek_y[jj]*evek_y[kk] + evek_z[jj]*evek_z[kk]; // cos_nmk;
          // Start initialization of fun_g3
          int iii = int((eta - R_sp_g[0])*shag_sp_g);
          iii += 1; 
          if (iii >= n_sp_g[is][is]) iii = n_sp_g[is][is] - 1;
          double dr = eta - R_sp_g[iii-1];
          iii -= 1;
          // End initialization of fun_g3
          for (i1_bas = 0; i1_bas < n_f3[is]; i1_bas++) // do i1_bas = 1, n_f3(is)
          {
            for (i2_bas = 0; i2_bas < n_f3[is]; i2_bas++) // do i2_bas = 1, n_f3(is)
            {
              // Start optimized call of fun_g3 and funp_g3
              fung = a_sp_g3[is][iii][i1_bas][i2_bas] + dr*(b_sp_g3[is][iii][i1_bas][i2_bas] + dr*(c_sp_g3[is][iii][i1_bas][i2_bas] + dr*(d_sp_g3[is][iii][i1_bas][i2_bas])));
              fungp = b_sp_g3[is][iii][i1_bas][i2_bas] + dr*(2.0*c_sp_g3[is][iii][i1_bas][i2_bas] + dr*(3.0*d_sp_g3[is][iii][i1_bas][i2_bas]));
              // End optimized call of fun_g3 and funp_g3
              Dmn += (funfp[jj][i1_bas]*funf[kk][i2_bas]*fung + funf[jj][i1_bas]*funf[kk][i2_bas]*fungp*(r_1[kk] - r_1[jj]*eta));
              Gmn_i += funf[jj][i1_bas]*funf[kk][i2_bas]*fungp;
              if (kk>jj) e_angle += funf[jj][i1_bas]*funf[kk][i2_bas]*fung;
            }
          }
          w1 = Gmn_i*(r_1[jj]*r_1[kk]*(vek_x[jj] - vek_x[kk]));
          w2 = Gmn_i*(r_1[jj]*r_1[kk]*(vek_y[jj] - vek_y[kk]));
          w3 = Gmn_i*(r_1[jj]*r_1[kk]*(vek_z[jj] - vek_z[kk]));
          Gmn[0] += w1;
          Gmn[1] += w2;
          Gmn[2] += w3;
        }

        f_at_g3_proc_x[tag[i]-1] += Dmn*evek_x[jj];
        f_at_g3_proc_y[tag[i]-1] += Dmn*evek_y[jj];
        f_at_g3_proc_z[tag[i]-1] += Dmn*evek_z[jj];

        f_at_g3_proc_x[tag[j]-1] += (Gmn[0] - Dmn*evek_x[jj]);
        f_at_g3_proc_y[tag[j]-1] += (Gmn[1] - Dmn*evek_y[jj]);
        f_at_g3_proc_z[tag[j]-1] += (Gmn[2] - Dmn*evek_z[jj]);

        // virial computation
        fi[0] = Dmn*evek_x[jj];
        fi[1] = Dmn*evek_y[jj];
        fi[2] = Dmn*evek_z[jj];
        deli[0] = xx;
        deli[1] = yy;
        deli[2] = zz;
        // if (evflag) ev_tally_xyz_full(i, 0.0, 0.0, Dmn*evek_x[jj], Dmn*evek_y[jj], Dmn*evek_z[jj], xx, yy, zz); // calculating global and per-atom virial
        if (vflag_atom) v_tally(i, (&fi)[3], (&deli)[3]); // calculating global and per-atom virial
      }
    }
    e_at[i] += e_angle;
    if (eflag_atom) eatom[i] += e_angle;
    eng_vdwl += e_angle;
  }


  MPI_Barrier(world); // waiting all MPI processes at this point
  MPI_Allreduce(f_at_g3_proc_x,f_at_g3_x,nglobal,MPI_DOUBLE,MPI_SUM,world); 
  MPI_Barrier(world); // waiting all MPI processes at this point
  MPI_Allreduce(f_at_g3_proc_y,f_at_g3_y,nglobal,MPI_DOUBLE,MPI_SUM,world); 
  MPI_Barrier(world); // waiting all MPI processes at this point
  MPI_Allreduce(f_at_g3_proc_z,f_at_g3_z,nglobal,MPI_DOUBLE,MPI_SUM,world); 

  for (i = 0; i < nlocal; i++)
  {
    f_at[i][0] += f_at_g3_x[tag[i]-1];
    f_at[i][1] += f_at_g3_y[tag[i]-1];
    f_at[i][2] += f_at_g3_z[tag[i]-1];
  }

 
  memory->destroy(f_at_g3_proc_x);
  memory->destroy(f_at_g3_proc_y);
  memory->destroy(f_at_g3_proc_z);
  memory->destroy(f_at_g3_x);
  memory->destroy(f_at_g3_y);
  memory->destroy(f_at_g3_z);
  
  return;
}









// Potential functions from fun_pot_ls.f

// fun_pot_ls.f(3-36):
double PairLS::fun_fi(double r, int is, int js)
{
  int i;
  double fun_fi_value, dr, r0_min;

  if (r >= R_sp_fi[is][js][n_sp_fi[is][js]-1]) 
  {
    fun_fi_value = 0.0;
    return fun_fi_value;
  }

  if (r < Rmin_fi_ZBL[is][js]) 
  {
    fun_fi_value = v_ZBL(r, is, js) + e0_ZBL[is][js];
    return fun_fi_value;
  }

  r0_min = R_sp_fi[is][js][0];

  if (r < r0_min) 
  {
    fun_fi_value = fun_fi_ZBL(r, is, js);
    return fun_fi_value;
  }

  i = int((r - R_sp_fi[is][js][0])*shag_sp_fi[is][js]);
  i = i + 1;
  if (i < 1) i = 1;
  dr = r - R_sp_fi[is][js][i-1];
  fun_fi_value = a_sp_fi[is][js][i-1] + dr*(b_sp_fi[is][js][i-1] + dr*(c_sp_fi[is][js][i-1] + dr*(d_sp_fi[is][js][i-1])));

  return fun_fi_value;

}

// fun_pot_ls.f(39-70):
double PairLS::funp_fi(double r, int is, int js)
{
  int i;
  double funp_fi_value, dr, r0_min;

  if (r >= R_sp_fi[is][js][n_sp_fi[is][js]-1]) 
  {
    funp_fi_value = 0.0;
    return funp_fi_value;
  }

  if (r < Rmin_fi_ZBL[is][js]) 
  {
    funp_fi_value = vp_ZBL(r, is, js);
    return funp_fi_value;
  }

  r0_min = R_sp_fi[is][js][0];

  if (r < r0_min) 
  {
    funp_fi_value = funp_fi_ZBL(r, is, js);
    return funp_fi_value;
  }

  i = int((r - R_sp_fi[is][js][0])*shag_sp_fi[is][js]);
  i = i + 1;
  if (i < 1) i = 1;
  dr = r - R_sp_fi[is][js][i-1];
  funp_fi_value = b_sp_fi[is][js][i-1] + dr*(2.0*c_sp_fi[is][js][i-1] + dr*(3.0*d_sp_fi[is][js][i-1]));

  return funp_fi_value;
}

// fun_pot_ls.f(74-106):
double PairLS::funpp_fi(double r, int is, int js)
{
  int i;
  double funpp_fi_value, dr, r0_min;

  if (r >= R_sp_fi[is][js][n_sp_fi[is][js]-1]) 
  {
    funpp_fi_value = 0.0;
    return funpp_fi_value;
  }

  if (r < Rmin_fi_ZBL[is][js]) 
  {
    funpp_fi_value = vpp_ZBL(r, is, js);
    return funpp_fi_value;
  }

  r0_min = R_sp_fi[is][js][0];

  if (r < r0_min) 
  {
    funpp_fi_value = funpp_fi_ZBL(r, is, js);
    return funpp_fi_value;
  }

  i = int((r - r0_min)*shag_sp_fi[is][js]);
  i = i + 1;
  if (i < 1) i = 1;
  dr = r - R_sp_fi[is][js][i-1];
  funpp_fi_value = 2.0*c_sp_fi[is][js][i-1] + dr*(6.0*d_sp_fi[is][js][i-1]);

  return funpp_fi_value;
}



// fun_pot_ls.f(112-139):
double PairLS::fun_ro(double r, int is, int js)
{
  int i;
  double fun_ro_value, dr, r0_min;

  if (r >= R_sp_ro[is][js][n_sp_ro[is][js]-1]) 
  {
    fun_ro_value = 0.0;
    return fun_ro_value;
  }

  r0_min = R_sp_ro[is][js][0];

  if (r < r0_min) 
  {
    fun_ro_value = a_sp_ro[is][js][0];
    return fun_ro_value;
  }

  i = int((r - r0_min)*shag_sp_ro[is][js]);
  i = i + 1;
  dr = r - R_sp_ro[is][js][i-1];
  fun_ro_value = a_sp_ro[is][js][i-1] + dr*(b_sp_ro[is][js][i-1] + dr*(c_sp_ro[is][js][i-1] + dr*(d_sp_ro[is][js][i-1])));

  return fun_ro_value;
}

// fun_pot_ls.f(142-169):
double PairLS::funp_ro(double r, int is, int js)
{
  int i;
  double funp_ro_value, dr, r0_min;

  if (r >= R_sp_ro[is][js][n_sp_ro[is][js]-1]) 
  {
    funp_ro_value = 0.0;
    return funp_ro_value;
  }

  r0_min = R_sp_ro[is][js][0];

  if (r < r0_min) 
  {
    funp_ro_value = 0.0;
    return funp_ro_value;
  }

  i = int((r - r0_min)*shag_sp_ro[is][js]);
  i = i + 1;
  dr = r - R_sp_ro[is][js][i-1];
  funp_ro_value = b_sp_ro[is][js][i-1] + dr*(2.0*c_sp_ro[is][js][i-1] + dr*(3.0*d_sp_ro[is][js][i-1]));

  return funp_ro_value;  
}

// fun_pot_ls.f(173-200):
double PairLS::funpp_ro(double r, int is, int js)
{
  int i;
  double funpp_ro_value, dr, r0_min;

  if (r >= R_sp_ro[is][js][n_sp_ro[is][js]-1]) 
  {
    funpp_ro_value = 0.0;
    return funpp_ro_value;
  }

  r0_min = R_sp_ro[is][js][0];

  if (r < r0_min) 
  {
    funpp_ro_value = 0.0;
    return funpp_ro_value;
  }

  i = int((r - r0_min)*shag_sp_ro[is][js]);
  i = i + 1;
  if (i <= 0) i = 1;
  dr = r - R_sp_ro[is][js][i-1];
  funpp_ro_value = 2.0*c_sp_ro[is][js][i-1] + dr*(6.0*d_sp_ro[is][js][i-1]);

  return funpp_ro_value;  
}  


// fun_pot_ls.f(209-245):
double PairLS::fun_emb(double r, int is)
{
  int i;
  double fun_emb_value, dr, r0_min;

  if (r >= R_sp_emb[is][n_sp_emb[is][is]-1]) 
  {
      fun_emb_value = a_sp_emb[is][n_sp_emb[is][is]-1];
      return fun_emb_value;
  }

  r0_min = R_sp_emb[is][0];

  if (r <= r0_min) 
  {
      fun_emb_value = b_sp_emb[is][0]*(r - r0_min);
      return fun_emb_value;
  }

  //c     
  i = int((r - R_sp_emb[is][0])*shag_sp_emb[is]);
  i = i + 1;
  dr = r - R_sp_emb[is][i-1];
  fun_emb_value = a_sp_emb[is][i-1] + dr*(b_sp_emb[is][i-1] + dr*(c_sp_emb[is][i-1] + dr*(d_sp_emb[is][i-1])));

  return fun_emb_value;  
}

// fun_pot_ls.f(248-273):
double PairLS::funp_emb(double r, int is)
{
  int i;
  double funp_emb_value, dr, r0_min;

  if (r >= R_sp_emb[is][n_sp_emb[is][is]-1]) 
  {
      funp_emb_value = 0.0;
      return funp_emb_value;
  }

  r0_min = R_sp_emb[is][0];

  if (r <= r0_min) 
  {
      funp_emb_value = b_sp_emb[is][0];
      return funp_emb_value;
  }

  //c     
  i = int((r - r0_min)*shag_sp_emb[is]);
  i = i + 1;
  dr = r - R_sp_emb[is][i-1];
  funp_emb_value = b_sp_emb[is][i-1] + dr*(2.0*c_sp_emb[is][i-1] + dr*(3.0*d_sp_emb[is][i-1]));

  return funp_emb_value;  
}

// fun_pot_ls.f(285-312):
double PairLS::funpp_emb(double r, int is)
{
  int i;
  double funpp_emb_value, dr, r0_min;

  if (r >= R_sp_emb[is][n_sp_emb[is][is]-1]) 
  {
      funpp_emb_value = 0.0;
      return funpp_emb_value;
  }

  r0_min = R_sp_emb[is][0];

  if (r <= r0_min) 
  {
      funpp_emb_value = 0.0;
      return funpp_emb_value;
  }

  //c     
  i = int((r - r0_min)*shag_sp_emb[is]);
  i = i + 1;
  if(i <= 0) i = 1; // maybe this condition should be added also for fun_emb and funp_emb?
  dr = r - R_sp_emb[is][i-1];
  funpp_emb_value = 2.0*c_sp_emb[is][i-1] + dr*(6.0*d_sp_emb[is][i-1]);

  return funpp_emb_value;    
} 


// fun_pot_ls.f(319-347):
double PairLS::fun_f3(double r, int i_f3, int js, int is)
{
  int i;
  double fun_f3_value, dr, r0_min;

  if (r >= R_sp_f[js][is][n_sp_f[js][is]-1]) 
  {
      fun_f3_value = 0.0;
      return fun_f3_value;
  }

  r0_min = R_sp_f[js][is][0];

  if (r <= r0_min) 
  {
      fun_f3_value = a_sp_f3[js][is][i_f3][0];
      return fun_f3_value;
  }

  i = int((r - r0_min)*shag_sp_f[js][is]);
  i = i + 1;
  dr = r - R_sp_f[js][is][i-1];
  // fun_f3_value = a_sp_f3[i-1][i_f3-1][js][is] + dr*(b_sp_f3[i-1][i_f3-1][js][is] + dr*(c_sp_f3[i-1][i_f3-1][js][is] + dr*(d_sp_f3[i-1][i_f3-1][js][is])));
  fun_f3_value = a_sp_f3[js][is][i_f3][i-1] + dr*(b_sp_f3[js][is][i_f3][i-1] + dr*(c_sp_f3[js][is][i_f3][i-1] + dr*(d_sp_f3[js][is][i_f3][i-1])));

  return fun_f3_value;
}

// fun_pot_ls.f(350-377):
double PairLS::funp_f3(double r, int i_f3, int js, int is)
{
  int i;
  double funp_f3_value, dr, r0_min;

  if (r >= R_sp_f[js][is][n_sp_f[js][is]-1]) 
  {
      funp_f3_value = 0.0;
      return funp_f3_value;
  }

  r0_min = R_sp_f[js][is][0];

  if (r <= r0_min) 
  {
      funp_f3_value = 0.0;
      return funp_f3_value;
  }

  i = int((r - r0_min)*shag_sp_f[js][is]);
  i = i + 1;
  dr = r - R_sp_f[js][is][i-1];
  // funp_f3_value = b_sp_f3[i-1][i_f3-1][js][is] + dr*(2.0*c_sp_f3[i-1][i_f3-1][js][is] + dr*(3.0*d_sp_f3[i-1][i_f3-1][js][is]));
  funp_f3_value = b_sp_f3[js][is][i_f3][i-1] + dr*(2.0*c_sp_f3[js][is][i_f3][i-1] + dr*(3.0*d_sp_f3[js][is][i_f3][i-1]));

  return funp_f3_value;  
}

// fun_pot_ls.f(381-406):
double PairLS::funpp_f3(double r, int i_f3, int js, int is)
{
  int i;
  double funpp_f3_value, dr, r0_min;

  if (r >= R_sp_f[js][is][n_sp_f[js][is]-1]) 
  {
      funpp_f3_value = 0.0;
      return funpp_f3_value;
  }

  r0_min = R_sp_f[js][is][0];

  if (r <= r0_min) 
  {
      funpp_f3_value = 0.0;
      return funpp_f3_value;
  }

  i = int((r - r0_min)*shag_sp_f[js][is]);
  i = i + 1;
  if (i <= 0) i = 1;
  dr = r - R_sp_f[js][is][i-1];
  // funpp_f3_value = 2.0*c_sp_f3[i-1][i_f3-1][js][is] + dr*(6.0*d_sp_f3[i-1][i_f3-1][js][is]);
  funpp_f3_value = 2.0*c_sp_f3[js][is][i_f3][i-1] + dr*(6.0*d_sp_f3[js][is][i_f3][i-1]);

  return funpp_f3_value;    
}


// fun_pot_ls.f(412-425):
double PairLS::fun_g3(double r, int i1, int i2, int is)
{
  int i;
  double fun_g3_value, dr;

  i = int((r - R_sp_g[0])*shag_sp_g);
  i = i + 1;
  if (i >= n_sp_g[is][is]) i = n_sp_g[is][is] - 1;
  dr = r - R_sp_g[i-1];
  // fun_g3_value = a_sp_g3[i-1][i1-1][i2-1][is] + dr*(b_sp_g3[i-1][i1-1][i2-1][is] + dr*(c_sp_g3[i-1][i1-1][i2-1][is] + dr*(d_sp_g3[i-1][i1-1][i2-1][is])));
  fun_g3_value = a_sp_g3[is][i-1][i1][i2] + dr*(b_sp_g3[is][i-1][i1][i2] + dr*(c_sp_g3[is][i-1][i1][i2] + dr*(d_sp_g3[is][i-1][i1][i2])));
  return fun_g3_value;
}

// fun_pot_ls.f(428-442):
double PairLS::funp_g3(double r, int i1, int i2, int is)
{
  int i;
  double funp_g3_value, dr;

  i = int((r - R_sp_g[0])*shag_sp_g);
  i = i + 1;
  if (i >= n_sp_g[is][is]) i = n_sp_g[is][is] - 1;
  dr = r - R_sp_g[i-1];
  // funp_g3_value = b_sp_g3[i-1][i1-1][i2-1][is] + dr*(2.0*c_sp_g3[i-1][i1-1][i2-1][is] + dr*(3.0*d_sp_g3[i-1][i1-1][i2-1][is]));
  funp_g3_value = b_sp_g3[is][i-1][i1][i2] + dr*(2.0*c_sp_g3[is][i-1][i1][i2] + dr*(3.0*d_sp_g3[is][i-1][i1][i2]));
  return funp_g3_value;
}

// fun_pot_ls.f(446-459):
double PairLS::funpp_g3(double r, int i1, int i2, int is)
{
  int i;
  double funpp_g3_value, dr;

  i = int((r - R_sp_g[0])*shag_sp_g);
  i = i + 1;
  if (i >= n_sp_g[is][is]) i = n_sp_g[is][is] - 1;
  dr = r - R_sp_g[i-1];
  // funpp_g3_value = 2.0*c_sp_g3[i-1][i1-1][i2-1][is] + dr*(6.0*d_sp_g3[i-1][i1-1][i2-1][is]);
  funpp_g3_value = 2.0*c_sp_g3[is][i-1][i1][i2] + dr*(6.0*d_sp_g3[is][i-1][i1][i2]);
  return funpp_g3_value;
}


// fun_pot_ls.f(603-623):
double PairLS::v_ZBL(double r, int is, int js)
{
  int i;
  double v_ZBL;
  double w, sum, zz_r;

  zz_r = zz_ZBL[is][js]/r;

  w = r/a_ZBL[is][js];

  sum = 0.0;
  for (i = 0; i < 4; i++)
  {
    // sum = sum + c_ZBL[i][is][js]*exp(-d_ZBL[i][is][js]*w);
    sum = sum + c_ZBL[i]*exp(-d_ZBL[i]*w);
  }

  v_ZBL = zz_r*sum;
        
  return v_ZBL;
}

// fun_pot_ls.f(627-655):
double PairLS::vp_ZBL(double r, int is, int js)
{
  int i;
  double vp_ZBL;
  double w, sum, sump, zz_r, zzp_r;

  zz_r = zz_ZBL[is][js]/r;
  zzp_r = -zz_r/r;

  w = r/a_ZBL[is][js];

  sum = 0.0;
  for (i = 0; i < 4; i++)
  {
    // sum = sum + c_ZBL[i][is][js]*exp(-d_ZBL[i][is][js]*w);
    sum = sum + c_ZBL[i]*exp(-d_ZBL[i]*w);
  }

  sump = 0.0;
  for (i = 0; i < 4; i++)
  {
    // sump = sump + c_ZBL[i][is][js]*exp(-d_ZBL[i][is][js]*w)*(-d_ZBL[i][is][js]/a_ZBL[is][js]);
    sump = sump + c_ZBL[i]*exp(-d_ZBL[i]*w)*(-d_ZBL[i]/a_ZBL[is][js]);
  }

  vp_ZBL = zzp_r*sum + zz_r*sump;
        
  return vp_ZBL;
}

// fun_pot_ls.f(659-694):
double PairLS::vpp_ZBL(double r, int is, int js)
{
  int i;
  double vpp_ZBL;
  double w, sum, sump, sumpp, zz_r, zzp_r, zzpp_r;

  zz_r = zz_ZBL[is][js]/r;
  zzp_r = -zz_r/r;
  zzpp_r = -2.0*zzp_r/r;

  w = r/a_ZBL[is][js];

  sum = 0.0;
  for (i = 0; i < 4; i++)
  {
    // sum = sum + c_ZBL[i][is][js]*exp(-d_ZBL[i][is][js]*w);
    sum = sum + c_ZBL[i]*exp(-d_ZBL[i]*w);
  }

  sump = 0.0;
  for (i = 0; i < 4; i++)
  {
    // sump = sump + c_ZBL[i][is][js]*exp(-d_ZBL[i][is][js]*w)*(-d_ZBL[i][is][js]/a_ZBL[is][js]);
    sump = sump + c_ZBL[i]*exp(-d_ZBL[i]*w)*(-d_ZBL[i]/a_ZBL[is][js]);
  }

  sumpp = 0.0;
  for (i = 0; i < 4; i++)
  {
    // sumpp = sumpp + c_ZBL[i][is][js]*exp(-d_ZBL[i][is][js]*w)*pow((d_ZBL[i][is][js]/a_ZBL[is][js]),2);
    sumpp = sumpp + c_ZBL[i]*exp(-d_ZBL[i]*w)*pow((d_ZBL[i]/a_ZBL[is][js]),2);
  }

  vpp_ZBL = zzpp_r*sum + 2.0*zzp_r*sump + zz_r*sumpp;
        
  return vpp_ZBL;
}


// fun_pot_ls.f(698-711):
double PairLS::fun_fi_ZBL(double r, int is, int js)
{
  double fun_fi_ZBL;

  fun_fi_ZBL = c_fi_ZBL[is][js][0] + r*(c_fi_ZBL[1][is][js] + r*(c_fi_ZBL[2][is][js] + r*(c_fi_ZBL[3][is][js] + r*(c_fi_ZBL[4][is][js] + r*(c_fi_ZBL[5][is][js])))));

  return fun_fi_ZBL;
}

// fun_pot_ls.f(715-727):
double PairLS::funp_fi_ZBL(double r, int is, int js)
{
  double funp_fi_ZBL;

  funp_fi_ZBL = c_fi_ZBL[1][is][js] + r*(2.0*c_fi_ZBL[2][is][js] + r*(3.0*c_fi_ZBL[3][is][js] + r*(4.0*c_fi_ZBL[4][is][js] + r*(5.0*c_fi_ZBL[5][is][js]))));

  return funp_fi_ZBL;
}

// fun_pot_ls.f(731-742):
double PairLS::funpp_fi_ZBL(double r, int is, int js)
{
  double funpp_fi_ZBL;

  funpp_fi_ZBL = 2.0*c_fi_ZBL[2][is][js] + r*(6.0*c_fi_ZBL[3][is][js] + r*(12.0*c_fi_ZBL[4][is][js] + r*(20.0*c_fi_ZBL[5][is][js])));

  return funpp_fi_ZBL;
}