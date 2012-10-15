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
   Contributing authors: Rolf Isele-Holder (Aachen University)
                         Paul Crozier (SNL)
------------------------------------------------------------------------- */

#include "lmptype.h"
#include "mpi.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"
#include "math.h"
#include "pppm_disp.h"
#include "math_const.h"
#include "atom.h"
#include "comm.h"
#include "neighbor.h"
#include "force.h"
#include "pair.h"
#include "bond.h"
#include "angle.h"
#include "domain.h"
#include "fft3d_wrap.h"
#include "remap_wrap.h"
#include "memory.h"
#include "error.h"

using namespace LAMMPS_NS;
using namespace MathConst;

#define MAXORDER   7
#define OFFSET 16384
#define SMALL 0.00001
#define LARGE 10000.0
#define EPS_HOC 1.0e-7
enum{GEOMETRIC,ARITHMETIC,SIXTHPOWER};

#ifdef FFT_SINGLE
#define ZEROF 0.0f
#define ONEF  1.0f
#else
#define ZEROF 0.0
#define ONEF  1.0
#endif

/* ---------------------------------------------------------------------- */

PPPMDisp::PPPMDisp(LAMMPS *lmp, int narg, char **arg) : KSpace(lmp, narg, arg)
{
  if (narg < 1) error->all(FLERR,"Illegal kspace_style pppm/disp command");

  pppmflag = dispersionflag = 1;
  accuracy_relative = atof(arg[0]);
  
  nfactors = 3;
  factors = new int[nfactors];
  factors[0] = 2;
  factors[1] = 3;
  factors[2] = 5;

  MPI_Comm_rank(world,&me);
  MPI_Comm_size(world,&nprocs);

  csumflag = 0;
  B = NULL;
  cii = NULL;
  csumi = NULL;
  peratom_allocate_flag = 0;

  density_brick = vdx_brick = vdy_brick = vdz_brick = NULL;
  density_fft = NULL;
  u_brick = v0_brick = v1_brick = v2_brick = v3_brick = 
    v4_brick = v5_brick = NULL;

  density_brick_g = vdx_brick_g = vdy_brick_g = vdz_brick_g = NULL;
  density_fft_g = NULL;
  u_brick_g = v0_brick_g = v1_brick_g = v2_brick_g = v3_brick_g = 
    v4_brick_g = v5_brick_g = NULL;

  density_brick_a0 = vdx_brick_a0 = vdy_brick_a0 = vdz_brick_a0 = NULL;
  density_fft_a0 = NULL;
  u_brick_a0 = v0_brick_a0 = v1_brick_a0 = v2_brick_a0 = v3_brick_a0 = 
    v4_brick_a0 = v5_brick_a0 = NULL;

  density_brick_a1 = vdx_brick_a1 = vdy_brick_a1 = vdz_brick_a1 = NULL;
  density_fft_a1 = NULL;
  u_brick_a1 = v0_brick_a1 = v1_brick_a1 = v2_brick_a1 = v3_brick_a1 = 
    v4_brick_a1 = v5_brick_a1 = NULL;

  density_brick_a2 = vdx_brick_a2 = vdy_brick_a2 = vdz_brick_a2 = NULL;
  density_fft_a2 = NULL;
  u_brick_a2 = v0_brick_a2 = v1_brick_a2 = v2_brick_a2 = v3_brick_a2 = 
    v4_brick_a2 = v5_brick_a2 = NULL;

  density_brick_a3 = vdx_brick_a3 = vdy_brick_a3 = vdz_brick_a3 = NULL;
  density_fft_a3 = NULL;
  u_brick_a3 = v0_brick_a3 = v1_brick_a3 = v2_brick_a3 = v3_brick_a3 = 
    v4_brick_a3 = v5_brick_a3 = NULL;

  density_brick_a4 = vdx_brick_a4 = vdy_brick_a4 = vdz_brick_a4 = NULL;
  density_fft_a4 = NULL;
  u_brick_a4 = v0_brick_a4 = v1_brick_a4 = v2_brick_a4 = v3_brick_a4 = 
    v4_brick_a4 = v5_brick_a4 = NULL;

  density_brick_a5 = vdx_brick_a5 = vdy_brick_a5 = vdz_brick_a5 = NULL;
  density_fft_a5 = NULL;
  u_brick_a5 = v0_brick_a5 = v1_brick_a5 = v2_brick_a5 = v3_brick_a5 = 
    v4_brick_a5 = v5_brick_a5 = NULL;

  density_brick_a6 = vdx_brick_a6 = vdy_brick_a6 = vdz_brick_a6 = NULL;
  density_fft_a6 = NULL;
  u_brick_a6 = v0_brick_a6 = v1_brick_a6 = v2_brick_a6 = v3_brick_a6 = 
    v4_brick_a6 = v5_brick_a6 = NULL;

  greensfn = NULL;
  greensfn_6 = NULL;
  work1 = work2 = NULL;
  work1_6 = work2_6 = NULL;
  vg = NULL;
  vg2 = NULL;
  vg_6 = NULL;
  vg2_6 = NULL;
  fkx = fky = fkz = NULL;
  fkx2 = fky2 = fkz2 = NULL;
  fkx_6 = fky_6 = fkz_6 = NULL;
  fkx2_6 = fky2_6 = fkz2_6 = NULL;
  buf1 = buf2 = buf3 = buf4 = NULL;
  buf1_6 = buf2_6 = buf3_6 = buf4_6= NULL;

  sf_precoeff1 = sf_precoeff2 = sf_precoeff3 = sf_precoeff4 = 
    sf_precoeff5 = sf_precoeff6 = NULL;
  sf_precoeff1_6 = sf_precoeff2_6 = sf_precoeff3_6 = sf_precoeff4_6 = 
    sf_precoeff5_6 = sf_precoeff6_6 = NULL;

  gf_b = NULL;
  gf_b_6 = NULL;
  rho1d = rho_coeff = NULL;
  drho1d = drho_coeff = NULL;
  rho1d_6 = rho_coeff_6 = NULL;
  drho1d_6 = drho_coeff_6 = NULL;
  fft1 = fft2 = NULL;
  fft1_6 = fft2_6 = NULL;
  remap = NULL;
  remap_6 = NULL;

  nmax = 0;
  part2grid = NULL;
  part2grid_6 = NULL;

  splitbuf1 = NULL;
  splitbuf2 = NULL;
  dict_send = NULL;
  dict_rec = NULL;
  com_each = NULL;
  com_order = NULL;
  split_1 = NULL;
  split_2 = NULL;
}

/* ----------------------------------------------------------------------
   free all memory 
------------------------------------------------------------------------- */

PPPMDisp::~PPPMDisp()
{
  delete [] factors;
  delete [] B;
  delete [] cii;
  delete [] csumi;
  deallocate();
  deallocate_peratom();
  memory->destroy(part2grid);
  memory->destroy(part2grid_6);
  memory->destroy(com_order);
  memory->destroy(com_each);
  memory->destroy(dict_send);
  memory->destroy(dict_rec);
  memory->destroy(splitbuf1);
  memory->destroy(splitbuf2);
}

/* ----------------------------------------------------------------------
   called once before run 
------------------------------------------------------------------------- */

void PPPMDisp::init()
{
  if (me == 0) {
    if (screen) fprintf(screen,"PPPMDisp initialization ...\n");
    if (logfile) fprintf(logfile,"PPPMDisp initialization ...\n");
  }

  if (domain->triclinic)
    error->all(FLERR,"Cannot (yet) use PPPMDisp with triclinic box");
  if (domain->dimension == 2)
    error->all(FLERR,"Cannot use PPPMDisp with 2d simulation");

  if (slabflag == 0 && domain->nonperiodic > 0)
    error->all(FLERR,"Cannot use nonperiodic boundaries with PPPMDisp");
  if (slabflag == 1) {
    if (domain->xperiodic != 1 || domain->yperiodic != 1 || 
	domain->boundary[2][0] != 1 || domain->boundary[2][1] != 1)
      error->all(FLERR,"Incorrect boundaries with slab PPPMDisp");
  }
 
  if (order > MAXORDER || order_6 > MAXORDER) {
    char str[128];
    sprintf(str,"PPPMDisp coulomb order cannot be greater than %d",MAXORDER);
    error->all(FLERR,str);
  }

  // free all arrays previously allocated

  deallocate();
  deallocate_peratom(); 

  // set scale

  scale = 1.0;

  triclinic = domain->triclinic;

  // check whether cutoff and pair style are set

  pair_check();

  int tmp;
  Pair *pair = force->pair;
  int *ptr = pair ? (int *) pair->extract("ewald_order",tmp) : NULL;
  double *p_cutoff = pair ? (double *) pair->extract("cut_coul",tmp) : NULL;
  double *p_cutoff_lj = pair ? (double *) pair->extract("cut_LJ",tmp) : NULL;
  if (!(ptr||*p_cutoff||*p_cutoff_lj)) 
    error->all(FLERR,"KSpace style is incompatible with Pair style");
  cutoff = *p_cutoff;
  cutoff_lj = *p_cutoff_lj;

  double tmp2;
  MPI_Allreduce(&cutoff, &tmp2,1,MPI_DOUBLE,MPI_SUM,world); 

  // check out which types of potentials will have to be calculated

  int ewald_order = ptr ? *((int *) ptr) : 1<<1;
  int ewald_mix = ptr ? *((int *) pair->extract("ewald_mix",tmp)) : GEOMETRIC;
  memset(function, 0, EWALD_FUNCS*sizeof(int));
  for (int i=0; i<=EWALD_MAXORDER; ++i)			// transcribe order
    if (ewald_order&(1<<i)) {				// from pair_style
      int  k;
      char str[128];
      switch (i) {
	case 1:
	  k = 0; break;
	case 6:
	  if (ewald_mix==GEOMETRIC) { k = 1; break; }
	  else if (ewald_mix==ARITHMETIC) { k = 2; break; }
	  sprintf(str, "Unsupported mixing rule in kspace_style pppm/disp for pair_style %s", force->pair_style);
	  error->all(FLERR,str);
	default:
	  sprintf(str, "Unsupported order in kspace_style pppm/disp pair_style %s", force->pair_style);
	  error->all(FLERR,str);
      }
      function[k] = 1;
    }
 

  // warn, if function[0] is not set but charge attribute is set!
  if (!function[0] && atom->q_flag && me == 0) {
    char str[128];
    sprintf(str, "Charges are set, but coulombic long-range solver is not used");
    error->warning(FLERR, str);
  }

  // compute qsum & qsqsum, if function[0] is set, print error if no charges are set or warn if not charge-neutral  
 
  if (function[0]) {
    if (!atom->q_flag) error->all(FLERR,"Kspace style with selected options requires atom attribute q");
 
    qsum = qsqsum = 0.0;
    for (int i = 0; i < atom->nlocal; i++) {
      qsum += atom->q[i];
      qsqsum += atom->q[i]*atom->q[i];

    }

    double tmp;
    MPI_Allreduce(&qsum,&tmp,1,MPI_DOUBLE,MPI_SUM,world);
    qsum = tmp;
    MPI_Allreduce(&qsqsum,&tmp,1,MPI_DOUBLE,MPI_SUM,world);
    qsqsum = tmp;

    if (qsqsum == 0.0)
      error->all(FLERR,"Cannot use kspace solver with selected options on system with no charge");
    if (fabs(qsum) > SMALL && me == 0) {
      char str[128];
      sprintf(str,"System is not charge neutral, net charge = %g",qsum);
      error->warning(FLERR,str);
    }
  }

  // if kspace is TIP4P, extract TIP4P params from pair style
  // bond/angle are not yet init(), so insure equilibrium request is valid

  qdist = 0.0;
 
  if (tip4pflag) {
    int itmp;
    double *p_qdist = (double *) force->pair->extract("qdist",itmp);
    int *p_typeO = (int *) force->pair->extract("typeO",itmp);
    int *p_typeH = (int *) force->pair->extract("typeH",itmp);
    int *p_typeA = (int *) force->pair->extract("typeA",itmp);
    int *p_typeB = (int *) force->pair->extract("typeB",itmp);
    if (!p_qdist || !p_typeO || !p_typeH || !p_typeA || !p_typeB)
      error->all(FLERR,"KSpace style is incompatible with Pair style");
    qdist = *p_qdist;
    typeO = *p_typeO;
    typeH = *p_typeH;
    int typeA = *p_typeA;
    int typeB = *p_typeB;

    if (force->angle == NULL || force->bond == NULL)
      error->all(FLERR,"Bond and angle potentials must be defined for TIP4P");
    if (typeA < 1 || typeA > atom->nangletypes || 
	force->angle->setflag[typeA] == 0)
      error->all(FLERR,"Bad TIP4P angle type for PPPMDisp/TIP4P");
    if (typeB < 1 || typeB > atom->nbondtypes || 
	force->bond->setflag[typeB] == 0)
      error->all(FLERR,"Bad TIP4P bond type for PPPMDisp/TIP4P");
    double theta = force->angle->equilibrium_angle(typeA);
    double blen = force->bond->equilibrium_distance(typeB);
    alpha = qdist / (cos(0.5*theta) * blen);
  }


  // initialize the pair style to get the coefficients

  pair->init();
  init_coeffs();

  //if g_ewald and g_ewald_6 have not been specified, set some initial value
  //  to avoid problems when calculating the energies!

  if (!gewaldflag) g_ewald = 1;
  if (!gewaldflag_6) g_ewald_6 = 1;

  // set accuracy (force units) from accuracy_relative or accuracy_absolute
  
  if (accuracy_absolute >= 0.0) accuracy = accuracy_absolute;
  else accuracy = accuracy_relative * two_charge_force;

  int iteration = 0;
  if (function[0]) {
    while (order > 0) {

      if (iteration && me == 0)
          error->warning(FLERR,"Reducing PPPMDisp Coulomb order b/c stencil extends "
			 "beyond neighbor processor.");
      iteration++;

      // set grid for dispersion interaction and coulomb interactions!
 
      set_grid();

      if (nx_pppm >= OFFSET || ny_pppm >= OFFSET || nz_pppm >= OFFSET)
      error->all(FLERR,"PPPMDisp Coulomb grid is too large");

      set_fft_parameters(nx_pppm, ny_pppm, nz_pppm,
                         nxlo_fft, nylo_fft, nzlo_fft,
                         nxhi_fft, nyhi_fft, nzhi_fft,
                         nxlo_in, nylo_in, nzlo_in,
                         nxhi_in, nyhi_in, nzhi_in,
                         nxlo_out, nylo_out, nzlo_out,
                         nxhi_out, nyhi_out, nzhi_out,
                         nxlo_ghost, nylo_ghost, nzlo_ghost,
                         nxhi_ghost, nyhi_ghost, nzhi_ghost,
                         nlower, nupper,
                         ngrid, nfft, nfft_both, nbuf, nbuf_pa,
                         shift, shiftone, order);

      int flag = 0;
      if (nxlo_ghost > nxhi_in-nxlo_in+1) flag = 1;
      if (nxhi_ghost > nxhi_in-nxlo_in+1) flag = 1;
      if (nylo_ghost > nyhi_in-nylo_in+1) flag = 1;
      if (nyhi_ghost > nyhi_in-nylo_in+1) flag = 1;
      if (nzlo_ghost > nzhi_in-nzlo_in+1) flag = 1;
      if (nzhi_ghost > nzhi_in-nzlo_in+1) flag = 1;

      int flag_all;
      MPI_Allreduce(&flag,&flag_all,1,MPI_INT,MPI_SUM,world);

      if (flag_all == 0) break;
      order--;
    }

    if (order == 1)
      error->all(FLERR,"Coulomb PPPMDisp order has been reduced to 1");

    // adjust g_ewald
  
    if (!gewaldflag) adjust_gewald();

    // calculate the final accuracy
  
    double acc = final_accuracy();
  
    // print stats

    int ngrid_max,nfft_both_max,nbuf_max;
    MPI_Allreduce(&ngrid,&ngrid_max,1,MPI_INT,MPI_MAX,world);
    MPI_Allreduce(&nfft_both,&nfft_both_max,1,MPI_INT,MPI_MAX,world);
    MPI_Allreduce(&nbuf,&nbuf_max,1,MPI_INT,MPI_MAX,world);

    if (me == 0) {
    #ifdef FFT_SINGLE
      const char fft_prec[] = "single";
    #else
      const char fft_prec[] = "double";
    #endif
  
      if (screen) {
        fprintf(screen,"  Coulomb G vector (1/distance)= %g\n",g_ewald);
        fprintf(screen,"  Coulomb grid = %d %d %d\n",nx_pppm,ny_pppm,nz_pppm);
        fprintf(screen,"  Coulomb stencil order = %d\n",order);
        fprintf(screen,"  Coulomb estimated absolute RMS force accuracy = %g\n",
                acc);
        fprintf(screen,"  Coulomb estimated relative force accuracy = %g\n",
                acc/two_charge_force);
        fprintf(screen,"  using %s precision FFTs\n",fft_prec);
        fprintf(screen,"  Coulomb brick FFT buffer size/proc = %d %d %d\n",
                          ngrid_max,nfft_both_max,nbuf_max);
      }
      if (logfile) {
        fprintf(logfile,"  Coulomb G vector (1/distance) = %g\n",g_ewald);
        fprintf(logfile,"  Coulomb grid = %d %d %d\n",nx_pppm,ny_pppm,nz_pppm);
        fprintf(logfile,"  Coulomb stencil order = %d\n",order);
        fprintf(logfile,"  Coulomb estimated absolute RMS force accuracy = %g\n",
                acc);
        fprintf(logfile,"  Coulomb estimated relative force accuracy = %g\n",
                acc/two_charge_force);
        fprintf(logfile,"  using %s precision FFTs\n",fft_prec);
        fprintf(logfile,"  Coulomb brick FFT buffer size/proc = %d %d %d\n",
                           ngrid_max,nfft_both_max,nbuf_max);
      }
    }
  }

  iteration = 0;
  if (function[1] + function[2]) {
   while (order_6 > 0) {

      if (iteration && me == 0)
          error->warning(FLERR,"Reducing PPPMDisp Dispersion order b/c stencil extends "
  		     "beyond neighbor processor");
      iteration++;

      set_grid_6();
   
      if (nx_pppm_6 >= OFFSET || ny_pppm_6 >= OFFSET || nz_pppm_6 >= OFFSET)
      error->all(FLERR,"PPPMDisp Dispersion grid is too large");

      set_fft_parameters(nx_pppm_6, ny_pppm_6, nz_pppm_6,
                         nxlo_fft_6, nylo_fft_6, nzlo_fft_6,
                         nxhi_fft_6, nyhi_fft_6, nzhi_fft_6,
                         nxlo_in_6, nylo_in_6, nzlo_in_6,
                         nxhi_in_6, nyhi_in_6, nzhi_in_6,
                         nxlo_out_6, nylo_out_6, nzlo_out_6,
                         nxhi_out_6, nyhi_out_6, nzhi_out_6,
                         nxlo_ghost_6, nylo_ghost_6, nzlo_ghost_6,
                         nxhi_ghost_6, nyhi_ghost_6, nzhi_ghost_6,
                         nlower_6, nupper_6,
                         ngrid_6, nfft_6, nfft_both_6, nbuf_6, nbuf_pa_6,
                         shift_6, shiftone_6, order_6);

      int flag = 0;
      if (nxlo_ghost_6 > nxhi_in_6-nxlo_in_6+1) flag = 1;
      if (nxhi_ghost_6 > nxhi_in_6-nxlo_in_6+1) flag = 1;
      if (nylo_ghost_6 > nyhi_in_6-nylo_in_6+1) flag = 1;
      if (nyhi_ghost_6 > nyhi_in_6-nylo_in_6+1) flag = 1;
      if (nzlo_ghost_6 > nzhi_in_6-nzlo_in_6+1) flag = 1;
      if (nzhi_ghost_6 > nzhi_in_6-nzlo_in_6+1) flag = 1;

      int flag_all;
      MPI_Allreduce(&flag,&flag_all,1,MPI_INT,MPI_SUM,world);

      if (flag_all == 0) break;
      order_6--;
    }

    if (order_6 == 0) error->all(FLERR,"Dispersion PPPMDisp order has been reduced to 0");

    // adjust g_ewald_6

    if (!gewaldflag_6) adjust_gewald_6();

    // calculate the final accuracy

    double acc = final_accuracy_6();


    // print stats

    int ngrid_max,nfft_both_max,nbuf_max;
    MPI_Allreduce(&ngrid_6,&ngrid_max,1,MPI_INT,MPI_MAX,world);
    MPI_Allreduce(&nfft_both_6,&nfft_both_max,1,MPI_INT,MPI_MAX,world);
    MPI_Allreduce(&nbuf_6,&nbuf_max,1,MPI_INT,MPI_MAX,world);

    if (me == 0) {
    #ifdef FFT_SINGLE
      const char fft_prec[] = "single";
    #else
      const char fft_prec[] = "double";
    #endif
  
      if (screen) {
        fprintf(screen,"  Dispersion G vector (1/distance)= %g\n",g_ewald_6);
        fprintf(screen,"  Dispersion grid = %d %d %d\n",nx_pppm_6,ny_pppm_6,nz_pppm_6);
        fprintf(screen,"  Dispersion stencil order = %d\n",order_6);
        fprintf(screen,"  Dispersion estimated absolute RMS force accuracy = %g\n",
                acc);
        fprintf(screen,"  Dispersion estimated relative force accuracy = %g\n",
                acc/two_charge_force);
        fprintf(screen,"  using %s precision FFTs\n",fft_prec);
        fprintf(screen,"  Dispersion brick FFT buffer size/proc = %d %d %d\n",
                          ngrid_max,nfft_both_max,nbuf_max);
      }
      if (logfile) {
        fprintf(logfile,"  Dispersion G vector (1/distance) = %g\n",g_ewald_6);
        fprintf(logfile,"  Dispersion grid = %d %d %d\n",nx_pppm_6,ny_pppm_6,nz_pppm_6);
        fprintf(logfile,"  Dispersion stencil order = %d\n",order_6);
        fprintf(logfile,"  Dispersion estimated absolute RMS force accuracy = %g\n",
                acc);
        fprintf(logfile,"  Disperion estimated relative force accuracy = %g\n",
                acc/two_charge_force);
        fprintf(logfile,"  using %s precision FFTs\n",fft_prec);
        fprintf(logfile,"  Dispersion brick FFT buffer size/proc = %d %d %d\n",
                           ngrid_max,nfft_both_max,nbuf_max);
      }
    }
  }
 
  // prepare the splitting of the Fourier Transformed vectors
  if (function[2]) prepare_splitting();

  // allocate K-space dependent memory

  allocate();

  // pre-compute Green's function denomiator expansion
  // pre-compute 1d charge distribution coefficients
  if (function[0]) {
    compute_gf_denom(gf_b, order);
    compute_rho_coeff(rho_coeff, drho_coeff, order);
    if (differentiation_flag == 1)
      compute_sf_precoeff(nx_pppm, ny_pppm, nz_pppm, order,
                          nxlo_fft, nylo_fft, nzlo_fft, 
                          nxhi_fft, nyhi_fft, nzhi_fft,
                          sf_precoeff1, sf_precoeff2, sf_precoeff3,
                          sf_precoeff4, sf_precoeff5, sf_precoeff6);
  }
  if (function[1] + function[2]) {
    compute_gf_denom(gf_b_6, order_6);
    compute_rho_coeff(rho_coeff_6, drho_coeff_6, order_6);
    if (differentiation_flag == 1)
      compute_sf_precoeff(nx_pppm_6, ny_pppm_6, nz_pppm_6, order_6,
                          nxlo_fft_6, nylo_fft_6, nzlo_fft_6, 
                          nxhi_fft_6, nyhi_fft_6, nzhi_fft_6,
                          sf_precoeff1_6, sf_precoeff2_6, sf_precoeff3_6,
                          sf_precoeff4_6, sf_precoeff5_6, sf_precoeff6_6);
  }

}

/* ----------------------------------------------------------------------
   adjust PPPM coeffs, called initially and whenever volume has changed 
------------------------------------------------------------------------- */

void PPPMDisp::setup()
{
  double *prd;

  // volume-dependent factors
  // adjust z dimension for 2d slab PPPM
  // z dimension for 3d PPPM is zprd since slab_volfactor = 1.0

  if (triclinic == 0) prd = domain->prd;
  else prd = domain->prd_lamda;

  double xprd = prd[0];
  double yprd = prd[1];
  double zprd = prd[2];
  double zprd_slab = zprd*slab_volfactor;
  volume = xprd * yprd * zprd_slab;

 // compute fkx,fky,fkz for my FFT grid pts

  double unitkx = (2.0*MY_PI/xprd);
  double unitky = (2.0*MY_PI/yprd);
  double unitkz = (2.0*MY_PI/zprd_slab);

  //compute the virial coefficients and green functions
  if (function[0]){

    delxinv = nx_pppm/xprd;
    delyinv = ny_pppm/yprd;
    delzinv = nz_pppm/zprd_slab;

    delvolinv = delxinv*delyinv*delzinv;

    double per;
    int i, j, k, n;

    for (i = nxlo_fft; i <= nxhi_fft; i++) {
      per = i - nx_pppm*(2*i/nx_pppm);
      fkx[i] = unitkx*per;
      j = (nx_pppm - i) % nx_pppm;
      per = j - nx_pppm*(2*j/nx_pppm);
      fkx2[i] = unitkx*per;
    }

    for (i = nylo_fft; i <= nyhi_fft; i++) {
      per = i - ny_pppm*(2*i/ny_pppm);
      fky[i] = unitky*per;
      j = (ny_pppm - i) % ny_pppm;
      per = j - ny_pppm*(2*j/ny_pppm);
      fky2[i] = unitky*per;
    }

    for (i = nzlo_fft; i <= nzhi_fft; i++) {
      per = i - nz_pppm*(2*i/nz_pppm);
      fkz[i] = unitkz*per;
      j = (nz_pppm - i) % nz_pppm;
      per = j - nz_pppm*(2*j/nz_pppm);
      fkz2[i] = unitkz*per;
    }

    double sqk,vterm;
    double gew2inv = 1/(g_ewald*g_ewald);
    n = 0;
    for (k = nzlo_fft; k <= nzhi_fft; k++) {
      for (j = nylo_fft; j <= nyhi_fft; j++) {
        for (i = nxlo_fft; i <= nxhi_fft; i++) {
	  sqk = fkx[i]*fkx[i] + fky[j]*fky[j] + fkz[k]*fkz[k];
	  if (sqk == 0.0) {
	    vg[n][0] = 0.0;
	    vg[n][1] = 0.0;
	    vg[n][2] = 0.0;
	    vg[n][3] = 0.0;
	    vg[n][4] = 0.0;
	    vg[n][5] = 0.0;
	  } else {
	    vterm = -2.0 * (1.0/sqk + 0.25*gew2inv);
	    vg[n][0] = 1.0 + vterm*fkx[i]*fkx[i];
	    vg[n][1] = 1.0 + vterm*fky[j]*fky[j];
	    vg[n][2] = 1.0 + vterm*fkz[k]*fkz[k];
	    vg[n][3] = vterm*fkx[i]*fky[j];
	    vg[n][4] = vterm*fkx[i]*fkz[k];
	    vg[n][5] = vterm*fky[j]*fkz[k];
            vg2[n][0] = vterm*0.5*(fkx[i]*fky[j] + fkx2[i]*fky2[j]);
            vg2[n][1] = vterm*0.5*(fkx[i]*fkz[k] + fkx2[i]*fkz2[k]);
            vg2[n][2] = vterm*0.5*(fky[j]*fkz[k] + fky2[j]*fkz2[k]);
  	  }
	  n++;
        }
      }
    }
    compute_gf();
    if (differentiation_flag == 1) compute_sf_coeff();
  }

 if (function[1] + function[2]) {
    delxinv_6 = nx_pppm_6/xprd;
    delyinv_6 = ny_pppm_6/yprd;
    delzinv_6 = nz_pppm_6/zprd_slab;
    delvolinv_6 = delxinv_6*delyinv_6*delzinv_6;

    double per;
    int i, j, k, n;
    for (i = nxlo_fft_6; i <= nxhi_fft_6; i++) {
      per = i - nx_pppm_6*(2*i/nx_pppm_6);
      fkx_6[i] = unitkx*per;
      j = (nx_pppm_6 - i) % nx_pppm_6;
      per = j - nx_pppm_6*(2*j/nx_pppm_6);
      fkx2_6[i] = unitkx*per;
    }
    for (i = nylo_fft_6; i <= nyhi_fft_6; i++) {
      per = i - ny_pppm_6*(2*i/ny_pppm_6);
      fky_6[i] = unitky*per;
      j = (ny_pppm_6 - i) % ny_pppm_6;
      per = j - ny_pppm_6*(2*j/ny_pppm_6);
      fky2_6[i] = unitky*per;
    }
    for (i = nzlo_fft_6; i <= nzhi_fft_6; i++) {
      per = i - nz_pppm_6*(2*i/nz_pppm_6);
      fkz_6[i] = unitkz*per;
      j = (nz_pppm_6 - i) % nz_pppm_6;
      per = j - nz_pppm_6*(2*j/nz_pppm_6);
      fkz2_6[i] = unitkz*per;
    }
    double sqk,vterm;
    long double erft, expt,nom, denom;
    long double b, bs, bt;
    double rtpi = sqrt(MY_PI);
    double gewinv = 1/g_ewald_6;
    n = 0;
    for (k = nzlo_fft_6; k <= nzhi_fft_6; k++) {
      for (j = nylo_fft_6; j <= nyhi_fft_6; j++) {
        for (i = nxlo_fft_6; i <= nxhi_fft_6; i++) {
	  sqk = fkx_6[i]*fkx_6[i] + fky_6[j]*fky_6[j] + fkz_6[k]*fkz_6[k];
	  if (sqk == 0.0) {
	    vg_6[n][0] = 0.0;
	    vg_6[n][1] = 0.0;
	    vg_6[n][2] = 0.0;
	    vg_6[n][3] = 0.0;
	    vg_6[n][4] = 0.0;
	    vg_6[n][5] = 0.0;
	  } else {
            b = 0.5*sqrt(sqk)*gewinv;
            bs = b*b;
            bt = bs*b;
            erft = 2*bt*rtpi*erfc(b);
            expt = exp(-bs);
            nom = erft - 2*bs*expt;
            denom = nom + expt;
            if (denom == 0) vterm = 3.0/sqk;
            else vterm = 3.0*nom/(sqk*denom);
	    vg_6[n][0] = 1.0 + vterm*fkx_6[i]*fkx_6[i];
	    vg_6[n][1] = 1.0 + vterm*fky_6[j]*fky_6[j];
	    vg_6[n][2] = 1.0 + vterm*fkz_6[k]*fkz_6[k];
	    vg_6[n][3] = vterm*fkx_6[i]*fky_6[j];
	    vg_6[n][4] = vterm*fkx_6[i]*fkz_6[k];
	    vg_6[n][5] = vterm*fky_6[j]*fkz_6[k];
            vg2_6[n][0] = vterm*0.5*(fkx_6[i]*fky_6[j] + fkx2_6[i]*fky2_6[j]);
            vg2_6[n][1] = vterm*0.5*(fkx_6[i]*fkz_6[k] + fkx2_6[i]*fkz2_6[k]);
            vg2_6[n][2] = vterm*0.5*(fky_6[j]*fkz_6[k] + fky2_6[j]*fkz2_6[k]);
	  }
	  n++;
        }
      }
    }
    compute_gf_6();
    if (differentiation_flag == 1) compute_sf_coeff_6();
  }
}

/* ----------------------------------------------------------------------
   compute the PPPM long-range force, energy, virial 
------------------------------------------------------------------------- */

void PPPMDisp::compute(int eflag, int vflag)
{

  int i;
  // convert atoms from box to lamda coords

  if (eflag || vflag) ev_setup(eflag,vflag);
  else evflag = evflag_atom = eflag_global = vflag_global = 
	 eflag_atom = vflag_atom = 0;
  
  if (evflag_atom && !peratom_allocate_flag) {
    allocate_peratom();
    peratom_allocate_flag = 1;
  }
  
  if (triclinic == 0) boxlo = domain->boxlo;
  else {
    boxlo = domain->boxlo_lamda;
    domain->x2lamda(atom->nlocal);
  }
  // extend size of per-atom arrays if necessary

  if (atom->nlocal > nmax) {

    if (function[0]) memory->destroy(part2grid);
    if (function[1] + function[2]) memory->destroy(part2grid_6);
    nmax = atom->nmax;
    if (function[0]) memory->create(part2grid,nmax,3,"pppm/disp:part2grid");
    if (function[1] + function[2]) memory->create(part2grid_6,nmax,3,"pppm/disp:part2grid_6");
  }

  energy = 0.0;
  energy_1 = 0.0;
  energy_6 = 0.0;
  if (vflag) for (i = 0; i < 6; i++) virial_6[i] = virial_1[i] = 0.0;
  // find grid points for all my particles
  // distribute partcles' charges/dispersion coefficients on the grid
  // communication between processors and remapping two fft
  // Solution of poissons equation in k-space and backtransformation
  // communication between processors
  // calculation of forces
  if (function[0]) {

    //perfrom calculations for coulomb interactions only
    particle_map_c(delxinv, delyinv, delzinv, shift, part2grid, nupper, nlower,
                 nxlo_out, nylo_out, nzlo_out, nxhi_out, nyhi_out, nzhi_out);
    make_rho_c();
    brick2fft(nxlo_out, nylo_out, nzlo_out, nxhi_out, nyhi_out, nzhi_out,
              nxlo_in, nylo_in, nzlo_in, nxhi_in, nyhi_in, nzhi_in,
              nxlo_ghost, nylo_ghost, nzlo_ghost, nxhi_ghost, nyhi_ghost, nzhi_ghost,
	      density_brick, density_fft, work1, buf1, buf2, nbuf, remap); 
    if (differentiation_flag == 1) {
      poisson_ad(work1, work2, density_fft, fft1, fft2,
                 nx_pppm, ny_pppm, nz_pppm, nfft,
                 nxlo_fft, nylo_fft, nzlo_fft, nxhi_fft, nyhi_fft, nzhi_fft,
                 nxlo_in, nylo_in, nzlo_in, nxhi_in, nyhi_in, nzhi_in,
                 energy_1, greensfn, 
                 virial_1, vg,vg2,
                 u_brick, v0_brick, v1_brick, v2_brick, v3_brick, v4_brick, v5_brick);

      fillbrick_ad(nxlo_out, nylo_out, nzlo_out, nxhi_out, nyhi_out, nzhi_out,
                   nxlo_in, nylo_in, nzlo_in, nxhi_in, nyhi_in, nzhi_in,
                   nxlo_ghost, nylo_ghost, nzlo_ghost, nxhi_ghost, nyhi_ghost, nzhi_ghost,
                   buf1, buf2, nbuf, u_brick);
 
      fieldforce_c_ad(); 

      if (vflag_atom) fillbrick_peratom_ad(nxlo_out, nylo_out, nzlo_out, nxhi_out, nyhi_out, nzhi_out,
              nxlo_in, nylo_in, nzlo_in, nxhi_in, nyhi_in, nzhi_in,
              nxlo_ghost, nylo_ghost, nzlo_ghost, nxhi_ghost, nyhi_ghost, nzhi_ghost,
	      buf3, buf4, nbuf_pa, v0_brick, v1_brick, v2_brick,
	      v3_brick, v4_brick, v5_brick);

    } else {
      poisson_ik(work1, work2, density_fft, fft1, fft2,
                 nx_pppm, ny_pppm, nz_pppm, nfft,
                 nxlo_fft, nylo_fft, nzlo_fft, nxhi_fft, nyhi_fft, nzhi_fft,
                 nxlo_in, nylo_in, nzlo_in, nxhi_in, nyhi_in, nzhi_in,
                 energy_1, greensfn, 
	         fkx, fky, fkz,fkx2, fky2, fkz2,
                 vdx_brick, vdy_brick, vdz_brick, virial_1, vg,vg2,
                 u_brick, v0_brick, v1_brick, v2_brick, v3_brick, v4_brick, v5_brick);

      fillbrick_ik(nxlo_out, nylo_out, nzlo_out, nxhi_out, nyhi_out, nzhi_out,
                    nxlo_in, nylo_in, nzlo_in, nxhi_in, nyhi_in, nzhi_in,
                    nxlo_ghost, nylo_ghost, nzlo_ghost, nxhi_ghost, nyhi_ghost, nzhi_ghost,
                    buf1, buf2, nbuf, vdx_brick, vdy_brick, vdz_brick);

      fieldforce_c_ik(); 

      if (evflag_atom) fillbrick_peratom_ik(nxlo_out, nylo_out, nzlo_out, nxhi_out, nyhi_out, nzhi_out,
              nxlo_in, nylo_in, nzlo_in, nxhi_in, nyhi_in, nzhi_in,
              nxlo_ghost, nylo_ghost, nzlo_ghost, nxhi_ghost, nyhi_ghost, nzhi_ghost,
	      buf3, buf4, nbuf_pa, u_brick, v0_brick, v1_brick, v2_brick,
	      v3_brick, v4_brick, v5_brick);
    }
    if (evflag_atom) fieldforce_c_peratom();
  }

  if (function[1]) {
    //perfrom calculations for geometric mixing
    particle_map(delxinv_6, delyinv_6, delzinv_6, shift_6, part2grid_6, nupper_6, nlower_6,
                 nxlo_out_6, nylo_out_6, nzlo_out_6, nxhi_out_6, nyhi_out_6, nzhi_out_6);
    make_rho_g();
    brick2fft(nxlo_out_6, nylo_out_6, nzlo_out_6, nxhi_out_6, nyhi_out_6, nzhi_out_6,
              nxlo_in_6, nylo_in_6, nzlo_in_6, nxhi_in_6, nyhi_in_6, nzhi_in_6,
              nxlo_ghost_6, nylo_ghost_6, nzlo_ghost_6, nxhi_ghost_6, nyhi_ghost_6, nzhi_ghost_6,
	      density_brick_g, density_fft_g, work1_6, buf1_6, buf2_6, nbuf_6, remap_6);

    if (differentiation_flag == 1) {

      poisson_ad(work1_6, work2_6, density_fft_g, fft1_6, fft2_6,
                 nx_pppm_6, ny_pppm_6, nz_pppm_6, nfft_6,
                 nxlo_fft_6, nylo_fft_6, nzlo_fft_6, nxhi_fft_6, nyhi_fft_6, nzhi_fft_6,
                 nxlo_in_6, nylo_in_6, nzlo_in_6, nxhi_in_6, nyhi_in_6, nzhi_in_6,
                 energy_6, greensfn_6, 
                 virial_6, vg_6, vg2_6,
                 u_brick_g, v0_brick_g, v1_brick_g, v2_brick_g, v3_brick_g, v4_brick_g, v5_brick_g);

      fillbrick_ad(nxlo_out_6, nylo_out_6, nzlo_out_6, nxhi_out_6, nyhi_out_6, nzhi_out_6,
                   nxlo_in_6, nylo_in_6, nzlo_in_6, nxhi_in_6, nyhi_in_6, nzhi_in_6,
                   nxlo_ghost_6, nylo_ghost_6, nzlo_ghost_6, nxhi_ghost_6, nyhi_ghost_6, nzhi_ghost_6,
                   buf1_6, buf2_6, nbuf_6, u_brick_g);

      fieldforce_g_ad();

      if (vflag_atom) fillbrick_peratom_ad(nxlo_out_6, nylo_out_6, nzlo_out_6, nxhi_out_6, nyhi_out_6, nzhi_out_6,
              nxlo_in_6, nylo_in_6, nzlo_in_6, nxhi_in_6, nyhi_in_6, nzhi_in_6,
              nxlo_ghost_6, nylo_ghost_6, nzlo_ghost_6, nxhi_ghost_6, nyhi_ghost_6, nzhi_ghost_6,
	      buf3_6, buf4_6, nbuf_pa_6, v0_brick_g, v1_brick_g, v2_brick_g,
	      v3_brick_g, v4_brick_g, v5_brick_g);
    } else {
      poisson_ik(work1_6, work2_6, density_fft_g, fft1_6, fft2_6,
                 nx_pppm_6, ny_pppm_6, nz_pppm_6, nfft_6,
                 nxlo_fft_6, nylo_fft_6, nzlo_fft_6, nxhi_fft_6, nyhi_fft_6, nzhi_fft_6,
                 nxlo_in_6, nylo_in_6, nzlo_in_6, nxhi_in_6, nyhi_in_6, nzhi_in_6,
                 energy_6, greensfn_6, 
	         fkx_6, fky_6, fkz_6,fkx2_6, fky2_6, fkz2_6,
                 vdx_brick_g, vdy_brick_g, vdz_brick_g, virial_6, vg_6, vg2_6,
                 u_brick_g, v0_brick_g, v1_brick_g, v2_brick_g, v3_brick_g, v4_brick_g, v5_brick_g);

      fillbrick_ik(nxlo_out_6, nylo_out_6, nzlo_out_6, nxhi_out_6, nyhi_out_6, nzhi_out_6,
                   nxlo_in_6, nylo_in_6, nzlo_in_6, nxhi_in_6, nyhi_in_6, nzhi_in_6,
                   nxlo_ghost_6, nylo_ghost_6, nzlo_ghost_6, nxhi_ghost_6, nyhi_ghost_6, nzhi_ghost_6,
                   buf1_6, buf2_6, nbuf_6, vdx_brick_g, vdy_brick_g, vdz_brick_g);

      fieldforce_g_ik();

      if (evflag_atom) fillbrick_peratom_ik(nxlo_out_6, nylo_out_6, nzlo_out_6, nxhi_out_6, nyhi_out_6, nzhi_out_6,
              nxlo_in_6, nylo_in_6, nzlo_in_6, nxhi_in_6, nyhi_in_6, nzhi_in_6,
              nxlo_ghost_6, nylo_ghost_6, nzlo_ghost_6, nxhi_ghost_6, nyhi_ghost_6, nzhi_ghost_6,
	      buf3_6, buf4_6, nbuf_pa_6, u_brick_g, v0_brick_g, v1_brick_g, v2_brick_g,
	      v3_brick_g, v4_brick_g, v5_brick_g);
    }
    if (evflag_atom) fieldforce_g_peratom();
  }

  if (function[2]) {
    //perform calculations for arithmetic mixing
    particle_map(delxinv_6, delyinv_6, delzinv_6, shift_6, part2grid_6, nupper_6, nlower_6,
                 nxlo_out_6, nylo_out_6, nzlo_out_6, nxhi_out_6, nyhi_out_6, nzhi_out_6);
    make_rho_a();
    brick2fft_a();

    if ( differentiation_flag == 1) {

      poisson_ad(work1_6, work2_6, density_fft_a3, fft1_6, fft2_6,
                 nx_pppm_6, ny_pppm_6, nz_pppm_6, nfft_6,
                 nxlo_fft_6, nylo_fft_6, nzlo_fft_6, nxhi_fft_6, nyhi_fft_6, nzhi_fft_6,
                 nxlo_in_6, nylo_in_6, nzlo_in_6, nxhi_in_6, nyhi_in_6, nzhi_in_6,
                 energy_6, greensfn_6, 
                 virial_6, vg_6, vg2_6,
                 u_brick_a3, v0_brick_a3, v1_brick_a3, v2_brick_a3, v3_brick_a3, v4_brick_a3, v5_brick_a3);
      poisson_2s_ad(density_fft_a0, density_fft_a6,
                    u_brick_a0, v0_brick_a0, v1_brick_a0, v2_brick_a0, v3_brick_a0, v4_brick_a0, v5_brick_a0,
                    u_brick_a6, v0_brick_a6, v1_brick_a6, v2_brick_a6, v3_brick_a6, v4_brick_a6, v5_brick_a6);
      poisson_2s_ad(density_fft_a1, density_fft_a5,
                    u_brick_a1, v0_brick_a1, v1_brick_a1, v2_brick_a1, v3_brick_a1, v4_brick_a1, v5_brick_a1,
                    u_brick_a5, v0_brick_a5, v1_brick_a5, v2_brick_a5, v3_brick_a5, v4_brick_a5, v5_brick_a5);
      poisson_2s_ad(density_fft_a2, density_fft_a4,
                    u_brick_a2, v0_brick_a2, v1_brick_a2, v2_brick_a2, v3_brick_a2, v4_brick_a2, v5_brick_a2,
                    u_brick_a4, v0_brick_a4, v1_brick_a4, v2_brick_a4, v3_brick_a4, v4_brick_a4, v5_brick_a4);

      fillbrick_a_ad();

      fieldforce_a_ad();

      if (evflag_atom) fillbrick_a_peratom_ad();

    }  else {
    
      poisson_ik(work1_6, work2_6, density_fft_a3, fft1_6, fft2_6,
                 nx_pppm_6, ny_pppm_6, nz_pppm_6, nfft_6,
                 nxlo_fft_6, nylo_fft_6, nzlo_fft_6, nxhi_fft_6, nyhi_fft_6, nzhi_fft_6,
                 nxlo_in_6, nylo_in_6, nzlo_in_6, nxhi_in_6, nyhi_in_6, nzhi_in_6,
                 energy_6, greensfn_6, 
	         fkx_6, fky_6, fkz_6,fkx2_6, fky2_6, fkz2_6,
                 vdx_brick_a3, vdy_brick_a3, vdz_brick_a3, virial_6, vg_6, vg2_6,
                 u_brick_a3, v0_brick_a3, v1_brick_a3, v2_brick_a3, v3_brick_a3, v4_brick_a3, v5_brick_a3);
      poisson_2s_ik(density_fft_a0, density_fft_a6,
                    vdx_brick_a0, vdy_brick_a0, vdz_brick_a0,
                    vdx_brick_a6, vdy_brick_a6, vdz_brick_a6,
                    u_brick_a0, v0_brick_a0, v1_brick_a0, v2_brick_a0, v3_brick_a0, v4_brick_a0, v5_brick_a0,
                    u_brick_a6, v0_brick_a6, v1_brick_a6, v2_brick_a6, v3_brick_a6, v4_brick_a6, v5_brick_a6);
      poisson_2s_ik(density_fft_a1, density_fft_a5,
                    vdx_brick_a1, vdy_brick_a1, vdz_brick_a1,
                    vdx_brick_a5, vdy_brick_a5, vdz_brick_a5,
                    u_brick_a1, v0_brick_a1, v1_brick_a1, v2_brick_a1, v3_brick_a1, v4_brick_a1, v5_brick_a1,
                    u_brick_a5, v0_brick_a5, v1_brick_a5, v2_brick_a5, v3_brick_a5, v4_brick_a5, v5_brick_a5);
      poisson_2s_ik(density_fft_a2, density_fft_a4,
                    vdx_brick_a2, vdy_brick_a2, vdz_brick_a2,
                    vdx_brick_a4, vdy_brick_a4, vdz_brick_a4,
                    u_brick_a2, v0_brick_a2, v1_brick_a2, v2_brick_a2, v3_brick_a2, v4_brick_a2, v5_brick_a2,
                    u_brick_a4, v0_brick_a4, v1_brick_a4, v2_brick_a4, v3_brick_a4, v4_brick_a4, v5_brick_a4);

      fillbrick_a_ik();

      fieldforce_a_ik();

      if (evflag_atom) fillbrick_a_peratom_ik();
    }
    if (evflag_atom) fieldforce_a_peratom();
  }

  // sum energy across procs and add in volume-dependent term

  const double qscale = force->qqrd2e * scale;
  if (eflag_global) {
    double energy_all;
    MPI_Allreduce(&energy_1,&energy_all,1,MPI_DOUBLE,MPI_SUM,world);
    energy_1 = energy_all;
    MPI_Allreduce(&energy_6,&energy_all,1,MPI_DOUBLE,MPI_SUM,world);
    energy_6 = energy_all;
   
    energy_1 *= 0.5*volume;
    energy_6 *= 0.5*volume;
    
    energy_1 -= g_ewald*qsqsum/MY_PIS +
      MY_PI2*qsum*qsum / (g_ewald*g_ewald*volume);
    energy_6 += - MY_PI*MY_PIS/(6*volume)*pow(g_ewald_6,3)*csumij +
      1.0/12.0*pow(g_ewald_6,6)*csum;
    energy_1 *= qscale;
  }

  // sum virial across procs

  if (vflag_global) {
    double virial_all[6];
    MPI_Allreduce(virial_1,virial_all,6,MPI_DOUBLE,MPI_SUM,world);
    for (i = 0; i < 6; i++) virial[i] = 0.5*qscale*volume*virial_all[i];
    MPI_Allreduce(virial_6,virial_all,6,MPI_DOUBLE,MPI_SUM,world);
    for (i = 0; i < 6; i++) virial[i] += 0.5*volume*virial_all[i];
    if (function[1]+function[2]){
      double a =  MY_PI*MY_PIS/(6*volume)*pow(g_ewald_6,3)*csumij;
      virial[0] -= a;
      virial[1] -= a;
      virial[2] -= a;
    }
  }

  if (eflag_atom) {
    if (function[0]) {
      double *q = atom->q;
      for (i = 0; i < atom->nlocal; i++) {
        eatom[i] -= qscale*g_ewald*q[i]*q[i]/MY_PIS + qscale*MY_PI2*q[i]*qsum / (g_ewald*g_ewald*volume); //coulomb self energy correction
      }
    }
    if (function[1] + function[2]) {
      int tmp;
      for (i = 0; i < atom->nlocal; i++) {
        tmp = atom->type[i];
        eatom[i] += - MY_PI*MY_PIS/(6*volume)*pow(g_ewald_6,3)*csumi[tmp] +
                      1.0/12.0*pow(g_ewald_6,6)*cii[tmp];
      }
    }
  }
            
  if (vflag_atom) {
    if (function[1] + function[2]) {
      int tmp;
      for (i = 0; i < atom->nlocal; i++) {
        tmp = atom->type[i];
        for (int n = 0; n < 3; n++) vatom[i][n] -= MY_PI*MY_PIS/(6*volume)*pow(g_ewald_6,3)*csumi[tmp]; //dispersion self virial correction
      }
    }
  }


  // 2d slab correction

  if (slabflag) slabcorr(eflag);
  energy = energy_1 + energy_6;

  // convert atoms back from lamda to box coords
  
  if (triclinic) domain->lamda2x(atom->nlocal);
}

/* ----------------------------------------------------------------------
   initialize coefficients needed for the dispersion density on the grids
------------------------------------------------------------------------- */

void PPPMDisp::init_coeffs()				// local pair coeffs
{
  int tmp;
  int n = atom->ntypes;
  delete [] B;
  if (function[1]) {					// geometric 1/r^6
    double **b = (double **) force->pair->extract("B",tmp);
    B = new double[n+1];
    for (int i=0; i<=n; ++i) B[i] = sqrt(fabs(b[i][i]));
  }
  if (function[2]) {					// arithmetic 1/r^6
    //cannot use epsilon, because this has not been set yet
    double **epsilon = (double **) force->pair->extract("epsilon",tmp);  
    //cannot use sigma, because this has not been set yet
    double **sigma = (double **) force->pair->extract("sigma",tmp);  
    if (!(epsilon&&sigma))
      error->all(FLERR,"epsilon or sigma reference not set by pair style in PPPMDisp");
    double eps_i, sigma_i, sigma_n, *bi = B = new double[7*n+7];
    double c[7] = {
      1.0, sqrt(6.0), sqrt(15.0), sqrt(20.0), sqrt(15.0), sqrt(6.0), 1.0};
    for (int i=0; i<=n; ++i) {
      eps_i = sqrt(epsilon[i][i]);
      sigma_i = sigma[i][i];
      sigma_n = 1.0;
      for (int j=0; j<7; ++j) {
	*(bi++) = sigma_n*eps_i*c[j]*0.25;
        sigma_n *= sigma_i;
      }
    }
  }
}

/* ----------------------------------------------------------------------
   allocate memory that depends on # of K-vectors and order 
------------------------------------------------------------------------- */

void PPPMDisp::allocate()
{
  if (function[0]) {
    memory->create(work1,2*nfft_both,"pppm/disp:work1");
    memory->create(work2,2*nfft_both,"pppm/disp:work2");

    memory->create1d_offset(fkx,nxlo_fft,nxhi_fft,"pppm/disp:fkx");
    memory->create1d_offset(fky,nylo_fft,nyhi_fft,"pppm/disp:fky");
    memory->create1d_offset(fkz,nzlo_fft,nzhi_fft,"pppm/disp:fkz");

    memory->create1d_offset(fkx2,nxlo_fft,nxhi_fft,"pppm/disp:fkx2");
    memory->create1d_offset(fky2,nylo_fft,nyhi_fft,"pppm/disp:fky2");
    memory->create1d_offset(fkz2,nzlo_fft,nzhi_fft,"pppm/disp:fkz2");


    memory->create(gf_b,order,"pppm/disp:gf_b");
    memory->create2d_offset(rho1d,3,-order/2,order/2,"pppm/disp:rho1d");
    memory->create2d_offset(rho_coeff,order,(1-order)/2,order/2,"pppm/disp:rho_coeff");
    memory->create2d_offset(drho1d,3,-order/2,order/2,"pppm/disp:rho1d");
    memory->create2d_offset(drho_coeff,order,(1-order)/2,order/2,"pppm/disp:drho_coeff");

    memory->create(buf1,nbuf,"pppm/disp:buf1");
    memory->create(buf2,nbuf,"pppm/disp:buf2");

    memory->create(greensfn,nfft_both,"pppm/disp:greensfn");
    memory->create(vg,nfft_both,6,"pppm/disp:vg");
    memory->create(vg2,nfft_both,3,"pppm/disp:vg2");

    memory->create3d_offset(density_brick,nzlo_out,nzhi_out,nylo_out,nyhi_out,
  			    nxlo_out,nxhi_out,"pppm/disp:density_brick");
    if ( differentiation_flag == 1) {
      memory->create3d_offset(u_brick,nzlo_out,nzhi_out,nylo_out,nyhi_out,
  		  	      nxlo_out,nxhi_out,"pppm/disp:u_brick");
      memory->create(sf_precoeff1,nfft_both,"pppm/disp:sf_precoeff1");
      memory->create(sf_precoeff2,nfft_both,"pppm/disp:sf_precoeff2");
      memory->create(sf_precoeff3,nfft_both,"pppm/disp:sf_precoeff3");
      memory->create(sf_precoeff4,nfft_both,"pppm/disp:sf_precoeff4");
      memory->create(sf_precoeff5,nfft_both,"pppm/disp:sf_precoeff5");
      memory->create(sf_precoeff6,nfft_both,"pppm/disp:sf_precoeff6");

    } else {
      memory->create3d_offset(vdx_brick,nzlo_out,nzhi_out,nylo_out,nyhi_out,
  			      nxlo_out,nxhi_out,"pppm/disp:vdx_brick");
      memory->create3d_offset(vdy_brick,nzlo_out,nzhi_out,nylo_out,nyhi_out,
			      nxlo_out,nxhi_out,"pppm/disp:vdy_brick");
      memory->create3d_offset(vdz_brick,nzlo_out,nzhi_out,nylo_out,nyhi_out,
			      nxlo_out,nxhi_out,"pppm/disp:vdz_brick");
    }
    memory->create(density_fft,nfft_both,"pppm/disp:density_fft");

    int tmp;

    fft1 = new FFT3d(lmp,world,nx_pppm,ny_pppm,nz_pppm,
		     nxlo_fft,nxhi_fft,nylo_fft,nyhi_fft,nzlo_fft,nzhi_fft,
		     nxlo_fft,nxhi_fft,nylo_fft,nyhi_fft,nzlo_fft,nzhi_fft,
		     0,0,&tmp);

    fft2 = new FFT3d(lmp,world,nx_pppm,ny_pppm,nz_pppm,
		     nxlo_fft,nxhi_fft,nylo_fft,nyhi_fft,nzlo_fft,nzhi_fft,
		     nxlo_in,nxhi_in,nylo_in,nyhi_in,nzlo_in,nzhi_in,
		     0,0,&tmp);

    remap = new Remap(lmp,world,
		      nxlo_in,nxhi_in,nylo_in,nyhi_in,nzlo_in,nzhi_in,
		      nxlo_fft,nxhi_fft,nylo_fft,nyhi_fft,nzlo_fft,nzhi_fft,
		      1,0,0,FFT_PRECISION);
  }

  if (function[1]) {
    memory->create(work1_6,2*nfft_both_6,"pppm/disp:work1_6");
    memory->create(work2_6,2*nfft_both_6,"pppm/disp:work2_6");

    memory->create1d_offset(fkx_6,nxlo_fft_6,nxhi_fft_6,"pppm/disp:fkx_6");
    memory->create1d_offset(fky_6,nylo_fft_6,nyhi_fft_6,"pppm/disp:fky_6");
    memory->create1d_offset(fkz_6,nzlo_fft_6,nzhi_fft_6,"pppm/disp:fkz_6");

    memory->create1d_offset(fkx2_6,nxlo_fft_6,nxhi_fft_6,"pppm/disp:fkx2_6");
    memory->create1d_offset(fky2_6,nylo_fft_6,nyhi_fft_6,"pppm/disp:fky2_6");
    memory->create1d_offset(fkz2_6,nzlo_fft_6,nzhi_fft_6,"pppm/disp:fkz2_6");

    memory->create(gf_b_6,order_6,"pppm/disp:gf_b_6");
    memory->create2d_offset(rho1d_6,3,-order_6/2,order_6/2,"pppm/disp:rho1d_6");
    memory->create2d_offset(rho_coeff_6,order_6,(1-order_6)/2,order_6/2,"pppm/disp:rho_coeff_6");
    memory->create2d_offset(drho1d_6,3,-order_6/2,order_6/2,"pppm/disp:drho1d_6");
    memory->create2d_offset(drho_coeff_6,order_6,(1-order_6)/2,order_6/2,"pppm/disp:drho_coeff_6");

    memory->create(buf1_6,nbuf_6,"pppm/disp:buf1_6");
    memory->create(buf2_6,nbuf_6,"pppm/disp:buf2_6");

    memory->create(greensfn_6,nfft_both_6,"pppm/disp:greensfn_6");
    memory->create(vg_6,nfft_both_6,6,"pppm/disp:vg_6");
    memory->create(vg2_6,nfft_both_6,3,"pppm/disp:vg2_6");

    memory->create3d_offset(density_brick_g,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  			    nxlo_out_6,nxhi_out_6,"pppm/disp:density_brick_g");
    if ( differentiation_flag == 1) {
      memory->create3d_offset(u_brick_g,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	      nxlo_out_6,nxhi_out_6,"pppm/disp:u_brick_g");

      memory->create(sf_precoeff1_6,nfft_both_6,"pppm/disp:sf_precoeff1_6");
      memory->create(sf_precoeff2_6,nfft_both_6,"pppm/disp:sf_precoeff2_6");
      memory->create(sf_precoeff3_6,nfft_both_6,"pppm/disp:sf_precoeff3_6");
      memory->create(sf_precoeff4_6,nfft_both_6,"pppm/disp:sf_precoeff4_6");
      memory->create(sf_precoeff5_6,nfft_both_6,"pppm/disp:sf_precoeff5_6");
      memory->create(sf_precoeff6_6,nfft_both_6,"pppm/disp:sf_precoeff6_6");

    }  else {
      memory->create3d_offset(vdx_brick_g,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
			      nxlo_out_6,nxhi_out_6,"pppm/disp:vdx_brick_g");
      memory->create3d_offset(vdy_brick_g,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
			      nxlo_out_6,nxhi_out_6,"pppm/disp:vdy_brick_g");
      memory->create3d_offset(vdz_brick_g,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
			      nxlo_out_6,nxhi_out_6,"pppm/disp:vdz_brick_g");
    }
    memory->create(density_fft_g,nfft_both_6,"pppm/disp:density_fft_g");


    int tmp;

    fft1_6 = new FFT3d(lmp,world,nx_pppm_6,ny_pppm_6,nz_pppm_6,
		     nxlo_fft_6,nxhi_fft_6,nylo_fft_6,nyhi_fft_6,nzlo_fft_6,nzhi_fft_6,
		     nxlo_fft_6,nxhi_fft_6,nylo_fft_6,nyhi_fft_6,nzlo_fft_6,nzhi_fft_6,
		     0,0,&tmp);

    fft2_6 = new FFT3d(lmp,world,nx_pppm_6,ny_pppm_6,nz_pppm_6,
		     nxlo_fft_6,nxhi_fft_6,nylo_fft_6,nyhi_fft_6,nzlo_fft_6,nzhi_fft_6,
		     nxlo_in_6,nxhi_in_6,nylo_in_6,nyhi_in_6,nzlo_in_6,nzhi_in_6,
		     0,0,&tmp);

    remap_6 = new Remap(lmp,world,
		      nxlo_in_6,nxhi_in_6,nylo_in_6,nyhi_in_6,nzlo_in_6,nzhi_in_6,
		      nxlo_fft_6,nxhi_fft_6,nylo_fft_6,nyhi_fft_6,nzlo_fft_6,nzhi_fft_6,
		      1,0,0,FFT_PRECISION);
  }

  if (function[2]) {
    memory->create(work1_6,2*nfft_both_6,"pppm/disp:work1_6");
    memory->create(work2_6,2*nfft_both_6,"pppm/disp:work2_6");

    memory->create1d_offset(fkx_6,nxlo_fft_6,nxhi_fft_6,"pppm/disp:fkx_6");
    memory->create1d_offset(fky_6,nylo_fft_6,nyhi_fft_6,"pppm/disp:fky_6");
    memory->create1d_offset(fkz_6,nzlo_fft_6,nzhi_fft_6,"pppm/disp:fkz_6");

    memory->create1d_offset(fkx2_6,nxlo_fft_6,nxhi_fft_6,"pppm/disp:fkx2_6");
    memory->create1d_offset(fky2_6,nylo_fft_6,nyhi_fft_6,"pppm/disp:fky2_6");
    memory->create1d_offset(fkz2_6,nzlo_fft_6,nzhi_fft_6,"pppm/disp:fkz2_6");

    memory->create(gf_b_6,order_6,"pppm/disp:gf_b_6");
    memory->create2d_offset(rho1d_6,3,-order_6/2,order_6/2,"pppm/disp:rho1d_6");
    memory->create2d_offset(rho_coeff_6,order_6,(1-order_6)/2,order_6/2,"pppm/disp:rho_coeff_6");
    memory->create2d_offset(drho1d_6,3,-order_6/2,order_6/2,"pppm/disp:drho1d_6");
    memory->create2d_offset(drho_coeff_6,order_6,(1-order_6)/2,order_6/2,"pppm/disp:drho_coeff_6");

    memory->create(buf1_6,7*nbuf_6,"pppm/disp:buf1_6");
    memory->create(buf2_6,7*nbuf_6,"pppm/disp:buf2_6");
    memory->create(split_1,2*nfft_both_6 , "pppm/disp:split_1");
    memory->create(split_2,2*nfft_both_6 , "pppm/disp:split_2");
    memory->create(greensfn_6,nfft_both_6,"pppm/disp:greensfn_6");
    memory->create(vg_6,nfft_both_6,6,"pppm/disp:vg_6");
    memory->create(vg2_6,nfft_both_6,3,"pppm/disp:vg2_6");

    memory->create3d_offset(density_brick_a0,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  			    nxlo_out_6,nxhi_out_6,"pppm/disp:density_brick_a0");
    memory->create3d_offset(density_brick_a1,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  			    nxlo_out_6,nxhi_out_6,"pppm/disp:density_brick_a1");
    memory->create3d_offset(density_brick_a2,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  			    nxlo_out_6,nxhi_out_6,"pppm/disp:density_brick_a2");
    memory->create3d_offset(density_brick_a3,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  			    nxlo_out_6,nxhi_out_6,"pppm/disp:density_brick_a3");
    memory->create3d_offset(density_brick_a4,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  			    nxlo_out_6,nxhi_out_6,"pppm/disp:density_brick_a4");
    memory->create3d_offset(density_brick_a5,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  			    nxlo_out_6,nxhi_out_6,"pppm/disp:density_brick_a5");
    memory->create3d_offset(density_brick_a6,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  			    nxlo_out_6,nxhi_out_6,"pppm/disp:density_brick_a6");

    memory->create(density_fft_a0,nfft_both_6,"pppm/disp:density_fft_a0");
    memory->create(density_fft_a1,nfft_both_6,"pppm/disp:density_fft_a1");
    memory->create(density_fft_a2,nfft_both_6,"pppm/disp:density_fft_a2");
    memory->create(density_fft_a3,nfft_both_6,"pppm/disp:density_fft_a3");
    memory->create(density_fft_a4,nfft_both_6,"pppm/disp:density_fft_a4");
    memory->create(density_fft_a5,nfft_both_6,"pppm/disp:density_fft_a5");
    memory->create(density_fft_a6,nfft_both_6,"pppm/disp:density_fft_a6");


    if ( differentiation_flag == 1 ) {
      memory->create3d_offset(u_brick_a0,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	      nxlo_out_6,nxhi_out_6,"pppm/disp:u_brick_a0");
      memory->create3d_offset(u_brick_a1,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	      nxlo_out_6,nxhi_out_6,"pppm/disp:u_brick_a1");
      memory->create3d_offset(u_brick_a2,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	      nxlo_out_6,nxhi_out_6,"pppm/disp:u_brick_a2");
      memory->create3d_offset(u_brick_a3,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	      nxlo_out_6,nxhi_out_6,"pppm/disp:u_brick_a3");
      memory->create3d_offset(u_brick_a4,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	      nxlo_out_6,nxhi_out_6,"pppm/disp:u_brick_a4");
      memory->create3d_offset(u_brick_a5,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	      nxlo_out_6,nxhi_out_6,"pppm/disp:u_brick_a5");
      memory->create3d_offset(u_brick_a6,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	      nxlo_out_6,nxhi_out_6,"pppm/disp:u_brick_a6");

      memory->create(sf_precoeff1_6,nfft_both_6,"pppm/disp:sf_precoeff1_6");
      memory->create(sf_precoeff2_6,nfft_both_6,"pppm/disp:sf_precoeff2_6");
      memory->create(sf_precoeff3_6,nfft_both_6,"pppm/disp:sf_precoeff3_6");
      memory->create(sf_precoeff4_6,nfft_both_6,"pppm/disp:sf_precoeff4_6");
      memory->create(sf_precoeff5_6,nfft_both_6,"pppm/disp:sf_precoeff5_6");
      memory->create(sf_precoeff6_6,nfft_both_6,"pppm/disp:sf_precoeff6_6");

    } else {

      memory->create3d_offset(vdx_brick_a0,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
			      nxlo_out_6,nxhi_out_6,"pppm/disp:vdx_brick_a0");
      memory->create3d_offset(vdy_brick_a0,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
			      nxlo_out_6,nxhi_out_6,"pppm/disp:vdy_brick_a0");
      memory->create3d_offset(vdz_brick_a0,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
			      nxlo_out_6,nxhi_out_6,"pppm/disp:vdz_brick_a0");

      memory->create3d_offset(vdx_brick_a1,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
			      nxlo_out_6,nxhi_out_6,"pppm/disp:vdx_brick_a1");
      memory->create3d_offset(vdy_brick_a1,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
			      nxlo_out_6,nxhi_out_6,"pppm/disp:vdy_brick_a1");
      memory->create3d_offset(vdz_brick_a1,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
			      nxlo_out_6,nxhi_out_6,"pppm/disp:vdz_brick_a1");

      memory->create3d_offset(vdx_brick_a2,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
			      nxlo_out_6,nxhi_out_6,"pppm/disp:vdx_brick_a2");
      memory->create3d_offset(vdy_brick_a2,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
			      nxlo_out_6,nxhi_out_6,"pppm/disp:vdy_brick_a2");
      memory->create3d_offset(vdz_brick_a2,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
			      nxlo_out_6,nxhi_out_6,"pppm/disp:vdz_brick_a2");

      memory->create3d_offset(vdx_brick_a3,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
			      nxlo_out_6,nxhi_out_6,"pppm/disp:vdx_brick_a3");
      memory->create3d_offset(vdy_brick_a3,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
			      nxlo_out_6,nxhi_out_6,"pppm/disp:vdy_brick_a3");
      memory->create3d_offset(vdz_brick_a3,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
			      nxlo_out_6,nxhi_out_6,"pppm/disp:vdz_brick_a3");

      memory->create3d_offset(vdx_brick_a4,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
			      nxlo_out_6,nxhi_out_6,"pppm/disp:vdx_brick_a4");
      memory->create3d_offset(vdy_brick_a4,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
			      nxlo_out_6,nxhi_out_6,"pppm/disp:vdy_brick_a4");
      memory->create3d_offset(vdz_brick_a4,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
			      nxlo_out_6,nxhi_out_6,"pppm/disp:vdz_brick_a4");

      memory->create3d_offset(vdx_brick_a5,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
			      nxlo_out_6,nxhi_out_6,"pppm/disp:vdx_brick_a5");
      memory->create3d_offset(vdy_brick_a5,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
			      nxlo_out_6,nxhi_out_6,"pppm/disp:vdy_brick_a5");
      memory->create3d_offset(vdz_brick_a5,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
			      nxlo_out_6,nxhi_out_6,"pppm/disp:vdz_brick_a5");

      memory->create3d_offset(vdx_brick_a6,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
			      nxlo_out_6,nxhi_out_6,"pppm/disp:vdx_brick_a6");
      memory->create3d_offset(vdy_brick_a6,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
			      nxlo_out_6,nxhi_out_6,"pppm/disp:vdy_brick_a6");
      memory->create3d_offset(vdz_brick_a6,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
			      nxlo_out_6,nxhi_out_6,"pppm/disp:vdz_brick_a6");
    }



    int tmp;

    fft1_6 = new FFT3d(lmp,world,nx_pppm_6,ny_pppm_6,nz_pppm_6,
		     nxlo_fft_6,nxhi_fft_6,nylo_fft_6,nyhi_fft_6,nzlo_fft_6,nzhi_fft_6,
		     nxlo_fft_6,nxhi_fft_6,nylo_fft_6,nyhi_fft_6,nzlo_fft_6,nzhi_fft_6,
		     0,0,&tmp);

    fft2_6 = new FFT3d(lmp,world,nx_pppm_6,ny_pppm_6,nz_pppm_6,
		     nxlo_fft_6,nxhi_fft_6,nylo_fft_6,nyhi_fft_6,nzlo_fft_6,nzhi_fft_6,
		     nxlo_in_6,nxhi_in_6,nylo_in_6,nyhi_in_6,nzlo_in_6,nzhi_in_6,
		     0,0,&tmp);

    remap_6 = new Remap(lmp,world,
		      nxlo_in_6,nxhi_in_6,nylo_in_6,nyhi_in_6,nzlo_in_6,nzhi_in_6,
		      nxlo_fft_6,nxhi_fft_6,nylo_fft_6,nyhi_fft_6,nzlo_fft_6,nzhi_fft_6,
		      1,0,0,FFT_PRECISION);
  }  
}

/* ----------------------------------------------------------------------
   allocate memory that depends on # of K-vectors and order
   for per atom calculations 
------------------------------------------------------------------------- */

void PPPMDisp::allocate_peratom()
{
  if (function[0]) {
    memory->create(buf3,nbuf_pa,"pppm/disp:buf3");
    memory->create(buf4,nbuf_pa,"pppm/disp:buf4");
    if (differentiation_flag != 1)
      memory->create3d_offset(u_brick,nzlo_out,nzhi_out,nylo_out,nyhi_out,
    	                      nxlo_out,nxhi_out,"pppm/disp:u_brick");

    memory->create3d_offset(v0_brick,nzlo_out,nzhi_out,nylo_out,nyhi_out,
			    nxlo_out,nxhi_out,"pppm/disp:v0_brick");
    memory->create3d_offset(v1_brick,nzlo_out,nzhi_out,nylo_out,nyhi_out,
  			    nxlo_out,nxhi_out,"pppm/disp:v1_brick");
    memory->create3d_offset(v2_brick,nzlo_out,nzhi_out,nylo_out,nyhi_out,
  			    nxlo_out,nxhi_out,"pppm/disp:v2_brick");
    memory->create3d_offset(v3_brick,nzlo_out,nzhi_out,nylo_out,nyhi_out,
  			    nxlo_out,nxhi_out,"pppm/disp:v3_brick");
    memory->create3d_offset(v4_brick,nzlo_out,nzhi_out,nylo_out,nyhi_out,
  			    nxlo_out,nxhi_out,"pppm/disp:v4_brick");
    memory->create3d_offset(v5_brick,nzlo_out,nzhi_out,nylo_out,nyhi_out,
  			    nxlo_out,nxhi_out,"pppm/disp:v5_brick");
  }


  if (function[1]) {
    memory->create(buf3_6,nbuf_pa_6,"pppm/disp:buf3_6");
    memory->create(buf4_6,nbuf_pa_6,"pppm/disp:buf4_6");

    if ( differentiation_flag != 1 )
      memory->create3d_offset(u_brick_g,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	      nxlo_out_6,nxhi_out_6,"pppm/disp:u_brick_g");

    memory->create3d_offset(v0_brick_g,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	    nxlo_out_6,nxhi_out_6,"pppm/disp:v0_brick_g");
    memory->create3d_offset(v1_brick_g,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	    nxlo_out_6,nxhi_out_6,"pppm/disp:v1_brick_g");
    memory->create3d_offset(v2_brick_g,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	    nxlo_out_6,nxhi_out_6,"pppm/disp:v2_brick_g");
    memory->create3d_offset(v3_brick_g,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	    nxlo_out_6,nxhi_out_6,"pppm/disp:v3_brick_g");
    memory->create3d_offset(v4_brick_g,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	    nxlo_out_6,nxhi_out_6,"pppm/disp:v4_brick_g");
    memory->create3d_offset(v5_brick_g,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	    nxlo_out_6,nxhi_out_6,"pppm/disp:v5_brick_g");
  }

  if (function[2]) {
    memory->create(buf3_6,7*nbuf_pa_6,"pppm/disp:buf3");
    memory->create(buf4_6,7*nbuf_pa_6,"pppm/disp:buf4");
   
    if ( differentiation_flag != 1 ) {
      memory->create3d_offset(u_brick_a0,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	      nxlo_out_6,nxhi_out_6,"pppm/disp:u_brick_a0");
      memory->create3d_offset(u_brick_a1,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	      nxlo_out_6,nxhi_out_6,"pppm/disp:u_brick_a1");
      memory->create3d_offset(u_brick_a2,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	      nxlo_out_6,nxhi_out_6,"pppm/disp:u_brick_a2");
      memory->create3d_offset(u_brick_a3,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	      nxlo_out_6,nxhi_out_6,"pppm/disp:u_brick_a3");
      memory->create3d_offset(u_brick_a4,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	      nxlo_out_6,nxhi_out_6,"pppm/disp:u_brick_a4");
      memory->create3d_offset(u_brick_a5,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	      nxlo_out_6,nxhi_out_6,"pppm/disp:u_brick_a5");
      memory->create3d_offset(u_brick_a6,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	      nxlo_out_6,nxhi_out_6,"pppm/disp:u_brick_a6");
    }

    memory->create3d_offset(v0_brick_a0,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	        nxlo_out_6,nxhi_out_6,"pppm/disp:v0_brick_a0");
    memory->create3d_offset(v1_brick_a0,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
    	                        nxlo_out_6,nxhi_out_6,"pppm/disp:v1_brick_a0");
    memory->create3d_offset(v2_brick_a0,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	        nxlo_out_6,nxhi_out_6,"pppm/disp:v2_brick_a0");
    memory->create3d_offset(v3_brick_a0,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	        nxlo_out_6,nxhi_out_6,"pppm/disp:v3_brick_a0");
    memory->create3d_offset(v4_brick_a0,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	        nxlo_out_6,nxhi_out_6,"pppm/disp:v4_brick_a0");
    memory->create3d_offset(v5_brick_a0,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	        nxlo_out_6,nxhi_out_6,"pppm/disp:v5_brick_a0");

    memory->create3d_offset(v0_brick_a1,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	        nxlo_out_6,nxhi_out_6,"pppm/disp:v0_brick_a1");
    memory->create3d_offset(v1_brick_a1,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
   	                        nxlo_out_6,nxhi_out_6,"pppm/disp:v1_brick_a1");
    memory->create3d_offset(v2_brick_a1,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	        nxlo_out_6,nxhi_out_6,"pppm/disp:v2_brick_a1");
    memory->create3d_offset(v3_brick_a1,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	        nxlo_out_6,nxhi_out_6,"pppm/disp:v3_brick_a1");
    memory->create3d_offset(v4_brick_a1,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  	  	                nxlo_out_6,nxhi_out_6,"pppm/disp:v4_brick_a1");
    memory->create3d_offset(v5_brick_a1,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	        nxlo_out_6,nxhi_out_6,"pppm/disp:v5_brick_a1");

    memory->create3d_offset(v0_brick_a2,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	        nxlo_out_6,nxhi_out_6,"pppm/disp:v0_brick_a2");
    memory->create3d_offset(v1_brick_a2,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	        nxlo_out_6,nxhi_out_6,"pppm/disp:v1_brick_a2");
    memory->create3d_offset(v2_brick_a2,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	        nxlo_out_6,nxhi_out_6,"pppm/disp:v2_brick_a2");
    memory->create3d_offset(v3_brick_a2,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	        nxlo_out_6,nxhi_out_6,"pppm/disp:v3_brick_a2");
    memory->create3d_offset(v4_brick_a2,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	        nxlo_out_6,nxhi_out_6,"pppm/disp:v4_brick_a2");
    memory->create3d_offset(v5_brick_a2,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	        nxlo_out_6,nxhi_out_6,"pppm/disp:v5_brick_a2");

    memory->create3d_offset(v0_brick_a3,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	        nxlo_out_6,nxhi_out_6,"pppm/disp:v0_brick_a3");
    memory->create3d_offset(v1_brick_a3,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	        nxlo_out_6,nxhi_out_6,"pppm/disp:v1_brick_a3");
    memory->create3d_offset(v2_brick_a3,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	        nxlo_out_6,nxhi_out_6,"pppm/disp:v2_brick_a3");
    memory->create3d_offset(v3_brick_a3,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  	  	                nxlo_out_6,nxhi_out_6,"pppm/disp:v3_brick_a3");
    memory->create3d_offset(v4_brick_a3,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	        nxlo_out_6,nxhi_out_6,"pppm/disp:v4_brick_a3");
    memory->create3d_offset(v5_brick_a3,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	        nxlo_out_6,nxhi_out_6,"pppm/disp:v5_brick_a3");

    memory->create3d_offset(v0_brick_a4,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	        nxlo_out_6,nxhi_out_6,"pppm/disp:v0_brick_a4");
    memory->create3d_offset(v1_brick_a4,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	        nxlo_out_6,nxhi_out_6,"pppm/disp:v1_brick_a4");
    memory->create3d_offset(v2_brick_a4,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	        nxlo_out_6,nxhi_out_6,"pppm/disp:v2_brick_a4");
    memory->create3d_offset(v3_brick_a4,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	        nxlo_out_6,nxhi_out_6,"pppm/disp:v3_brick_a4");
    memory->create3d_offset(v4_brick_a4,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	        nxlo_out_6,nxhi_out_6,"pppm/disp:v4_brick_a4");
    memory->create3d_offset(v5_brick_a4,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	        nxlo_out_6,nxhi_out_6,"pppm/disp:v5_brick_a4");

    memory->create3d_offset(v0_brick_a5,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	        nxlo_out_6,nxhi_out_6,"pppm/disp:v0_brick_a5");
    memory->create3d_offset(v1_brick_a5,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	        nxlo_out_6,nxhi_out_6,"pppm/disp:v1_brick_a5");
    memory->create3d_offset(v2_brick_a5,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	        nxlo_out_6,nxhi_out_6,"pppm/disp:v2_brick_a5");
    memory->create3d_offset(v3_brick_a5,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	        nxlo_out_6,nxhi_out_6,"pppm/disp:v3_brick_a5");
    memory->create3d_offset(v4_brick_a5,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	        nxlo_out_6,nxhi_out_6,"pppm/disp:v4_brick_a5");
    memory->create3d_offset(v5_brick_a5,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	        nxlo_out_6,nxhi_out_6,"pppm/disp:v5_brick_a5");

    memory->create3d_offset(v0_brick_a6,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  	  	                nxlo_out_6,nxhi_out_6,"pppm/disp:v0_brick_a6");
    memory->create3d_offset(v1_brick_a6,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	        nxlo_out_6,nxhi_out_6,"pppm/disp:v1_brick_a6");
    memory->create3d_offset(v2_brick_a6,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	        nxlo_out_6,nxhi_out_6,"pppm/disp:v2_brick_a6");
    memory->create3d_offset(v3_brick_a6,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	        nxlo_out_6,nxhi_out_6,"pppm/disp:v3_brick_a6");
    memory->create3d_offset(v4_brick_a6,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	        nxlo_out_6,nxhi_out_6,"pppm/disp:v4_brick_a6");
    memory->create3d_offset(v5_brick_a6,nzlo_out_6,nzhi_out_6,nylo_out_6,nyhi_out_6,
  		  	        nxlo_out_6,nxhi_out_6,"pppm/disp:v5_brick_a6");

  }  
}


/* ----------------------------------------------------------------------
   deallocate memory that depends on # of K-vectors and order 
------------------------------------------------------------------------- */

void PPPMDisp::deallocate()
{
  memory->destroy3d_offset(density_brick,nzlo_out,nylo_out,nxlo_out);
  memory->destroy3d_offset(vdx_brick,nzlo_out,nylo_out,nxlo_out);
  memory->destroy3d_offset(vdy_brick,nzlo_out,nylo_out,nxlo_out);
  memory->destroy3d_offset(vdz_brick,nzlo_out,nylo_out,nxlo_out);
  memory->destroy(density_fft);

  memory->destroy3d_offset(density_brick_g,nzlo_out_6,nylo_out_6,nxlo_out_6);
  memory->destroy3d_offset(vdx_brick_g,nzlo_out_6,nylo_out_6,nxlo_out_6);
  memory->destroy3d_offset(vdy_brick_g,nzlo_out_6,nylo_out_6,nxlo_out_6);
  memory->destroy3d_offset(vdz_brick_g,nzlo_out_6,nylo_out_6,nxlo_out_6);
  memory->destroy(density_fft_g);

  memory->destroy3d_offset(density_brick_a0,nzlo_out_6,nylo_out_6,nxlo_out_6);
  memory->destroy3d_offset(vdx_brick_a0,nzlo_out_6,nylo_out_6,nxlo_out_6);
  memory->destroy3d_offset(vdy_brick_a0,nzlo_out_6,nylo_out_6,nxlo_out_6);
  memory->destroy3d_offset(vdz_brick_a0,nzlo_out_6,nylo_out_6,nxlo_out_6);
  memory->destroy(density_fft_a0);

  memory->destroy3d_offset(density_brick_a1,nzlo_out_6,nylo_out_6,nxlo_out_6);
  memory->destroy3d_offset(vdx_brick_a1,nzlo_out_6,nylo_out_6,nxlo_out_6);
  memory->destroy3d_offset(vdy_brick_a1,nzlo_out_6,nylo_out_6,nxlo_out_6);
  memory->destroy3d_offset(vdz_brick_a1,nzlo_out_6,nylo_out_6,nxlo_out_6);
  memory->destroy(density_fft_a1);

  memory->destroy3d_offset(density_brick_a2,nzlo_out_6,nylo_out_6,nxlo_out_6);
  memory->destroy3d_offset(vdx_brick_a2,nzlo_out_6,nylo_out_6,nxlo_out_6);
  memory->destroy3d_offset(vdy_brick_a2,nzlo_out_6,nylo_out_6,nxlo_out_6);
  memory->destroy3d_offset(vdz_brick_a2,nzlo_out_6,nylo_out_6,nxlo_out_6);
  memory->destroy(density_fft_a2);

  memory->destroy3d_offset(density_brick_a3,nzlo_out_6,nylo_out_6,nxlo_out_6);
  memory->destroy3d_offset(vdx_brick_a3,nzlo_out_6,nylo_out_6,nxlo_out_6);
  memory->destroy3d_offset(vdy_brick_a3,nzlo_out_6,nylo_out_6,nxlo_out_6);
  memory->destroy3d_offset(vdz_brick_a3,nzlo_out_6,nylo_out_6,nxlo_out_6);
  memory->destroy(density_fft_a3);
 
  memory->destroy3d_offset(density_brick_a4,nzlo_out_6,nylo_out_6,nxlo_out_6);
  memory->destroy3d_offset(vdx_brick_a4,nzlo_out_6,nylo_out_6,nxlo_out_6);
  memory->destroy3d_offset(vdy_brick_a4,nzlo_out_6,nylo_out_6,nxlo_out_6);
  memory->destroy3d_offset(vdz_brick_a4,nzlo_out_6,nylo_out_6,nxlo_out_6);
  memory->destroy(density_fft_a4);
 
  memory->destroy3d_offset(density_brick_a5,nzlo_out_6,nylo_out_6,nxlo_out_6);
  memory->destroy3d_offset(vdx_brick_a5,nzlo_out_6,nylo_out_6,nxlo_out_6);
  memory->destroy3d_offset(vdy_brick_a5,nzlo_out_6,nylo_out_6,nxlo_out_6);
  memory->destroy3d_offset(vdz_brick_a5,nzlo_out_6,nylo_out_6,nxlo_out_6);
  memory->destroy(density_fft_a5);

  memory->destroy3d_offset(density_brick_a6,nzlo_out_6,nylo_out_6,nxlo_out_6);
  memory->destroy3d_offset(vdx_brick_a6,nzlo_out_6,nylo_out_6,nxlo_out_6);
  memory->destroy3d_offset(vdy_brick_a6,nzlo_out_6,nylo_out_6,nxlo_out_6);
  memory->destroy3d_offset(vdz_brick_a6,nzlo_out_6,nylo_out_6,nxlo_out_6);
  memory->destroy(density_fft_a6);

  memory->destroy(sf_precoeff1);
  memory->destroy(sf_precoeff2);
  memory->destroy(sf_precoeff3);
  memory->destroy(sf_precoeff4);
  memory->destroy(sf_precoeff5);
  memory->destroy(sf_precoeff6);

  memory->destroy(sf_precoeff1_6);
  memory->destroy(sf_precoeff2_6);
  memory->destroy(sf_precoeff3_6);
  memory->destroy(sf_precoeff4_6);
  memory->destroy(sf_precoeff5_6);
  memory->destroy(sf_precoeff6_6);

  memory->destroy(greensfn);
  memory->destroy(greensfn_6);
  memory->destroy(work1);
  memory->destroy(work2);
  memory->destroy(work1_6);
  memory->destroy(work2_6);
  memory->destroy(vg);
  memory->destroy(vg2);
  memory->destroy(vg_6);
  memory->destroy(vg2_6);

  memory->destroy1d_offset(fkx,nxlo_fft);
  memory->destroy1d_offset(fky,nylo_fft);
  memory->destroy1d_offset(fkz,nzlo_fft);

  memory->destroy1d_offset(fkx2,nxlo_fft);
  memory->destroy1d_offset(fky2,nylo_fft);
  memory->destroy1d_offset(fkz2,nzlo_fft);

  memory->destroy1d_offset(fkx_6,nxlo_fft_6);
  memory->destroy1d_offset(fky_6,nylo_fft_6);
  memory->destroy1d_offset(fkz_6,nzlo_fft_6);

  memory->destroy1d_offset(fkx2_6,nxlo_fft_6);
  memory->destroy1d_offset(fky2_6,nylo_fft_6);
  memory->destroy1d_offset(fkz2_6,nzlo_fft_6);
 
  memory->destroy(buf1);
  memory->destroy(buf2);

  memory->destroy(buf1_6);
  memory->destroy(buf2_6);

  memory->destroy(split_1);
  memory->destroy(split_2);
 

  memory->destroy(gf_b);
  memory->destroy2d_offset(rho1d,-order/2);
  memory->destroy2d_offset(rho_coeff,(1-order)/2);
  memory->destroy2d_offset(drho1d,-order/2);
  memory->destroy2d_offset(drho_coeff, (1-order)/2);

  memory->destroy(gf_b_6);
  memory->destroy2d_offset(rho1d_6,-order_6/2);
  memory->destroy2d_offset(rho_coeff_6,(1-order_6)/2);
  memory->destroy2d_offset(drho1d_6,-order_6/2); 
  memory->destroy2d_offset(drho_coeff_6,(1-order_6)/2);

  delete fft1;
  delete fft2;
  delete remap;

  delete fft1_6;
  delete fft2_6;
  delete remap_6;
}


/* ----------------------------------------------------------------------
   deallocate memory that depends on # of K-vectors and order
   for per atom calculations 
------------------------------------------------------------------------- */

void PPPMDisp::deallocate_peratom()
{
  memory->destroy3d_offset(u_brick, nzlo_out, nylo_out, nxlo_out);
  memory->destroy3d_offset(v0_brick, nzlo_out, nylo_out, nxlo_out);
  memory->destroy3d_offset(v1_brick, nzlo_out, nylo_out, nxlo_out);
  memory->destroy3d_offset(v2_brick, nzlo_out, nylo_out, nxlo_out);
  memory->destroy3d_offset(v3_brick, nzlo_out, nylo_out, nxlo_out);
  memory->destroy3d_offset(v4_brick, nzlo_out, nylo_out, nxlo_out);
  memory->destroy3d_offset(v5_brick, nzlo_out, nylo_out, nxlo_out);

  memory->destroy3d_offset(u_brick_g, nzlo_out_6, nylo_out_6, nxlo_out_6);
  memory->destroy3d_offset(v0_brick_g, nzlo_out_6, nylo_out_6, nxlo_out_6);
  memory->destroy3d_offset(v1_brick_g, nzlo_out_6, nylo_out_6, nxlo_out_6);
  memory->destroy3d_offset(v2_brick_g, nzlo_out_6, nylo_out_6, nxlo_out_6);
  memory->destroy3d_offset(v3_brick_g, nzlo_out_6, nylo_out_6, nxlo_out_6);
  memory->destroy3d_offset(v4_brick_g, nzlo_out_6, nylo_out_6, nxlo_out_6);
  memory->destroy3d_offset(v5_brick_g, nzlo_out_6, nylo_out_6, nxlo_out_6);

  memory->destroy3d_offset(u_brick_a0, nzlo_out_6, nylo_out_6, nxlo_out_6);
  memory->destroy3d_offset(v0_brick_a0, nzlo_out_6, nylo_out_6, nxlo_out_6);
  memory->destroy3d_offset(v1_brick_a0, nzlo_out_6, nylo_out_6, nxlo_out_6);
  memory->destroy3d_offset(v2_brick_a0, nzlo_out_6, nylo_out_6, nxlo_out_6);
  memory->destroy3d_offset(v3_brick_a0, nzlo_out_6, nylo_out_6, nxlo_out_6);
  memory->destroy3d_offset(v4_brick_a0, nzlo_out_6, nylo_out_6, nxlo_out_6);
  memory->destroy3d_offset(v5_brick_a0, nzlo_out_6, nylo_out_6, nxlo_out_6);

  memory->destroy3d_offset(u_brick_a1, nzlo_out_6, nylo_out_6, nxlo_out_6);
  memory->destroy3d_offset(v0_brick_a1, nzlo_out_6, nylo_out_6, nxlo_out_6);
  memory->destroy3d_offset(v1_brick_a1, nzlo_out_6, nylo_out_6, nxlo_out_6);
  memory->destroy3d_offset(v2_brick_a1, nzlo_out_6, nylo_out_6, nxlo_out_6);
  memory->destroy3d_offset(v3_brick_a1, nzlo_out_6, nylo_out_6, nxlo_out_6);
  memory->destroy3d_offset(v4_brick_a1, nzlo_out_6, nylo_out_6, nxlo_out_6);
  memory->destroy3d_offset(v5_brick_a1, nzlo_out_6, nylo_out_6, nxlo_out_6);

  memory->destroy3d_offset(u_brick_a2, nzlo_out_6, nylo_out_6, nxlo_out_6);
  memory->destroy3d_offset(v0_brick_a2, nzlo_out_6, nylo_out_6, nxlo_out_6);
  memory->destroy3d_offset(v1_brick_a2, nzlo_out_6, nylo_out_6, nxlo_out_6);
  memory->destroy3d_offset(v2_brick_a2, nzlo_out_6, nylo_out_6, nxlo_out_6);
  memory->destroy3d_offset(v3_brick_a2, nzlo_out_6, nylo_out_6, nxlo_out_6);
  memory->destroy3d_offset(v4_brick_a2, nzlo_out_6, nylo_out_6, nxlo_out_6);
  memory->destroy3d_offset(v5_brick_a2, nzlo_out_6, nylo_out_6, nxlo_out_6);

  memory->destroy3d_offset(u_brick_a3, nzlo_out_6, nylo_out_6, nxlo_out_6);
  memory->destroy3d_offset(v0_brick_a3, nzlo_out_6, nylo_out_6, nxlo_out_6);
  memory->destroy3d_offset(v1_brick_a3, nzlo_out_6, nylo_out_6, nxlo_out_6);
  memory->destroy3d_offset(v2_brick_a3, nzlo_out_6, nylo_out_6, nxlo_out_6);
  memory->destroy3d_offset(v3_brick_a3, nzlo_out_6, nylo_out_6, nxlo_out_6);
  memory->destroy3d_offset(v4_brick_a3, nzlo_out_6, nylo_out_6, nxlo_out_6);
  memory->destroy3d_offset(v5_brick_a3, nzlo_out_6, nylo_out_6, nxlo_out_6);
 
  memory->destroy3d_offset(u_brick_a4, nzlo_out_6, nylo_out_6, nxlo_out_6);
  memory->destroy3d_offset(v0_brick_a4, nzlo_out_6, nylo_out_6, nxlo_out_6);
  memory->destroy3d_offset(v1_brick_a4, nzlo_out_6, nylo_out_6, nxlo_out_6);
  memory->destroy3d_offset(v2_brick_a4, nzlo_out_6, nylo_out_6, nxlo_out_6);
  memory->destroy3d_offset(v3_brick_a4, nzlo_out_6, nylo_out_6, nxlo_out_6);
  memory->destroy3d_offset(v4_brick_a4, nzlo_out_6, nylo_out_6, nxlo_out_6);
  memory->destroy3d_offset(v5_brick_a4, nzlo_out_6, nylo_out_6, nxlo_out_6);
 
  memory->destroy3d_offset(u_brick_a5, nzlo_out_6, nylo_out_6, nxlo_out_6);
  memory->destroy3d_offset(v0_brick_a5, nzlo_out_6, nylo_out_6, nxlo_out_6);
  memory->destroy3d_offset(v1_brick_a5, nzlo_out_6, nylo_out_6, nxlo_out_6);
  memory->destroy3d_offset(v2_brick_a5, nzlo_out_6, nylo_out_6, nxlo_out_6);
  memory->destroy3d_offset(v3_brick_a5, nzlo_out_6, nylo_out_6, nxlo_out_6);
  memory->destroy3d_offset(v4_brick_a5, nzlo_out_6, nylo_out_6, nxlo_out_6);
  memory->destroy3d_offset(v5_brick_a5, nzlo_out_6, nylo_out_6, nxlo_out_6);

  memory->destroy3d_offset(u_brick_a6, nzlo_out_6, nylo_out_6, nxlo_out_6);
  memory->destroy3d_offset(v0_brick_a6, nzlo_out_6, nylo_out_6, nxlo_out_6);
  memory->destroy3d_offset(v1_brick_a6, nzlo_out_6, nylo_out_6, nxlo_out_6);
  memory->destroy3d_offset(v2_brick_a6, nzlo_out_6, nylo_out_6, nxlo_out_6);
  memory->destroy3d_offset(v3_brick_a6, nzlo_out_6, nylo_out_6, nxlo_out_6);
  memory->destroy3d_offset(v4_brick_a6, nzlo_out_6, nylo_out_6, nxlo_out_6);
  memory->destroy3d_offset(v5_brick_a6, nzlo_out_6, nylo_out_6, nxlo_out_6);

  memory->destroy(buf3);
  memory->destroy(buf4);

  memory->destroy(buf3_6);
  memory->destroy(buf4_6);
}

/* ----------------------------------------------------------------------
   set size of FFT grid (nx,ny,nz_pppm) and g_ewald
   for Coulomb interactions
------------------------------------------------------------------------- */

void PPPMDisp::set_grid()
{
  double q2 = qsqsum * force->qqrd2e / force->dielectric;

  // use xprd,yprd,zprd even if triclinic so grid size is the same
  // adjust z dimension for 2d slab PPPM
  // 3d PPPM just uses zprd since slab_volfactor = 1.0

  double xprd = domain->xprd;
  double yprd = domain->yprd;
  double zprd = domain->zprd;
  double zprd_slab = zprd*slab_volfactor;
  
  // make initial g_ewald estimate
  // based on desired accuracy and real space cutoff
  // fluid-occupied volume used to estimate real-space error
  // zprd used rather than zprd_slab

  double h, h_x,h_y,h_z;
  bigint natoms = atom->natoms;

  if (!gewaldflag) {
    g_ewald = accuracy*sqrt(natoms*cutoff*xprd*yprd*zprd) / (2.0*q2);
    if (g_ewald >= 1.0)  
      error->all(FLERR,"KSpace accuracy too large to estimate G vector");
    g_ewald = sqrt(-log(g_ewald)) / cutoff;
  } 

  // set optimal nx_pppm,ny_pppm,nz_pppm based on order and accuracy
  // nz_pppm uses extended zprd_slab instead of zprd
  // reduce it until accuracy target is met

  if (!gridflag) {
    h = h_x = h_y = h_z = 4.0/g_ewald;  
    int count = 0;
    while (1) {
      
      // set grid dimension
      nx_pppm = static_cast<int> (xprd/h_x);
      ny_pppm = static_cast<int> (yprd/h_y);
      nz_pppm = static_cast<int> (zprd_slab/h_z);

      if (nx_pppm <= 1) nx_pppm = 2;
      if (ny_pppm <= 1) ny_pppm = 2;
      if (nz_pppm <= 1) nz_pppm = 2;

      //set local grid dimension
      int npey_fft,npez_fft;
      if (nz_pppm >= nprocs) {
        npey_fft = 1;
        npez_fft = nprocs;
      } else procs2grid2d(nprocs,ny_pppm,nz_pppm,&npey_fft,&npez_fft);

      int me_y = me % npey_fft;
      int me_z = me / npey_fft;

      nxlo_fft = 0;
      nxhi_fft = nx_pppm - 1;
      nylo_fft = me_y*ny_pppm/npey_fft;
      nyhi_fft = (me_y+1)*ny_pppm/npey_fft - 1;
      nzlo_fft = me_z*nz_pppm/npez_fft;
      nzhi_fft = (me_z+1)*nz_pppm/npez_fft - 1;

      double qopt = compute_qopt();
   
      double dfkspace = sqrt(qopt/natoms)*q2/(xprd*yprd*zprd_slab);

      count++;

      // break loop if the accuracy has been reached or too many loops have been performed
      if (dfkspace <= accuracy) break;
      if (count > 500) error->all(FLERR, "Could not compute grid size for Coulomb interaction!");
      h *= 0.95;
      h_x = h_y = h_z = h;
    }
  }
  
  // boost grid size until it is factorable

  while (!factorable(nx_pppm)) nx_pppm++;
  while (!factorable(ny_pppm)) ny_pppm++;
  while (!factorable(nz_pppm)) nz_pppm++;
}

/* ----------------------------------------------------------------------
   set the FFT parameters 
------------------------------------------------------------------------- */

void PPPMDisp::set_fft_parameters(int& nx_p,int& ny_p,int& nz_p,
                                   int& nxlo_f,int& nylo_f,int& nzlo_f,
                                   int& nxhi_f,int& nyhi_f,int& nzhi_f,
                                   int& nxlo_i,int& nylo_i,int& nzlo_i,
                                   int& nxhi_i,int& nyhi_i,int& nzhi_i,
                                   int& nxlo_o,int& nylo_o,int& nzlo_o,
                                   int& nxhi_o,int& nyhi_o,int& nzhi_o,
                                   int& nxlo_g,int& nylo_g,int& nzlo_g,
                                   int& nxhi_g,int& nyhi_g,int& nzhi_g,
		                   int& nlow, int& nupp,
                                   int& ng, int& nf, int& nfb, int& nb, int& nb_p,
		                   double& sft,double& sftone, int& ord)
{
  // global indices of PPPM grid range from 0 to N-1
  // nlo_in,nhi_in = lower/upper limits of the 3d sub-brick of
  //   global PPPM grid that I own without ghost cells
  // for slab PPPM, assign z grid as if it were not extended

  nxlo_i = static_cast<int> (comm->xsplit[comm->myloc[0]] * nx_p);
  nxhi_i = static_cast<int> (comm->xsplit[comm->myloc[0]+1] * nx_p) - 1;

  nylo_i = static_cast<int> (comm->ysplit[comm->myloc[1]] * ny_p);
  nyhi_i = static_cast<int> (comm->ysplit[comm->myloc[1]+1] * ny_p) - 1;

  nzlo_i = static_cast<int> 
      (comm->zsplit[comm->myloc[2]] * nz_p/slab_volfactor);
  nzhi_i = static_cast<int> 
      (comm->zsplit[comm->myloc[2]+1] * nz_p/slab_volfactor) - 1;


  // nlow,nupp = stencil size for mapping particles to PPPM grid

  nlow = -(ord-1)/2;
  nupp = ord/2;

  // sft values for particle <-> grid mapping
  // add/subtract OFFSET to avoid int(-0.75) = 0 when want it to be -1

  if (ord % 2) sft = OFFSET + 0.5;
  else sft = OFFSET;
  if (ord % 2) sftone = 0.0;
  else sftone = 0.5;

  // nlo_out,nhi_out = lower/upper limits of the 3d sub-brick of
  //   global PPPM grid that my particles can contribute charge to
  // effectively nlo_in,nhi_in + ghost cells
  // nlo,nhi = global coords of grid pt to "lower left" of smallest/largest
  //           position a particle in my box can be at
  // dist[3] = particle position bound = subbox + skin/2.0 + qdist
  //   qdist = offset due to TIP4P fictitious charge
  //   convert to triclinic if necessary
  // nlo_out,nhi_out = nlo,nhi + stencil size for particle mapping
  // for slab PPPM, assign z grid as if it were not extended

  double *prd,*sublo,*subhi;

  if (triclinic == 0) {
    prd = domain->prd;
    boxlo = domain->boxlo;
    sublo = domain->sublo;
    subhi = domain->subhi;
  } else {
    prd = domain->prd_lamda;
    boxlo = domain->boxlo_lamda;
    sublo = domain->sublo_lamda;
    subhi = domain->subhi_lamda;
  }

  double xprd = prd[0];
  double yprd = prd[1];
  double zprd = prd[2];
  double zprd_slab = zprd*slab_volfactor;

  double dist[3];
  double cuthalf = 0.5*neighbor->skin + qdist;
  if (triclinic == 0) dist[0] = dist[1] = dist[2] = cuthalf;
  else {
    dist[0] = cuthalf/domain->prd[0];
    dist[1] = cuthalf/domain->prd[1];
    dist[2] = cuthalf/domain->prd[2];
  }
    
  int nlo,nhi;
    
  nlo = static_cast<int> ((sublo[0]-dist[0]-boxlo[0]) * 
                            nx_p/xprd + sft) - OFFSET;
  nhi = static_cast<int> ((subhi[0]+dist[0]-boxlo[0]) * 
                            nx_p/xprd + sft) - OFFSET;
  nxlo_o = nlo + nlow;
  nxhi_o = nhi + nupp;

  nlo = static_cast<int> ((sublo[1]-dist[1]-boxlo[1]) * 
                            ny_p/yprd + sft) - OFFSET;
  nhi = static_cast<int> ((subhi[1]+dist[1]-boxlo[1]) * 
                            ny_p/yprd + sft) - OFFSET;
  nylo_o = nlo + nlow;
  nyhi_o = nhi + nupp;

  nlo = static_cast<int> ((sublo[2]-dist[2]-boxlo[2]) * 
                            nz_p/zprd_slab + sft) - OFFSET;
  nhi = static_cast<int> ((subhi[2]+dist[2]-boxlo[2]) * 
                            nz_p/zprd_slab + sft) - OFFSET;
  nzlo_o = nlo + nlow;
  nzhi_o = nhi + nupp;

  // for slab PPPM, change the grid boundary for processors at +z end
  //   to include the empty volume between periodically repeating slabs
  // for slab PPPM, want charge data communicated from -z proc to +z proc,
  //   but not vice versa, also want field data communicated from +z proc to
  //   -z proc, but not vice versa
  // this is accomplished by nzhi_i = nzhi_o on +z end (no ghost cells)

  if (slabflag && (comm->myloc[2] == comm->procgrid[2]-1)) {
    nzhi_i = nz_p - 1;
    nzhi_o = nz_p - 1;
  }
  
  // nlo_ghost,nhi_ghost = # of planes I will recv from 6 directions
  //   that overlay domain I own
  // proc in that direction tells me via sendrecv()
  // if no neighbor proc, value is from self since I have ghosts regardless

  int nplanes;
  MPI_Status status;

  nplanes = nxlo_i - nxlo_o;
  if (comm->procneigh[0][0] != me)
      MPI_Sendrecv(&nplanes,1,MPI_INT,comm->procneigh[0][0],0,
                   &nxhi_g,1,MPI_INT,comm->procneigh[0][1],0,
                   world,&status);
  else nxhi_g = nplanes;

  nplanes = nxhi_o - nxhi_i;
  if (comm->procneigh[0][1] != me)
      MPI_Sendrecv(&nplanes,1,MPI_INT,comm->procneigh[0][1],0,
                   &nxlo_g,1,MPI_INT,comm->procneigh[0][0],
                   0,world,&status);
  else nxlo_g = nplanes;

  nplanes = nylo_i - nylo_o;
  if (comm->procneigh[1][0] != me)
    MPI_Sendrecv(&nplanes,1,MPI_INT,comm->procneigh[1][0],0,
                 &nyhi_g,1,MPI_INT,comm->procneigh[1][1],0,
                 world,&status);
  else nyhi_g = nplanes;

  nplanes = nyhi_o - nyhi_i;
  if (comm->procneigh[1][1] != me)
    MPI_Sendrecv(&nplanes,1,MPI_INT,comm->procneigh[1][1],0,
                 &nylo_g,1,MPI_INT,comm->procneigh[1][0],0,
                 world,&status);
  else nylo_g = nplanes;

  nplanes = nzlo_i - nzlo_o;
  if (comm->procneigh[2][0] != me)
    MPI_Sendrecv(&nplanes,1,MPI_INT,comm->procneigh[2][0],0,
                 &nzhi_g,1,MPI_INT,comm->procneigh[2][1],0,
                 world,&status);
  else nzhi_g = nplanes;

  nplanes = nzhi_o - nzhi_i;
  if (comm->procneigh[2][1] != me)
    MPI_Sendrecv(&nplanes,1,MPI_INT,comm->procneigh[2][1],0,
                 &nzlo_g,1,MPI_INT,comm->procneigh[2][0],0,
                 world,&status);
  else nzlo_g = nplanes;

  // decomposition of FFT mesh
  // global indices range from 0 to N-1
  // proc owns entire x-dimension, clump of columns in y,z dimensions
  // npey_fft,npez_fft = # of procs in y,z dims
  // if nprocs is small enough, proc can own 1 or more entire xy planes,
  //   else proc owns 2d sub-blocks of yz plane
  // me_y,me_z = which proc (0-npe_fft-1) I am in y,z dimensions
  // nlo_fft,nhi_fft = lower/upper limit of the section
  //   of the global FFT mesh that I own

  int npey_fft,npez_fft;
  if (nz_p >= nprocs) {
    npey_fft = 1;
    npez_fft = nprocs;
  } else procs2grid2d(nprocs,ny_p,nz_p,&npey_fft,&npez_fft);

  int me_y = me % npey_fft;
  int me_z = me / npey_fft;

  nxlo_f = 0;
  nxhi_f = nx_p - 1;
  nylo_f = me_y*ny_p/npey_fft;
  nyhi_f = (me_y+1)*ny_p/npey_fft - 1;
  nzlo_f = me_z*nz_p/npez_fft;
  nzhi_f = (me_z+1)*nz_p/npez_fft - 1;

  // PPPM grid for this proc, including ghosts

  ng = (nxhi_o-nxlo_o+1) * (nyhi_o-nylo_o+1) *
    (nzhi_o-nzlo_o+1);

  // FFT arrays on this proc, without ghosts
  // nfft = FFT points in FFT decomposition on this proc
  // nfft_brick = FFT points in 3d brick-decomposition on this proc
  // nfft_both = greater of 2 values

  nf = (nxhi_f-nxlo_f+1) * (nyhi_f-nylo_f+1) *
    (nzhi_f-nzlo_f+1);
  int nfft_brick = (nxhi_i-nxlo_i+1) * (nyhi_i-nylo_i+1) *
    (nzhi_i-nzlo_i+1);
  nfb = MAX(nf,nfft_brick);

  // buffer space for use in brick2fft and fillbrick
  // idel = max # of ghost planes to send or recv in +/- dir of each dim
  // nx,ny,nz = owned planes (including ghosts) in each dim
  // nxx,nyy,nzz = max # of grid cells to send in each dim
  // nbuf = max in any dim

  int idelx,idely,idelz,nx,ny,nz,nxx,nyy,nzz;

  idelx = MAX(nxlo_g,nxhi_g);
  idelx = MAX(idelx,nxhi_o-nxhi_i);
  idelx = MAX(idelx,nxlo_i-nxlo_o);

  idely = MAX(nylo_g,nyhi_g);
  idely = MAX(idely,nyhi_o-nyhi_i);
  idely = MAX(idely,nylo_i-nylo_o);

  idelz = MAX(nzlo_g,nzhi_g);
  idelz = MAX(idelz,nzhi_o-nzhi_i);
  idelz = MAX(idelz,nzlo_i-nzlo_o);

  nx = nxhi_o - nxlo_o + 1;
  ny = nyhi_o - nylo_o + 1;
  nz = nzhi_o - nzlo_o + 1;

  nxx = idelx * ny * nz;
  nyy = idely * nx * nz;
  nzz = idelz * nx * ny;

  nb = MAX(nxx,nyy);
  nb = MAX(nb,nzz);

  nb_p = 6*nb;
  if (differentiation_flag != 1) {
    nb *=3;
    nb_p++;
  }
}

/* ----------------------------------------------------------------------
   check if all factors of n are in list of factors
   return 1 if yes, 0 if no 
------------------------------------------------------------------------- */

int PPPMDisp::factorable(int n)
{
  int i;

  while (n > 1) {
    for (i = 0; i < nfactors; i++) {
      if (n % factors[i] == 0) {
	n /= factors[i];
	break;
      }
    }
    if (i == nfactors) return 0;
  }

  return 1;
}

/* ----------------------------------------------------------------------
   pre-compute Green's function denominator expansion coeffs, Gamma(2n) 
------------------------------------------------------------------------- */
void PPPMDisp::adjust_gewald()
{
  
  // Use Newton solver to find g_ewald

  double dx;
        
  // Begin algorithm
  
  for (int i = 0; i < LARGE; i++) {
    dx = f() / derivf(); 
    g_ewald -= dx; //Update g_ewald
    if (fabs(f()) < SMALL) return;
  }
   
  // Failed to converge
  
  char str[128];
  sprintf(str, "Could not compute g_ewald");
  error->all(FLERR, str);

}

/* ----------------------------------------------------------------------
 Calculate f(x)
 ------------------------------------------------------------------------- */

double PPPMDisp::f()
{
  double df_rspace, df_kspace;
  double q2 = qsqsum * force->qqrd2e / force->dielectric;
  double xprd = domain->xprd;
  double yprd = domain->yprd;
  double zprd = domain->zprd;
  double zprd_slab = zprd*slab_volfactor;
  bigint natoms = atom->natoms;

  df_rspace = 2.0*q2*exp(-g_ewald*g_ewald*cutoff*cutoff) / 
       sqrt(natoms*cutoff*xprd*yprd*zprd);
   
  double qopt = compute_qopt();
  df_kspace = sqrt(qopt/natoms)*q2/(xprd*yprd*zprd_slab);
   
  return df_rspace - df_kspace;
}

/* ----------------------------------------------------------------------
 Calculate numerical derivative f'(x) using forward difference
 [f(x + h) - f(x)] / h
 ------------------------------------------------------------------------- */
            
double PPPMDisp::derivf()
{  
  double h = 0.000001;  //Derivative step-size
  double df,f1,f2,g_ewald_old;
  
  f1 = f();
  g_ewald_old = g_ewald;
  g_ewald += h;
  f2 = f();
  g_ewald = g_ewald_old;
  df = (f2 - f1)/h;
  
  return df;
} 

/* ----------------------------------------------------------------------
   Calculate the final estimator for the accuracy
------------------------------------------------------------------------- */

double PPPMDisp::final_accuracy()
{
  double df_rspace, df_kspace;
  double q2 = qsqsum * force->qqrd2e / force->dielectric;
  double xprd = domain->xprd;
  double yprd = domain->yprd;
  double zprd = domain->zprd;
  double zprd_slab = zprd*slab_volfactor;
  bigint natoms = atom->natoms;
  df_rspace = 2.0*q2 * exp(-g_ewald*g_ewald*cutoff*cutoff) / 
             sqrt(natoms*cutoff*xprd*yprd*zprd);

  double qopt = compute_qopt();

  df_kspace = sqrt(qopt/natoms)*q2/(xprd*yprd*zprd_slab);

  double acc = sqrt(df_rspace*df_rspace + df_kspace*df_kspace);
  return acc;
}

/* ----------------------------------------------------------------------
   Calculate the final estimator for the Dispersion accuracy
------------------------------------------------------------------------- */

double PPPMDisp::final_accuracy_6()
{
  double df_rspace, df_kspace;
  double xprd = domain->xprd;
  double yprd = domain->yprd;
  double zprd = domain->zprd;
  double zprd_slab = zprd*slab_volfactor;
  bigint natoms = atom->natoms;
  df_rspace = lj_rspace_error();

  double qopt = compute_qopt_6();

  df_kspace = sqrt(qopt/natoms)*csum/(xprd*yprd*zprd_slab);

  double acc = sqrt(df_rspace*df_rspace + df_kspace*df_kspace);
  return acc;
}

/* ----------------------------------------------------------------------
   Compute qopt for Coulomb interactions
------------------------------------------------------------------------- */

double PPPMDisp::compute_qopt()
{
  double qopt;
  if (differentiation_flag == 1) {
    qopt = compute_qopt_ad();
  } else {
    qopt = compute_qopt_ik();
  }
  double qopt_all;
  MPI_Allreduce(&qopt,&qopt_all,1,MPI_DOUBLE,MPI_SUM,world);
  return qopt_all;
}

/* ----------------------------------------------------------------------
   Compute qopt for Dispersion interactions
------------------------------------------------------------------------- */

double PPPMDisp::compute_qopt_6()
{
  double qopt;
  if (differentiation_flag == 1) {
    qopt = compute_qopt_6_ad();
  } else {
    qopt = compute_qopt_6_ik();
  }
  double qopt_all;
  MPI_Allreduce(&qopt,&qopt_all,1,MPI_DOUBLE,MPI_SUM,world);
  return qopt_all;
}

/* ----------------------------------------------------------------------
   Compute qopt for the ik differentiation scheme and Coulomb interaction
------------------------------------------------------------------------- */

double PPPMDisp::compute_qopt_ik()
{
  double qopt = 0.0;
  int k,l,m;
  double *prd;

  if (triclinic == 0) prd = domain->prd;
  else prd = domain->prd_lamda;

  double xprd = prd[0];
  double yprd = prd[1];
  double zprd = prd[2];
  double zprd_slab = zprd*slab_volfactor;

  double unitkx = (2.0*MY_PI/xprd);
  double unitky = (2.0*MY_PI/yprd);
  double unitkz = (2.0*MY_PI/zprd_slab);

  int nx,ny,nz,kper,lper,mper;
  double sqk, u2;
  double argx,argy,argz,wx,wy,wz,sx,sy,sz,qx,qy,qz;
  double sum1,sum2, sum3,dot1,dot2;

  int nbx = 2;
  int nby = 2;
  int nbz = 2;

  for (m = nzlo_fft; m <= nzhi_fft; m++) {
    mper = m - nz_pppm*(2*m/nz_pppm);

    for (l = nylo_fft; l <= nyhi_fft; l++) {
      lper = l - ny_pppm*(2*l/ny_pppm);

      for (k = nxlo_fft; k <= nxhi_fft; k++) {
        kper = k - nx_pppm*(2*k/nx_pppm);
      
        sqk = pow(unitkx*kper,2.0) + pow(unitky*lper,2.0) + 
          pow(unitkz*mper,2.0);

        if (sqk != 0.0) {
          sum1 = 0.0;
          sum2 = 0.0;
          sum3 = 0.0;
          for (nx = -nbx; nx <= nbx; nx++) {
            qx = unitkx*(kper+nx_pppm*nx);
            sx = exp(-0.25*pow(qx/g_ewald,2.0));
            wx = 1.0;
            argx = 0.5*qx*xprd/nx_pppm;
            if (argx != 0.0) wx = pow(sin(argx)/argx,order);
            for (ny = -nby; ny <= nby; ny++) {
              qy = unitky*(lper+ny_pppm*ny);
              sy = exp(-0.25*pow(qy/g_ewald,2.0));
              wy = 1.0;
              argy = 0.5*qy*yprd/ny_pppm;
              if (argy != 0.0) wy = pow(sin(argy)/argy,order);
              for (nz = -nbz; nz <= nbz; nz++) {
                qz = unitkz*(mper+nz_pppm*nz);
                sz = exp(-0.25*pow(qz/g_ewald,2.0));
                wz = 1.0;
                argz = 0.5*qz*zprd_slab/nz_pppm;
                if (argz != 0.0) wz = pow(sin(argz)/argz,order);

                dot1 = unitkx*kper*qx + unitky*lper*qy + unitkz*mper*qz;
                dot2 = qx*qx+qy*qy+qz*qz;
                u2 =  pow(wx*wy*wz,2.0);
                sum1 += sx*sy*sz*sx*sy*sz/dot2*4.0*4.0*MY_PI*MY_PI;
                sum2 += u2*sx*sy*sz*4.0*MY_PI/dot2*dot1;
		sum3 += u2;
              }
            }
          }
	  sum2 *= sum2;
	  sum3 *= sum3*sqk;
          qopt += sum1 -sum2/sum3;
        }
      }
    }
  }
  return qopt;
}

/* ----------------------------------------------------------------------
   Compute qopt for the ad differentiation scheme and Coulomb interaction
------------------------------------------------------------------------- */

double PPPMDisp::compute_qopt_ad()
{
  double qopt = 0.0;
  int k,l,m;
  double *prd;

  if (triclinic == 0) prd = domain->prd;
  else prd = domain->prd_lamda;

  double xprd = prd[0];
  double yprd = prd[1];
  double zprd = prd[2];
  double zprd_slab = zprd*slab_volfactor;


  double unitkx = (2.0*MY_PI/xprd);
  double unitky = (2.0*MY_PI/yprd);
  double unitkz = (2.0*MY_PI/zprd_slab);

  int nx,ny,nz,kper,lper,mper;
  double argx,argy,argz,wx,wy,wz,sx,sy,sz,qx,qy,qz;
  double u2, sqk;
  double sum1,sum2,sum3,sum4,dot2;
  double numerator;

  int nbx = 2;
  int nby = 2;
  int nbz = 2;
  double form = 1.0;

  for (m = nzlo_fft; m <= nzhi_fft; m++) {
    mper = m - nz_pppm*(2*m/nz_pppm);

    for (l = nylo_fft; l <= nyhi_fft; l++) {
      lper = l - ny_pppm*(2*l/ny_pppm);

      for (k = nxlo_fft; k <= nxhi_fft; k++) {
        kper = k - nx_pppm*(2*k/nx_pppm);
      
        sqk = pow(unitkx*kper,2.0) + pow(unitky*lper,2.0) + 
          pow(unitkz*mper,2.0);

        if (sqk != 0.0) {
          numerator = form*12.5663706;
    
          sum1 = 0.0;
          sum2 = 0.0;
          sum3 = 0.0;
          sum4 = 0.0;
          for (nx = -nbx; nx <= nbx; nx++) {
            qx = unitkx*(kper+nx_pppm*nx);
            sx = exp(-0.25*pow(qx/g_ewald,2.0));
            wx = 1.0;
            argx = 0.5*qx*xprd/nx_pppm;
            if (argx != 0.0) wx = pow(sin(argx)/argx,order);
            for (ny = -nby; ny <= nby; ny++) {
              qy = unitky*(lper+ny_pppm*ny);
              sy = exp(-0.25*pow(qy/g_ewald,2.0));
              wy = 1.0;
              argy = 0.5*qy*yprd/ny_pppm;
              if (argy != 0.0) wy = pow(sin(argy)/argy,order);
              for (nz = -nbz; nz <= nbz; nz++) {
                qz = unitkz*(mper+nz_pppm*nz);
                sz = exp(-0.25*pow(qz/g_ewald,2.0));
                wz = 1.0;
                argz = 0.5*qz*zprd_slab/nz_pppm;
                if (argz != 0.0) wz = pow(sin(argz)/argz,order);

                dot2 = qx*qx+qy*qy+qz*qz;
                u2 =  pow(wx*wy*wz,2.0);
                sum1 += sx*sy*sz*sx*sy*sz/dot2*4.0*4.0*MY_PI*MY_PI;
                sum2 += sx*sy*sz * u2*4.0*MY_PI;
                sum3 += u2;
                sum4 += dot2*u2;
              }
            }
          }
          sum2 *= sum2;
          qopt += sum1 - sum2/(sum3*sum4);
        }
      }
    }
  }
  return qopt;
}

/* ----------------------------------------------------------------------
   Compute qopt for the ik differentiation scheme and Dispersion interaction
------------------------------------------------------------------------- */

double PPPMDisp::compute_qopt_6_ik()
{
  double qopt = 0.0;
  int k,l,m,n;
  double *prd;

  if (triclinic == 0) prd = domain->prd;
  else prd = domain->prd_lamda;

  double xprd = prd[0];
  double yprd = prd[1];
  double zprd = prd[2];
  double zprd_slab = zprd*slab_volfactor;

  double unitkx = (2.0*MY_PI/xprd);
  double unitky = (2.0*MY_PI/yprd);
  double unitkz = (2.0*MY_PI/zprd_slab);

  int nx,ny,nz,kper,lper,mper;
  double sqk, u2;
  double argx,argy,argz,wx,wy,wz,sx,sy,sz,qx,qy,qz;
  double sum1,sum2, sum3;
  double dot1,dot2, rtdot2, term;
  double inv2ew = 2*g_ewald_6;
  inv2ew = 1.0/inv2ew;
  double rtpi = sqrt(MY_PI);

  int nbx = 2;
  int nby = 2;
  int nbz = 2;

  n = 0;
  for (m = nzlo_fft_6; m <= nzhi_fft_6; m++) {
    mper = m - nz_pppm_6*(2*m/nz_pppm_6);

    for (l = nylo_fft_6; l <= nyhi_fft_6; l++) {
      lper = l - ny_pppm_6*(2*l/ny_pppm_6);

      for (k = nxlo_fft_6; k <= nxhi_fft_6; k++) {
        kper = k - nx_pppm_6*(2*k/nx_pppm_6);
      
        sqk = pow(unitkx*kper,2.0) + pow(unitky*lper,2.0) + 
          pow(unitkz*mper,2.0);

        if (sqk != 0.0) {
          sum1 = 0.0;
          sum2 = 0.0;
          sum3 = 0.0;
          for (nx = -nbx; nx <= nbx; nx++) {
            qx = unitkx*(kper+nx_pppm_6*nx);
            sx = exp(-qx*qx*inv2ew*inv2ew);
            wx = 1.0;
            argx = 0.5*qx*xprd/nx_pppm_6;
            if (argx != 0.0) wx = pow(sin(argx)/argx,order_6);
            for (ny = -nby; ny <= nby; ny++) {
              qy = unitky*(lper+ny_pppm_6*ny);
              sy = exp(-qy*qy*inv2ew*inv2ew);
              wy = 1.0;
              argy = 0.5*qy*yprd/ny_pppm_6;
              if (argy != 0.0) wy = pow(sin(argy)/argy,order_6);
              for (nz = -nbz; nz <= nbz; nz++) {
                qz = unitkz*(mper+nz_pppm_6*nz);
                sz = exp(-qz*qz*inv2ew*inv2ew);
                wz = 1.0;
                argz = 0.5*qz*zprd_slab/nz_pppm_6;
                if (argz != 0.0) wz = pow(sin(argz)/argz,order_6);

                dot1 = unitkx*kper*qx + unitky*lper*qy + unitkz*mper*qz;
                dot2 = qx*qx+qy*qy+qz*qz;
                rtdot2 = sqrt(dot2);
                term = (1-2*dot2*inv2ew*inv2ew)*sx*sy*sz +
		       2*dot2*rtdot2*inv2ew*inv2ew*inv2ew*rtpi*erfc(rtdot2*inv2ew);
                term *= g_ewald_6*g_ewald_6*g_ewald_6;
                u2 =  pow(wx*wy*wz,2.0);
                sum1 += term*term*MY_PI*MY_PI*MY_PI/9.0 * dot2;
                sum2 += -u2*term*MY_PI*rtpi/3.0*dot1;
		sum3 += u2;
              }
            }
          }
	  sum2 *= sum2;
	  sum3 *= sum3*sqk;
          qopt += sum1 -sum2/sum3;
        }
      }
    }
  }
  return qopt;
}

/* ----------------------------------------------------------------------
   Compute qopt for the ad differentiation scheme and Dispersion interaction
------------------------------------------------------------------------- */

double PPPMDisp::compute_qopt_6_ad()
{
  double qopt = 0.0;
  int k,l,m;
  double *prd;

  if (triclinic == 0) prd = domain->prd;
  else prd = domain->prd_lamda;

  double xprd = prd[0];
  double yprd = prd[1];
  double zprd = prd[2];
  double zprd_slab = zprd*slab_volfactor;

  double unitkx = (2.0*MY_PI/xprd);
  double unitky = (2.0*MY_PI/yprd);
  double unitkz = (2.0*MY_PI/zprd_slab);

  int nx,ny,nz,kper,lper,mper;
  double argx,argy,argz,wx,wy,wz,sx,sy,sz,qx,qy,qz;
  double u2, sqk;
  double sum1,sum2,sum3,sum4;
  double dot2, rtdot2, term;
  double inv2ew = 2*g_ewald_6;
  inv2ew = 1/inv2ew;
  double rtpi = sqrt(MY_PI);

  int nbx = 2;
  int nby = 2;
  int nbz = 2;

  for (m = nzlo_fft_6; m <= nzhi_fft_6; m++) {
    mper = m - nz_pppm_6*(2*m/nz_pppm_6);

    for (l = nylo_fft_6; l <= nyhi_fft_6; l++) {
      lper = l - ny_pppm_6*(2*l/ny_pppm_6);

      for (k = nxlo_fft_6; k <= nxhi_fft_6; k++) {
        kper = k - nx_pppm_6*(2*k/nx_pppm_6);
      
        sqk = pow(unitkx*kper,2.0) + pow(unitky*lper,2.0) + 
          pow(unitkz*mper,2.0);

        if (sqk != 0.0) {
    
          sum1 = 0.0;
          sum2 = 0.0;
          sum3 = 0.0;
          sum4 = 0.0;
          for (nx = -nbx; nx <= nbx; nx++) {
            qx = unitkx*(kper+nx_pppm_6*nx);
            sx = exp(-qx*qx*inv2ew*inv2ew);
            wx = 1.0;
            argx = 0.5*qx*xprd/nx_pppm_6;
            if (argx != 0.0) wx = pow(sin(argx)/argx,order_6);
            for (ny = -nby; ny <= nby; ny++) {
              qy = unitky*(lper+ny_pppm_6*ny);
              sy = exp(-qy*qy*inv2ew*inv2ew);
              wy = 1.0;
              argy = 0.5*qy*yprd/ny_pppm_6;
              if (argy != 0.0) wy = pow(sin(argy)/argy,order_6);
              for (nz = -nbz; nz <= nbz; nz++) {
                qz = unitkz*(mper+nz_pppm_6*nz);
                sz = exp(-qz*qz*inv2ew*inv2ew);
                wz = 1.0;
                argz = 0.5*qz*zprd_slab/nz_pppm_6;
                if (argz != 0.0) wz = pow(sin(argz)/argz,order_6);

                dot2 = qx*qx+qy*qy+qz*qz;
                rtdot2 = sqrt(dot2);
                term = (1-2*dot2*inv2ew*inv2ew)*sx*sy*sz +
		       2*dot2*rtdot2*inv2ew*inv2ew*inv2ew*rtpi*erfc(rtdot2*inv2ew);
                term *= g_ewald_6*g_ewald_6*g_ewald_6;
                u2 =  pow(wx*wy*wz,2.0);
                sum1 += term*term*MY_PI*MY_PI*MY_PI/9.0 * dot2;
                sum2 += -term*MY_PI*rtpi/3.0 * u2 * dot2;
                sum3 += u2;
                sum4 += dot2*u2;
              }
            }
          }
          sum2 *= sum2;
          qopt += sum1 - sum2/(sum3*sum4);
        }
      }
    }
  }
  return qopt;
}

/* ----------------------------------------------------------------------
   set size of FFT grid  and g_ewald_6
   for Dispersion interactions
------------------------------------------------------------------------- */

void PPPMDisp::set_grid_6()
{
  // Calculate csum
  if (!csumflag) calc_csum();
  if (!gewaldflag_6) set_init_g6();
  if (!gridflag_6) set_n_pppm_6();
  while (!factorable(nx_pppm_6)) nx_pppm_6++;
  while (!factorable(ny_pppm_6)) ny_pppm_6++;
  while (!factorable(nz_pppm_6)) nz_pppm_6++;
  
}

/* ----------------------------------------------------------------------
   Calculate the sum of the squared dispersion coefficients and other 
   related quantities required for the calculations
------------------------------------------------------------------------- */

void PPPMDisp::calc_csum()
{
  csumij = 0.0;
  csum = 0.0;

  int ntypes = atom->ntypes;   
  int i,j,k;

  delete [] cii;
  cii = new double[ntypes +1];
  for (i = 0; i<=ntypes; i++) cii[i] = 0.0;
  delete [] csumi; 
  csumi = new double[ntypes +1];
  for (i = 0; i<=ntypes; i++) csumi[i] = 0.0; 
  int *neach = new int[ntypes+1];
  for (i = 0; i<=ntypes; i++) neach[i] = 0; 

  //the following variables are needed to distinguish between arithmetic
  //  and geometric mixing

  double mix1;    // scales 20/16 to 4
  int mix2;       // shifts the value to the sigma^3 value
  int mix3;       // shifts the value to the right atom type
  if (function[1]) {
    mix1 = 1;
    mix2 = 0;
    mix3 = 1;
  }
  if (function[2]) {
    mix1 = 64.0 / 20.0;
    mix2 = 3;
    mix3 = 7;
  }
  for (i = 1; i <= ntypes; i++) {
    cii[i] = mix1*B[mix3*i+mix2]*B[mix3*i+mix2];
  }

  int tmp;
  for (i = 0; i < atom->nlocal; i++) {
    tmp = atom->type[i];
    neach[tmp]++;
    csum += mix1*B[mix3*tmp+mix2]*B[mix3*tmp+mix2];    
  }

  double tmp2;
  MPI_Allreduce(&csum,&tmp2,1,MPI_DOUBLE,MPI_SUM,world);
  csum = tmp2;
  csumflag = 1;

  int *neach_all = new int[ntypes+1];
  MPI_Allreduce(neach,neach_all,ntypes+1,MPI_INT,MPI_SUM,world);

  // copmute csumij and csumi

  if (function[1]){
    for (i=1; i<=ntypes; i++) {
      for (j=1; j<=ntypes; j++) {
        csumi[i] += neach_all[j]*B[i]*B[j];
        csumij += neach_all[i]*neach_all[j]*B[i]*B[j]; 
      }
    }
  } else {
    for (i=1; i<=ntypes; i++) {
      for (j=1; j<=ntypes; j++) {
        for (k=0; k<=6; k++) {
          csumi[i] += neach_all[j]*B[7*i + k]*B[7*(j+1)-k-1];
          csumij += neach_all[i]*neach_all[j]*B[7*i + k]*B[7*(j+1)-k-1];
        }
      }
    }
  }    

  delete [] neach;
  delete [] neach_all;
}

/* ----------------------------------------------------------------------
   adjust g_ewald_6 to the new grid size
------------------------------------------------------------------------- */

void PPPMDisp::adjust_gewald_6()
{
  // Use Newton solver to find g_ewald_6
  double dx;

  // Start loop

  for (int i = 0; i <  LARGE; i++) {
    dx = f_6() / derivf_6();
    g_ewald_6 -= dx; //update g_ewald_6
    if (fabs(f_6()) < SMALL) return;
  }

  // Failed to converge

  char str[128];
  sprintf(str, "Could not adjust g_ewald_6");
  error->all(FLERR, str);

}

/* ----------------------------------------------------------------------
 Calculate f(x) for Dispersion interaction
 ------------------------------------------------------------------------- */

double PPPMDisp::f_6()
{
  double df_rspace, df_kspace;
  double *prd;

  if (triclinic == 0) prd = domain->prd;
  else prd = domain->prd_lamda;

  double xprd = prd[0];
  double yprd = prd[1];
  double zprd = prd[2];
  double zprd_slab = zprd*slab_volfactor;
  bigint natoms = atom->natoms;

  df_rspace = lj_rspace_error();
   
  double qopt = compute_qopt_6();
  df_kspace = sqrt(qopt/natoms)*csum/(xprd*yprd*zprd_slab);
   
  return df_rspace - df_kspace;
}

/* ----------------------------------------------------------------------
 Calculate numerical derivative f'(x) using forward difference
 [f(x + h) - f(x)] / h
 ------------------------------------------------------------------------- */
            
double PPPMDisp::derivf_6()
{  
  double h = 0.000001;  //Derivative step-size
  double df,f1,f2,g_ewald_old;
  
  f1 = f_6();
  g_ewald_old = g_ewald_6;
  g_ewald_6 += h;
  f2 = f_6();
  g_ewald_6 = g_ewald_old;
  df = (f2 - f1)/h;
  
  return df;
} 


/* ----------------------------------------------------------------------
   calculate an initial value for g_ewald_6
   ---------------------------------------------------------------------- */

void PPPMDisp::set_init_g6()
{
  // use xprd,yprd,zprd even if triclinic so grid size is the same
  // adjust z dimension for 2d slab PPPM
  // 3d PPPM just uses zprd since slab_volfactor = 1.0

  // make initial g_ewald estimate
  // based on desired error and real space cutoff
 
  // compute initial value for df_real with g_ewald_6 = 1/cutoff_lj
  // if df_real > 0, repeat divide g_ewald_6 by 2 until df_real < 0
  // else, repeat multiply g_ewald_6 by 2 until df_real > 0
  // perform bisection for the last two values of
  double df_real;
  double g_ewald_old; 
  double gmin, gmax;

  g_ewald_6 = 1.0/cutoff_lj;
  df_real = lj_rspace_error() - accuracy;
  int counter = 0;
  if (df_real > 0) {
    while (df_real > 0 && counter < LARGE) {
      counter++;
      g_ewald_old = g_ewald_6;
      g_ewald_6 *= 2;
      df_real = lj_rspace_error() - accuracy;
    }
  }

  if (df_real < 0) {
    while (df_real < 0 && counter < LARGE) {
      counter++;
      g_ewald_old = g_ewald_6;
      g_ewald_6 *= 0.5;
      df_real = lj_rspace_error() - accuracy;
    }
  }

  if (counter >= LARGE-1) error->all(FLERR,"Cannot compute initial g_ewald_disp");

  gmin = MIN(g_ewald_6, g_ewald_old);
  gmax = MAX(g_ewald_6, g_ewald_old);
  g_ewald_6 = gmin + 0.5*(gmax-gmin);
  counter = 0;
  while (gmax-gmin > SMALL && counter < LARGE) {
    counter++;
    df_real = lj_rspace_error() -accuracy;
    if (df_real < 0) gmax = g_ewald_6;
    else gmin = g_ewald_6;
    g_ewald_6 = gmin + 0.5*(gmax-gmin);
  }
  if (counter >= LARGE-1) error->all(FLERR,"Cannot compute initial g_ewald_disp");

}

/* ----------------------------------------------------------------------
   calculate nx_pppm, ny_pppm, nz_pppm for dispersion interaction
   ---------------------------------------------------------------------- */

void PPPMDisp::set_n_pppm_6()
{
  bigint natoms = atom->natoms;

  double *prd;

  if (triclinic == 0) prd = domain->prd;
  else prd = domain->prd_lamda;

  double xprd = prd[0];
  double yprd = prd[1];
  double zprd = prd[2];
  double zprd_slab = zprd*slab_volfactor;
  double h, h_x,h_y,h_z;

  // initial value for the grid spacing
  h = h_x = h_y = h_z = 4.0/g_ewald_6;
  // decrease grid spacing untill required precision is obtained
  int count = 0;
  while(1) {
  
    // set grid dimension
    nx_pppm_6 = static_cast<int> (xprd/h_x);
    ny_pppm_6 = static_cast<int> (yprd/h_y);
    nz_pppm_6 = static_cast<int> (zprd_slab/h_z);

    if (nx_pppm_6 <= 1) nx_pppm_6 = 2;
    if (ny_pppm_6 <= 1) ny_pppm_6 = 2;
    if (nz_pppm_6 <= 1) nz_pppm_6 = 2;

    //set local grid dimension
    int npey_fft,npez_fft;
    if (nz_pppm_6 >= nprocs) {
      npey_fft = 1;
      npez_fft = nprocs;
    } else procs2grid2d(nprocs,ny_pppm_6,nz_pppm_6,&npey_fft,&npez_fft);

    int me_y = me % npey_fft;
    int me_z = me / npey_fft;

    nxlo_fft_6 = 0;
    nxhi_fft_6 = nx_pppm_6 - 1;
    nylo_fft_6 = me_y*ny_pppm_6/npey_fft;
    nyhi_fft_6 = (me_y+1)*ny_pppm_6/npey_fft - 1;
    nzlo_fft_6 = me_z*nz_pppm_6/npez_fft;
    nzhi_fft_6 = (me_z+1)*nz_pppm_6/npez_fft - 1;

    double qopt = compute_qopt_6();
 
    double df_kspace = sqrt(qopt/natoms)*csum/(xprd*yprd*zprd_slab);

    count++;

    // break loop if the accuracy has been reached or too many loops have been performed
    if (df_kspace <= accuracy) break;
    if (count > 500) error->all(FLERR, "Could not compute grid size for Dispersion!");
    h *= 0.95;
    h_x = h_y = h_z = h;
  }
}

/* ----------------------------------------------------------------------
   calculate the real space error for dispersion interactions
   ---------------------------------------------------------------------- */

double PPPMDisp::lj_rspace_error()
{
  bigint natoms = atom->natoms;
  double xprd = domain->xprd;
  double yprd = domain->yprd;
  double zprd = domain->zprd;
  double zprd_slab = zprd*slab_volfactor;

  double deltaf;
  double rgs = (cutoff_lj*g_ewald_6);
  rgs *= rgs;
  double rgs_inv = 1.0/rgs;
  deltaf = csum/sqrt(natoms*xprd*yprd*zprd_slab*cutoff_lj)*sqrt(MY_PI)*pow(g_ewald_6, 5)*
    exp(-rgs)*(1+rgs_inv*(3+rgs_inv*(6+rgs_inv*6)));
  return deltaf;
}

/* ----------------------------------------------------------------------
   make all preperations for later being able to rapidely split the
   fourier transformed vectors
------------------------------------------------------------------------- */

void PPPMDisp::prepare_splitting()
{
  // allocate vectors
  // communication = stores how many points are exchanged with each processor
  // com_matrix, com_matrix_all = communication matrix between the processors
  // fftpoins stores the maximum and minimum value of the fft points of each proc
  int *communication;
  int **com_matrix;
  int **com_matrix_all;
  int **fftpoints;
 
  memory->create(communication, nprocs, "pppm/disp:communication");
  memory->create(com_matrix, nprocs, nprocs, "pppm/disp:com_matrix");
  memory->create(com_matrix_all, nprocs, nprocs, "pppm/disp:com_matrix_all");
  memory->create(fftpoints, nprocs, 4, "pppm/disp:fftpoints");
  memset(&(com_matrix[0][0]), 0, nprocs*nprocs*sizeof(int));
  memset(communication, 0, nprocs*sizeof(int));
 
  //// loop over all values of me to determine the fft_points

  int npey_fft,npez_fft;
  if (nz_pppm_6 >= nprocs) {
    npey_fft = 1;
    npez_fft = nprocs;
  } else procs2grid2d(nprocs,ny_pppm_6,nz_pppm_6,&npey_fft,&npez_fft);

  int me_y = me % npey_fft;
  int me_z = me / npey_fft;

  int i,m,n;
  for (m = 0; m < nprocs; m++) {
    me_y = m % npey_fft;
    me_z = m / npey_fft;
    fftpoints[m][0] = me_y*ny_pppm_6/npey_fft;
    fftpoints[m][1] = (me_y+1)*ny_pppm_6/npey_fft - 1;
    fftpoints[m][2] = me_z*nz_pppm_6/npez_fft;
    fftpoints[m][3] = (me_z+1)*nz_pppm_6/npez_fft - 1;
  }

  //// loop over all local fft points to find out on which processor its counterpart is!
  int x1,y1,z1,x2,y2,z2;
  for (x1 = nxlo_fft_6; x1 <= nxhi_fft_6; x1++)
    for (y1 = nylo_fft_6; y1 <= nyhi_fft_6; y1++) {
      y2 = (ny_pppm_6 - y1) % ny_pppm_6;
      for (z1 = nzlo_fft_6; z1 <= nzhi_fft_6; z1++) {
        z2 = (nz_pppm_6 - z1) % nz_pppm_6;
        m = -1;
        while (1) {
          m++;
          if (y2 >= fftpoints[m][0] && y2 <= fftpoints[m][1] &&
              z2 >= fftpoints[m][2] && z2 <= fftpoints[m][3] ) break;
        }
        communication[m]++;
        com_matrix[m][me] = 1;
        com_matrix[me][m] = 1;
      }
    }

  //// set com_max and com_procs
  //// com_max = maximum amount of points that have to be communicated with a processor
  //// com_procs = number of processors with which has to be communicated
  com_max = 0;
  com_procs = 0;
  for (m = 0; m < nprocs; m++) {
    com_max = MAX(com_max, communication[m]);
    com_procs += com_matrix[me][m];
  }
  if (!com_matrix[me][me]) com_procs++;
 
  //// allocate further vectors
  memory->create(splitbuf1, com_procs, com_max*2, "pppm/disp:splitbuf1");
  memory->create(splitbuf2, com_procs, com_max*2, "pppm/disp:splitbuf2");
  memory->create(dict_send, nfft_6, 2, "pppm/disp:dict_send");
  memory->create(dict_rec,com_procs, com_max, "pppm/disp:dict_rec");
  memory->create(com_each, com_procs, "pppm/disp:com_each");
  memory->create(com_order, com_procs, "pppm/disp:com_order");
 
  //// exchange communication matrix between the procs
  if (nprocs > 1){
    for (m = 0; m < nprocs; m++) MPI_Allreduce(com_matrix[m],
      com_matrix_all[m], nprocs, MPI_INT, MPI_MAX, world);
  }
  //// determine the communication order!!!

  split_order(com_matrix_all);
 
  //// fill com_each
  for (i = 0; i < com_procs; i++) com_each[i] = 2*communication[com_order[i]];
 
  int *com_send;
  memory->create(com_send, com_procs, "pppm/disp:com_send");
  memset(com_send, 0, com_procs*sizeof(int));
  int **changelist;
  memory->create(changelist, nfft_6, 5, "pppm/disp:changelist");
  int whichproc;
 
  //// loop over mesh points to fill dict_send
  n = 0;
  for (z1 = nzlo_fft_6; z1 <= nzhi_fft_6; z1++) {
    z2 = (nz_pppm_6 - z1) % nz_pppm_6;
    for (y1 = nylo_fft_6; y1 <= nyhi_fft_6; y1++) {
      y2 = (ny_pppm_6 - y1) % ny_pppm_6;
      for (x1 = nxlo_fft_6; x1 <= nxhi_fft_6; x1++) {
        x2 = (nx_pppm_6 - x1) % nx_pppm_6;
        m = -1;
        while (1) {
          m++;
          if (y2 >= fftpoints[m][0] && y2 <= fftpoints[m][1] &&
              z2 >= fftpoints[m][2] && z2 <= fftpoints[m][3] ) break;
        }
        whichproc = -1;
        while (1) {
          whichproc++;
          if (m == com_order[whichproc]) break;
	}
        dict_send[n][0] = whichproc;
        dict_send[n][1] = 2*com_send[whichproc]++;
        changelist[n][0] = x2;
        changelist[n][1] = y2;
        changelist[n][2] = z2;
        changelist[n][3] = n;;
        changelist[n++][4] = whichproc;
      }
    }
  }

  //// change the order of changelist
  int changed;
  int help;
  int j, k, l;
  for ( l = 0; l < 3; l++) {
    k = nfft_6;
    changed = 1;
    while (k > 1 && changed == 1) {
      changed = 0;
      for (i = 0; i < k-1; i++) {
        if (changelist[i][l] > changelist[i+1][l]){
          for (j = 0; j < 5; j++) {
            help = changelist[i][j];
            changelist[i][j] = changelist[i+1][j];
            changelist[i+1][j] = help;
	  }
          changed = 1;
        }
      }
      k = k - 1;
    }
  }

  //// determine the values for dict_rec
  memset(com_send, 0, com_procs*sizeof(int));
  for (n = 0; n < nfft_6; n++) {
    whichproc = changelist[n][4];
    dict_rec[whichproc][com_send[whichproc]++] = 2*changelist[n][3];
  }

  memory->destroy(communication);
  memory->destroy(com_matrix);
  memory->destroy(com_matrix_all);
  memory->destroy(fftpoints);
  memory->destroy(com_send);
  memory->destroy(changelist);
}

/* ----------------------------------------------------------------------
   Compyute the modified (hockney-eastwood) coulomb green function
   ---------------------------------------------------------------------- */ 

void PPPMDisp::compute_gf()
{
  int k,l,m,n;
  double *prd;

  if (triclinic == 0) prd = domain->prd;
  else prd = domain->prd_lamda;

  double xprd = prd[0];
  double yprd = prd[1];
  double zprd = prd[2];
  double zprd_slab = zprd*slab_volfactor;
  volume = xprd * yprd * zprd_slab;

  double unitkx = (2.0*MY_PI/xprd);
  double unitky = (2.0*MY_PI/yprd);
  double unitkz = (2.0*MY_PI/zprd_slab);

  int kper,lper,mper;
  double snx,sny,snz,snx2,sny2,snz2;
  double sqk;
  double argx,argy,argz,wx,wy,wz,sx,sy,sz,qx,qy,qz;
  double numerator,denominator;


  n = 0;
  for (m = nzlo_fft; m <= nzhi_fft; m++) {
    mper = m - nz_pppm*(2*m/nz_pppm);
    qz = unitkz*mper;
    snz = sin(0.5*qz*zprd_slab/nz_pppm);
    snz2 = snz*snz;
    sz = exp(-0.25*pow(qz/g_ewald,2.0));
    wz = 1.0;
    argz = 0.5*qz*zprd_slab/nz_pppm;
    if (argz != 0.0) wz = pow(sin(argz)/argz,order);
    wz *= wz;

    for (l = nylo_fft; l <= nyhi_fft; l++) {
      lper = l - ny_pppm*(2*l/ny_pppm);
      qy = unitky*lper;
      sny = sin(0.5*qy*yprd/ny_pppm);
      sny2 = sny*sny;
      sy = exp(-0.25*pow(qy/g_ewald,2.0));
      wy = 1.0;
      argy = 0.5*qy*yprd/ny_pppm;
      if (argy != 0.0) wy = pow(sin(argy)/argy,order);
      wy *= wy;

      for (k = nxlo_fft; k <= nxhi_fft; k++) {
        kper = k - nx_pppm*(2*k/nx_pppm);
        qx = unitkx*kper;
        snx = sin(0.5*qx*xprd/nx_pppm);
        snx2 = snx*snx;
        sx = exp(-0.25*pow(qx/g_ewald,2.0));
        wx = 1.0;
        argx = 0.5*qx*xprd/nx_pppm;
        if (argx != 0.0) wx = pow(sin(argx)/argx,order);
        wx *= wx;

        sqk = pow(qx,2.0) + pow(qy,2.0) + pow(qz,2.0);

        if (sqk != 0.0) {
          numerator = 4.0*MY_PI/sqk;
          denominator = gf_denom(snx2,sny2,snz2, gf_b, order);  
          greensfn[n++] = numerator*sx*sy*sz*wx*wy*wz/denominator;
        } else greensfn[n++] = 0.0;
      }
    }
  }
}

/* ----------------------------------------------------------------------
   compute self force coefficients for ad-differentiation scheme
   and Coulomb interaction 
------------------------------------------------------------------------- */

void PPPMDisp::compute_sf_precoeff(int nxp, int nyp, int nzp, int ord, 
                                    int nxlo_ft, int nylo_ft, int nzlo_ft,
                                    int nxhi_ft, int nyhi_ft, int nzhi_ft,
                                    double *sf_pre1, double *sf_pre2, double *sf_pre3,
                                    double *sf_pre4, double *sf_pre5, double *sf_pre6)
{

  int i,k,l,m,n;
  double *prd;

  // volume-dependent factors
  // adjust z dimension for 2d slab PPPM
  // z dimension for 3d PPPM is zprd since slab_volfactor = 1.0

  if (triclinic == 0) prd = domain->prd;
  else prd = domain->prd_lamda;

  double xprd = prd[0];
  double yprd = prd[1];
  double zprd = prd[2];
  double zprd_slab = zprd*slab_volfactor;

  double unitkx = (2.0*MY_PI/xprd);
  double unitky = (2.0*MY_PI/yprd);
  double unitkz = (2.0*MY_PI/zprd_slab);

  int nx,ny,nz,kper,lper,mper;
  double argx,argy,argz;
  double wx0[5],wy0[5],wz0[5],wx1[5],wy1[5],wz1[5],wx2[5],wy2[5],wz2[5];
  double qx0,qy0,qz0,qx1,qy1,qz1,qx2,qy2,qz2;
  double u0,u1,u2,u3,u4,u5,u6;
  double sum1,sum2,sum3,sum4,sum5,sum6;

  int nb = 2;

  n = 0;
  for (m = nzlo_ft; m <= nzhi_ft; m++) {
    mper = m - nzp*(2*m/nzp);

    for (l = nylo_ft; l <= nyhi_ft; l++) {
      lper = l - nyp*(2*l/nyp);

      for (k = nxlo_ft; k <= nxhi_ft; k++) {
        kper = k - nxp*(2*k/nxp);
      
        sum1 = sum2 = sum3 = sum4 = sum5 = sum6 = 0.0;
        for (i = -nb; i <= nb; i++) {

          qx0 = unitkx*(kper+nxp*i);
          qx1 = unitkx*(kper+nxp*(i+1));
          qx2 = unitkx*(kper+nxp*(i+2));
          wx0[i+2] = 1.0;
          wx1[i+2] = 1.0;
          wx2[i+2] = 1.0;
          argx = 0.5*qx0*xprd/nxp;
          if (argx != 0.0) wx0[i+2] = pow(sin(argx)/argx,ord);
          argx = 0.5*qx1*xprd/nxp;
          if (argx != 0.0) wx1[i+2] = pow(sin(argx)/argx,ord);
          argx = 0.5*qx2*xprd/nxp;
          if (argx != 0.0) wx2[i+2] = pow(sin(argx)/argx,ord);

          qy0 = unitky*(lper+nyp*i);
          qy1 = unitky*(lper+nyp*(i+1));
          qy2 = unitky*(lper+nyp*(i+2));
          wy0[i+2] = 1.0;
          wy1[i+2] = 1.0;
          wy2[i+2] = 1.0;
          argy = 0.5*qy0*yprd/nyp;
          if (argy != 0.0) wy0[i+2] = pow(sin(argy)/argy,ord);
          argy = 0.5*qy1*yprd/nyp;
          if (argy != 0.0) wy1[i+2] = pow(sin(argy)/argy,ord);
          argy = 0.5*qy2*yprd/nyp;
          if (argy != 0.0) wy2[i+2] = pow(sin(argy)/argy,ord);
   
          qz0 = unitkz*(mper+nzp*i);
          qz1 = unitkz*(mper+nzp*(i+1));
          qz2 = unitkz*(mper+nzp*(i+2));
          wz0[i+2] = 1.0;
          wz1[i+2] = 1.0;
          wz2[i+2] = 1.0;
          argz = 0.5*qz0*zprd_slab/nzp;
          if (argz != 0.0) wz0[i+2] = pow(sin(argz)/argz,ord);
          argz = 0.5*qz1*zprd_slab/nzp;
          if (argz != 0.0) wz1[i+2] = pow(sin(argz)/argz,ord);
           argz = 0.5*qz2*zprd_slab/nzp;
          if (argz != 0.0) wz2[i+2] = pow(sin(argz)/argz,ord);
        }
    
        for (nx = 0; nx <= 4; nx++) {
          for (ny = 0; ny <= 4; ny++) {
            for (nz = 0; nz <= 4; nz++) {
              u0 = wx0[nx]*wy0[ny]*wz0[nz];
              u1 = wx1[nx]*wy0[ny]*wz0[nz];
              u2 = wx2[nx]*wy0[ny]*wz0[nz];
              u3 = wx0[nx]*wy1[ny]*wz0[nz];
              u4 = wx0[nx]*wy2[ny]*wz0[nz];
              u5 = wx0[nx]*wy0[ny]*wz1[nz];
              u6 = wx0[nx]*wy0[ny]*wz2[nz];

              sum1 += u0*u1;
              sum2 += u0*u2;
              sum3 += u0*u3;
              sum4 += u0*u4;
              sum5 += u0*u5;
              sum6 += u0*u6;
            }
          }
        }
        
        // store values

        sf_pre1[n] = sum1;
        sf_pre2[n] = sum2;
        sf_pre3[n] = sum3;
        sf_pre4[n] = sum4;
        sf_pre5[n] = sum5;
        sf_pre6[n++] = sum6;
      }
    }
  }
}

/* ----------------------------------------------------------------------
   Compute the modified (hockney-eastwood) dispersion green function
   ---------------------------------------------------------------------- */

void PPPMDisp::compute_gf_6()
{
  double *prd;
  int k,l,m,n;

  // volume-dependent factors
  // adjust z dimension for 2d slab PPPM
  // z dimension for 3d PPPM is zprd since slab_volfactor = 1.0

  if (triclinic == 0) prd = domain->prd;
  else prd = domain->prd_lamda;

  double xprd = prd[0];
  double yprd = prd[1];
  double zprd = prd[2];
  double zprd_slab = zprd*slab_volfactor;

  double unitkx = (2.0*MY_PI/xprd);
  double unitky = (2.0*MY_PI/yprd);
  double unitkz = (2.0*MY_PI/zprd_slab);

  int kper,lper,mper;
  double sqk;
  double snx,sny,snz,snx2,sny2,snz2;
  double argx,argy,argz,wx,wy,wz,sx,sy,sz;
  double qx,qy,qz;
  double rtsqk, term;
  double numerator,denominator;
  double inv2ew = 2*g_ewald_6;
  inv2ew = 1/inv2ew;
  double rtpi = sqrt(MY_PI);

  numerator = -MY_PI*rtpi*g_ewald_6*g_ewald_6*g_ewald_6/(3.0);

  n = 0;
  for (m = nzlo_fft_6; m <= nzhi_fft_6; m++) {
    mper = m - nz_pppm_6*(2*m/nz_pppm_6);
    qz = unitkz*mper;
    snz = sin(0.5*unitkz*mper*zprd_slab/nz_pppm_6);
    snz2 = snz*snz;
    sz = exp(-qz*qz*inv2ew*inv2ew);
    wz = 1.0;
    argz = 0.5*qz*zprd_slab/nz_pppm_6;
    if (argz != 0.0) wz = pow(sin(argz)/argz,order_6);
    wz *= wz;
              
    for (l = nylo_fft_6; l <= nyhi_fft_6; l++) {
      lper = l - ny_pppm_6*(2*l/ny_pppm_6);
      qy = unitky*lper;
      sny = sin(0.5*unitky*lper*yprd/ny_pppm_6);
      sny2 = sny*sny;
      sy = exp(-qy*qy*inv2ew*inv2ew);
      wy = 1.0;
      argy = 0.5*qy*yprd/ny_pppm_6;
      if (argy != 0.0) wy = pow(sin(argy)/argy,order_6);
      wy *= wy;

      for (k = nxlo_fft_6; k <= nxhi_fft_6; k++) {
	kper = k - nx_pppm_6*(2*k/nx_pppm_6);
        qx = unitkx*kper;
	snx = sin(0.5*unitkx*kper*xprd/nx_pppm_6);
	snx2 = snx*snx;
        sx = exp(-qx*qx*inv2ew*inv2ew);
	wx = 1.0;
	argx = 0.5*qx*xprd/nx_pppm_6;
	if (argx != 0.0) wx = pow(sin(argx)/argx,order_6);
        wx *= wx;
      
	sqk = pow(qx,2.0) + pow(qy,2.0) + pow(qz,2.0);

	if (sqk != 0.0) {
	  denominator = gf_denom(snx2,sny2,snz2, gf_b_6, order_6); 
	  rtsqk = sqrt(sqk);
          term = (1-2*sqk*inv2ew*inv2ew)*sx*sy*sz +
                 2*sqk*rtsqk*inv2ew*inv2ew*inv2ew*rtpi*erfc(rtsqk*inv2ew);
	  greensfn_6[n++] = numerator*term*wx*wy*wz/denominator;
	} else greensfn_6[n++] = 0.0;
      }
    }
  }
}

/* ----------------------------------------------------------------------
   compute self force coefficients for ad-differentiation scheme
   and Coulomb interaction 
------------------------------------------------------------------------- */
void PPPMDisp::compute_sf_coeff()
{
  int i,k,l,m,n;
  double *prd;

  if (triclinic == 0) prd = domain->prd;
  else prd = domain->prd_lamda;

  double xprd = prd[0];
  double yprd = prd[1];
  double zprd = prd[2];
  double zprd_slab = zprd*slab_volfactor;
  volume = xprd * yprd * zprd_slab;

  for (i = 0; i <= 5; i++) sf_coeff[i] = 0.0;

  n = 0;
  for (m = nzlo_fft; m <= nzhi_fft; m++) {
    for (l = nylo_fft; l <= nyhi_fft; l++) {
      for (k = nxlo_fft; k <= nxhi_fft; k++) {
        sf_coeff[0] += sf_precoeff1[n]*greensfn[n];
        sf_coeff[1] += sf_precoeff2[n]*greensfn[n];
        sf_coeff[2] += sf_precoeff3[n]*greensfn[n];
        sf_coeff[3] += sf_precoeff4[n]*greensfn[n];
        sf_coeff[4] += sf_precoeff5[n]*greensfn[n];
        sf_coeff[5] += sf_precoeff6[n]*greensfn[n++];
      }
    }
  }

  // Compute the coefficients for the self-force correction

  double prex, prey, prez;
  prex = prey = prez = MY_PI/volume;
  prex *= nx_pppm/xprd;
  prey *= ny_pppm/yprd;
  prez *= nz_pppm/zprd_slab;
  sf_coeff[0] *= prex;
  sf_coeff[1] *= prex*2;
  sf_coeff[2] *= prey;
  sf_coeff[3] *= prey*2;
  sf_coeff[4] *= prez;
  sf_coeff[5] *= prez*2;

  // communicate values with other procs

  double tmp[6];
  MPI_Allreduce(sf_coeff,tmp,6,MPI_DOUBLE,MPI_SUM,world);
  for (n = 0; n < 6; n++) sf_coeff[n] = tmp[n];
}

/* ----------------------------------------------------------------------
   compute self force coefficients for ad-differentiation scheme
   and Dispersion interaction 
------------------------------------------------------------------------- */

void PPPMDisp::compute_sf_coeff_6()
{
  int i,k,l,m,n;
  double *prd;

  if (triclinic == 0) prd = domain->prd;
  else prd = domain->prd_lamda;

  double xprd = prd[0];
  double yprd = prd[1];
  double zprd = prd[2];
  double zprd_slab = zprd*slab_volfactor;
  volume = xprd * yprd * zprd_slab;

  for (i = 0; i <= 5; i++) sf_coeff_6[i] = 0.0;

  n = 0;
  for (m = nzlo_fft_6; m <= nzhi_fft_6; m++) {
    for (l = nylo_fft_6; l <= nyhi_fft_6; l++) {
      for (k = nxlo_fft_6; k <= nxhi_fft_6; k++) {
        sf_coeff_6[0] += sf_precoeff1_6[n]*greensfn_6[n];
        sf_coeff_6[1] += sf_precoeff2_6[n]*greensfn_6[n];
        sf_coeff_6[2] += sf_precoeff3_6[n]*greensfn_6[n];
        sf_coeff_6[3] += sf_precoeff4_6[n]*greensfn_6[n];
        sf_coeff_6[4] += sf_precoeff5_6[n]*greensfn_6[n];
        sf_coeff_6[5] += sf_precoeff6_6[n]*greensfn_6[n++];
      }
    }
  }

  
  // perform multiplication with prefactors
  
  double prex, prey, prez;
  prex = prey = prez = MY_PI/volume;
  prex *= nx_pppm_6/xprd;
  prey *= ny_pppm_6/yprd;
  prez *= nz_pppm_6/zprd_slab;
  sf_coeff_6[0] *= prex;
  sf_coeff_6[1] *= prex*2;
  sf_coeff_6[2] *= prey;
  sf_coeff_6[3] *= prey*2;
  sf_coeff_6[4] *= prez;
  sf_coeff_6[5] *= prez*2;
  
  // communicate values with other procs
  
  double tmp[6];
  MPI_Allreduce(sf_coeff_6,tmp,6,MPI_DOUBLE,MPI_SUM,world);
  for (n = 0; n < 6; n++) sf_coeff_6[n] = tmp[n];

}

/* ----------------------------------------------------------------------
   denominator for Hockney-Eastwood Green's function
     of x,y,z = sin(kx*deltax/2), etc

            inf                 n-1
   S(n,k) = Sum  W(k+pi*j)**2 = Sum b(l)*(z*z)**l
           j=-inf               l=0

          = -(z*z)**n /(2n-1)! * (d/dx)**(2n-1) cot(x)  at z = sin(x)
   gf_b = denominator expansion coeffs 
------------------------------------------------------------------------- */

double PPPMDisp::gf_denom(double x, double y, double z, double *g_b, int ord)
{
  double sx,sy,sz;
  sz = sy = sx = 0.0;
  for (int l = ord-1; l >= 0; l--) {
    sx = g_b[l] + sx*x;
    sy = g_b[l] + sy*y;
    sz = g_b[l] + sz*z;
  }
  double s = sx*sy*sz;
  return s*s;
}

/* ----------------------------------------------------------------------
   pre-compute Green's function denominator expansion coeffs, Gamma(2n) 
------------------------------------------------------------------------- */

void PPPMDisp::compute_gf_denom(double* gf, int ord)
{
  int k,l,m;
  
  for (l = 1; l < ord; l++) gf[l] = 0.0;
  gf[0] = 1.0;
  
  for (m = 1; m < ord; m++) {
    for (l = m; l > 0; l--) 
      gf[l] = 4.0 * (gf[l]*(l-m)*(l-m-0.5)-gf[l-1]*(l-m-1)*(l-m-1));
    gf[0] = 4.0 * (gf[0]*(l-m)*(l-m-0.5));
  }

  bigint ifact = 1;
  for (k = 1; k < 2*ord; k++) ifact *= k;
  double gaminv = 1.0/ifact;
  for (l = 0; l < ord; l++) gf[l] *= gaminv;
}

/* ----------------------------------------------------------------------
   ghost-swap to accumulate full density in brick decomposition 
   remap density from 3d brick decomposition to FFTdecomposition
   for coulomb interaction or dispersion interaction with geometric
   mixing
------------------------------------------------------------------------- */

void PPPMDisp::brick2fft(int nxlo_o, int nylo_o, int nzlo_o,
                          int nxhi_o, int nyhi_o, int nzhi_o,
                          int nxlo_i, int nylo_i, int nzlo_i,
                          int nxhi_i, int nyhi_i, int nzhi_i,
                          int nxlo_g, int nylo_g, int nzlo_g,
                          int nxhi_g, int nyhi_g, int nzhi_g,
                          FFT_SCALAR*** dbrick, FFT_SCALAR* dfft, FFT_SCALAR* work,
                          FFT_SCALAR* bf1, FFT_SCALAR* bf2, int nbf, LAMMPS_NS::Remap* rmp)
{
  int i,n,ix,iy,iz;
  MPI_Request request;
  MPI_Status status;

  // pack my ghosts for +x processor
  // pass data to self or +x processor
  // unpack and sum recv data into my real cells

  n = 0;
  for (iz = nzlo_o; iz <= nzhi_o; iz++)
    for (iy = nylo_o; iy <= nyhi_o; iy++)
      for (ix = nxhi_i+1; ix <= nxhi_o; ix++)
	bf1[n++] = dbrick[iz][iy][ix];

  if (comm->procneigh[0][1] == me)
    for (i = 0; i < n; i++) bf2[i] = bf1[i];
  else {
    MPI_Irecv(bf2,nbf,MPI_FFT_SCALAR,comm->procneigh[0][0],0,world,&request);
    MPI_Send(bf1,n,MPI_FFT_SCALAR,comm->procneigh[0][1],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzlo_o; iz <= nzhi_o; iz++)
    for (iy = nylo_o; iy <= nyhi_o; iy++)
      for (ix = nxlo_i; ix < nxlo_i+nxlo_g; ix++)
	dbrick[iz][iy][ix] += bf2[n++];

  // pack my ghosts for -x processor
  // pass data to self or -x processor
  // unpack and sum recv data into my real cells

  n = 0;
  for (iz = nzlo_o; iz <= nzhi_o; iz++)
    for (iy = nylo_o; iy <= nyhi_o; iy++)
      for (ix = nxlo_o; ix < nxlo_i; ix++)
	bf1[n++] = dbrick[iz][iy][ix];

  if (comm->procneigh[0][0] == me)
    for (i = 0; i < n; i++) bf2[i] = bf1[i];
  else {
    MPI_Irecv(bf2,nbf,MPI_FFT_SCALAR,comm->procneigh[0][1],0,world,&request);
    MPI_Send(bf1,n,MPI_FFT_SCALAR,comm->procneigh[0][0],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzlo_o; iz <= nzhi_o; iz++)
    for (iy = nylo_o; iy <= nyhi_o; iy++)
      for (ix = nxhi_i-nxhi_g+1; ix <= nxhi_i; ix++)
	dbrick[iz][iy][ix] += bf2[n++];

  // pack my ghosts for +y processor
  // pass data to self or +y processor
  // unpack and sum recv data into my real cells

  n = 0;
  for (iz = nzlo_o; iz <= nzhi_o; iz++)
    for (iy = nyhi_i+1; iy <= nyhi_o; iy++)
      for (ix = nxlo_i; ix <= nxhi_i; ix++)
	bf1[n++] = dbrick[iz][iy][ix];

  if (comm->procneigh[1][1] == me)
    for (i = 0; i < n; i++) bf2[i] = bf1[i];
  else {
    MPI_Irecv(bf2,nbf,MPI_FFT_SCALAR,comm->procneigh[1][0],0,world,&request);
    MPI_Send(bf1,n,MPI_FFT_SCALAR,comm->procneigh[1][1],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzlo_o; iz <= nzhi_o; iz++)
    for (iy = nylo_i; iy < nylo_i+nylo_g; iy++)
      for (ix = nxlo_i; ix <= nxhi_i; ix++)
	dbrick[iz][iy][ix] += bf2[n++];

  // pack my ghosts for -y processor
  // pass data to self or -y processor
  // unpack and sum recv data into my real cells

  n = 0;
  for (iz = nzlo_o; iz <= nzhi_o; iz++)
    for (iy = nylo_o; iy < nylo_i; iy++)
      for (ix = nxlo_i; ix <= nxhi_i; ix++)
	bf1[n++] = dbrick[iz][iy][ix];

  if (comm->procneigh[1][0] == me)
    for (i = 0; i < n; i++) bf2[i] = bf1[i];
  else {
    MPI_Irecv(bf2,nbf,MPI_FFT_SCALAR,comm->procneigh[1][1],0,world,&request);
    MPI_Send(bf1,n,MPI_FFT_SCALAR,comm->procneigh[1][0],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzlo_o; iz <= nzhi_o; iz++)
    for (iy = nyhi_i-nyhi_g+1; iy <= nyhi_i; iy++)
      for (ix = nxlo_i; ix <= nxhi_i; ix++)
	dbrick[iz][iy][ix] += bf2[n++];

  // pack my ghosts for +z processor
  // pass data to self or +z processor
  // unpack and sum recv data into my real cells

  n = 0;
  for (iz = nzhi_i+1; iz <= nzhi_o; iz++)
    for (iy = nylo_i; iy <= nyhi_i; iy++)
      for (ix = nxlo_i; ix <= nxhi_i; ix++)
	bf1[n++] = dbrick[iz][iy][ix];

  if (comm->procneigh[2][1] == me)
    for (i = 0; i < n; i++) bf2[i] = bf1[i];
  else {
    MPI_Irecv(bf2,nbf,MPI_FFT_SCALAR,comm->procneigh[2][0],0,world,&request);
    MPI_Send(bf1,n,MPI_FFT_SCALAR,comm->procneigh[2][1],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzlo_i; iz < nzlo_i+nzlo_g; iz++)
    for (iy = nylo_i; iy <= nyhi_i; iy++)
      for (ix = nxlo_i; ix <= nxhi_i; ix++)
	dbrick[iz][iy][ix] += bf2[n++];

  // pack my ghosts for -z processor
  // pass data to self or -z processor
  // unpack and sum recv data into my real cells

  n = 0;
  for (iz = nzlo_o; iz < nzlo_i; iz++)
    for (iy = nylo_i; iy <= nyhi_i; iy++)
      for (ix = nxlo_i; ix <= nxhi_i; ix++)
	bf1[n++] = dbrick[iz][iy][ix];

  if (comm->procneigh[2][0] == me)
    for (i = 0; i < n; i++) bf2[i] = bf1[i];
  else {
    MPI_Irecv(bf2,nbf,MPI_FFT_SCALAR,comm->procneigh[2][1],0,world,&request);
    MPI_Send(bf1,n,MPI_FFT_SCALAR,comm->procneigh[2][0],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzhi_i-nzhi_g+1; iz <= nzhi_i; iz++)
    for (iy = nylo_i; iy <= nyhi_i; iy++)
      for (ix = nxlo_i; ix <= nxhi_i; ix++)
	dbrick[iz][iy][ix] += bf2[n++];

  // remap from 3d brick decomposition to FFT decomposition
  // copy grabs inner portion of density from 3d brick
  // remap could be done as pre-stage of FFT,
  //   but this works optimally on only double values, not complex values

  n = 0;
  for (iz = nzlo_i; iz <= nzhi_i; iz++)
    for (iy = nylo_i; iy <= nyhi_i; iy++)
      for (ix = nxlo_i; ix <= nxhi_i; ix++)
	dfft[n++] = dbrick[iz][iy][ix];

  rmp->perform(dfft,dfft,work);
}


/* ----------------------------------------------------------------------
   ghost-swap to accumulate full density in brick decomposition 
   remap density from 3d brick decomposition to FFTdecomposition
   for dispersion with arithmetic mixing rule
------------------------------------------------------------------------- */

void PPPMDisp::brick2fft_a()
{
  int i,n,ix,iy,iz;
  MPI_Request request;
  MPI_Status status;

  // pack my ghosts for +x processor
  // pass data to self or +x processor
  // unpack and sum recv data into my real cells

  n = 0;
  for (iz = nzlo_out_6; iz <= nzhi_out_6; iz++)
    for (iy = nylo_out_6; iy <= nyhi_out_6; iy++)
      for (ix = nxhi_in_6+1; ix <= nxhi_out_6; ix++) {
        buf1_6[n++] = density_brick_a0[iz][iy][ix];
        buf1_6[n++] = density_brick_a1[iz][iy][ix];
        buf1_6[n++] = density_brick_a2[iz][iy][ix];
        buf1_6[n++] = density_brick_a3[iz][iy][ix];
        buf1_6[n++] = density_brick_a4[iz][iy][ix];
        buf1_6[n++] = density_brick_a5[iz][iy][ix];
        buf1_6[n++] = density_brick_a6[iz][iy][ix];
      }


  if (comm->procneigh[0][1] == me)
    for (i = 0; i < n; i++) buf2_6[i] = buf1_6[i];
  else {
    MPI_Irecv(buf2_6,7*nbuf_6,MPI_FFT_SCALAR,comm->procneigh[0][0],0,world,&request);
    MPI_Send(buf1_6,n,MPI_FFT_SCALAR,comm->procneigh[0][1],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzlo_out_6; iz <= nzhi_out_6; iz++)
    for (iy = nylo_out_6; iy <= nyhi_out_6; iy++)
      for (ix = nxlo_in_6; ix < nxlo_in_6+nxlo_ghost_6; ix++) {
        density_brick_a0[iz][iy][ix] += buf2_6[n++];
        density_brick_a1[iz][iy][ix] += buf2_6[n++];
        density_brick_a2[iz][iy][ix] += buf2_6[n++];
        density_brick_a3[iz][iy][ix] += buf2_6[n++];
        density_brick_a4[iz][iy][ix] += buf2_6[n++];
        density_brick_a5[iz][iy][ix] += buf2_6[n++];
        density_brick_a6[iz][iy][ix] += buf2_6[n++];
      }

  // pack my ghosts for -x processor
  // pass data to self or -x processor
  // unpack and sum recv data into my real cells

  n = 0;
  for (iz = nzlo_out_6; iz <= nzhi_out_6; iz++)
    for (iy = nylo_out_6; iy <= nyhi_out_6; iy++)
      for (ix = nxlo_out_6; ix < nxlo_in_6; ix++) {
        buf1_6[n++] = density_brick_a0[iz][iy][ix];
        buf1_6[n++] = density_brick_a1[iz][iy][ix];
        buf1_6[n++] = density_brick_a2[iz][iy][ix];
        buf1_6[n++] = density_brick_a3[iz][iy][ix];
        buf1_6[n++] = density_brick_a4[iz][iy][ix];
        buf1_6[n++] = density_brick_a5[iz][iy][ix];
        buf1_6[n++] = density_brick_a6[iz][iy][ix];
      }

  if (comm->procneigh[0][0] == me)
    for (i = 0; i < n; i++) buf2_6[i] = buf1_6[i];
  else {
    MPI_Irecv(buf2_6,7*nbuf_6,MPI_FFT_SCALAR,comm->procneigh[0][1],0,world,&request);
    MPI_Send(buf1_6,n,MPI_FFT_SCALAR,comm->procneigh[0][0],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzlo_out_6; iz <= nzhi_out_6; iz++)
    for (iy = nylo_out_6; iy <= nyhi_out_6; iy++)
      for (ix = nxhi_in_6-nxhi_ghost_6+1; ix <= nxhi_in_6; ix++) {
        density_brick_a0[iz][iy][ix] += buf2_6[n++];
        density_brick_a1[iz][iy][ix] += buf2_6[n++];
        density_brick_a2[iz][iy][ix] += buf2_6[n++];
        density_brick_a3[iz][iy][ix] += buf2_6[n++];
        density_brick_a4[iz][iy][ix] += buf2_6[n++];
        density_brick_a5[iz][iy][ix] += buf2_6[n++];
        density_brick_a6[iz][iy][ix] += buf2_6[n++];
      }

  // pack my ghosts for +y processor
  // pass data to self or +y processor
  // unpack and sum recv data into my real cells

  n = 0;
  for (iz = nzlo_out_6; iz <= nzhi_out_6; iz++)
    for (iy = nyhi_in_6+1; iy <= nyhi_out_6; iy++)
      for (ix = nxlo_in_6; ix <= nxhi_in_6; ix++) {
        buf1_6[n++] = density_brick_a0[iz][iy][ix];
        buf1_6[n++] = density_brick_a1[iz][iy][ix];
        buf1_6[n++] = density_brick_a2[iz][iy][ix];
        buf1_6[n++] = density_brick_a3[iz][iy][ix];
        buf1_6[n++] = density_brick_a4[iz][iy][ix];
        buf1_6[n++] = density_brick_a5[iz][iy][ix];
        buf1_6[n++] = density_brick_a6[iz][iy][ix];
      }

  if (comm->procneigh[1][1] == me)
    for (i = 0; i < n; i++) buf2_6[i] = buf1_6[i];
  else {
    MPI_Irecv(buf2_6,7*nbuf_6,MPI_FFT_SCALAR,comm->procneigh[1][0],0,world,&request);
    MPI_Send(buf1_6,n,MPI_FFT_SCALAR,comm->procneigh[1][1],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzlo_out_6; iz <= nzhi_out_6; iz++)
    for (iy = nylo_in_6; iy < nylo_in_6+nylo_ghost_6; iy++)
      for (ix = nxlo_in_6; ix <= nxhi_in_6; ix++) {
        density_brick_a0[iz][iy][ix] += buf2_6[n++];
        density_brick_a1[iz][iy][ix] += buf2_6[n++];
        density_brick_a2[iz][iy][ix] += buf2_6[n++];
        density_brick_a3[iz][iy][ix] += buf2_6[n++];
        density_brick_a4[iz][iy][ix] += buf2_6[n++];
        density_brick_a5[iz][iy][ix] += buf2_6[n++];
        density_brick_a6[iz][iy][ix] += buf2_6[n++];
      }

  // pack my ghosts for -y processor
  // pass data to self or -y processor
  // unpack and sum recv data into my real cells

  n = 0;
  for (iz = nzlo_out_6; iz <= nzhi_out_6; iz++)
    for (iy = nylo_out_6; iy < nylo_in_6; iy++)
      for (ix = nxlo_in_6; ix <= nxhi_in_6; ix++) {
        buf1_6[n++] = density_brick_a0[iz][iy][ix];
        buf1_6[n++] = density_brick_a1[iz][iy][ix];
        buf1_6[n++] = density_brick_a2[iz][iy][ix];
        buf1_6[n++] = density_brick_a3[iz][iy][ix];
        buf1_6[n++] = density_brick_a4[iz][iy][ix];
        buf1_6[n++] = density_brick_a5[iz][iy][ix];
        buf1_6[n++] = density_brick_a6[iz][iy][ix];
      }

  if (comm->procneigh[1][0] == me)
    for (i = 0; i < n; i++) buf2_6[i] = buf1_6[i];
  else {
    MPI_Irecv(buf2_6,7*nbuf_6,MPI_FFT_SCALAR,comm->procneigh[1][1],0,world,&request);
    MPI_Send(buf1_6,n,MPI_FFT_SCALAR,comm->procneigh[1][0],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzlo_out_6; iz <= nzhi_out_6; iz++)
    for (iy = nyhi_in_6-nyhi_ghost_6+1; iy <= nyhi_in_6; iy++)
      for (ix = nxlo_in_6; ix <= nxhi_in_6; ix++)  {
        density_brick_a0[iz][iy][ix] += buf2_6[n++];
        density_brick_a1[iz][iy][ix] += buf2_6[n++];
        density_brick_a2[iz][iy][ix] += buf2_6[n++];
        density_brick_a3[iz][iy][ix] += buf2_6[n++];
        density_brick_a4[iz][iy][ix] += buf2_6[n++];
        density_brick_a5[iz][iy][ix] += buf2_6[n++];
        density_brick_a6[iz][iy][ix] += buf2_6[n++];
      }

  // pack my ghosts for +z processor
  // pass data to self or +z processor
  // unpack and sum recv data into my real cells

  n = 0;
  for (iz = nzhi_in_6+1; iz <= nzhi_out_6; iz++)
    for (iy = nylo_in_6; iy <= nyhi_in_6; iy++)
      for (ix = nxlo_in_6; ix <= nxhi_in_6; ix++) {
        buf1_6[n++] = density_brick_a0[iz][iy][ix];
        buf1_6[n++] = density_brick_a1[iz][iy][ix];
        buf1_6[n++] = density_brick_a2[iz][iy][ix];
        buf1_6[n++] = density_brick_a3[iz][iy][ix];
        buf1_6[n++] = density_brick_a4[iz][iy][ix];
        buf1_6[n++] = density_brick_a5[iz][iy][ix];
        buf1_6[n++] = density_brick_a6[iz][iy][ix];
      }

  if (comm->procneigh[2][1] == me)
    for (i = 0; i < n; i++) buf2_6[i] = buf1_6[i];
  else {
    MPI_Irecv(buf2_6,7*nbuf_6,MPI_FFT_SCALAR,comm->procneigh[2][0],0,world,&request);
    MPI_Send(buf1_6,n,MPI_FFT_SCALAR,comm->procneigh[2][1],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzlo_in_6; iz < nzlo_in_6+nzlo_ghost_6; iz++)
    for (iy = nylo_in_6; iy <= nyhi_in_6; iy++)
      for (ix = nxlo_in_6; ix <= nxhi_in_6; ix++) {
        density_brick_a0[iz][iy][ix] += buf2_6[n++];
        density_brick_a1[iz][iy][ix] += buf2_6[n++];
        density_brick_a2[iz][iy][ix] += buf2_6[n++];
        density_brick_a3[iz][iy][ix] += buf2_6[n++];
        density_brick_a4[iz][iy][ix] += buf2_6[n++];
        density_brick_a5[iz][iy][ix] += buf2_6[n++];
        density_brick_a6[iz][iy][ix] += buf2_6[n++];
      }
  // pack my ghosts for -z processor
  // pass data to self or -z processor
  // unpack and sum recv data into my real cells

  n = 0;
  for (iz = nzlo_out_6; iz < nzlo_in_6; iz++)
    for (iy = nylo_in_6; iy <= nyhi_in_6; iy++)
      for (ix = nxlo_in_6; ix <= nxhi_in_6; ix++) {
        buf1_6[n++] = density_brick_a0[iz][iy][ix];
        buf1_6[n++] = density_brick_a1[iz][iy][ix];
        buf1_6[n++] = density_brick_a2[iz][iy][ix];
        buf1_6[n++] = density_brick_a3[iz][iy][ix];
        buf1_6[n++] = density_brick_a4[iz][iy][ix];
        buf1_6[n++] = density_brick_a5[iz][iy][ix];
        buf1_6[n++] = density_brick_a6[iz][iy][ix];
      }

  if (comm->procneigh[2][0] == me)
    for (i = 0; i < n; i++) buf2_6[i] = buf1_6[i];
  else {
    MPI_Irecv(buf2_6,7*nbuf_6,MPI_FFT_SCALAR,comm->procneigh[2][1],0,world,&request);
    MPI_Send(buf1_6,n,MPI_FFT_SCALAR,comm->procneigh[2][0],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzhi_in_6-nzhi_ghost_6+1; iz <= nzhi_in_6; iz++)
    for (iy = nylo_in_6; iy <= nyhi_in_6; iy++)
      for (ix = nxlo_in_6; ix <= nxhi_in_6; ix++) {
        density_brick_a0[iz][iy][ix] += buf2_6[n++];
        density_brick_a1[iz][iy][ix] += buf2_6[n++];
        density_brick_a2[iz][iy][ix] += buf2_6[n++];
        density_brick_a3[iz][iy][ix] += buf2_6[n++];
        density_brick_a4[iz][iy][ix] += buf2_6[n++];
        density_brick_a5[iz][iy][ix] += buf2_6[n++];
        density_brick_a6[iz][iy][ix] += buf2_6[n++];
      }

  // remap from 3d brick decomposition to FFT decomposition
  // copy grabs inner portion of density from 3d brick
  // remap could be done as pre-stage of FFT,
  //   but this works optimally on only double values, not complex values

  n = 0;
  for (iz = nzlo_in_6; iz <= nzhi_in_6; iz++)
    for (iy = nylo_in_6; iy <= nyhi_in_6; iy++)
      for (ix = nxlo_in_6; ix <= nxhi_in_6; ix++) {
        density_fft_a0[n] = density_brick_a0[iz][iy][ix];
        density_fft_a1[n] = density_brick_a1[iz][iy][ix];
        density_fft_a2[n] = density_brick_a2[iz][iy][ix];
        density_fft_a3[n] = density_brick_a3[iz][iy][ix];
        density_fft_a4[n] = density_brick_a4[iz][iy][ix];
        density_fft_a5[n] = density_brick_a5[iz][iy][ix];
        density_fft_a6[n++] = density_brick_a6[iz][iy][ix];
      }

  remap_6->perform(density_fft_a0,density_fft_a0,work1_6);
  remap_6->perform(density_fft_a1,density_fft_a1,work1_6);
  remap_6->perform(density_fft_a2,density_fft_a2,work1_6);
  remap_6->perform(density_fft_a3,density_fft_a3,work1_6);
  remap_6->perform(density_fft_a4,density_fft_a4,work1_6);
  remap_6->perform(density_fft_a5,density_fft_a5,work1_6);
  remap_6->perform(density_fft_a6,density_fft_a6,work1_6);

}


/* ----------------------------------------------------------------------
   ghost-swap to fill ghost cells of my brick with field values
   when using coulomb interactions or dispersion with geometric
   mixing only for ik differentiation
------------------------------------------------------------------------- */

void PPPMDisp::fillbrick_ik(int nxlo_o, int nylo_o, int nzlo_o,
                             int nxhi_o, int nyhi_o, int nzhi_o,
                             int nxlo_i, int nylo_i, int nzlo_i,
                             int nxhi_i, int nyhi_i, int nzhi_i,
                             int nxlo_g, int nylo_g, int nzlo_g,
                             int nxhi_g, int nyhi_g, int nzhi_g,
                             FFT_SCALAR* bf1, FFT_SCALAR* bf2, int nbf,
                             FFT_SCALAR*** vx_brick, FFT_SCALAR*** vy_brick, FFT_SCALAR*** vz_brick)
{
  int i,n,ix,iy,iz;
  MPI_Request request;
  MPI_Status status;

  // pack my real cells for +z processor
  // pass data to self or +z processor
  // unpack and sum recv data into my ghost cells

  n = 0;
  for (iz = nzhi_i-nzhi_g+1; iz <= nzhi_i; iz++)
    for (iy = nylo_i; iy <= nyhi_i; iy++)
      for (ix = nxlo_i; ix <= nxhi_i; ix++) {
	bf1[n++] = vx_brick[iz][iy][ix];
	bf1[n++] = vy_brick[iz][iy][ix];
	bf1[n++] = vz_brick[iz][iy][ix];
      }

  if (comm->procneigh[2][1] == me)
    for (i = 0; i < n; i++) bf2[i] = bf1[i];
  else {
    MPI_Irecv(bf2,nbf,MPI_FFT_SCALAR,comm->procneigh[2][0],0,world,&request);
    MPI_Send(bf1,n,MPI_FFT_SCALAR,comm->procneigh[2][1],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzlo_o; iz < nzlo_i; iz++)
    for (iy = nylo_i; iy <= nyhi_i; iy++)
      for (ix = nxlo_i; ix <= nxhi_i; ix++) {
	vx_brick[iz][iy][ix] = bf2[n++];
	vy_brick[iz][iy][ix] = bf2[n++];
	vz_brick[iz][iy][ix] = bf2[n++];
      }

  // pack my real cells for -z processor
  // pass data to self or -z processor
  // unpack and sum recv data into my ghost cells

  n = 0;
  for (iz = nzlo_i; iz < nzlo_i+nzlo_g; iz++)
    for (iy = nylo_i; iy <= nyhi_i; iy++)
      for (ix = nxlo_i; ix <= nxhi_i; ix++) {
	bf1[n++] = vx_brick[iz][iy][ix];
	bf1[n++] = vy_brick[iz][iy][ix];
	bf1[n++] = vz_brick[iz][iy][ix];
      }

  if (comm->procneigh[2][0] == me)
    for (i = 0; i < n; i++) bf2[i] = bf1[i];
  else {
    MPI_Irecv(bf2,nbf,MPI_FFT_SCALAR,comm->procneigh[2][1],0,world,&request);
    MPI_Send(bf1,n,MPI_FFT_SCALAR,comm->procneigh[2][0],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzhi_i+1; iz <= nzhi_o; iz++)
    for (iy = nylo_i; iy <= nyhi_i; iy++)
      for (ix = nxlo_i; ix <= nxhi_i; ix++) {
	vx_brick[iz][iy][ix] = bf2[n++];
	vy_brick[iz][iy][ix] = bf2[n++];
	vz_brick[iz][iy][ix] = bf2[n++];
      }

  // pack my real cells for +y processor
  // pass data to self or +y processor
  // unpack and sum recv data into my ghost cells

  n = 0;
  for (iz = nzlo_o; iz <= nzhi_o; iz++)
    for (iy = nyhi_i-nyhi_g+1; iy <= nyhi_i; iy++)
      for (ix = nxlo_i; ix <= nxhi_i; ix++) {
	bf1[n++] = vx_brick[iz][iy][ix];
	bf1[n++] = vy_brick[iz][iy][ix];
	bf1[n++] = vz_brick[iz][iy][ix];
      }

  if (comm->procneigh[1][1] == me)
    for (i = 0; i < n; i++) bf2[i] = bf1[i];
  else {
    MPI_Irecv(bf2,nbf,MPI_FFT_SCALAR,comm->procneigh[1][0],0,world,&request);
    MPI_Send(bf1,n,MPI_FFT_SCALAR,comm->procneigh[1][1],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzlo_o; iz <= nzhi_o; iz++)
    for (iy = nylo_o; iy < nylo_i; iy++)
      for (ix = nxlo_i; ix <= nxhi_i; ix++) {
	vx_brick[iz][iy][ix] = bf2[n++];
	vy_brick[iz][iy][ix] = bf2[n++];
	vz_brick[iz][iy][ix] = bf2[n++];
      }

  // pack my real cells for -y processor
  // pass data to self or -y processor
  // unpack and sum recv data into my ghost cells

  n = 0;
  for (iz = nzlo_o; iz <= nzhi_o; iz++)
    for (iy = nylo_i; iy < nylo_i+nylo_g; iy++)
      for (ix = nxlo_i; ix <= nxhi_i; ix++) {
	bf1[n++] = vx_brick[iz][iy][ix];
	bf1[n++] = vy_brick[iz][iy][ix];
	bf1[n++] = vz_brick[iz][iy][ix];
      }

  if (comm->procneigh[1][0] == me)
    for (i = 0; i < n; i++) bf2[i] = bf1[i];
  else {
    MPI_Irecv(bf2,nbf,MPI_FFT_SCALAR,comm->procneigh[1][1],0,world,&request);
    MPI_Send(bf1,n,MPI_FFT_SCALAR,comm->procneigh[1][0],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzlo_o; iz <= nzhi_o; iz++)
    for (iy = nyhi_i+1; iy <= nyhi_o; iy++)
      for (ix = nxlo_i; ix <= nxhi_i; ix++) {
	vx_brick[iz][iy][ix] = bf2[n++];
	vy_brick[iz][iy][ix] = bf2[n++];
	vz_brick[iz][iy][ix] = bf2[n++];
      }

  // pack my real cells for +x processor
  // pass data to self or +x processor
  // unpack and sum recv data into my ghost cells

  n = 0;
  for (iz = nzlo_o; iz <= nzhi_o; iz++)
    for (iy = nylo_o; iy <= nyhi_o; iy++)
      for (ix = nxhi_i-nxhi_g+1; ix <= nxhi_i; ix++) {
	bf1[n++] = vx_brick[iz][iy][ix];
	bf1[n++] = vy_brick[iz][iy][ix];
	bf1[n++] = vz_brick[iz][iy][ix];
      }

  if (comm->procneigh[0][1] == me)
    for (i = 0; i < n; i++) bf2[i] = bf1[i];
  else {
    MPI_Irecv(bf2,nbf,MPI_FFT_SCALAR,comm->procneigh[0][0],0,world,&request);
    MPI_Send(bf1,n,MPI_FFT_SCALAR,comm->procneigh[0][1],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzlo_o; iz <= nzhi_o; iz++)
    for (iy = nylo_o; iy <= nyhi_o; iy++)
      for (ix = nxlo_o; ix < nxlo_i; ix++) {
	vx_brick[iz][iy][ix] = bf2[n++];
	vy_brick[iz][iy][ix] = bf2[n++];
	vz_brick[iz][iy][ix] = bf2[n++];
      }

  // pack my real cells for -x processor
  // pass data to self or -x processor
  // unpack and sum recv data into my ghost cells

  n = 0;
  for (iz = nzlo_o; iz <= nzhi_o; iz++)
    for (iy = nylo_o; iy <= nyhi_o; iy++)
      for (ix = nxlo_i; ix < nxlo_i+nxlo_g; ix++) {
	bf1[n++] = vx_brick[iz][iy][ix];
	bf1[n++] = vy_brick[iz][iy][ix];
	bf1[n++] = vz_brick[iz][iy][ix];
      }

  if (comm->procneigh[0][0] == me)
    for (i = 0; i < n; i++) bf2[i] = bf1[i];
  else {
    MPI_Irecv(bf2,nbf,MPI_FFT_SCALAR,comm->procneigh[0][1],0,world,&request);
    MPI_Send(bf1,n,MPI_FFT_SCALAR,comm->procneigh[0][0],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzlo_o; iz <= nzhi_o; iz++)
    for (iy = nylo_o; iy <= nyhi_o; iy++)
      for (ix = nxhi_i+1; ix <= nxhi_o; ix++) {
	vx_brick[iz][iy][ix] = bf2[n++];
	vy_brick[iz][iy][ix] = bf2[n++];
	vz_brick[iz][iy][ix] = bf2[n++];
      }
}


/* ----------------------------------------------------------------------
   ghost-swap to fill ghost cells of my brick with field values
   when using coulomb interactions or dispersion with geometric
   mixing only for ad differentitaion
------------------------------------------------------------------------- */

void PPPMDisp::fillbrick_ad(int nxlo_o, int nylo_o, int nzlo_o,
                             int nxhi_o, int nyhi_o, int nzhi_o,
                             int nxlo_i, int nylo_i, int nzlo_i,
                             int nxhi_i, int nyhi_i, int nzhi_i,
                             int nxlo_g, int nylo_g, int nzlo_g,
                             int nxhi_g, int nyhi_g, int nzhi_g,
                             FFT_SCALAR* bf1, FFT_SCALAR* bf2, int nbf,
                             FFT_SCALAR*** u_brck)
{
  int i,n,ix,iy,iz;
  MPI_Request request;
  MPI_Status status;

  // pack my real cells for +z processor
  // pass data to self or +z processor
  // unpack and sum recv data into my ghost cells

  n = 0;
  for (iz = nzhi_i-nzhi_g+1; iz <= nzhi_i; iz++)
    for (iy = nylo_i; iy <= nyhi_i; iy++)
      for (ix = nxlo_i; ix <= nxhi_i; ix++) {
	bf1[n++] = u_brck[iz][iy][ix];
      }

  if (comm->procneigh[2][1] == me)
    for (i = 0; i < n; i++) bf2[i] = bf1[i];
  else {
    MPI_Irecv(bf2,nbf,MPI_FFT_SCALAR,comm->procneigh[2][0],0,world,&request);
    MPI_Send(bf1,n,MPI_FFT_SCALAR,comm->procneigh[2][1],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzlo_o; iz < nzlo_i; iz++)
    for (iy = nylo_i; iy <= nyhi_i; iy++)
      for (ix = nxlo_i; ix <= nxhi_i; ix++) {
	u_brck[iz][iy][ix] = bf2[n++];
      }

  // pack my real cells for -z processor
  // pass data to self or -z processor
  // unpack and sum recv data into my ghost cells

  n = 0;
  for (iz = nzlo_i; iz < nzlo_i+nzlo_g; iz++)
    for (iy = nylo_i; iy <= nyhi_i; iy++)
      for (ix = nxlo_i; ix <= nxhi_i; ix++) {
	bf1[n++] = u_brck[iz][iy][ix];
      }

  if (comm->procneigh[2][0] == me)
    for (i = 0; i < n; i++) bf2[i] = bf1[i];
  else {
    MPI_Irecv(bf2,nbf,MPI_FFT_SCALAR,comm->procneigh[2][1],0,world,&request);
    MPI_Send(bf1,n,MPI_FFT_SCALAR,comm->procneigh[2][0],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzhi_i+1; iz <= nzhi_o; iz++)
    for (iy = nylo_i; iy <= nyhi_i; iy++)
      for (ix = nxlo_i; ix <= nxhi_i; ix++) {
	u_brck[iz][iy][ix] = bf2[n++];
      }

  // pack my real cells for +y processor
  // pass data to self or +y processor
  // unpack and sum recv data into my ghost cells

  n = 0;
  for (iz = nzlo_o; iz <= nzhi_o; iz++)
    for (iy = nyhi_i-nyhi_g+1; iy <= nyhi_i; iy++)
      for (ix = nxlo_i; ix <= nxhi_i; ix++) {
	bf1[n++] = u_brck[iz][iy][ix];
      }

  if (comm->procneigh[1][1] == me)
    for (i = 0; i < n; i++) bf2[i] = bf1[i];
  else {
    MPI_Irecv(bf2,nbf,MPI_FFT_SCALAR,comm->procneigh[1][0],0,world,&request);
    MPI_Send(bf1,n,MPI_FFT_SCALAR,comm->procneigh[1][1],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzlo_o; iz <= nzhi_o; iz++)
    for (iy = nylo_o; iy < nylo_i; iy++)
      for (ix = nxlo_i; ix <= nxhi_i; ix++) {
	u_brck[iz][iy][ix] = bf2[n++];
      }

  // pack my real cells for -y processor
  // pass data to self or -y processor
  // unpack and sum recv data into my ghost cells

  n = 0;
  for (iz = nzlo_o; iz <= nzhi_o; iz++)
    for (iy = nylo_i; iy < nylo_i+nylo_g; iy++)
      for (ix = nxlo_i; ix <= nxhi_i; ix++) {
	bf1[n++] = u_brck[iz][iy][ix];
      }

  if (comm->procneigh[1][0] == me)
    for (i = 0; i < n; i++) bf2[i] = bf1[i];
  else {
    MPI_Irecv(bf2,nbf,MPI_FFT_SCALAR,comm->procneigh[1][1],0,world,&request);
    MPI_Send(bf1,n,MPI_FFT_SCALAR,comm->procneigh[1][0],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzlo_o; iz <= nzhi_o; iz++)
    for (iy = nyhi_i+1; iy <= nyhi_o; iy++)
      for (ix = nxlo_i; ix <= nxhi_i; ix++) {
	u_brck[iz][iy][ix] = bf2[n++];
      }

  // pack my real cells for +x processor
  // pass data to self or +x processor
  // unpack and sum recv data into my ghost cells

  n = 0;
  for (iz = nzlo_o; iz <= nzhi_o; iz++)
    for (iy = nylo_o; iy <= nyhi_o; iy++)
      for (ix = nxhi_i-nxhi_g+1; ix <= nxhi_i; ix++) {
	bf1[n++] = u_brck[iz][iy][ix];
      }

  if (comm->procneigh[0][1] == me)
    for (i = 0; i < n; i++) bf2[i] = bf1[i];
  else {
    MPI_Irecv(bf2,nbf,MPI_FFT_SCALAR,comm->procneigh[0][0],0,world,&request);
    MPI_Send(bf1,n,MPI_FFT_SCALAR,comm->procneigh[0][1],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzlo_o; iz <= nzhi_o; iz++)
    for (iy = nylo_o; iy <= nyhi_o; iy++)
      for (ix = nxlo_o; ix < nxlo_i; ix++) {
	u_brck[iz][iy][ix] = bf2[n++];
      }

  // pack my real cells for -x processor
  // pass data to self or -x processor
  // unpack and sum recv data into my ghost cells

  n = 0;
  for (iz = nzlo_o; iz <= nzhi_o; iz++)
    for (iy = nylo_o; iy <= nyhi_o; iy++)
      for (ix = nxlo_i; ix < nxlo_i+nxlo_g; ix++) {
	bf1[n++] = u_brck[iz][iy][ix];
      }

  if (comm->procneigh[0][0] == me)
    for (i = 0; i < n; i++) bf2[i] = bf1[i];
  else {
    MPI_Irecv(bf2,nbf,MPI_FFT_SCALAR,comm->procneigh[0][1],0,world,&request);
    MPI_Send(bf1,n,MPI_FFT_SCALAR,comm->procneigh[0][0],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzlo_o; iz <= nzhi_o; iz++)
    for (iy = nylo_o; iy <= nyhi_o; iy++)
      for (ix = nxhi_i+1; ix <= nxhi_o; ix++) {
	u_brck[iz][iy][ix] = bf2[n++];
      }
}

/* ----------------------------------------------------------------------
   communication between procs required for per atom calculations
   for ik scheme
------------------------------------------------------------------------- */

void PPPMDisp::fillbrick_peratom_ik(int nxlo_o, int nylo_o, int nzlo_o,
                                     int nxhi_o, int nyhi_o, int nzhi_o,
                                     int nxlo_i, int nylo_i, int nzlo_i,
                                     int nxhi_i, int nyhi_i, int nzhi_i,
                                     int nxlo_g, int nylo_g, int nzlo_g,
                                     int nxhi_g, int nyhi_g, int nzhi_g,
                                     FFT_SCALAR* bf1, FFT_SCALAR* bf2, int nbf,
			             FFT_SCALAR*** u_pa, FFT_SCALAR*** v0_pa, FFT_SCALAR*** v1_pa, FFT_SCALAR*** v2_pa,
                                     FFT_SCALAR*** v3_pa, FFT_SCALAR*** v4_pa, FFT_SCALAR*** v5_pa)
{
  int i,n,ix,iy,iz;
  MPI_Request request;
  MPI_Status status;

  // pack my real cells for +z processor
  // pass data to self or +z processor
  // unpack and sum recv data into my ghost cells

  n = 0;
  for (iz = nzhi_i-nzhi_g+1; iz <= nzhi_i; iz++)
    for (iy = nylo_i; iy <= nyhi_i; iy++)
      for (ix = nxlo_i; ix <= nxhi_i; ix++) {
        if (eflag_atom) bf1[n++] = u_pa[iz][iy][ix];
        if (vflag_atom) {
	  bf1[n++] = v0_pa[iz][iy][ix];
	  bf1[n++] = v1_pa[iz][iy][ix];
	  bf1[n++] = v2_pa[iz][iy][ix];
	  bf1[n++] = v3_pa[iz][iy][ix];
	  bf1[n++] = v4_pa[iz][iy][ix];
	  bf1[n++] = v5_pa[iz][iy][ix];
	}
      }

  if (comm->procneigh[2][1] == me)
    for (i = 0; i < n; i++) bf2[i] = bf1[i];
  else {
    MPI_Irecv(bf2,nbf,MPI_FFT_SCALAR,comm->procneigh[2][0],0,world,&request);
    MPI_Send(bf1,n,MPI_FFT_SCALAR,comm->procneigh[2][1],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzlo_o; iz < nzlo_i; iz++)
    for (iy = nylo_i; iy <= nyhi_i; iy++)
      for (ix = nxlo_i; ix <= nxhi_i; ix++) {
        if (eflag_atom) u_pa[iz][iy][ix] = bf2[n++];
        if (vflag_atom) {
	  v0_pa[iz][iy][ix] = bf2[n++];
	  v1_pa[iz][iy][ix] = bf2[n++];
	  v2_pa[iz][iy][ix] = bf2[n++];
	  v3_pa[iz][iy][ix] = bf2[n++];
	  v4_pa[iz][iy][ix] = bf2[n++];
	  v5_pa[iz][iy][ix] = bf2[n++];
	}
      }

  // pack my real cells for -z processor
  // pass data to self or -z processor
  // unpack and sum recv data into my ghost cells

  n = 0;
  for (iz = nzlo_i; iz < nzlo_i+nzlo_g; iz++)
    for (iy = nylo_i; iy <= nyhi_i; iy++)
      for (ix = nxlo_i; ix <= nxhi_i; ix++) {
        if (eflag_atom) bf1[n++] = u_pa[iz][iy][ix];
        if (vflag_atom) {
	  bf1[n++] = v0_pa[iz][iy][ix];
	  bf1[n++] = v1_pa[iz][iy][ix];
	  bf1[n++] = v2_pa[iz][iy][ix];
	  bf1[n++] = v3_pa[iz][iy][ix];
	  bf1[n++] = v4_pa[iz][iy][ix];
	  bf1[n++] = v5_pa[iz][iy][ix];
	}
      }

  if (comm->procneigh[2][0] == me)
    for (i = 0; i < n; i++) bf2[i] = bf1[i];
  else {
    MPI_Irecv(bf2,nbf,MPI_FFT_SCALAR,comm->procneigh[2][1],0,world,&request);
    MPI_Send(bf1,n,MPI_FFT_SCALAR,comm->procneigh[2][0],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzhi_i+1; iz <= nzhi_o; iz++)
    for (iy = nylo_i; iy <= nyhi_i; iy++)
      for (ix = nxlo_i; ix <= nxhi_i; ix++) {
        if (eflag_atom) u_pa[iz][iy][ix] = bf2[n++];
        if (vflag_atom) {
	  v0_pa[iz][iy][ix] = bf2[n++];
	  v1_pa[iz][iy][ix] = bf2[n++];
	  v2_pa[iz][iy][ix] = bf2[n++];
	  v3_pa[iz][iy][ix] = bf2[n++];
	  v4_pa[iz][iy][ix] = bf2[n++];
	  v5_pa[iz][iy][ix] = bf2[n++];
	}
      }

  // pack my real cells for +y processor
  // pass data to self or +y processor
  // unpack and sum recv data into my ghost cells

  n = 0;
  for (iz = nzlo_o; iz <= nzhi_o; iz++)
    for (iy = nyhi_i-nyhi_g+1; iy <= nyhi_i; iy++)
      for (ix = nxlo_i; ix <= nxhi_i; ix++) {
        if (eflag_atom) bf1[n++] = u_pa[iz][iy][ix];
        if (vflag_atom) {
	  bf1[n++] = v0_pa[iz][iy][ix];
	  bf1[n++] = v1_pa[iz][iy][ix];
	  bf1[n++] = v2_pa[iz][iy][ix];
	  bf1[n++] = v3_pa[iz][iy][ix];
	  bf1[n++] = v4_pa[iz][iy][ix];
	  bf1[n++] = v5_pa[iz][iy][ix];
	}
      }

  if (comm->procneigh[1][1] == me)
    for (i = 0; i < n; i++) bf2[i] = bf1[i];
  else {
    MPI_Irecv(bf2,nbf,MPI_FFT_SCALAR,comm->procneigh[1][0],0,world,&request);
    MPI_Send(bf1,n,MPI_FFT_SCALAR,comm->procneigh[1][1],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzlo_o; iz <= nzhi_o; iz++)
    for (iy = nylo_o; iy < nylo_i; iy++)
      for (ix = nxlo_i; ix <= nxhi_i; ix++) {
        if (eflag_atom) u_pa[iz][iy][ix] = bf2[n++];
        if (vflag_atom) {
	  v0_pa[iz][iy][ix] = bf2[n++];
	  v1_pa[iz][iy][ix] = bf2[n++];
	  v2_pa[iz][iy][ix] = bf2[n++];
	  v3_pa[iz][iy][ix] = bf2[n++];
	  v4_pa[iz][iy][ix] = bf2[n++];
	  v5_pa[iz][iy][ix] = bf2[n++];
	}
      }

  // pack my real cells for -y processor
  // pass data to self or -y processor
  // unpack and sum recv data into my ghost cells

  n = 0;
  for (iz = nzlo_o; iz <= nzhi_o; iz++)
    for (iy = nylo_i; iy < nylo_i+nylo_g; iy++)
      for (ix = nxlo_i; ix <= nxhi_i; ix++) {
        if (eflag_atom) bf1[n++] = u_pa[iz][iy][ix];
        if (vflag_atom) {
	  bf1[n++] = v0_pa[iz][iy][ix];
	  bf1[n++] = v1_pa[iz][iy][ix];
	  bf1[n++] = v2_pa[iz][iy][ix];
	  bf1[n++] = v3_pa[iz][iy][ix];
	  bf1[n++] = v4_pa[iz][iy][ix];
	  bf1[n++] = v5_pa[iz][iy][ix];
	}
      }

  if (comm->procneigh[1][0] == me)
    for (i = 0; i < n; i++) bf2[i] = bf1[i];
  else {
    MPI_Irecv(bf2,nbf,MPI_FFT_SCALAR,comm->procneigh[1][1],0,world,&request);
    MPI_Send(bf1,n,MPI_FFT_SCALAR,comm->procneigh[1][0],0,world);
    MPI_Wait(&request,&status);
  }
 
  n = 0;
  for (iz = nzlo_o; iz <= nzhi_o; iz++)
    for (iy = nyhi_i+1; iy <= nyhi_o; iy++)
      for (ix = nxlo_i; ix <= nxhi_i; ix++) {
        if (eflag_atom) u_pa[iz][iy][ix] = bf2[n++];
        if (vflag_atom) {
	  v0_pa[iz][iy][ix] = bf2[n++];
	  v1_pa[iz][iy][ix] = bf2[n++];
	  v2_pa[iz][iy][ix] = bf2[n++];
	  v3_pa[iz][iy][ix] = bf2[n++];
	  v4_pa[iz][iy][ix] = bf2[n++];
	  v5_pa[iz][iy][ix] = bf2[n++];
	}
      }
 
  // pack my real cells for +x processor
  // pass data to self or +x processor
  // unpack and sum recv data into my ghost cells

  n = 0;
  for (iz = nzlo_o; iz <= nzhi_o; iz++)
    for (iy = nylo_o; iy <= nyhi_o; iy++)
      for (ix = nxhi_i-nxhi_g+1; ix <= nxhi_i; ix++) {
        if (eflag_atom) bf1[n++] = u_pa[iz][iy][ix];
        if (vflag_atom) {
	  bf1[n++] = v0_pa[iz][iy][ix];
	  bf1[n++] = v1_pa[iz][iy][ix];
	  bf1[n++] = v2_pa[iz][iy][ix];
	  bf1[n++] = v3_pa[iz][iy][ix];
	  bf1[n++] = v4_pa[iz][iy][ix];
	  bf1[n++] = v5_pa[iz][iy][ix];
	}
      }
 
  if (comm->procneigh[0][1] == me)
    for (i = 0; i < n; i++) bf2[i] = bf1[i];
  else {
    MPI_Irecv(bf2,nbf,MPI_FFT_SCALAR,comm->procneigh[0][0],0,world,&request);
    MPI_Send(bf1,n,MPI_FFT_SCALAR,comm->procneigh[0][1],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzlo_o; iz <= nzhi_o; iz++)
    for (iy = nylo_o; iy <= nyhi_o; iy++)
      for (ix = nxlo_o; ix < nxlo_i; ix++) {
        if (eflag_atom) u_pa[iz][iy][ix] = bf2[n++];
        if (vflag_atom) {
	  v0_pa[iz][iy][ix] = bf2[n++];
	  v1_pa[iz][iy][ix] = bf2[n++];
	  v2_pa[iz][iy][ix] = bf2[n++];
	  v3_pa[iz][iy][ix] = bf2[n++];
	  v4_pa[iz][iy][ix] = bf2[n++];
	  v5_pa[iz][iy][ix] = bf2[n++];
	}
      }
 
  // pack my real cells for -x processor
  // pass data to self or -x processor
  // unpack and sum recv data into my ghost cells

  n = 0;
  for (iz = nzlo_o; iz <= nzhi_o; iz++)
    for (iy = nylo_o; iy <= nyhi_o; iy++)
      for (ix = nxlo_i; ix < nxlo_i+nxlo_g; ix++) {
        if (eflag_atom) bf1[n++] = u_pa[iz][iy][ix];
        if (vflag_atom) {
	  bf1[n++] = v0_pa[iz][iy][ix];
	  bf1[n++] = v1_pa[iz][iy][ix];
	  bf1[n++] = v2_pa[iz][iy][ix];
	  bf1[n++] = v3_pa[iz][iy][ix];
	  bf1[n++] = v4_pa[iz][iy][ix];
	  bf1[n++] = v5_pa[iz][iy][ix];
	}
      }

  if (comm->procneigh[0][0] == me)
    for (i = 0; i < n; i++) bf2[i] = bf1[i];
  else {
    MPI_Irecv(bf2,nbf,MPI_FFT_SCALAR,comm->procneigh[0][1],0,world,&request);
    MPI_Send(bf1,n,MPI_FFT_SCALAR,comm->procneigh[0][0],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzlo_o; iz <= nzhi_o; iz++)
    for (iy = nylo_o; iy <= nyhi_o; iy++)
      for (ix = nxhi_i+1; ix <= nxhi_o; ix++) {
        if (eflag_atom) u_pa[iz][iy][ix] = bf2[n++];
        if (vflag_atom) {
	  v0_pa[iz][iy][ix] = bf2[n++];
	  v1_pa[iz][iy][ix] = bf2[n++];
	  v2_pa[iz][iy][ix] = bf2[n++];
	  v3_pa[iz][iy][ix] = bf2[n++];
	  v4_pa[iz][iy][ix] = bf2[n++];
	  v5_pa[iz][iy][ix] = bf2[n++];
	}
      }
}

/* ----------------------------------------------------------------------
   communication between procs required for per atom calculations
   for ik scheme
------------------------------------------------------------------------- */

void PPPMDisp::fillbrick_peratom_ad(int nxlo_o, int nylo_o, int nzlo_o,
                                     int nxhi_o, int nyhi_o, int nzhi_o,
                                     int nxlo_i, int nylo_i, int nzlo_i,
                                     int nxhi_i, int nyhi_i, int nzhi_i,
                                     int nxlo_g, int nylo_g, int nzlo_g,
                                     int nxhi_g, int nyhi_g, int nzhi_g,
                                     FFT_SCALAR* bf1, FFT_SCALAR* bf2, int nbf,
			             FFT_SCALAR*** v0_pa, FFT_SCALAR*** v1_pa, FFT_SCALAR*** v2_pa,
                                     FFT_SCALAR*** v3_pa, FFT_SCALAR*** v4_pa, FFT_SCALAR*** v5_pa)
{
  int i,n,ix,iy,iz;
  MPI_Request request;
  MPI_Status status;

  // pack my real cells for +z processor
  // pass data to self or +z processor
  // unpack and sum recv data into my ghost cells

  n = 0;
  for (iz = nzhi_i-nzhi_g+1; iz <= nzhi_i; iz++)
    for (iy = nylo_i; iy <= nyhi_i; iy++)
      for (ix = nxlo_i; ix <= nxhi_i; ix++) {
	bf1[n++] = v0_pa[iz][iy][ix];
	bf1[n++] = v1_pa[iz][iy][ix];
	bf1[n++] = v2_pa[iz][iy][ix];
	bf1[n++] = v3_pa[iz][iy][ix];
	bf1[n++] = v4_pa[iz][iy][ix];
	bf1[n++] = v5_pa[iz][iy][ix];
      }

  if (comm->procneigh[2][1] == me)
    for (i = 0; i < n; i++) bf2[i] = bf1[i];
  else {
    MPI_Irecv(bf2,nbf,MPI_FFT_SCALAR,comm->procneigh[2][0],0,world,&request);
    MPI_Send(bf1,n,MPI_FFT_SCALAR,comm->procneigh[2][1],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzlo_o; iz < nzlo_i; iz++)
    for (iy = nylo_i; iy <= nyhi_i; iy++)
      for (ix = nxlo_i; ix <= nxhi_i; ix++) {
	v0_pa[iz][iy][ix] = bf2[n++];
        v1_pa[iz][iy][ix] = bf2[n++];
        v2_pa[iz][iy][ix] = bf2[n++];
        v3_pa[iz][iy][ix] = bf2[n++];
        v4_pa[iz][iy][ix] = bf2[n++];
	v5_pa[iz][iy][ix] = bf2[n++];
      }


  // pack my real cells for -z processor
  // pass data to self or -z processor
  // unpack and sum recv data into my ghost cells

  n = 0;
  for (iz = nzlo_i; iz < nzlo_i+nzlo_g; iz++)
    for (iy = nylo_i; iy <= nyhi_i; iy++)
      for (ix = nxlo_i; ix <= nxhi_i; ix++) {
	bf1[n++] = v0_pa[iz][iy][ix];
	bf1[n++] = v1_pa[iz][iy][ix];
	bf1[n++] = v2_pa[iz][iy][ix];
	bf1[n++] = v3_pa[iz][iy][ix];
	bf1[n++] = v4_pa[iz][iy][ix];
	bf1[n++] = v5_pa[iz][iy][ix];
      }
      
  if (comm->procneigh[2][0] == me)
    for (i = 0; i < n; i++) bf2[i] = bf1[i];
  else {
    MPI_Irecv(bf2,nbf,MPI_FFT_SCALAR,comm->procneigh[2][1],0,world,&request);
    MPI_Send(bf1,n,MPI_FFT_SCALAR,comm->procneigh[2][0],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzhi_i+1; iz <= nzhi_o; iz++)
    for (iy = nylo_i; iy <= nyhi_i; iy++)
      for (ix = nxlo_i; ix <= nxhi_i; ix++) {
	v0_pa[iz][iy][ix] = bf2[n++];
	v1_pa[iz][iy][ix] = bf2[n++];
	v2_pa[iz][iy][ix] = bf2[n++];
	v3_pa[iz][iy][ix] = bf2[n++];
	v4_pa[iz][iy][ix] = bf2[n++];
	v5_pa[iz][iy][ix] = bf2[n++];
      }
      
  // pack my real cells for +y processor
  // pass data to self or +y processor
  // unpack and sum recv data into my ghost cells

  n = 0;
  for (iz = nzlo_o; iz <= nzhi_o; iz++)
    for (iy = nyhi_i-nyhi_g+1; iy <= nyhi_i; iy++)
      for (ix = nxlo_i; ix <= nxhi_i; ix++) {
	bf1[n++] = v0_pa[iz][iy][ix];
	bf1[n++] = v1_pa[iz][iy][ix];
	bf1[n++] = v2_pa[iz][iy][ix];
	bf1[n++] = v3_pa[iz][iy][ix];
	bf1[n++] = v4_pa[iz][iy][ix];
	bf1[n++] = v5_pa[iz][iy][ix];
      }
      
  if (comm->procneigh[1][1] == me)
    for (i = 0; i < n; i++) bf2[i] = bf1[i];
  else {
    MPI_Irecv(bf2,nbf,MPI_FFT_SCALAR,comm->procneigh[1][0],0,world,&request);
    MPI_Send(bf1,n,MPI_FFT_SCALAR,comm->procneigh[1][1],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzlo_o; iz <= nzhi_o; iz++)
    for (iy = nylo_o; iy < nylo_i; iy++)
      for (ix = nxlo_i; ix <= nxhi_i; ix++) {
	v0_pa[iz][iy][ix] = bf2[n++];
	v1_pa[iz][iy][ix] = bf2[n++];
	v2_pa[iz][iy][ix] = bf2[n++];
	v3_pa[iz][iy][ix] = bf2[n++];
	v4_pa[iz][iy][ix] = bf2[n++];
	v5_pa[iz][iy][ix] = bf2[n++];
      }

  // pack my real cells for -y processor
  // pass data to self or -y processor
  // unpack and sum recv data into my ghost cells

  n = 0;
  for (iz = nzlo_o; iz <= nzhi_o; iz++)
    for (iy = nylo_i; iy < nylo_i+nylo_g; iy++)
      for (ix = nxlo_i; ix <= nxhi_i; ix++) {
	bf1[n++] = v0_pa[iz][iy][ix];
	bf1[n++] = v1_pa[iz][iy][ix];
	bf1[n++] = v2_pa[iz][iy][ix];
	bf1[n++] = v3_pa[iz][iy][ix];
	bf1[n++] = v4_pa[iz][iy][ix];
	bf1[n++] = v5_pa[iz][iy][ix];
      }
      
  if (comm->procneigh[1][0] == me)
    for (i = 0; i < n; i++) bf2[i] = bf1[i];
  else {
    MPI_Irecv(bf2,nbf,MPI_FFT_SCALAR,comm->procneigh[1][1],0,world,&request);
    MPI_Send(bf1,n,MPI_FFT_SCALAR,comm->procneigh[1][0],0,world);
    MPI_Wait(&request,&status);
  }
 
  n = 0;
  for (iz = nzlo_o; iz <= nzhi_o; iz++)
    for (iy = nyhi_i+1; iy <= nyhi_o; iy++)
      for (ix = nxlo_i; ix <= nxhi_i; ix++) {
	v0_pa[iz][iy][ix] = bf2[n++];
	v1_pa[iz][iy][ix] = bf2[n++];
	v2_pa[iz][iy][ix] = bf2[n++];
	v3_pa[iz][iy][ix] = bf2[n++];
	v4_pa[iz][iy][ix] = bf2[n++];
	v5_pa[iz][iy][ix] = bf2[n++];
      }
       
  // pack my real cells for +x processor
  // pass data to self or +x processor
  // unpack and sum recv data into my ghost cells

  n = 0;
  for (iz = nzlo_o; iz <= nzhi_o; iz++)
    for (iy = nylo_o; iy <= nyhi_o; iy++)
      for (ix = nxhi_i-nxhi_g+1; ix <= nxhi_i; ix++) {
	bf1[n++] = v0_pa[iz][iy][ix];
	bf1[n++] = v1_pa[iz][iy][ix];
	bf1[n++] = v2_pa[iz][iy][ix];
	bf1[n++] = v3_pa[iz][iy][ix];
	bf1[n++] = v4_pa[iz][iy][ix];
	bf1[n++] = v5_pa[iz][iy][ix];
      }
 
  if (comm->procneigh[0][1] == me)
    for (i = 0; i < n; i++) bf2[i] = bf1[i];
  else {
    MPI_Irecv(bf2,nbf,MPI_FFT_SCALAR,comm->procneigh[0][0],0,world,&request);
    MPI_Send(bf1,n,MPI_FFT_SCALAR,comm->procneigh[0][1],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzlo_o; iz <= nzhi_o; iz++)
    for (iy = nylo_o; iy <= nyhi_o; iy++)
      for (ix = nxlo_o; ix < nxlo_i; ix++) {
	v0_pa[iz][iy][ix] = bf2[n++];
	v1_pa[iz][iy][ix] = bf2[n++];
	v2_pa[iz][iy][ix] = bf2[n++];
	v3_pa[iz][iy][ix] = bf2[n++];
	v4_pa[iz][iy][ix] = bf2[n++];
	v5_pa[iz][iy][ix] = bf2[n++];
      }
       
  // pack my real cells for -x processor
  // pass data to self or -x processor
  // unpack and sum recv data into my ghost cells

  n = 0;
  for (iz = nzlo_o; iz <= nzhi_o; iz++)
    for (iy = nylo_o; iy <= nyhi_o; iy++)
      for (ix = nxlo_i; ix < nxlo_i+nxlo_g; ix++) {
	bf1[n++] = v0_pa[iz][iy][ix];
	bf1[n++] = v1_pa[iz][iy][ix];
	bf1[n++] = v2_pa[iz][iy][ix];
	bf1[n++] = v3_pa[iz][iy][ix];
	bf1[n++] = v4_pa[iz][iy][ix];
	bf1[n++] = v5_pa[iz][iy][ix];
      }
      
  if (comm->procneigh[0][0] == me)
    for (i = 0; i < n; i++) bf2[i] = bf1[i];
  else {
    MPI_Irecv(bf2,nbf,MPI_FFT_SCALAR,comm->procneigh[0][1],0,world,&request);
    MPI_Send(bf1,n,MPI_FFT_SCALAR,comm->procneigh[0][0],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzlo_o; iz <= nzhi_o; iz++)
    for (iy = nylo_o; iy <= nyhi_o; iy++)
      for (ix = nxhi_i+1; ix <= nxhi_o; ix++) {
	v0_pa[iz][iy][ix] = bf2[n++];
	v1_pa[iz][iy][ix] = bf2[n++];
	v2_pa[iz][iy][ix] = bf2[n++];
	v3_pa[iz][iy][ix] = bf2[n++];
	v4_pa[iz][iy][ix] = bf2[n++];
	v5_pa[iz][iy][ix] = bf2[n++];
      }
      
}

/* ----------------------------------------------------------------------
   ghost-swap to fill ghost cells of my brick with field values
   when using dispersion with arithmetic mixing only for ik scheme
------------------------------------------------------------------------- */

void PPPMDisp::fillbrick_a_ik()
{
  int i,n,ix,iy,iz;
  MPI_Request request;
  MPI_Status status;

  // pack my real cells for +z processor
  // pass data to self or +z processor
  // unpack and sum recv data into my ghost cells

  n = 0;
  for (iz = nzhi_in_6-nzhi_ghost_6+1; iz <= nzhi_in_6; iz++)
    for (iy = nylo_in_6; iy <= nyhi_in_6; iy++)
      for (ix = nxlo_in_6; ix <= nxhi_in_6; ix++) {
	buf1_6[n++] = vdx_brick_a0[iz][iy][ix];
	buf1_6[n++] = vdx_brick_a1[iz][iy][ix];
	buf1_6[n++] = vdx_brick_a2[iz][iy][ix];
	buf1_6[n++] = vdx_brick_a3[iz][iy][ix];
	buf1_6[n++] = vdx_brick_a4[iz][iy][ix];
	buf1_6[n++] = vdx_brick_a5[iz][iy][ix];
	buf1_6[n++] = vdx_brick_a6[iz][iy][ix];
	buf1_6[n++] = vdy_brick_a0[iz][iy][ix];
	buf1_6[n++] = vdy_brick_a1[iz][iy][ix];
	buf1_6[n++] = vdy_brick_a2[iz][iy][ix];
	buf1_6[n++] = vdy_brick_a3[iz][iy][ix];
	buf1_6[n++] = vdy_brick_a4[iz][iy][ix];
	buf1_6[n++] = vdy_brick_a5[iz][iy][ix];
	buf1_6[n++] = vdy_brick_a6[iz][iy][ix];
	buf1_6[n++] = vdz_brick_a0[iz][iy][ix];
	buf1_6[n++] = vdz_brick_a1[iz][iy][ix];
	buf1_6[n++] = vdz_brick_a2[iz][iy][ix];
	buf1_6[n++] = vdz_brick_a3[iz][iy][ix];
	buf1_6[n++] = vdz_brick_a4[iz][iy][ix];
	buf1_6[n++] = vdz_brick_a5[iz][iy][ix];
	buf1_6[n++] = vdz_brick_a6[iz][iy][ix];
      }

  if (comm->procneigh[2][1] == me)
    for (i = 0; i < n; i++) buf2_6[i] = buf1_6[i];
  else {
    MPI_Irecv(buf2_6,7*nbuf_6,MPI_FFT_SCALAR,comm->procneigh[2][0],0,world,&request);
    MPI_Send(buf1_6,n,MPI_FFT_SCALAR,comm->procneigh[2][1],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzlo_out_6; iz < nzlo_in_6; iz++)
    for (iy = nylo_in_6; iy <= nyhi_in_6; iy++)
      for (ix = nxlo_in_6; ix <= nxhi_in_6; ix++) {
	vdx_brick_a0[iz][iy][ix] = buf2_6[n++];
        vdx_brick_a1[iz][iy][ix] = buf2_6[n++];
        vdx_brick_a2[iz][iy][ix] = buf2_6[n++];
        vdx_brick_a3[iz][iy][ix] = buf2_6[n++];
        vdx_brick_a4[iz][iy][ix] = buf2_6[n++];
        vdx_brick_a5[iz][iy][ix] = buf2_6[n++];
        vdx_brick_a6[iz][iy][ix] = buf2_6[n++];
	vdy_brick_a0[iz][iy][ix] = buf2_6[n++];
        vdy_brick_a1[iz][iy][ix] = buf2_6[n++];
        vdy_brick_a2[iz][iy][ix] = buf2_6[n++];
        vdy_brick_a3[iz][iy][ix] = buf2_6[n++];
        vdy_brick_a4[iz][iy][ix] = buf2_6[n++];
        vdy_brick_a5[iz][iy][ix] = buf2_6[n++];
        vdy_brick_a6[iz][iy][ix] = buf2_6[n++];
	vdz_brick_a0[iz][iy][ix] = buf2_6[n++];
        vdz_brick_a1[iz][iy][ix] = buf2_6[n++];
        vdz_brick_a2[iz][iy][ix] = buf2_6[n++];
        vdz_brick_a3[iz][iy][ix] = buf2_6[n++];
        vdz_brick_a4[iz][iy][ix] = buf2_6[n++];
        vdz_brick_a5[iz][iy][ix] = buf2_6[n++];
        vdz_brick_a6[iz][iy][ix] = buf2_6[n++];
      }

  // pack my real cells for -z processor
  // pass data to self or -z processor
  // unpack and sum recv data into my ghost cells

  n = 0;
  for (iz = nzlo_in_6; iz < nzlo_in_6+nzlo_ghost_6; iz++)
    for (iy = nylo_in_6; iy <= nyhi_in_6; iy++)
      for (ix = nxlo_in_6; ix <= nxhi_in_6; ix++) {
	buf1_6[n++] = vdx_brick_a0[iz][iy][ix];
	buf1_6[n++] = vdx_brick_a1[iz][iy][ix];
	buf1_6[n++] = vdx_brick_a2[iz][iy][ix];
	buf1_6[n++] = vdx_brick_a3[iz][iy][ix];
	buf1_6[n++] = vdx_brick_a4[iz][iy][ix];
	buf1_6[n++] = vdx_brick_a5[iz][iy][ix];
	buf1_6[n++] = vdx_brick_a6[iz][iy][ix];
	buf1_6[n++] = vdy_brick_a0[iz][iy][ix];
	buf1_6[n++] = vdy_brick_a1[iz][iy][ix];
	buf1_6[n++] = vdy_brick_a2[iz][iy][ix];
	buf1_6[n++] = vdy_brick_a3[iz][iy][ix];
	buf1_6[n++] = vdy_brick_a4[iz][iy][ix];
	buf1_6[n++] = vdy_brick_a5[iz][iy][ix];
	buf1_6[n++] = vdy_brick_a6[iz][iy][ix];
	buf1_6[n++] = vdz_brick_a0[iz][iy][ix];
	buf1_6[n++] = vdz_brick_a1[iz][iy][ix];
	buf1_6[n++] = vdz_brick_a2[iz][iy][ix];
	buf1_6[n++] = vdz_brick_a3[iz][iy][ix];
	buf1_6[n++] = vdz_brick_a4[iz][iy][ix];
	buf1_6[n++] = vdz_brick_a5[iz][iy][ix];
	buf1_6[n++] = vdz_brick_a6[iz][iy][ix];
      }

  if (comm->procneigh[2][0] == me)
    for (i = 0; i < n; i++) buf2_6[i] = buf1_6[i];
  else {
    MPI_Irecv(buf2_6,7*nbuf_6,MPI_FFT_SCALAR,comm->procneigh[2][1],0,world,&request);
    MPI_Send(buf1_6,n,MPI_FFT_SCALAR,comm->procneigh[2][0],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzhi_in_6+1; iz <= nzhi_out_6; iz++)
    for (iy = nylo_in_6; iy <= nyhi_in_6; iy++)
      for (ix = nxlo_in_6; ix <= nxhi_in_6; ix++) {
	vdx_brick_a0[iz][iy][ix] = buf2_6[n++];
        vdx_brick_a1[iz][iy][ix] = buf2_6[n++];
        vdx_brick_a2[iz][iy][ix] = buf2_6[n++];
        vdx_brick_a3[iz][iy][ix] = buf2_6[n++];
        vdx_brick_a4[iz][iy][ix] = buf2_6[n++];
        vdx_brick_a5[iz][iy][ix] = buf2_6[n++];
        vdx_brick_a6[iz][iy][ix] = buf2_6[n++];
	vdy_brick_a0[iz][iy][ix] = buf2_6[n++];
        vdy_brick_a1[iz][iy][ix] = buf2_6[n++];
        vdy_brick_a2[iz][iy][ix] = buf2_6[n++];
        vdy_brick_a3[iz][iy][ix] = buf2_6[n++];
        vdy_brick_a4[iz][iy][ix] = buf2_6[n++];
        vdy_brick_a5[iz][iy][ix] = buf2_6[n++];
        vdy_brick_a6[iz][iy][ix] = buf2_6[n++];
	vdz_brick_a0[iz][iy][ix] = buf2_6[n++];
        vdz_brick_a1[iz][iy][ix] = buf2_6[n++];
        vdz_brick_a2[iz][iy][ix] = buf2_6[n++];
        vdz_brick_a3[iz][iy][ix] = buf2_6[n++];
        vdz_brick_a4[iz][iy][ix] = buf2_6[n++];
        vdz_brick_a5[iz][iy][ix] = buf2_6[n++];
        vdz_brick_a6[iz][iy][ix] = buf2_6[n++];
      }

  // pack my real cells for +y processor
  // pass data to self or +y processor
  // unpack and sum recv data into my ghost cells

  n = 0;
  for (iz = nzlo_out_6; iz <= nzhi_out_6; iz++)
    for (iy = nyhi_in_6-nyhi_ghost_6+1; iy <= nyhi_in_6; iy++)
      for (ix = nxlo_in_6; ix <= nxhi_in_6; ix++) {
	buf1_6[n++] = vdx_brick_a0[iz][iy][ix];
	buf1_6[n++] = vdx_brick_a1[iz][iy][ix];
	buf1_6[n++] = vdx_brick_a2[iz][iy][ix];
	buf1_6[n++] = vdx_brick_a3[iz][iy][ix];
	buf1_6[n++] = vdx_brick_a4[iz][iy][ix];
	buf1_6[n++] = vdx_brick_a5[iz][iy][ix];
	buf1_6[n++] = vdx_brick_a6[iz][iy][ix];
	buf1_6[n++] = vdy_brick_a0[iz][iy][ix];
	buf1_6[n++] = vdy_brick_a1[iz][iy][ix];
	buf1_6[n++] = vdy_brick_a2[iz][iy][ix];
	buf1_6[n++] = vdy_brick_a3[iz][iy][ix];
	buf1_6[n++] = vdy_brick_a4[iz][iy][ix];
	buf1_6[n++] = vdy_brick_a5[iz][iy][ix];
	buf1_6[n++] = vdy_brick_a6[iz][iy][ix];
	buf1_6[n++] = vdz_brick_a0[iz][iy][ix];
	buf1_6[n++] = vdz_brick_a1[iz][iy][ix];
	buf1_6[n++] = vdz_brick_a2[iz][iy][ix];
	buf1_6[n++] = vdz_brick_a3[iz][iy][ix];
	buf1_6[n++] = vdz_brick_a4[iz][iy][ix];
	buf1_6[n++] = vdz_brick_a5[iz][iy][ix];
	buf1_6[n++] = vdz_brick_a6[iz][iy][ix];
      }

  if (comm->procneigh[1][1] == me)
    for (i = 0; i < n; i++) buf2_6[i] = buf1_6[i];
  else {
    MPI_Irecv(buf2_6,7*nbuf_6,MPI_FFT_SCALAR,comm->procneigh[1][0],0,world,&request);
    MPI_Send(buf1_6,n,MPI_FFT_SCALAR,comm->procneigh[1][1],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzlo_out_6; iz <= nzhi_out_6; iz++)
    for (iy = nylo_out_6; iy < nylo_in_6; iy++)
      for (ix = nxlo_in_6; ix <= nxhi_in_6; ix++) {
	vdx_brick_a0[iz][iy][ix] = buf2_6[n++];
        vdx_brick_a1[iz][iy][ix] = buf2_6[n++];
        vdx_brick_a2[iz][iy][ix] = buf2_6[n++];
        vdx_brick_a3[iz][iy][ix] = buf2_6[n++];
        vdx_brick_a4[iz][iy][ix] = buf2_6[n++];
        vdx_brick_a5[iz][iy][ix] = buf2_6[n++];
        vdx_brick_a6[iz][iy][ix] = buf2_6[n++];
	vdy_brick_a0[iz][iy][ix] = buf2_6[n++];
        vdy_brick_a1[iz][iy][ix] = buf2_6[n++];
        vdy_brick_a2[iz][iy][ix] = buf2_6[n++];
        vdy_brick_a3[iz][iy][ix] = buf2_6[n++];
        vdy_brick_a4[iz][iy][ix] = buf2_6[n++];
        vdy_brick_a5[iz][iy][ix] = buf2_6[n++];
        vdy_brick_a6[iz][iy][ix] = buf2_6[n++];
	vdz_brick_a0[iz][iy][ix] = buf2_6[n++];
        vdz_brick_a1[iz][iy][ix] = buf2_6[n++];
        vdz_brick_a2[iz][iy][ix] = buf2_6[n++];
        vdz_brick_a3[iz][iy][ix] = buf2_6[n++];
        vdz_brick_a4[iz][iy][ix] = buf2_6[n++];
        vdz_brick_a5[iz][iy][ix] = buf2_6[n++];
        vdz_brick_a6[iz][iy][ix] = buf2_6[n++];
      }

  // pack my real cells for -y processor
  // pass data to self or -y processor
  // unpack and sum recv data into my ghost cells

  n = 0;
  for (iz = nzlo_out_6; iz <= nzhi_out_6; iz++)
    for (iy = nylo_in_6; iy < nylo_in_6+nylo_ghost_6; iy++)
      for (ix = nxlo_in_6; ix <= nxhi_in_6; ix++) {
	buf1_6[n++] = vdx_brick_a0[iz][iy][ix];
	buf1_6[n++] = vdx_brick_a1[iz][iy][ix];
	buf1_6[n++] = vdx_brick_a2[iz][iy][ix];
	buf1_6[n++] = vdx_brick_a3[iz][iy][ix];
	buf1_6[n++] = vdx_brick_a4[iz][iy][ix];
	buf1_6[n++] = vdx_brick_a5[iz][iy][ix];
	buf1_6[n++] = vdx_brick_a6[iz][iy][ix];
	buf1_6[n++] = vdy_brick_a0[iz][iy][ix];
	buf1_6[n++] = vdy_brick_a1[iz][iy][ix];
	buf1_6[n++] = vdy_brick_a2[iz][iy][ix];
	buf1_6[n++] = vdy_brick_a3[iz][iy][ix];
	buf1_6[n++] = vdy_brick_a4[iz][iy][ix];
	buf1_6[n++] = vdy_brick_a5[iz][iy][ix];
	buf1_6[n++] = vdy_brick_a6[iz][iy][ix];
	buf1_6[n++] = vdz_brick_a0[iz][iy][ix];
	buf1_6[n++] = vdz_brick_a1[iz][iy][ix];
	buf1_6[n++] = vdz_brick_a2[iz][iy][ix];
	buf1_6[n++] = vdz_brick_a3[iz][iy][ix];
	buf1_6[n++] = vdz_brick_a4[iz][iy][ix];
	buf1_6[n++] = vdz_brick_a5[iz][iy][ix];
	buf1_6[n++] = vdz_brick_a6[iz][iy][ix];
      }

  if (comm->procneigh[1][0] == me)
    for (i = 0; i < n; i++) buf2_6[i] = buf1_6[i];
  else {
    MPI_Irecv(buf2_6,7*nbuf_6,MPI_FFT_SCALAR,comm->procneigh[1][1],0,world,&request);
    MPI_Send(buf1_6,n,MPI_FFT_SCALAR,comm->procneigh[1][0],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzlo_out_6; iz <= nzhi_out_6; iz++)
    for (iy = nyhi_in_6+1; iy <= nyhi_out_6; iy++)
      for (ix = nxlo_in_6; ix <= nxhi_in_6; ix++) {
	vdx_brick_a0[iz][iy][ix] = buf2_6[n++];
        vdx_brick_a1[iz][iy][ix] = buf2_6[n++];
        vdx_brick_a2[iz][iy][ix] = buf2_6[n++];
        vdx_brick_a3[iz][iy][ix] = buf2_6[n++];
        vdx_brick_a4[iz][iy][ix] = buf2_6[n++];
        vdx_brick_a5[iz][iy][ix] = buf2_6[n++];
        vdx_brick_a6[iz][iy][ix] = buf2_6[n++];
	vdy_brick_a0[iz][iy][ix] = buf2_6[n++];
        vdy_brick_a1[iz][iy][ix] = buf2_6[n++];
        vdy_brick_a2[iz][iy][ix] = buf2_6[n++];
        vdy_brick_a3[iz][iy][ix] = buf2_6[n++];
        vdy_brick_a4[iz][iy][ix] = buf2_6[n++];
        vdy_brick_a5[iz][iy][ix] = buf2_6[n++];
        vdy_brick_a6[iz][iy][ix] = buf2_6[n++];
	vdz_brick_a0[iz][iy][ix] = buf2_6[n++];
        vdz_brick_a1[iz][iy][ix] = buf2_6[n++];
        vdz_brick_a2[iz][iy][ix] = buf2_6[n++];
        vdz_brick_a3[iz][iy][ix] = buf2_6[n++];
        vdz_brick_a4[iz][iy][ix] = buf2_6[n++];
        vdz_brick_a5[iz][iy][ix] = buf2_6[n++];
        vdz_brick_a6[iz][iy][ix] = buf2_6[n++];
      }

  // pack my real cells for +x processor
  // pass data to self or +x processor
  // unpack and sum recv data into my ghost cells

  n = 0;
  for (iz = nzlo_out_6; iz <= nzhi_out_6; iz++)
    for (iy = nylo_out_6; iy <= nyhi_out_6; iy++)
      for (ix = nxhi_in_6-nxhi_ghost_6+1; ix <= nxhi_in_6; ix++) {
	buf1_6[n++] = vdx_brick_a0[iz][iy][ix];
	buf1_6[n++] = vdx_brick_a1[iz][iy][ix];
	buf1_6[n++] = vdx_brick_a2[iz][iy][ix];
	buf1_6[n++] = vdx_brick_a3[iz][iy][ix];
	buf1_6[n++] = vdx_brick_a4[iz][iy][ix];
	buf1_6[n++] = vdx_brick_a5[iz][iy][ix];
	buf1_6[n++] = vdx_brick_a6[iz][iy][ix];
	buf1_6[n++] = vdy_brick_a0[iz][iy][ix];
	buf1_6[n++] = vdy_brick_a1[iz][iy][ix];
	buf1_6[n++] = vdy_brick_a2[iz][iy][ix];
	buf1_6[n++] = vdy_brick_a3[iz][iy][ix];
	buf1_6[n++] = vdy_brick_a4[iz][iy][ix];
	buf1_6[n++] = vdy_brick_a5[iz][iy][ix];
	buf1_6[n++] = vdy_brick_a6[iz][iy][ix];
	buf1_6[n++] = vdz_brick_a0[iz][iy][ix];
	buf1_6[n++] = vdz_brick_a1[iz][iy][ix];
	buf1_6[n++] = vdz_brick_a2[iz][iy][ix];
	buf1_6[n++] = vdz_brick_a3[iz][iy][ix];
	buf1_6[n++] = vdz_brick_a4[iz][iy][ix];
	buf1_6[n++] = vdz_brick_a5[iz][iy][ix];
	buf1_6[n++] = vdz_brick_a6[iz][iy][ix];
      }

  if (comm->procneigh[0][1] == me)
    for (i = 0; i < n; i++) buf2_6[i] = buf1_6[i];
  else {
    MPI_Irecv(buf2_6,7*nbuf_6,MPI_FFT_SCALAR,comm->procneigh[0][0],0,world,&request);
    MPI_Send(buf1_6,n,MPI_FFT_SCALAR,comm->procneigh[0][1],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzlo_out_6; iz <= nzhi_out_6; iz++)
    for (iy = nylo_out_6; iy <= nyhi_out_6; iy++)
      for (ix = nxlo_out_6; ix < nxlo_in_6; ix++) {
	vdx_brick_a0[iz][iy][ix] = buf2_6[n++];
        vdx_brick_a1[iz][iy][ix] = buf2_6[n++];
        vdx_brick_a2[iz][iy][ix] = buf2_6[n++];
        vdx_brick_a3[iz][iy][ix] = buf2_6[n++];
        vdx_brick_a4[iz][iy][ix] = buf2_6[n++];
        vdx_brick_a5[iz][iy][ix] = buf2_6[n++];
        vdx_brick_a6[iz][iy][ix] = buf2_6[n++];
	vdy_brick_a0[iz][iy][ix] = buf2_6[n++];
        vdy_brick_a1[iz][iy][ix] = buf2_6[n++];
        vdy_brick_a2[iz][iy][ix] = buf2_6[n++];
        vdy_brick_a3[iz][iy][ix] = buf2_6[n++];
        vdy_brick_a4[iz][iy][ix] = buf2_6[n++];
        vdy_brick_a5[iz][iy][ix] = buf2_6[n++];
        vdy_brick_a6[iz][iy][ix] = buf2_6[n++];
	vdz_brick_a0[iz][iy][ix] = buf2_6[n++];
        vdz_brick_a1[iz][iy][ix] = buf2_6[n++];
        vdz_brick_a2[iz][iy][ix] = buf2_6[n++];
        vdz_brick_a3[iz][iy][ix] = buf2_6[n++];
        vdz_brick_a4[iz][iy][ix] = buf2_6[n++];
        vdz_brick_a5[iz][iy][ix] = buf2_6[n++];
        vdz_brick_a6[iz][iy][ix] = buf2_6[n++];
      }

  // pack my real cells for -x processor
  // pass data to self or -x processor
  // unpack and sum recv data into my ghost cells

  n = 0;
  for (iz = nzlo_out_6; iz <= nzhi_out_6; iz++)
    for (iy = nylo_out_6; iy <= nyhi_out_6; iy++)
      for (ix = nxlo_in_6; ix < nxlo_in_6+nxlo_ghost_6; ix++) {
	buf1_6[n++] = vdx_brick_a0[iz][iy][ix];
	buf1_6[n++] = vdx_brick_a1[iz][iy][ix];
	buf1_6[n++] = vdx_brick_a2[iz][iy][ix];
	buf1_6[n++] = vdx_brick_a3[iz][iy][ix];
	buf1_6[n++] = vdx_brick_a4[iz][iy][ix];
	buf1_6[n++] = vdx_brick_a5[iz][iy][ix];
	buf1_6[n++] = vdx_brick_a6[iz][iy][ix];
	buf1_6[n++] = vdy_brick_a0[iz][iy][ix];
	buf1_6[n++] = vdy_brick_a1[iz][iy][ix];
	buf1_6[n++] = vdy_brick_a2[iz][iy][ix];
	buf1_6[n++] = vdy_brick_a3[iz][iy][ix];
	buf1_6[n++] = vdy_brick_a4[iz][iy][ix];
	buf1_6[n++] = vdy_brick_a5[iz][iy][ix];
	buf1_6[n++] = vdy_brick_a6[iz][iy][ix];
	buf1_6[n++] = vdz_brick_a0[iz][iy][ix];
	buf1_6[n++] = vdz_brick_a1[iz][iy][ix];
	buf1_6[n++] = vdz_brick_a2[iz][iy][ix];
	buf1_6[n++] = vdz_brick_a3[iz][iy][ix];
	buf1_6[n++] = vdz_brick_a4[iz][iy][ix];
	buf1_6[n++] = vdz_brick_a5[iz][iy][ix];
	buf1_6[n++] = vdz_brick_a6[iz][iy][ix];
      }

  if (comm->procneigh[0][0] == me)
    for (i = 0; i < n; i++) buf2_6[i] = buf1_6[i];
  else {
    MPI_Irecv(buf2_6,7*nbuf_6,MPI_FFT_SCALAR,comm->procneigh[0][1],0,world,&request);
    MPI_Send(buf1_6,n,MPI_FFT_SCALAR,comm->procneigh[0][0],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzlo_out_6; iz <= nzhi_out_6; iz++)
    for (iy = nylo_out_6; iy <= nyhi_out_6; iy++)
      for (ix = nxhi_in_6+1; ix <= nxhi_out_6; ix++) {
	vdx_brick_a0[iz][iy][ix] = buf2_6[n++];
        vdx_brick_a1[iz][iy][ix] = buf2_6[n++];
        vdx_brick_a2[iz][iy][ix] = buf2_6[n++];
        vdx_brick_a3[iz][iy][ix] = buf2_6[n++];
        vdx_brick_a4[iz][iy][ix] = buf2_6[n++];
        vdx_brick_a5[iz][iy][ix] = buf2_6[n++];
        vdx_brick_a6[iz][iy][ix] = buf2_6[n++];
	vdy_brick_a0[iz][iy][ix] = buf2_6[n++];
        vdy_brick_a1[iz][iy][ix] = buf2_6[n++];
        vdy_brick_a2[iz][iy][ix] = buf2_6[n++];
        vdy_brick_a3[iz][iy][ix] = buf2_6[n++];
        vdy_brick_a4[iz][iy][ix] = buf2_6[n++];
        vdy_brick_a5[iz][iy][ix] = buf2_6[n++];
        vdy_brick_a6[iz][iy][ix] = buf2_6[n++];
	vdz_brick_a0[iz][iy][ix] = buf2_6[n++];
        vdz_brick_a1[iz][iy][ix] = buf2_6[n++];
        vdz_brick_a2[iz][iy][ix] = buf2_6[n++];
        vdz_brick_a3[iz][iy][ix] = buf2_6[n++];
        vdz_brick_a4[iz][iy][ix] = buf2_6[n++];
        vdz_brick_a5[iz][iy][ix] = buf2_6[n++];
        vdz_brick_a6[iz][iy][ix] = buf2_6[n++];
      }
}

/* ----------------------------------------------------------------------
   ghost-swap to fill ghost cells of my brick with field values
   when using dispersion with arithmetic mixing only for ad scheme
------------------------------------------------------------------------- */

void PPPMDisp::fillbrick_a_ad()
{
  int i,n,ix,iy,iz;
  MPI_Request request;
  MPI_Status status;

  // pack my real cells for +z processor
  // pass data to self or +z processor
  // unpack and sum recv data into my ghost cells

  n = 0;
  for (iz = nzhi_in_6-nzhi_ghost_6+1; iz <= nzhi_in_6; iz++)
    for (iy = nylo_in_6; iy <= nyhi_in_6; iy++)
      for (ix = nxlo_in_6; ix <= nxhi_in_6; ix++) {
	buf1_6[n++] = u_brick_a0[iz][iy][ix];
	buf1_6[n++] = u_brick_a1[iz][iy][ix];
	buf1_6[n++] = u_brick_a2[iz][iy][ix];
	buf1_6[n++] = u_brick_a3[iz][iy][ix];
	buf1_6[n++] = u_brick_a4[iz][iy][ix];
	buf1_6[n++] = u_brick_a5[iz][iy][ix];
	buf1_6[n++] = u_brick_a6[iz][iy][ix];
      }

  if (comm->procneigh[2][1] == me)
    for (i = 0; i < n; i++) buf2_6[i] = buf1_6[i];
  else {
    MPI_Irecv(buf2_6,7*nbuf_6,MPI_FFT_SCALAR,comm->procneigh[2][0],0,world,&request);
    MPI_Send(buf1_6,n,MPI_FFT_SCALAR,comm->procneigh[2][1],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzlo_out_6; iz < nzlo_in_6; iz++)
    for (iy = nylo_in_6; iy <= nyhi_in_6; iy++)
      for (ix = nxlo_in_6; ix <= nxhi_in_6; ix++) {
	u_brick_a0[iz][iy][ix] = buf2_6[n++];
        u_brick_a1[iz][iy][ix] = buf2_6[n++];
        u_brick_a2[iz][iy][ix] = buf2_6[n++];
        u_brick_a3[iz][iy][ix] = buf2_6[n++];
        u_brick_a4[iz][iy][ix] = buf2_6[n++];
        u_brick_a5[iz][iy][ix] = buf2_6[n++];
        u_brick_a6[iz][iy][ix] = buf2_6[n++];
      }

  // pack my real cells for -z processor
  // pass data to self or -z processor
  // unpack and sum recv data into my ghost cells

  n = 0;
  for (iz = nzlo_in_6; iz < nzlo_in_6+nzlo_ghost_6; iz++)
    for (iy = nylo_in_6; iy <= nyhi_in_6; iy++)
      for (ix = nxlo_in_6; ix <= nxhi_in_6; ix++) {
	buf1_6[n++] = u_brick_a0[iz][iy][ix];
	buf1_6[n++] = u_brick_a1[iz][iy][ix];
	buf1_6[n++] = u_brick_a2[iz][iy][ix];
	buf1_6[n++] = u_brick_a3[iz][iy][ix];
	buf1_6[n++] = u_brick_a4[iz][iy][ix];
	buf1_6[n++] = u_brick_a5[iz][iy][ix];
	buf1_6[n++] = u_brick_a6[iz][iy][ix];
      }

  if (comm->procneigh[2][0] == me)
    for (i = 0; i < n; i++) buf2_6[i] = buf1_6[i];
  else {
    MPI_Irecv(buf2_6,7*nbuf_6,MPI_FFT_SCALAR,comm->procneigh[2][1],0,world,&request);
    MPI_Send(buf1_6,n,MPI_FFT_SCALAR,comm->procneigh[2][0],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzhi_in_6+1; iz <= nzhi_out_6; iz++)
    for (iy = nylo_in_6; iy <= nyhi_in_6; iy++)
      for (ix = nxlo_in_6; ix <= nxhi_in_6; ix++) {
	u_brick_a0[iz][iy][ix] = buf2_6[n++];
        u_brick_a1[iz][iy][ix] = buf2_6[n++];
        u_brick_a2[iz][iy][ix] = buf2_6[n++];
        u_brick_a3[iz][iy][ix] = buf2_6[n++];
        u_brick_a4[iz][iy][ix] = buf2_6[n++];
        u_brick_a5[iz][iy][ix] = buf2_6[n++];
        u_brick_a6[iz][iy][ix] = buf2_6[n++];
      }

  // pack my real cells for +y processor
  // pass data to self or +y processor
  // unpack and sum recv data into my ghost cells

  n = 0;
  for (iz = nzlo_out_6; iz <= nzhi_out_6; iz++)
    for (iy = nyhi_in_6-nyhi_ghost_6+1; iy <= nyhi_in_6; iy++)
      for (ix = nxlo_in_6; ix <= nxhi_in_6; ix++) {
	buf1_6[n++] = u_brick_a0[iz][iy][ix];
	buf1_6[n++] = u_brick_a1[iz][iy][ix];
	buf1_6[n++] = u_brick_a2[iz][iy][ix];
	buf1_6[n++] = u_brick_a3[iz][iy][ix];
	buf1_6[n++] = u_brick_a4[iz][iy][ix];
	buf1_6[n++] = u_brick_a5[iz][iy][ix];
	buf1_6[n++] = u_brick_a6[iz][iy][ix];
      }

  if (comm->procneigh[1][1] == me)
    for (i = 0; i < n; i++) buf2_6[i] = buf1_6[i];
  else {
    MPI_Irecv(buf2_6,7*nbuf_6,MPI_FFT_SCALAR,comm->procneigh[1][0],0,world,&request);
    MPI_Send(buf1_6,n,MPI_FFT_SCALAR,comm->procneigh[1][1],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzlo_out_6; iz <= nzhi_out_6; iz++)
    for (iy = nylo_out_6; iy < nylo_in_6; iy++)
      for (ix = nxlo_in_6; ix <= nxhi_in_6; ix++) {
	u_brick_a0[iz][iy][ix] = buf2_6[n++];
        u_brick_a1[iz][iy][ix] = buf2_6[n++];
        u_brick_a2[iz][iy][ix] = buf2_6[n++];
        u_brick_a3[iz][iy][ix] = buf2_6[n++];
        u_brick_a4[iz][iy][ix] = buf2_6[n++];
        u_brick_a5[iz][iy][ix] = buf2_6[n++];
        u_brick_a6[iz][iy][ix] = buf2_6[n++];
      }

  // pack my real cells for -y processor
  // pass data to self or -y processor
  // unpack and sum recv data into my ghost cells

  n = 0;
  for (iz = nzlo_out_6; iz <= nzhi_out_6; iz++)
    for (iy = nylo_in_6; iy < nylo_in_6+nylo_ghost_6; iy++)
      for (ix = nxlo_in_6; ix <= nxhi_in_6; ix++) {
	buf1_6[n++] = u_brick_a0[iz][iy][ix];
	buf1_6[n++] = u_brick_a1[iz][iy][ix];
	buf1_6[n++] = u_brick_a2[iz][iy][ix];
	buf1_6[n++] = u_brick_a3[iz][iy][ix];
	buf1_6[n++] = u_brick_a4[iz][iy][ix];
	buf1_6[n++] = u_brick_a5[iz][iy][ix];
	buf1_6[n++] = u_brick_a6[iz][iy][ix];
      }

  if (comm->procneigh[1][0] == me)
    for (i = 0; i < n; i++) buf2_6[i] = buf1_6[i];
  else {
    MPI_Irecv(buf2_6,7*nbuf_6,MPI_FFT_SCALAR,comm->procneigh[1][1],0,world,&request);
    MPI_Send(buf1_6,n,MPI_FFT_SCALAR,comm->procneigh[1][0],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzlo_out_6; iz <= nzhi_out_6; iz++)
    for (iy = nyhi_in_6+1; iy <= nyhi_out_6; iy++)
      for (ix = nxlo_in_6; ix <= nxhi_in_6; ix++) {
	u_brick_a0[iz][iy][ix] = buf2_6[n++];
        u_brick_a1[iz][iy][ix] = buf2_6[n++];
        u_brick_a2[iz][iy][ix] = buf2_6[n++];
        u_brick_a3[iz][iy][ix] = buf2_6[n++];
        u_brick_a4[iz][iy][ix] = buf2_6[n++];
        u_brick_a5[iz][iy][ix] = buf2_6[n++];
        u_brick_a6[iz][iy][ix] = buf2_6[n++];
      }

  // pack my real cells for +x processor
  // pass data to self or +x processor
  // unpack and sum recv data into my ghost cells

  n = 0;
  for (iz = nzlo_out_6; iz <= nzhi_out_6; iz++)
    for (iy = nylo_out_6; iy <= nyhi_out_6; iy++)
      for (ix = nxhi_in_6-nxhi_ghost_6+1; ix <= nxhi_in_6; ix++) {
	buf1_6[n++] = u_brick_a0[iz][iy][ix];
	buf1_6[n++] = u_brick_a1[iz][iy][ix];
	buf1_6[n++] = u_brick_a2[iz][iy][ix];
	buf1_6[n++] = u_brick_a3[iz][iy][ix];
	buf1_6[n++] = u_brick_a4[iz][iy][ix];
	buf1_6[n++] = u_brick_a5[iz][iy][ix];
	buf1_6[n++] = u_brick_a6[iz][iy][ix];
      }

  if (comm->procneigh[0][1] == me)
    for (i = 0; i < n; i++) buf2_6[i] = buf1_6[i];
  else {
    MPI_Irecv(buf2_6,7*nbuf_6,MPI_FFT_SCALAR,comm->procneigh[0][0],0,world,&request);
    MPI_Send(buf1_6,n,MPI_FFT_SCALAR,comm->procneigh[0][1],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzlo_out_6; iz <= nzhi_out_6; iz++)
    for (iy = nylo_out_6; iy <= nyhi_out_6; iy++)
      for (ix = nxlo_out_6; ix < nxlo_in_6; ix++) {
	u_brick_a0[iz][iy][ix] = buf2_6[n++];
        u_brick_a1[iz][iy][ix] = buf2_6[n++];
        u_brick_a2[iz][iy][ix] = buf2_6[n++];
        u_brick_a3[iz][iy][ix] = buf2_6[n++];
        u_brick_a4[iz][iy][ix] = buf2_6[n++];
        u_brick_a5[iz][iy][ix] = buf2_6[n++];
        u_brick_a6[iz][iy][ix] = buf2_6[n++];
      }

  // pack my real cells for -x processor
  // pass data to self or -x processor
  // unpack and sum recv data into my ghost cells

  n = 0;
  for (iz = nzlo_out_6; iz <= nzhi_out_6; iz++)
    for (iy = nylo_out_6; iy <= nyhi_out_6; iy++)
      for (ix = nxlo_in_6; ix < nxlo_in_6+nxlo_ghost_6; ix++) {
	buf1_6[n++] = u_brick_a0[iz][iy][ix];
	buf1_6[n++] = u_brick_a1[iz][iy][ix];
	buf1_6[n++] = u_brick_a2[iz][iy][ix];
	buf1_6[n++] = u_brick_a3[iz][iy][ix];
	buf1_6[n++] = u_brick_a4[iz][iy][ix];
	buf1_6[n++] = u_brick_a5[iz][iy][ix];
	buf1_6[n++] = u_brick_a6[iz][iy][ix];
      }

  if (comm->procneigh[0][0] == me)
    for (i = 0; i < n; i++) buf2_6[i] = buf1_6[i];
  else {
    MPI_Irecv(buf2_6,7*nbuf_6,MPI_FFT_SCALAR,comm->procneigh[0][1],0,world,&request);
    MPI_Send(buf1_6,n,MPI_FFT_SCALAR,comm->procneigh[0][0],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzlo_out_6; iz <= nzhi_out_6; iz++)
    for (iy = nylo_out_6; iy <= nyhi_out_6; iy++)
      for (ix = nxhi_in_6+1; ix <= nxhi_out_6; ix++) {
	u_brick_a0[iz][iy][ix] = buf2_6[n++];
        u_brick_a1[iz][iy][ix] = buf2_6[n++];
        u_brick_a2[iz][iy][ix] = buf2_6[n++];
        u_brick_a3[iz][iy][ix] = buf2_6[n++];
        u_brick_a4[iz][iy][ix] = buf2_6[n++];
        u_brick_a5[iz][iy][ix] = buf2_6[n++];
        u_brick_a6[iz][iy][ix] = buf2_6[n++];
      }
}

/* ----------------------------------------------------------------------
   communication between procs for per atom calculations
   for ik scheme
------------------------------------------------------------------------- */

void PPPMDisp::fillbrick_a_peratom_ik()
{
  int i,n,ix,iy,iz;
  MPI_Request request;
  MPI_Status status;

  // pack my real cells for +z processor
  // pass data to self or +z processor
  // unpack and sum recv data into my ghost cells

  n = 0;
  for (iz = nzhi_in_6-nzhi_ghost_6+1; iz <= nzhi_in_6; iz++)
    for (iy = nylo_in_6; iy <= nyhi_in_6; iy++)
      for (ix = nxlo_in_6; ix <= nxhi_in_6; ix++) {
        if (eflag_atom) {
          buf3_6[n++] = u_brick_a0[iz][iy][ix];
          buf3_6[n++] = u_brick_a1[iz][iy][ix];
          buf3_6[n++] = u_brick_a2[iz][iy][ix];
          buf3_6[n++] = u_brick_a3[iz][iy][ix];
          buf3_6[n++] = u_brick_a4[iz][iy][ix];
          buf3_6[n++] = u_brick_a5[iz][iy][ix];
          buf3_6[n++] = u_brick_a6[iz][iy][ix];
        }
        if (vflag_atom) {
          buf3_6[n++] = v0_brick_a0[iz][iy][ix];
          buf3_6[n++] = v0_brick_a1[iz][iy][ix];
          buf3_6[n++] = v0_brick_a2[iz][iy][ix];
          buf3_6[n++] = v0_brick_a3[iz][iy][ix];
          buf3_6[n++] = v0_brick_a4[iz][iy][ix];
          buf3_6[n++] = v0_brick_a5[iz][iy][ix];
          buf3_6[n++] = v0_brick_a6[iz][iy][ix];
          buf3_6[n++] = v1_brick_a0[iz][iy][ix];
          buf3_6[n++] = v1_brick_a1[iz][iy][ix];
          buf3_6[n++] = v1_brick_a2[iz][iy][ix];
          buf3_6[n++] = v1_brick_a3[iz][iy][ix];
          buf3_6[n++] = v1_brick_a4[iz][iy][ix];
          buf3_6[n++] = v1_brick_a5[iz][iy][ix];
          buf3_6[n++] = v1_brick_a6[iz][iy][ix];
          buf3_6[n++] = v2_brick_a0[iz][iy][ix];
          buf3_6[n++] = v2_brick_a1[iz][iy][ix];
          buf3_6[n++] = v2_brick_a2[iz][iy][ix];
          buf3_6[n++] = v2_brick_a3[iz][iy][ix];
          buf3_6[n++] = v2_brick_a4[iz][iy][ix];
          buf3_6[n++] = v2_brick_a5[iz][iy][ix];
          buf3_6[n++] = v2_brick_a6[iz][iy][ix];
          buf3_6[n++] = v3_brick_a0[iz][iy][ix];
          buf3_6[n++] = v3_brick_a1[iz][iy][ix];
          buf3_6[n++] = v3_brick_a2[iz][iy][ix];
          buf3_6[n++] = v3_brick_a3[iz][iy][ix];
          buf3_6[n++] = v3_brick_a4[iz][iy][ix];
          buf3_6[n++] = v3_brick_a5[iz][iy][ix];
          buf3_6[n++] = v3_brick_a6[iz][iy][ix];
          buf3_6[n++] = v4_brick_a0[iz][iy][ix];
          buf3_6[n++] = v4_brick_a1[iz][iy][ix];
          buf3_6[n++] = v4_brick_a2[iz][iy][ix];
          buf3_6[n++] = v4_brick_a3[iz][iy][ix];
          buf3_6[n++] = v4_brick_a4[iz][iy][ix];
          buf3_6[n++] = v4_brick_a5[iz][iy][ix];
          buf3_6[n++] = v4_brick_a6[iz][iy][ix];
          buf3_6[n++] = v5_brick_a0[iz][iy][ix];
          buf3_6[n++] = v5_brick_a1[iz][iy][ix];
          buf3_6[n++] = v5_brick_a2[iz][iy][ix];
          buf3_6[n++] = v5_brick_a3[iz][iy][ix];
          buf3_6[n++] = v5_brick_a4[iz][iy][ix];
          buf3_6[n++] = v5_brick_a5[iz][iy][ix];
          buf3_6[n++] = v5_brick_a6[iz][iy][ix];
        }
      }


  if (comm->procneigh[2][1] == me)
    for (i = 0; i < n; i++) buf4_6[i] = buf3_6[i];
  else {
    MPI_Irecv(buf4_6,7*nbuf_pa_6,MPI_FFT_SCALAR,comm->procneigh[2][0],0,world,&request);
    MPI_Send(buf3_6,n,MPI_FFT_SCALAR,comm->procneigh[2][1],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzlo_out_6; iz < nzlo_in_6; iz++)
    for (iy = nylo_in_6; iy <= nyhi_in_6; iy++)
      for (ix = nxlo_in_6; ix <= nxhi_in_6; ix++) {
        if (eflag_atom) {
          u_brick_a0[iz][iy][ix] = buf4_6[n++];
          u_brick_a1[iz][iy][ix] = buf4_6[n++];
          u_brick_a2[iz][iy][ix] = buf4_6[n++];
          u_brick_a3[iz][iy][ix] = buf4_6[n++];
          u_brick_a4[iz][iy][ix] = buf4_6[n++];
          u_brick_a5[iz][iy][ix] = buf4_6[n++];
          u_brick_a6[iz][iy][ix] = buf4_6[n++];
        }
        if (vflag_atom) {
          v0_brick_a0[iz][iy][ix] = buf4_6[n++];
          v0_brick_a1[iz][iy][ix] = buf4_6[n++];
          v0_brick_a2[iz][iy][ix] = buf4_6[n++];
          v0_brick_a3[iz][iy][ix] = buf4_6[n++];
          v0_brick_a4[iz][iy][ix] = buf4_6[n++];
          v0_brick_a5[iz][iy][ix] = buf4_6[n++];
          v0_brick_a6[iz][iy][ix] = buf4_6[n++];
          v1_brick_a0[iz][iy][ix] = buf4_6[n++];
          v1_brick_a1[iz][iy][ix] = buf4_6[n++];
          v1_brick_a2[iz][iy][ix] = buf4_6[n++];
          v1_brick_a3[iz][iy][ix] = buf4_6[n++];
          v1_brick_a4[iz][iy][ix] = buf4_6[n++];
          v1_brick_a5[iz][iy][ix] = buf4_6[n++];
          v1_brick_a6[iz][iy][ix] = buf4_6[n++];
          v2_brick_a0[iz][iy][ix] = buf4_6[n++];
          v2_brick_a1[iz][iy][ix] = buf4_6[n++];
          v2_brick_a2[iz][iy][ix] = buf4_6[n++];
          v2_brick_a3[iz][iy][ix] = buf4_6[n++];
          v2_brick_a4[iz][iy][ix] = buf4_6[n++];
          v2_brick_a5[iz][iy][ix] = buf4_6[n++];
          v2_brick_a6[iz][iy][ix] = buf4_6[n++];
          v3_brick_a0[iz][iy][ix] = buf4_6[n++];
          v3_brick_a1[iz][iy][ix] = buf4_6[n++];
          v3_brick_a2[iz][iy][ix] = buf4_6[n++];
          v3_brick_a3[iz][iy][ix] = buf4_6[n++];
          v3_brick_a4[iz][iy][ix] = buf4_6[n++];
          v3_brick_a5[iz][iy][ix] = buf4_6[n++];
          v3_brick_a6[iz][iy][ix] = buf4_6[n++];
          v4_brick_a0[iz][iy][ix] = buf4_6[n++];
          v4_brick_a1[iz][iy][ix] = buf4_6[n++];
          v4_brick_a2[iz][iy][ix] = buf4_6[n++];
          v4_brick_a3[iz][iy][ix] = buf4_6[n++];
          v4_brick_a4[iz][iy][ix] = buf4_6[n++];
          v4_brick_a5[iz][iy][ix] = buf4_6[n++];
          v4_brick_a6[iz][iy][ix] = buf4_6[n++];
          v5_brick_a0[iz][iy][ix] = buf4_6[n++];
          v5_brick_a1[iz][iy][ix] = buf4_6[n++];
          v5_brick_a2[iz][iy][ix] = buf4_6[n++];
          v5_brick_a3[iz][iy][ix] = buf4_6[n++];
          v5_brick_a4[iz][iy][ix] = buf4_6[n++];
          v5_brick_a5[iz][iy][ix] = buf4_6[n++];
          v5_brick_a6[iz][iy][ix] = buf4_6[n++];
        }
      }

  // pack my real cells for -z processor
  // pass data to self or -z processor
  // unpack and sum recv data into my ghost cells

  n = 0;
  for (iz = nzlo_in_6; iz < nzlo_in_6+nzlo_ghost_6; iz++)
    for (iy = nylo_in_6; iy <= nyhi_in_6; iy++)
      for (ix = nxlo_in_6; ix <= nxhi_in_6; ix++) {
        if (eflag_atom) {
          buf3_6[n++] = u_brick_a0[iz][iy][ix];
          buf3_6[n++] = u_brick_a1[iz][iy][ix];
          buf3_6[n++] = u_brick_a2[iz][iy][ix];
          buf3_6[n++] = u_brick_a3[iz][iy][ix];
          buf3_6[n++] = u_brick_a4[iz][iy][ix];
          buf3_6[n++] = u_brick_a5[iz][iy][ix];
          buf3_6[n++] = u_brick_a6[iz][iy][ix];
        }
        if (vflag_atom) {
          buf3_6[n++] = v0_brick_a0[iz][iy][ix];
          buf3_6[n++] = v0_brick_a1[iz][iy][ix];
          buf3_6[n++] = v0_brick_a2[iz][iy][ix];
          buf3_6[n++] = v0_brick_a3[iz][iy][ix];
          buf3_6[n++] = v0_brick_a4[iz][iy][ix];
          buf3_6[n++] = v0_brick_a5[iz][iy][ix];
          buf3_6[n++] = v0_brick_a6[iz][iy][ix];
          buf3_6[n++] = v1_brick_a0[iz][iy][ix];
          buf3_6[n++] = v1_brick_a1[iz][iy][ix];
          buf3_6[n++] = v1_brick_a2[iz][iy][ix];
          buf3_6[n++] = v1_brick_a3[iz][iy][ix];
          buf3_6[n++] = v1_brick_a4[iz][iy][ix];
          buf3_6[n++] = v1_brick_a5[iz][iy][ix];
          buf3_6[n++] = v1_brick_a6[iz][iy][ix];
          buf3_6[n++] = v2_brick_a0[iz][iy][ix];
          buf3_6[n++] = v2_brick_a1[iz][iy][ix];
          buf3_6[n++] = v2_brick_a2[iz][iy][ix];
          buf3_6[n++] = v2_brick_a3[iz][iy][ix];
          buf3_6[n++] = v2_brick_a4[iz][iy][ix];
          buf3_6[n++] = v2_brick_a5[iz][iy][ix];
          buf3_6[n++] = v2_brick_a6[iz][iy][ix];
          buf3_6[n++] = v3_brick_a0[iz][iy][ix];
          buf3_6[n++] = v3_brick_a1[iz][iy][ix];
          buf3_6[n++] = v3_brick_a2[iz][iy][ix];
          buf3_6[n++] = v3_brick_a3[iz][iy][ix];
          buf3_6[n++] = v3_brick_a4[iz][iy][ix];
          buf3_6[n++] = v3_brick_a5[iz][iy][ix];
          buf3_6[n++] = v3_brick_a6[iz][iy][ix];
          buf3_6[n++] = v4_brick_a0[iz][iy][ix];
          buf3_6[n++] = v4_brick_a1[iz][iy][ix];
          buf3_6[n++] = v4_brick_a2[iz][iy][ix];
          buf3_6[n++] = v4_brick_a3[iz][iy][ix];
          buf3_6[n++] = v4_brick_a4[iz][iy][ix];
          buf3_6[n++] = v4_brick_a5[iz][iy][ix];
          buf3_6[n++] = v4_brick_a6[iz][iy][ix];
          buf3_6[n++] = v5_brick_a0[iz][iy][ix];
          buf3_6[n++] = v5_brick_a1[iz][iy][ix];
          buf3_6[n++] = v5_brick_a2[iz][iy][ix];
          buf3_6[n++] = v5_brick_a3[iz][iy][ix];
          buf3_6[n++] = v5_brick_a4[iz][iy][ix];
          buf3_6[n++] = v5_brick_a5[iz][iy][ix];
          buf3_6[n++] = v5_brick_a6[iz][iy][ix];
        }
      }

  if (comm->procneigh[2][0] == me)
    for (i = 0; i < n; i++) buf4_6[i] = buf3_6[i];
  else {
    MPI_Irecv(buf4_6,7*nbuf_pa_6,MPI_FFT_SCALAR,comm->procneigh[2][1],0,world,&request);
    MPI_Send(buf3_6,n,MPI_FFT_SCALAR,comm->procneigh[2][0],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzhi_in_6+1; iz <= nzhi_out_6; iz++)
    for (iy = nylo_in_6; iy <= nyhi_in_6; iy++)
      for (ix = nxlo_in_6; ix <= nxhi_in_6; ix++) {
        if (eflag_atom) {
          u_brick_a0[iz][iy][ix] = buf4_6[n++];
          u_brick_a1[iz][iy][ix] = buf4_6[n++];
          u_brick_a2[iz][iy][ix] = buf4_6[n++];
          u_brick_a3[iz][iy][ix] = buf4_6[n++];
          u_brick_a4[iz][iy][ix] = buf4_6[n++];
          u_brick_a5[iz][iy][ix] = buf4_6[n++];
          u_brick_a6[iz][iy][ix] = buf4_6[n++];
        }
        if (vflag_atom) {
          v0_brick_a0[iz][iy][ix] = buf4_6[n++];
          v0_brick_a1[iz][iy][ix] = buf4_6[n++];
          v0_brick_a2[iz][iy][ix] = buf4_6[n++];
          v0_brick_a3[iz][iy][ix] = buf4_6[n++];
          v0_brick_a4[iz][iy][ix] = buf4_6[n++];
          v0_brick_a5[iz][iy][ix] = buf4_6[n++];
          v0_brick_a6[iz][iy][ix] = buf4_6[n++];
          v1_brick_a0[iz][iy][ix] = buf4_6[n++];
          v1_brick_a1[iz][iy][ix] = buf4_6[n++];
          v1_brick_a2[iz][iy][ix] = buf4_6[n++];
          v1_brick_a3[iz][iy][ix] = buf4_6[n++];
          v1_brick_a4[iz][iy][ix] = buf4_6[n++];
          v1_brick_a5[iz][iy][ix] = buf4_6[n++];
          v1_brick_a6[iz][iy][ix] = buf4_6[n++];
          v2_brick_a0[iz][iy][ix] = buf4_6[n++];
          v2_brick_a1[iz][iy][ix] = buf4_6[n++];
          v2_brick_a2[iz][iy][ix] = buf4_6[n++];
          v2_brick_a3[iz][iy][ix] = buf4_6[n++];
          v2_brick_a4[iz][iy][ix] = buf4_6[n++];
          v2_brick_a5[iz][iy][ix] = buf4_6[n++];
          v2_brick_a6[iz][iy][ix] = buf4_6[n++];
          v3_brick_a0[iz][iy][ix] = buf4_6[n++];
          v3_brick_a1[iz][iy][ix] = buf4_6[n++];
          v3_brick_a2[iz][iy][ix] = buf4_6[n++];
          v3_brick_a3[iz][iy][ix] = buf4_6[n++];
          v3_brick_a4[iz][iy][ix] = buf4_6[n++];
          v3_brick_a5[iz][iy][ix] = buf4_6[n++];
          v3_brick_a6[iz][iy][ix] = buf4_6[n++];
          v4_brick_a0[iz][iy][ix] = buf4_6[n++];
          v4_brick_a1[iz][iy][ix] = buf4_6[n++];
          v4_brick_a2[iz][iy][ix] = buf4_6[n++];
          v4_brick_a3[iz][iy][ix] = buf4_6[n++];
          v4_brick_a4[iz][iy][ix] = buf4_6[n++];
          v4_brick_a5[iz][iy][ix] = buf4_6[n++];
          v4_brick_a6[iz][iy][ix] = buf4_6[n++];
          v5_brick_a0[iz][iy][ix] = buf4_6[n++];
          v5_brick_a1[iz][iy][ix] = buf4_6[n++];
          v5_brick_a2[iz][iy][ix] = buf4_6[n++];
          v5_brick_a3[iz][iy][ix] = buf4_6[n++];
          v5_brick_a4[iz][iy][ix] = buf4_6[n++];
          v5_brick_a5[iz][iy][ix] = buf4_6[n++];
          v5_brick_a6[iz][iy][ix] = buf4_6[n++];
        }
      }

  // pack my real cells for +y processor
  // pass data to self or +y processor
  // unpack and sum recv data into my ghost cells

  n = 0;
  for (iz = nzlo_out_6; iz <= nzhi_out_6; iz++)
    for (iy = nyhi_in_6-nyhi_ghost_6+1; iy <= nyhi_in_6; iy++)
      for (ix = nxlo_in_6; ix <= nxhi_in_6; ix++) {
        if (eflag_atom) {
          buf3_6[n++] = u_brick_a0[iz][iy][ix];
          buf3_6[n++] = u_brick_a1[iz][iy][ix];
          buf3_6[n++] = u_brick_a2[iz][iy][ix];
          buf3_6[n++] = u_brick_a3[iz][iy][ix];
          buf3_6[n++] = u_brick_a4[iz][iy][ix];
          buf3_6[n++] = u_brick_a5[iz][iy][ix];
          buf3_6[n++] = u_brick_a6[iz][iy][ix];
        }
        if (vflag_atom) {
          buf3_6[n++] = v0_brick_a0[iz][iy][ix];
          buf3_6[n++] = v0_brick_a1[iz][iy][ix];
          buf3_6[n++] = v0_brick_a2[iz][iy][ix];
          buf3_6[n++] = v0_brick_a3[iz][iy][ix];
          buf3_6[n++] = v0_brick_a4[iz][iy][ix];
          buf3_6[n++] = v0_brick_a5[iz][iy][ix];
          buf3_6[n++] = v0_brick_a6[iz][iy][ix];
          buf3_6[n++] = v1_brick_a0[iz][iy][ix];
          buf3_6[n++] = v1_brick_a1[iz][iy][ix];
          buf3_6[n++] = v1_brick_a2[iz][iy][ix];
          buf3_6[n++] = v1_brick_a3[iz][iy][ix];
          buf3_6[n++] = v1_brick_a4[iz][iy][ix];
          buf3_6[n++] = v1_brick_a5[iz][iy][ix];
          buf3_6[n++] = v1_brick_a6[iz][iy][ix];
          buf3_6[n++] = v2_brick_a0[iz][iy][ix];
          buf3_6[n++] = v2_brick_a1[iz][iy][ix];
          buf3_6[n++] = v2_brick_a2[iz][iy][ix];
          buf3_6[n++] = v2_brick_a3[iz][iy][ix];
          buf3_6[n++] = v2_brick_a4[iz][iy][ix];
          buf3_6[n++] = v2_brick_a5[iz][iy][ix];
          buf3_6[n++] = v2_brick_a6[iz][iy][ix];
          buf3_6[n++] = v3_brick_a0[iz][iy][ix];
          buf3_6[n++] = v3_brick_a1[iz][iy][ix];
          buf3_6[n++] = v3_brick_a2[iz][iy][ix];
          buf3_6[n++] = v3_brick_a3[iz][iy][ix];
          buf3_6[n++] = v3_brick_a4[iz][iy][ix];
          buf3_6[n++] = v3_brick_a5[iz][iy][ix];
          buf3_6[n++] = v3_brick_a6[iz][iy][ix];
          buf3_6[n++] = v4_brick_a0[iz][iy][ix];
          buf3_6[n++] = v4_brick_a1[iz][iy][ix];
          buf3_6[n++] = v4_brick_a2[iz][iy][ix];
          buf3_6[n++] = v4_brick_a3[iz][iy][ix];
          buf3_6[n++] = v4_brick_a4[iz][iy][ix];
          buf3_6[n++] = v4_brick_a5[iz][iy][ix];
          buf3_6[n++] = v4_brick_a6[iz][iy][ix];
          buf3_6[n++] = v5_brick_a0[iz][iy][ix];
          buf3_6[n++] = v5_brick_a1[iz][iy][ix];
          buf3_6[n++] = v5_brick_a2[iz][iy][ix];
          buf3_6[n++] = v5_brick_a3[iz][iy][ix];
          buf3_6[n++] = v5_brick_a4[iz][iy][ix];
          buf3_6[n++] = v5_brick_a5[iz][iy][ix];
          buf3_6[n++] = v5_brick_a6[iz][iy][ix];
        }
      }

  if (comm->procneigh[1][1] == me)
    for (i = 0; i < n; i++) buf4_6[i] = buf3_6[i];
  else {
    MPI_Irecv(buf4_6,7*nbuf_pa_6,MPI_FFT_SCALAR,comm->procneigh[1][0],0,world,&request);
    MPI_Send(buf3_6,n,MPI_FFT_SCALAR,comm->procneigh[1][1],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzlo_out_6; iz <= nzhi_out_6; iz++)
    for (iy = nylo_out_6; iy < nylo_in_6; iy++)
      for (ix = nxlo_in_6; ix <= nxhi_in_6; ix++) {
        if (eflag_atom) {
          u_brick_a0[iz][iy][ix] = buf4_6[n++];
          u_brick_a1[iz][iy][ix] = buf4_6[n++];
          u_brick_a2[iz][iy][ix] = buf4_6[n++];
          u_brick_a3[iz][iy][ix] = buf4_6[n++];
          u_brick_a4[iz][iy][ix] = buf4_6[n++];
          u_brick_a5[iz][iy][ix] = buf4_6[n++];
          u_brick_a6[iz][iy][ix] = buf4_6[n++];
        }
        if (vflag_atom) {
          v0_brick_a0[iz][iy][ix] = buf4_6[n++];
          v0_brick_a1[iz][iy][ix] = buf4_6[n++];
          v0_brick_a2[iz][iy][ix] = buf4_6[n++];
          v0_brick_a3[iz][iy][ix] = buf4_6[n++];
          v0_brick_a4[iz][iy][ix] = buf4_6[n++];
          v0_brick_a5[iz][iy][ix] = buf4_6[n++];
          v0_brick_a6[iz][iy][ix] = buf4_6[n++];
          v1_brick_a0[iz][iy][ix] = buf4_6[n++];
          v1_brick_a1[iz][iy][ix] = buf4_6[n++];
          v1_brick_a2[iz][iy][ix] = buf4_6[n++];
          v1_brick_a3[iz][iy][ix] = buf4_6[n++];
          v1_brick_a4[iz][iy][ix] = buf4_6[n++];
          v1_brick_a5[iz][iy][ix] = buf4_6[n++];
          v1_brick_a6[iz][iy][ix] = buf4_6[n++];
          v2_brick_a0[iz][iy][ix] = buf4_6[n++];
          v2_brick_a1[iz][iy][ix] = buf4_6[n++];
          v2_brick_a2[iz][iy][ix] = buf4_6[n++];
          v2_brick_a3[iz][iy][ix] = buf4_6[n++];
          v2_brick_a4[iz][iy][ix] = buf4_6[n++];
          v2_brick_a5[iz][iy][ix] = buf4_6[n++];
          v2_brick_a6[iz][iy][ix] = buf4_6[n++];
          v3_brick_a0[iz][iy][ix] = buf4_6[n++];
          v3_brick_a1[iz][iy][ix] = buf4_6[n++];
          v3_brick_a2[iz][iy][ix] = buf4_6[n++];
          v3_brick_a3[iz][iy][ix] = buf4_6[n++];
          v3_brick_a4[iz][iy][ix] = buf4_6[n++];
          v3_brick_a5[iz][iy][ix] = buf4_6[n++];
          v3_brick_a6[iz][iy][ix] = buf4_6[n++];
          v4_brick_a0[iz][iy][ix] = buf4_6[n++];
          v4_brick_a1[iz][iy][ix] = buf4_6[n++];
          v4_brick_a2[iz][iy][ix] = buf4_6[n++];
          v4_brick_a3[iz][iy][ix] = buf4_6[n++];
          v4_brick_a4[iz][iy][ix] = buf4_6[n++];
          v4_brick_a5[iz][iy][ix] = buf4_6[n++];
          v4_brick_a6[iz][iy][ix] = buf4_6[n++];
          v5_brick_a0[iz][iy][ix] = buf4_6[n++];
          v5_brick_a1[iz][iy][ix] = buf4_6[n++];
          v5_brick_a2[iz][iy][ix] = buf4_6[n++];
          v5_brick_a3[iz][iy][ix] = buf4_6[n++];
          v5_brick_a4[iz][iy][ix] = buf4_6[n++];
          v5_brick_a5[iz][iy][ix] = buf4_6[n++];
          v5_brick_a6[iz][iy][ix] = buf4_6[n++];
        }
      }

  // pack my real cells for -y processor
  // pass data to self or -y processor
  // unpack and sum recv data into my ghost cells

  n = 0;
  for (iz = nzlo_out_6; iz <= nzhi_out_6; iz++)
    for (iy = nylo_in_6; iy < nylo_in_6+nylo_ghost_6; iy++)
      for (ix = nxlo_in_6; ix <= nxhi_in_6; ix++) {
        if (eflag_atom) {
          buf3_6[n++] = u_brick_a0[iz][iy][ix];
          buf3_6[n++] = u_brick_a1[iz][iy][ix];
          buf3_6[n++] = u_brick_a2[iz][iy][ix];
          buf3_6[n++] = u_brick_a3[iz][iy][ix];
          buf3_6[n++] = u_brick_a4[iz][iy][ix];
          buf3_6[n++] = u_brick_a5[iz][iy][ix];
          buf3_6[n++] = u_brick_a6[iz][iy][ix];
        }
        if (vflag_atom) {
          buf3_6[n++] = v0_brick_a0[iz][iy][ix];
          buf3_6[n++] = v0_brick_a1[iz][iy][ix];
          buf3_6[n++] = v0_brick_a2[iz][iy][ix];
          buf3_6[n++] = v0_brick_a3[iz][iy][ix];
          buf3_6[n++] = v0_brick_a4[iz][iy][ix];
          buf3_6[n++] = v0_brick_a5[iz][iy][ix];
          buf3_6[n++] = v0_brick_a6[iz][iy][ix];
          buf3_6[n++] = v1_brick_a0[iz][iy][ix];
          buf3_6[n++] = v1_brick_a1[iz][iy][ix];
          buf3_6[n++] = v1_brick_a2[iz][iy][ix];
          buf3_6[n++] = v1_brick_a3[iz][iy][ix];
          buf3_6[n++] = v1_brick_a4[iz][iy][ix];
          buf3_6[n++] = v1_brick_a5[iz][iy][ix];
          buf3_6[n++] = v1_brick_a6[iz][iy][ix];
          buf3_6[n++] = v2_brick_a0[iz][iy][ix];
          buf3_6[n++] = v2_brick_a1[iz][iy][ix];
          buf3_6[n++] = v2_brick_a2[iz][iy][ix];
          buf3_6[n++] = v2_brick_a3[iz][iy][ix];
          buf3_6[n++] = v2_brick_a4[iz][iy][ix];
          buf3_6[n++] = v2_brick_a5[iz][iy][ix];
          buf3_6[n++] = v2_brick_a6[iz][iy][ix];
          buf3_6[n++] = v3_brick_a0[iz][iy][ix];
          buf3_6[n++] = v3_brick_a1[iz][iy][ix];
          buf3_6[n++] = v3_brick_a2[iz][iy][ix];
          buf3_6[n++] = v3_brick_a3[iz][iy][ix];
          buf3_6[n++] = v3_brick_a4[iz][iy][ix];
          buf3_6[n++] = v3_brick_a5[iz][iy][ix];
          buf3_6[n++] = v3_brick_a6[iz][iy][ix];
          buf3_6[n++] = v4_brick_a0[iz][iy][ix];
          buf3_6[n++] = v4_brick_a1[iz][iy][ix];
          buf3_6[n++] = v4_brick_a2[iz][iy][ix];
          buf3_6[n++] = v4_brick_a3[iz][iy][ix];
          buf3_6[n++] = v4_brick_a4[iz][iy][ix];
          buf3_6[n++] = v4_brick_a5[iz][iy][ix];
          buf3_6[n++] = v4_brick_a6[iz][iy][ix];
          buf3_6[n++] = v5_brick_a0[iz][iy][ix];
          buf3_6[n++] = v5_brick_a1[iz][iy][ix];
          buf3_6[n++] = v5_brick_a2[iz][iy][ix];
          buf3_6[n++] = v5_brick_a3[iz][iy][ix];
          buf3_6[n++] = v5_brick_a4[iz][iy][ix];
          buf3_6[n++] = v5_brick_a5[iz][iy][ix];
          buf3_6[n++] = v5_brick_a6[iz][iy][ix];
        }
      }

  if (comm->procneigh[1][0] == me)
    for (i = 0; i < n; i++) buf4_6[i] = buf3_6[i];
  else {
    MPI_Irecv(buf4_6,7*nbuf_pa_6,MPI_FFT_SCALAR,comm->procneigh[1][1],0,world,&request);
    MPI_Send(buf3_6,n,MPI_FFT_SCALAR,comm->procneigh[1][0],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzlo_out_6; iz <= nzhi_out_6; iz++)
    for (iy = nyhi_in_6+1; iy <= nyhi_out_6; iy++)
      for (ix = nxlo_in_6; ix <= nxhi_in_6; ix++) {
        if (eflag_atom) {
          u_brick_a0[iz][iy][ix] = buf4_6[n++];
          u_brick_a1[iz][iy][ix] = buf4_6[n++];
          u_brick_a2[iz][iy][ix] = buf4_6[n++];
          u_brick_a3[iz][iy][ix] = buf4_6[n++];
          u_brick_a4[iz][iy][ix] = buf4_6[n++];
          u_brick_a5[iz][iy][ix] = buf4_6[n++];
          u_brick_a6[iz][iy][ix] = buf4_6[n++];
        }
        if (vflag_atom) {
          v0_brick_a0[iz][iy][ix] = buf4_6[n++];
          v0_brick_a1[iz][iy][ix] = buf4_6[n++];
          v0_brick_a2[iz][iy][ix] = buf4_6[n++];
          v0_brick_a3[iz][iy][ix] = buf4_6[n++];
          v0_brick_a4[iz][iy][ix] = buf4_6[n++];
          v0_brick_a5[iz][iy][ix] = buf4_6[n++];
          v0_brick_a6[iz][iy][ix] = buf4_6[n++];
          v1_brick_a0[iz][iy][ix] = buf4_6[n++];
          v1_brick_a1[iz][iy][ix] = buf4_6[n++];
          v1_brick_a2[iz][iy][ix] = buf4_6[n++];
          v1_brick_a3[iz][iy][ix] = buf4_6[n++];
          v1_brick_a4[iz][iy][ix] = buf4_6[n++];
          v1_brick_a5[iz][iy][ix] = buf4_6[n++];
          v1_brick_a6[iz][iy][ix] = buf4_6[n++];
          v2_brick_a0[iz][iy][ix] = buf4_6[n++];
          v2_brick_a1[iz][iy][ix] = buf4_6[n++];
          v2_brick_a2[iz][iy][ix] = buf4_6[n++];
          v2_brick_a3[iz][iy][ix] = buf4_6[n++];
          v2_brick_a4[iz][iy][ix] = buf4_6[n++];
          v2_brick_a5[iz][iy][ix] = buf4_6[n++];
          v2_brick_a6[iz][iy][ix] = buf4_6[n++];
          v3_brick_a0[iz][iy][ix] = buf4_6[n++];
          v3_brick_a1[iz][iy][ix] = buf4_6[n++];
          v3_brick_a2[iz][iy][ix] = buf4_6[n++];
          v3_brick_a3[iz][iy][ix] = buf4_6[n++];
          v3_brick_a4[iz][iy][ix] = buf4_6[n++];
          v3_brick_a5[iz][iy][ix] = buf4_6[n++];
          v3_brick_a6[iz][iy][ix] = buf4_6[n++];
          v4_brick_a0[iz][iy][ix] = buf4_6[n++];
          v4_brick_a1[iz][iy][ix] = buf4_6[n++];
          v4_brick_a2[iz][iy][ix] = buf4_6[n++];
          v4_brick_a3[iz][iy][ix] = buf4_6[n++];
          v4_brick_a4[iz][iy][ix] = buf4_6[n++];
          v4_brick_a5[iz][iy][ix] = buf4_6[n++];
          v4_brick_a6[iz][iy][ix] = buf4_6[n++];
          v5_brick_a0[iz][iy][ix] = buf4_6[n++];
          v5_brick_a1[iz][iy][ix] = buf4_6[n++];
          v5_brick_a2[iz][iy][ix] = buf4_6[n++];
          v5_brick_a3[iz][iy][ix] = buf4_6[n++];
          v5_brick_a4[iz][iy][ix] = buf4_6[n++];
          v5_brick_a5[iz][iy][ix] = buf4_6[n++];
          v5_brick_a6[iz][iy][ix] = buf4_6[n++];
        }
      }

  // pack my real cells for +x processor
  // pass data to self or +x processor
  // unpack and sum recv data into my ghost cells

  n = 0;
  for (iz = nzlo_out_6; iz <= nzhi_out_6; iz++)
    for (iy = nylo_out_6; iy <= nyhi_out_6; iy++)
      for (ix = nxhi_in_6-nxhi_ghost_6+1; ix <= nxhi_in_6; ix++) {
        if (eflag_atom) {
          buf3_6[n++] = u_brick_a0[iz][iy][ix];
          buf3_6[n++] = u_brick_a1[iz][iy][ix];
          buf3_6[n++] = u_brick_a2[iz][iy][ix];
          buf3_6[n++] = u_brick_a3[iz][iy][ix];
          buf3_6[n++] = u_brick_a4[iz][iy][ix];
          buf3_6[n++] = u_brick_a5[iz][iy][ix];
          buf3_6[n++] = u_brick_a6[iz][iy][ix];
        }
        if (vflag_atom) {
          buf3_6[n++] = v0_brick_a0[iz][iy][ix];
          buf3_6[n++] = v0_brick_a1[iz][iy][ix];
          buf3_6[n++] = v0_brick_a2[iz][iy][ix];
          buf3_6[n++] = v0_brick_a3[iz][iy][ix];
          buf3_6[n++] = v0_brick_a4[iz][iy][ix];
          buf3_6[n++] = v0_brick_a5[iz][iy][ix];
          buf3_6[n++] = v0_brick_a6[iz][iy][ix];
          buf3_6[n++] = v1_brick_a0[iz][iy][ix];
          buf3_6[n++] = v1_brick_a1[iz][iy][ix];
          buf3_6[n++] = v1_brick_a2[iz][iy][ix];
          buf3_6[n++] = v1_brick_a3[iz][iy][ix];
          buf3_6[n++] = v1_brick_a4[iz][iy][ix];
          buf3_6[n++] = v1_brick_a5[iz][iy][ix];
          buf3_6[n++] = v1_brick_a6[iz][iy][ix];
          buf3_6[n++] = v2_brick_a0[iz][iy][ix];
          buf3_6[n++] = v2_brick_a1[iz][iy][ix];
          buf3_6[n++] = v2_brick_a2[iz][iy][ix];
          buf3_6[n++] = v2_brick_a3[iz][iy][ix];
          buf3_6[n++] = v2_brick_a4[iz][iy][ix];
          buf3_6[n++] = v2_brick_a5[iz][iy][ix];
          buf3_6[n++] = v2_brick_a6[iz][iy][ix];
          buf3_6[n++] = v3_brick_a0[iz][iy][ix];
          buf3_6[n++] = v3_brick_a1[iz][iy][ix];
          buf3_6[n++] = v3_brick_a2[iz][iy][ix];
          buf3_6[n++] = v3_brick_a3[iz][iy][ix];
          buf3_6[n++] = v3_brick_a4[iz][iy][ix];
          buf3_6[n++] = v3_brick_a5[iz][iy][ix];
          buf3_6[n++] = v3_brick_a6[iz][iy][ix];
          buf3_6[n++] = v4_brick_a0[iz][iy][ix];
          buf3_6[n++] = v4_brick_a1[iz][iy][ix];
          buf3_6[n++] = v4_brick_a2[iz][iy][ix];
          buf3_6[n++] = v4_brick_a3[iz][iy][ix];
          buf3_6[n++] = v4_brick_a4[iz][iy][ix];
          buf3_6[n++] = v4_brick_a5[iz][iy][ix];
          buf3_6[n++] = v4_brick_a6[iz][iy][ix];
          buf3_6[n++] = v5_brick_a0[iz][iy][ix];
          buf3_6[n++] = v5_brick_a1[iz][iy][ix];
          buf3_6[n++] = v5_brick_a2[iz][iy][ix];
          buf3_6[n++] = v5_brick_a3[iz][iy][ix];
          buf3_6[n++] = v5_brick_a4[iz][iy][ix];
          buf3_6[n++] = v5_brick_a5[iz][iy][ix];
          buf3_6[n++] = v5_brick_a6[iz][iy][ix];
        }
      }

  if (comm->procneigh[0][1] == me)
    for (i = 0; i < n; i++) buf4_6[i] = buf3_6[i];
  else {
    MPI_Irecv(buf4_6,7*nbuf_pa_6,MPI_FFT_SCALAR,comm->procneigh[0][0],0,world,&request);
    MPI_Send(buf3_6,n,MPI_FFT_SCALAR,comm->procneigh[0][1],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzlo_out_6; iz <= nzhi_out_6; iz++)
    for (iy = nylo_out_6; iy <= nyhi_out_6; iy++)
      for (ix = nxlo_out_6; ix < nxlo_in_6; ix++) {
        if (eflag_atom) {
          u_brick_a0[iz][iy][ix] = buf4_6[n++];
          u_brick_a1[iz][iy][ix] = buf4_6[n++];
          u_brick_a2[iz][iy][ix] = buf4_6[n++];
          u_brick_a3[iz][iy][ix] = buf4_6[n++];
          u_brick_a4[iz][iy][ix] = buf4_6[n++];
          u_brick_a5[iz][iy][ix] = buf4_6[n++];
          u_brick_a6[iz][iy][ix] = buf4_6[n++];
        }
        if (vflag_atom) {
          v0_brick_a0[iz][iy][ix] = buf4_6[n++];
          v0_brick_a1[iz][iy][ix] = buf4_6[n++];
          v0_brick_a2[iz][iy][ix] = buf4_6[n++];
          v0_brick_a3[iz][iy][ix] = buf4_6[n++];
          v0_brick_a4[iz][iy][ix] = buf4_6[n++];
          v0_brick_a5[iz][iy][ix] = buf4_6[n++];
          v0_brick_a6[iz][iy][ix] = buf4_6[n++];
          v1_brick_a0[iz][iy][ix] = buf4_6[n++];
          v1_brick_a1[iz][iy][ix] = buf4_6[n++];
          v1_brick_a2[iz][iy][ix] = buf4_6[n++];
          v1_brick_a3[iz][iy][ix] = buf4_6[n++];
          v1_brick_a4[iz][iy][ix] = buf4_6[n++];
          v1_brick_a5[iz][iy][ix] = buf4_6[n++];
          v1_brick_a6[iz][iy][ix] = buf4_6[n++];
          v2_brick_a0[iz][iy][ix] = buf4_6[n++];
          v2_brick_a1[iz][iy][ix] = buf4_6[n++];
          v2_brick_a2[iz][iy][ix] = buf4_6[n++];
          v2_brick_a3[iz][iy][ix] = buf4_6[n++];
          v2_brick_a4[iz][iy][ix] = buf4_6[n++];
          v2_brick_a5[iz][iy][ix] = buf4_6[n++];
          v2_brick_a6[iz][iy][ix] = buf4_6[n++];
          v3_brick_a0[iz][iy][ix] = buf4_6[n++];
          v3_brick_a1[iz][iy][ix] = buf4_6[n++];
          v3_brick_a2[iz][iy][ix] = buf4_6[n++];
          v3_brick_a3[iz][iy][ix] = buf4_6[n++];
          v3_brick_a4[iz][iy][ix] = buf4_6[n++];
          v3_brick_a5[iz][iy][ix] = buf4_6[n++];
          v3_brick_a6[iz][iy][ix] = buf4_6[n++];
          v4_brick_a0[iz][iy][ix] = buf4_6[n++];
          v4_brick_a1[iz][iy][ix] = buf4_6[n++];
          v4_brick_a2[iz][iy][ix] = buf4_6[n++];
          v4_brick_a3[iz][iy][ix] = buf4_6[n++];
          v4_brick_a4[iz][iy][ix] = buf4_6[n++];
          v4_brick_a5[iz][iy][ix] = buf4_6[n++];
          v4_brick_a6[iz][iy][ix] = buf4_6[n++];
          v5_brick_a0[iz][iy][ix] = buf4_6[n++];
          v5_brick_a1[iz][iy][ix] = buf4_6[n++];
          v5_brick_a2[iz][iy][ix] = buf4_6[n++];
          v5_brick_a3[iz][iy][ix] = buf4_6[n++];
          v5_brick_a4[iz][iy][ix] = buf4_6[n++];
          v5_brick_a5[iz][iy][ix] = buf4_6[n++];
          v5_brick_a6[iz][iy][ix] = buf4_6[n++];
        }
      }

  // pack my real cells for -x processor
  // pass data to self or -x processor
  // unpack and sum recv data into my ghost cells

  n = 0;
  for (iz = nzlo_out_6; iz <= nzhi_out_6; iz++)
    for (iy = nylo_out_6; iy <= nyhi_out_6; iy++)
      for (ix = nxlo_in_6; ix < nxlo_in_6+nxlo_ghost_6; ix++) {
        if (eflag_atom) {
          buf3_6[n++] = u_brick_a0[iz][iy][ix];
          buf3_6[n++] = u_brick_a1[iz][iy][ix];
          buf3_6[n++] = u_brick_a2[iz][iy][ix];
          buf3_6[n++] = u_brick_a3[iz][iy][ix];
          buf3_6[n++] = u_brick_a4[iz][iy][ix];
          buf3_6[n++] = u_brick_a5[iz][iy][ix];
          buf3_6[n++] = u_brick_a6[iz][iy][ix];
        }
        if (vflag_atom) {
          buf3_6[n++] = v0_brick_a0[iz][iy][ix];
          buf3_6[n++] = v0_brick_a1[iz][iy][ix];
          buf3_6[n++] = v0_brick_a2[iz][iy][ix];
          buf3_6[n++] = v0_brick_a3[iz][iy][ix];
          buf3_6[n++] = v0_brick_a4[iz][iy][ix];
          buf3_6[n++] = v0_brick_a5[iz][iy][ix];
          buf3_6[n++] = v0_brick_a6[iz][iy][ix];
          buf3_6[n++] = v1_brick_a0[iz][iy][ix];
          buf3_6[n++] = v1_brick_a1[iz][iy][ix];
          buf3_6[n++] = v1_brick_a2[iz][iy][ix];
          buf3_6[n++] = v1_brick_a3[iz][iy][ix];
          buf3_6[n++] = v1_brick_a4[iz][iy][ix];
          buf3_6[n++] = v1_brick_a5[iz][iy][ix];
          buf3_6[n++] = v1_brick_a6[iz][iy][ix];
          buf3_6[n++] = v2_brick_a0[iz][iy][ix];
          buf3_6[n++] = v2_brick_a1[iz][iy][ix];
          buf3_6[n++] = v2_brick_a2[iz][iy][ix];
          buf3_6[n++] = v2_brick_a3[iz][iy][ix];
          buf3_6[n++] = v2_brick_a4[iz][iy][ix];
          buf3_6[n++] = v2_brick_a5[iz][iy][ix];
          buf3_6[n++] = v2_brick_a6[iz][iy][ix];
          buf3_6[n++] = v3_brick_a0[iz][iy][ix];
          buf3_6[n++] = v3_brick_a1[iz][iy][ix];
          buf3_6[n++] = v3_brick_a2[iz][iy][ix];
          buf3_6[n++] = v3_brick_a3[iz][iy][ix];
          buf3_6[n++] = v3_brick_a4[iz][iy][ix];
          buf3_6[n++] = v3_brick_a5[iz][iy][ix];
          buf3_6[n++] = v3_brick_a6[iz][iy][ix];
          buf3_6[n++] = v4_brick_a0[iz][iy][ix];
          buf3_6[n++] = v4_brick_a1[iz][iy][ix];
          buf3_6[n++] = v4_brick_a2[iz][iy][ix];
          buf3_6[n++] = v4_brick_a3[iz][iy][ix];
          buf3_6[n++] = v4_brick_a4[iz][iy][ix];
          buf3_6[n++] = v4_brick_a5[iz][iy][ix];
          buf3_6[n++] = v4_brick_a6[iz][iy][ix];
          buf3_6[n++] = v5_brick_a0[iz][iy][ix];
          buf3_6[n++] = v5_brick_a1[iz][iy][ix];
          buf3_6[n++] = v5_brick_a2[iz][iy][ix];
          buf3_6[n++] = v5_brick_a3[iz][iy][ix];
          buf3_6[n++] = v5_brick_a4[iz][iy][ix];
          buf3_6[n++] = v5_brick_a5[iz][iy][ix];
          buf3_6[n++] = v5_brick_a6[iz][iy][ix];
        }
      }

  if (comm->procneigh[0][0] == me)
    for (i = 0; i < n; i++) buf4_6[i] = buf3_6[i];
  else {
    MPI_Irecv(buf4_6,7*nbuf_pa_6,MPI_FFT_SCALAR,comm->procneigh[0][1],0,world,&request);
    MPI_Send(buf3_6,n,MPI_FFT_SCALAR,comm->procneigh[0][0],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzlo_out_6; iz <= nzhi_out_6; iz++)
    for (iy = nylo_out_6; iy <= nyhi_out_6; iy++)
      for (ix = nxhi_in_6+1; ix <= nxhi_out_6; ix++) {
        if (eflag_atom) {
          u_brick_a0[iz][iy][ix] = buf4_6[n++];
          u_brick_a1[iz][iy][ix] = buf4_6[n++];
          u_brick_a2[iz][iy][ix] = buf4_6[n++];
          u_brick_a3[iz][iy][ix] = buf4_6[n++];
          u_brick_a4[iz][iy][ix] = buf4_6[n++];
          u_brick_a5[iz][iy][ix] = buf4_6[n++];
          u_brick_a6[iz][iy][ix] = buf4_6[n++];
        }
        if (vflag_atom) {
          v0_brick_a0[iz][iy][ix] = buf4_6[n++];
          v0_brick_a1[iz][iy][ix] = buf4_6[n++];
          v0_brick_a2[iz][iy][ix] = buf4_6[n++];
          v0_brick_a3[iz][iy][ix] = buf4_6[n++];
          v0_brick_a4[iz][iy][ix] = buf4_6[n++];
          v0_brick_a5[iz][iy][ix] = buf4_6[n++];
          v0_brick_a6[iz][iy][ix] = buf4_6[n++];
          v1_brick_a0[iz][iy][ix] = buf4_6[n++];
          v1_brick_a1[iz][iy][ix] = buf4_6[n++];
          v1_brick_a2[iz][iy][ix] = buf4_6[n++];
          v1_brick_a3[iz][iy][ix] = buf4_6[n++];
          v1_brick_a4[iz][iy][ix] = buf4_6[n++];
          v1_brick_a5[iz][iy][ix] = buf4_6[n++];
          v1_brick_a6[iz][iy][ix] = buf4_6[n++];
          v2_brick_a0[iz][iy][ix] = buf4_6[n++];
          v2_brick_a1[iz][iy][ix] = buf4_6[n++];
          v2_brick_a2[iz][iy][ix] = buf4_6[n++];
          v2_brick_a3[iz][iy][ix] = buf4_6[n++];
          v2_brick_a4[iz][iy][ix] = buf4_6[n++];
          v2_brick_a5[iz][iy][ix] = buf4_6[n++];
          v2_brick_a6[iz][iy][ix] = buf4_6[n++];
          v3_brick_a0[iz][iy][ix] = buf4_6[n++];
          v3_brick_a1[iz][iy][ix] = buf4_6[n++];
          v3_brick_a2[iz][iy][ix] = buf4_6[n++];
          v3_brick_a3[iz][iy][ix] = buf4_6[n++];
          v3_brick_a4[iz][iy][ix] = buf4_6[n++];
          v3_brick_a5[iz][iy][ix] = buf4_6[n++];
          v3_brick_a6[iz][iy][ix] = buf4_6[n++];
          v4_brick_a0[iz][iy][ix] = buf4_6[n++];
          v4_brick_a1[iz][iy][ix] = buf4_6[n++];
          v4_brick_a2[iz][iy][ix] = buf4_6[n++];
          v4_brick_a3[iz][iy][ix] = buf4_6[n++];
          v4_brick_a4[iz][iy][ix] = buf4_6[n++];
          v4_brick_a5[iz][iy][ix] = buf4_6[n++];
          v4_brick_a6[iz][iy][ix] = buf4_6[n++];
          v5_brick_a0[iz][iy][ix] = buf4_6[n++];
          v5_brick_a1[iz][iy][ix] = buf4_6[n++];
          v5_brick_a2[iz][iy][ix] = buf4_6[n++];
          v5_brick_a3[iz][iy][ix] = buf4_6[n++];
          v5_brick_a4[iz][iy][ix] = buf4_6[n++];
          v5_brick_a5[iz][iy][ix] = buf4_6[n++];
          v5_brick_a6[iz][iy][ix] = buf4_6[n++];
        }
      }
}

/* ----------------------------------------------------------------------
   communication between procs for per atom calculations
   for ik scheme
------------------------------------------------------------------------- */

void PPPMDisp::fillbrick_a_peratom_ad()
{
  int i,n,ix,iy,iz;
  MPI_Request request;
  MPI_Status status;

  // pack my real cells for +z processor
  // pass data to self or +z processor
  // unpack and sum recv data into my ghost cells

  n = 0;
  for (iz = nzhi_in_6-nzhi_ghost_6+1; iz <= nzhi_in_6; iz++)
    for (iy = nylo_in_6; iy <= nyhi_in_6; iy++)
      for (ix = nxlo_in_6; ix <= nxhi_in_6; ix++) {
        buf3_6[n++] = v0_brick_a0[iz][iy][ix];
        buf3_6[n++] = v0_brick_a1[iz][iy][ix];
        buf3_6[n++] = v0_brick_a2[iz][iy][ix];
        buf3_6[n++] = v0_brick_a3[iz][iy][ix];
        buf3_6[n++] = v0_brick_a4[iz][iy][ix];
        buf3_6[n++] = v0_brick_a5[iz][iy][ix];
        buf3_6[n++] = v0_brick_a6[iz][iy][ix];
        buf3_6[n++] = v1_brick_a0[iz][iy][ix];
        buf3_6[n++] = v1_brick_a1[iz][iy][ix];
        buf3_6[n++] = v1_brick_a2[iz][iy][ix];
        buf3_6[n++] = v1_brick_a3[iz][iy][ix];
        buf3_6[n++] = v1_brick_a4[iz][iy][ix];
        buf3_6[n++] = v1_brick_a5[iz][iy][ix];
        buf3_6[n++] = v1_brick_a6[iz][iy][ix];
        buf3_6[n++] = v2_brick_a0[iz][iy][ix];
        buf3_6[n++] = v2_brick_a1[iz][iy][ix];
        buf3_6[n++] = v2_brick_a2[iz][iy][ix];
        buf3_6[n++] = v2_brick_a3[iz][iy][ix];
        buf3_6[n++] = v2_brick_a4[iz][iy][ix];
        buf3_6[n++] = v2_brick_a5[iz][iy][ix];
        buf3_6[n++] = v2_brick_a6[iz][iy][ix];
        buf3_6[n++] = v3_brick_a0[iz][iy][ix];
        buf3_6[n++] = v3_brick_a1[iz][iy][ix];
        buf3_6[n++] = v3_brick_a2[iz][iy][ix];
        buf3_6[n++] = v3_brick_a3[iz][iy][ix];
        buf3_6[n++] = v3_brick_a4[iz][iy][ix];
        buf3_6[n++] = v3_brick_a5[iz][iy][ix];
        buf3_6[n++] = v3_brick_a6[iz][iy][ix];
        buf3_6[n++] = v4_brick_a0[iz][iy][ix];
        buf3_6[n++] = v4_brick_a1[iz][iy][ix];
        buf3_6[n++] = v4_brick_a2[iz][iy][ix];
        buf3_6[n++] = v4_brick_a3[iz][iy][ix];
        buf3_6[n++] = v4_brick_a4[iz][iy][ix];
        buf3_6[n++] = v4_brick_a5[iz][iy][ix];
        buf3_6[n++] = v4_brick_a6[iz][iy][ix];
        buf3_6[n++] = v5_brick_a0[iz][iy][ix];
        buf3_6[n++] = v5_brick_a1[iz][iy][ix];
        buf3_6[n++] = v5_brick_a2[iz][iy][ix];
        buf3_6[n++] = v5_brick_a3[iz][iy][ix];
        buf3_6[n++] = v5_brick_a4[iz][iy][ix];
        buf3_6[n++] = v5_brick_a5[iz][iy][ix];
        buf3_6[n++] = v5_brick_a6[iz][iy][ix];
      }
      
  if (comm->procneigh[2][1] == me)
    for (i = 0; i < n; i++) buf4_6[i] = buf3_6[i];
  else {
    MPI_Irecv(buf4_6,7*nbuf_pa_6,MPI_FFT_SCALAR,comm->procneigh[2][0],0,world,&request);
    MPI_Send(buf3_6,n,MPI_FFT_SCALAR,comm->procneigh[2][1],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzlo_out_6; iz < nzlo_in_6; iz++)
    for (iy = nylo_in_6; iy <= nyhi_in_6; iy++)
      for (ix = nxlo_in_6; ix <= nxhi_in_6; ix++) {
        v0_brick_a0[iz][iy][ix] = buf4_6[n++];
        v0_brick_a1[iz][iy][ix] = buf4_6[n++];
        v0_brick_a2[iz][iy][ix] = buf4_6[n++];
        v0_brick_a3[iz][iy][ix] = buf4_6[n++];
        v0_brick_a4[iz][iy][ix] = buf4_6[n++];
        v0_brick_a5[iz][iy][ix] = buf4_6[n++];
        v0_brick_a6[iz][iy][ix] = buf4_6[n++];
        v1_brick_a0[iz][iy][ix] = buf4_6[n++];
        v1_brick_a1[iz][iy][ix] = buf4_6[n++];
        v1_brick_a2[iz][iy][ix] = buf4_6[n++];
        v1_brick_a3[iz][iy][ix] = buf4_6[n++];
        v1_brick_a4[iz][iy][ix] = buf4_6[n++];
        v1_brick_a5[iz][iy][ix] = buf4_6[n++];
        v1_brick_a6[iz][iy][ix] = buf4_6[n++];
        v2_brick_a0[iz][iy][ix] = buf4_6[n++];
        v2_brick_a1[iz][iy][ix] = buf4_6[n++];
        v2_brick_a2[iz][iy][ix] = buf4_6[n++];
        v2_brick_a3[iz][iy][ix] = buf4_6[n++];
        v2_brick_a4[iz][iy][ix] = buf4_6[n++];
        v2_brick_a5[iz][iy][ix] = buf4_6[n++];
        v2_brick_a6[iz][iy][ix] = buf4_6[n++];
        v3_brick_a0[iz][iy][ix] = buf4_6[n++];
        v3_brick_a1[iz][iy][ix] = buf4_6[n++];
        v3_brick_a2[iz][iy][ix] = buf4_6[n++];
        v3_brick_a3[iz][iy][ix] = buf4_6[n++];
        v3_brick_a4[iz][iy][ix] = buf4_6[n++];
        v3_brick_a5[iz][iy][ix] = buf4_6[n++];
        v3_brick_a6[iz][iy][ix] = buf4_6[n++];
        v4_brick_a0[iz][iy][ix] = buf4_6[n++];
        v4_brick_a1[iz][iy][ix] = buf4_6[n++];
        v4_brick_a2[iz][iy][ix] = buf4_6[n++];
        v4_brick_a3[iz][iy][ix] = buf4_6[n++];
        v4_brick_a4[iz][iy][ix] = buf4_6[n++];
        v4_brick_a5[iz][iy][ix] = buf4_6[n++];
        v4_brick_a6[iz][iy][ix] = buf4_6[n++];
        v5_brick_a0[iz][iy][ix] = buf4_6[n++];
        v5_brick_a1[iz][iy][ix] = buf4_6[n++];
        v5_brick_a2[iz][iy][ix] = buf4_6[n++];
        v5_brick_a3[iz][iy][ix] = buf4_6[n++];
        v5_brick_a4[iz][iy][ix] = buf4_6[n++];
        v5_brick_a5[iz][iy][ix] = buf4_6[n++];
        v5_brick_a6[iz][iy][ix] = buf4_6[n++];
      }
      
  // pack my real cells for -z processor
  // pass data to self or -z processor
  // unpack and sum recv data into my ghost cells

  n = 0;
  for (iz = nzlo_in_6; iz < nzlo_in_6+nzlo_ghost_6; iz++)
    for (iy = nylo_in_6; iy <= nyhi_in_6; iy++)
      for (ix = nxlo_in_6; ix <= nxhi_in_6; ix++) {
        buf3_6[n++] = v0_brick_a0[iz][iy][ix];
        buf3_6[n++] = v0_brick_a1[iz][iy][ix];
        buf3_6[n++] = v0_brick_a2[iz][iy][ix];
        buf3_6[n++] = v0_brick_a3[iz][iy][ix];
        buf3_6[n++] = v0_brick_a4[iz][iy][ix];
        buf3_6[n++] = v0_brick_a5[iz][iy][ix];
        buf3_6[n++] = v0_brick_a6[iz][iy][ix];
        buf3_6[n++] = v1_brick_a0[iz][iy][ix];
        buf3_6[n++] = v1_brick_a1[iz][iy][ix];
        buf3_6[n++] = v1_brick_a2[iz][iy][ix];
        buf3_6[n++] = v1_brick_a3[iz][iy][ix];
        buf3_6[n++] = v1_brick_a4[iz][iy][ix];
        buf3_6[n++] = v1_brick_a5[iz][iy][ix];
        buf3_6[n++] = v1_brick_a6[iz][iy][ix];
        buf3_6[n++] = v2_brick_a0[iz][iy][ix];
        buf3_6[n++] = v2_brick_a1[iz][iy][ix];
        buf3_6[n++] = v2_brick_a2[iz][iy][ix];
        buf3_6[n++] = v2_brick_a3[iz][iy][ix];
        buf3_6[n++] = v2_brick_a4[iz][iy][ix];
        buf3_6[n++] = v2_brick_a5[iz][iy][ix];
        buf3_6[n++] = v2_brick_a6[iz][iy][ix];
        buf3_6[n++] = v3_brick_a0[iz][iy][ix];
        buf3_6[n++] = v3_brick_a1[iz][iy][ix];
        buf3_6[n++] = v3_brick_a2[iz][iy][ix];
        buf3_6[n++] = v3_brick_a3[iz][iy][ix];
        buf3_6[n++] = v3_brick_a4[iz][iy][ix];
        buf3_6[n++] = v3_brick_a5[iz][iy][ix];
        buf3_6[n++] = v3_brick_a6[iz][iy][ix];
        buf3_6[n++] = v4_brick_a0[iz][iy][ix];
        buf3_6[n++] = v4_brick_a1[iz][iy][ix];
        buf3_6[n++] = v4_brick_a2[iz][iy][ix];
        buf3_6[n++] = v4_brick_a3[iz][iy][ix];
        buf3_6[n++] = v4_brick_a4[iz][iy][ix];
        buf3_6[n++] = v4_brick_a5[iz][iy][ix];
        buf3_6[n++] = v4_brick_a6[iz][iy][ix];
        buf3_6[n++] = v5_brick_a0[iz][iy][ix];
        buf3_6[n++] = v5_brick_a1[iz][iy][ix];
        buf3_6[n++] = v5_brick_a2[iz][iy][ix];
        buf3_6[n++] = v5_brick_a3[iz][iy][ix];
        buf3_6[n++] = v5_brick_a4[iz][iy][ix];
        buf3_6[n++] = v5_brick_a5[iz][iy][ix];
        buf3_6[n++] = v5_brick_a6[iz][iy][ix];
      }
      
  if (comm->procneigh[2][0] == me)
    for (i = 0; i < n; i++) buf4_6[i] = buf3_6[i];
  else {
    MPI_Irecv(buf4_6,7*nbuf_pa_6,MPI_FFT_SCALAR,comm->procneigh[2][1],0,world,&request);
    MPI_Send(buf3_6,n,MPI_FFT_SCALAR,comm->procneigh[2][0],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzhi_in_6+1; iz <= nzhi_out_6; iz++)
    for (iy = nylo_in_6; iy <= nyhi_in_6; iy++)
      for (ix = nxlo_in_6; ix <= nxhi_in_6; ix++) {
        v0_brick_a0[iz][iy][ix] = buf4_6[n++];
        v0_brick_a1[iz][iy][ix] = buf4_6[n++];
        v0_brick_a2[iz][iy][ix] = buf4_6[n++];
        v0_brick_a3[iz][iy][ix] = buf4_6[n++];
        v0_brick_a4[iz][iy][ix] = buf4_6[n++];
        v0_brick_a5[iz][iy][ix] = buf4_6[n++];
        v0_brick_a6[iz][iy][ix] = buf4_6[n++];
        v1_brick_a0[iz][iy][ix] = buf4_6[n++];
        v1_brick_a1[iz][iy][ix] = buf4_6[n++];
        v1_brick_a2[iz][iy][ix] = buf4_6[n++];
        v1_brick_a3[iz][iy][ix] = buf4_6[n++];
        v1_brick_a4[iz][iy][ix] = buf4_6[n++];
        v1_brick_a5[iz][iy][ix] = buf4_6[n++];
        v1_brick_a6[iz][iy][ix] = buf4_6[n++];
        v2_brick_a0[iz][iy][ix] = buf4_6[n++];
        v2_brick_a1[iz][iy][ix] = buf4_6[n++];
        v2_brick_a2[iz][iy][ix] = buf4_6[n++];
        v2_brick_a3[iz][iy][ix] = buf4_6[n++];
        v2_brick_a4[iz][iy][ix] = buf4_6[n++];
        v2_brick_a5[iz][iy][ix] = buf4_6[n++];
        v2_brick_a6[iz][iy][ix] = buf4_6[n++];
        v3_brick_a0[iz][iy][ix] = buf4_6[n++];
        v3_brick_a1[iz][iy][ix] = buf4_6[n++];
        v3_brick_a2[iz][iy][ix] = buf4_6[n++];
        v3_brick_a3[iz][iy][ix] = buf4_6[n++];
        v3_brick_a4[iz][iy][ix] = buf4_6[n++];
        v3_brick_a5[iz][iy][ix] = buf4_6[n++];
        v3_brick_a6[iz][iy][ix] = buf4_6[n++];
        v4_brick_a0[iz][iy][ix] = buf4_6[n++];
        v4_brick_a1[iz][iy][ix] = buf4_6[n++];
        v4_brick_a2[iz][iy][ix] = buf4_6[n++];
        v4_brick_a3[iz][iy][ix] = buf4_6[n++];
        v4_brick_a4[iz][iy][ix] = buf4_6[n++];
        v4_brick_a5[iz][iy][ix] = buf4_6[n++];
        v4_brick_a6[iz][iy][ix] = buf4_6[n++];
        v5_brick_a0[iz][iy][ix] = buf4_6[n++];
        v5_brick_a1[iz][iy][ix] = buf4_6[n++];
        v5_brick_a2[iz][iy][ix] = buf4_6[n++];
        v5_brick_a3[iz][iy][ix] = buf4_6[n++];
        v5_brick_a4[iz][iy][ix] = buf4_6[n++];
        v5_brick_a5[iz][iy][ix] = buf4_6[n++];
        v5_brick_a6[iz][iy][ix] = buf4_6[n++];
      }
      
  // pack my real cells for +y processor
  // pass data to self or +y processor
  // unpack and sum recv data into my ghost cells

  n = 0;
  for (iz = nzlo_out_6; iz <= nzhi_out_6; iz++)
    for (iy = nyhi_in_6-nyhi_ghost_6+1; iy <= nyhi_in_6; iy++)
      for (ix = nxlo_in_6; ix <= nxhi_in_6; ix++) {
        buf3_6[n++] = v0_brick_a0[iz][iy][ix];
        buf3_6[n++] = v0_brick_a1[iz][iy][ix];
        buf3_6[n++] = v0_brick_a2[iz][iy][ix];
        buf3_6[n++] = v0_brick_a3[iz][iy][ix];
        buf3_6[n++] = v0_brick_a4[iz][iy][ix];
        buf3_6[n++] = v0_brick_a5[iz][iy][ix];
        buf3_6[n++] = v0_brick_a6[iz][iy][ix];
        buf3_6[n++] = v1_brick_a0[iz][iy][ix];
        buf3_6[n++] = v1_brick_a1[iz][iy][ix];
        buf3_6[n++] = v1_brick_a2[iz][iy][ix];
        buf3_6[n++] = v1_brick_a3[iz][iy][ix];
        buf3_6[n++] = v1_brick_a4[iz][iy][ix];
        buf3_6[n++] = v1_brick_a5[iz][iy][ix];
        buf3_6[n++] = v1_brick_a6[iz][iy][ix];
        buf3_6[n++] = v2_brick_a0[iz][iy][ix];
        buf3_6[n++] = v2_brick_a1[iz][iy][ix];
        buf3_6[n++] = v2_brick_a2[iz][iy][ix];
        buf3_6[n++] = v2_brick_a3[iz][iy][ix];
        buf3_6[n++] = v2_brick_a4[iz][iy][ix];
        buf3_6[n++] = v2_brick_a5[iz][iy][ix];
        buf3_6[n++] = v2_brick_a6[iz][iy][ix];
        buf3_6[n++] = v3_brick_a0[iz][iy][ix];
        buf3_6[n++] = v3_brick_a1[iz][iy][ix];
        buf3_6[n++] = v3_brick_a2[iz][iy][ix];
        buf3_6[n++] = v3_brick_a3[iz][iy][ix];
        buf3_6[n++] = v3_brick_a4[iz][iy][ix];
        buf3_6[n++] = v3_brick_a5[iz][iy][ix];
        buf3_6[n++] = v3_brick_a6[iz][iy][ix];
        buf3_6[n++] = v4_brick_a0[iz][iy][ix];
        buf3_6[n++] = v4_brick_a1[iz][iy][ix];
        buf3_6[n++] = v4_brick_a2[iz][iy][ix];
        buf3_6[n++] = v4_brick_a3[iz][iy][ix];
        buf3_6[n++] = v4_brick_a4[iz][iy][ix];
        buf3_6[n++] = v4_brick_a5[iz][iy][ix];
        buf3_6[n++] = v4_brick_a6[iz][iy][ix];
        buf3_6[n++] = v5_brick_a0[iz][iy][ix];
        buf3_6[n++] = v5_brick_a1[iz][iy][ix];
        buf3_6[n++] = v5_brick_a2[iz][iy][ix];
        buf3_6[n++] = v5_brick_a3[iz][iy][ix];
        buf3_6[n++] = v5_brick_a4[iz][iy][ix];
        buf3_6[n++] = v5_brick_a5[iz][iy][ix];
        buf3_6[n++] = v5_brick_a6[iz][iy][ix];
      }

  if (comm->procneigh[1][1] == me)
    for (i = 0; i < n; i++) buf4_6[i] = buf3_6[i];
  else {
    MPI_Irecv(buf4_6,7*nbuf_pa_6,MPI_FFT_SCALAR,comm->procneigh[1][0],0,world,&request);
    MPI_Send(buf3_6,n,MPI_FFT_SCALAR,comm->procneigh[1][1],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzlo_out_6; iz <= nzhi_out_6; iz++)
    for (iy = nylo_out_6; iy < nylo_in_6; iy++)
      for (ix = nxlo_in_6; ix <= nxhi_in_6; ix++) {
        v0_brick_a0[iz][iy][ix] = buf4_6[n++];
        v0_brick_a1[iz][iy][ix] = buf4_6[n++];
        v0_brick_a2[iz][iy][ix] = buf4_6[n++];
        v0_brick_a3[iz][iy][ix] = buf4_6[n++];
        v0_brick_a4[iz][iy][ix] = buf4_6[n++];
        v0_brick_a5[iz][iy][ix] = buf4_6[n++];
        v0_brick_a6[iz][iy][ix] = buf4_6[n++];
        v1_brick_a0[iz][iy][ix] = buf4_6[n++];
        v1_brick_a1[iz][iy][ix] = buf4_6[n++];
        v1_brick_a2[iz][iy][ix] = buf4_6[n++];
        v1_brick_a3[iz][iy][ix] = buf4_6[n++];
        v1_brick_a4[iz][iy][ix] = buf4_6[n++];
        v1_brick_a5[iz][iy][ix] = buf4_6[n++];
        v1_brick_a6[iz][iy][ix] = buf4_6[n++];
        v2_brick_a0[iz][iy][ix] = buf4_6[n++];
        v2_brick_a1[iz][iy][ix] = buf4_6[n++];
        v2_brick_a2[iz][iy][ix] = buf4_6[n++];
        v2_brick_a3[iz][iy][ix] = buf4_6[n++];
        v2_brick_a4[iz][iy][ix] = buf4_6[n++];
        v2_brick_a5[iz][iy][ix] = buf4_6[n++];
        v2_brick_a6[iz][iy][ix] = buf4_6[n++];
        v3_brick_a0[iz][iy][ix] = buf4_6[n++];
        v3_brick_a1[iz][iy][ix] = buf4_6[n++];
        v3_brick_a2[iz][iy][ix] = buf4_6[n++];
        v3_brick_a3[iz][iy][ix] = buf4_6[n++];
        v3_brick_a4[iz][iy][ix] = buf4_6[n++];
        v3_brick_a5[iz][iy][ix] = buf4_6[n++];
        v3_brick_a6[iz][iy][ix] = buf4_6[n++];
        v4_brick_a0[iz][iy][ix] = buf4_6[n++];
        v4_brick_a1[iz][iy][ix] = buf4_6[n++];
        v4_brick_a2[iz][iy][ix] = buf4_6[n++];
        v4_brick_a3[iz][iy][ix] = buf4_6[n++];
        v4_brick_a4[iz][iy][ix] = buf4_6[n++];
        v4_brick_a5[iz][iy][ix] = buf4_6[n++];
        v4_brick_a6[iz][iy][ix] = buf4_6[n++];
        v5_brick_a0[iz][iy][ix] = buf4_6[n++];
        v5_brick_a1[iz][iy][ix] = buf4_6[n++];
        v5_brick_a2[iz][iy][ix] = buf4_6[n++];
        v5_brick_a3[iz][iy][ix] = buf4_6[n++];
        v5_brick_a4[iz][iy][ix] = buf4_6[n++];
        v5_brick_a5[iz][iy][ix] = buf4_6[n++];
        v5_brick_a6[iz][iy][ix] = buf4_6[n++];
      }
      
  // pack my real cells for -y processor
  // pass data to self or -y processor
  // unpack and sum recv data into my ghost cells

  n = 0;
  for (iz = nzlo_out_6; iz <= nzhi_out_6; iz++)
    for (iy = nylo_in_6; iy < nylo_in_6+nylo_ghost_6; iy++)
      for (ix = nxlo_in_6; ix <= nxhi_in_6; ix++) {
        buf3_6[n++] = v0_brick_a0[iz][iy][ix];
        buf3_6[n++] = v0_brick_a1[iz][iy][ix];
        buf3_6[n++] = v0_brick_a2[iz][iy][ix];
        buf3_6[n++] = v0_brick_a3[iz][iy][ix];
        buf3_6[n++] = v0_brick_a4[iz][iy][ix];
        buf3_6[n++] = v0_brick_a5[iz][iy][ix];
        buf3_6[n++] = v0_brick_a6[iz][iy][ix];
        buf3_6[n++] = v1_brick_a0[iz][iy][ix];
        buf3_6[n++] = v1_brick_a1[iz][iy][ix];
        buf3_6[n++] = v1_brick_a2[iz][iy][ix];
        buf3_6[n++] = v1_brick_a3[iz][iy][ix];
        buf3_6[n++] = v1_brick_a4[iz][iy][ix];
        buf3_6[n++] = v1_brick_a5[iz][iy][ix];
        buf3_6[n++] = v1_brick_a6[iz][iy][ix];
        buf3_6[n++] = v2_brick_a0[iz][iy][ix];
        buf3_6[n++] = v2_brick_a1[iz][iy][ix];
        buf3_6[n++] = v2_brick_a2[iz][iy][ix];
        buf3_6[n++] = v2_brick_a3[iz][iy][ix];
        buf3_6[n++] = v2_brick_a4[iz][iy][ix];
        buf3_6[n++] = v2_brick_a5[iz][iy][ix];
        buf3_6[n++] = v2_brick_a6[iz][iy][ix];
        buf3_6[n++] = v3_brick_a0[iz][iy][ix];
        buf3_6[n++] = v3_brick_a1[iz][iy][ix];
        buf3_6[n++] = v3_brick_a2[iz][iy][ix];
        buf3_6[n++] = v3_brick_a3[iz][iy][ix];
        buf3_6[n++] = v3_brick_a4[iz][iy][ix];
        buf3_6[n++] = v3_brick_a5[iz][iy][ix];
        buf3_6[n++] = v3_brick_a6[iz][iy][ix];
        buf3_6[n++] = v4_brick_a0[iz][iy][ix];
        buf3_6[n++] = v4_brick_a1[iz][iy][ix];
        buf3_6[n++] = v4_brick_a2[iz][iy][ix];
        buf3_6[n++] = v4_brick_a3[iz][iy][ix];
        buf3_6[n++] = v4_brick_a4[iz][iy][ix];
        buf3_6[n++] = v4_brick_a5[iz][iy][ix];
        buf3_6[n++] = v4_brick_a6[iz][iy][ix];
        buf3_6[n++] = v5_brick_a0[iz][iy][ix];
        buf3_6[n++] = v5_brick_a1[iz][iy][ix];
        buf3_6[n++] = v5_brick_a2[iz][iy][ix];
        buf3_6[n++] = v5_brick_a3[iz][iy][ix];
        buf3_6[n++] = v5_brick_a4[iz][iy][ix];
        buf3_6[n++] = v5_brick_a5[iz][iy][ix];
        buf3_6[n++] = v5_brick_a6[iz][iy][ix];
      }
      
  if (comm->procneigh[1][0] == me)
    for (i = 0; i < n; i++) buf4_6[i] = buf3_6[i];
  else {
    MPI_Irecv(buf4_6,7*nbuf_pa_6,MPI_FFT_SCALAR,comm->procneigh[1][1],0,world,&request);
    MPI_Send(buf3_6,n,MPI_FFT_SCALAR,comm->procneigh[1][0],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzlo_out_6; iz <= nzhi_out_6; iz++)
    for (iy = nyhi_in_6+1; iy <= nyhi_out_6; iy++)
      for (ix = nxlo_in_6; ix <= nxhi_in_6; ix++) {
        v0_brick_a0[iz][iy][ix] = buf4_6[n++];
        v0_brick_a1[iz][iy][ix] = buf4_6[n++];
        v0_brick_a2[iz][iy][ix] = buf4_6[n++];
        v0_brick_a3[iz][iy][ix] = buf4_6[n++];
        v0_brick_a4[iz][iy][ix] = buf4_6[n++];
        v0_brick_a5[iz][iy][ix] = buf4_6[n++];
        v0_brick_a6[iz][iy][ix] = buf4_6[n++];
        v1_brick_a0[iz][iy][ix] = buf4_6[n++];
        v1_brick_a1[iz][iy][ix] = buf4_6[n++];
        v1_brick_a2[iz][iy][ix] = buf4_6[n++];
        v1_brick_a3[iz][iy][ix] = buf4_6[n++];
        v1_brick_a4[iz][iy][ix] = buf4_6[n++];
        v1_brick_a5[iz][iy][ix] = buf4_6[n++];
        v1_brick_a6[iz][iy][ix] = buf4_6[n++];
        v2_brick_a0[iz][iy][ix] = buf4_6[n++];
        v2_brick_a1[iz][iy][ix] = buf4_6[n++];
        v2_brick_a2[iz][iy][ix] = buf4_6[n++];
        v2_brick_a3[iz][iy][ix] = buf4_6[n++];
        v2_brick_a4[iz][iy][ix] = buf4_6[n++];
        v2_brick_a5[iz][iy][ix] = buf4_6[n++];
        v2_brick_a6[iz][iy][ix] = buf4_6[n++];
        v3_brick_a0[iz][iy][ix] = buf4_6[n++];
        v3_brick_a1[iz][iy][ix] = buf4_6[n++];
        v3_brick_a2[iz][iy][ix] = buf4_6[n++];
        v3_brick_a3[iz][iy][ix] = buf4_6[n++];
        v3_brick_a4[iz][iy][ix] = buf4_6[n++];
        v3_brick_a5[iz][iy][ix] = buf4_6[n++];
        v3_brick_a6[iz][iy][ix] = buf4_6[n++];
        v4_brick_a0[iz][iy][ix] = buf4_6[n++];
        v4_brick_a1[iz][iy][ix] = buf4_6[n++];
        v4_brick_a2[iz][iy][ix] = buf4_6[n++];
        v4_brick_a3[iz][iy][ix] = buf4_6[n++];
        v4_brick_a4[iz][iy][ix] = buf4_6[n++];
        v4_brick_a5[iz][iy][ix] = buf4_6[n++];
        v4_brick_a6[iz][iy][ix] = buf4_6[n++];
        v5_brick_a0[iz][iy][ix] = buf4_6[n++];
        v5_brick_a1[iz][iy][ix] = buf4_6[n++];
        v5_brick_a2[iz][iy][ix] = buf4_6[n++];
        v5_brick_a3[iz][iy][ix] = buf4_6[n++];
        v5_brick_a4[iz][iy][ix] = buf4_6[n++];
        v5_brick_a5[iz][iy][ix] = buf4_6[n++];
        v5_brick_a6[iz][iy][ix] = buf4_6[n++];
      }
      
  // pack my real cells for +x processor
  // pass data to self or +x processor
  // unpack and sum recv data into my ghost cells

  n = 0;
  for (iz = nzlo_out_6; iz <= nzhi_out_6; iz++)
    for (iy = nylo_out_6; iy <= nyhi_out_6; iy++)
      for (ix = nxhi_in_6-nxhi_ghost_6+1; ix <= nxhi_in_6; ix++) {
        buf3_6[n++] = v0_brick_a0[iz][iy][ix];
        buf3_6[n++] = v0_brick_a1[iz][iy][ix];
        buf3_6[n++] = v0_brick_a2[iz][iy][ix];
        buf3_6[n++] = v0_brick_a3[iz][iy][ix];
        buf3_6[n++] = v0_brick_a4[iz][iy][ix];
        buf3_6[n++] = v0_brick_a5[iz][iy][ix];
        buf3_6[n++] = v0_brick_a6[iz][iy][ix];
        buf3_6[n++] = v1_brick_a0[iz][iy][ix];
        buf3_6[n++] = v1_brick_a1[iz][iy][ix];
        buf3_6[n++] = v1_brick_a2[iz][iy][ix];
        buf3_6[n++] = v1_brick_a3[iz][iy][ix];
        buf3_6[n++] = v1_brick_a4[iz][iy][ix];
        buf3_6[n++] = v1_brick_a5[iz][iy][ix];
        buf3_6[n++] = v1_brick_a6[iz][iy][ix];
        buf3_6[n++] = v2_brick_a0[iz][iy][ix];
        buf3_6[n++] = v2_brick_a1[iz][iy][ix];
        buf3_6[n++] = v2_brick_a2[iz][iy][ix];
        buf3_6[n++] = v2_brick_a3[iz][iy][ix];
        buf3_6[n++] = v2_brick_a4[iz][iy][ix];
        buf3_6[n++] = v2_brick_a5[iz][iy][ix];
        buf3_6[n++] = v2_brick_a6[iz][iy][ix];
        buf3_6[n++] = v3_brick_a0[iz][iy][ix];
        buf3_6[n++] = v3_brick_a1[iz][iy][ix];
        buf3_6[n++] = v3_brick_a2[iz][iy][ix];
        buf3_6[n++] = v3_brick_a3[iz][iy][ix];
        buf3_6[n++] = v3_brick_a4[iz][iy][ix];
        buf3_6[n++] = v3_brick_a5[iz][iy][ix];
        buf3_6[n++] = v3_brick_a6[iz][iy][ix];
        buf3_6[n++] = v4_brick_a0[iz][iy][ix];
        buf3_6[n++] = v4_brick_a1[iz][iy][ix];
        buf3_6[n++] = v4_brick_a2[iz][iy][ix];
        buf3_6[n++] = v4_brick_a3[iz][iy][ix];
        buf3_6[n++] = v4_brick_a4[iz][iy][ix];
        buf3_6[n++] = v4_brick_a5[iz][iy][ix];
        buf3_6[n++] = v4_brick_a6[iz][iy][ix];
        buf3_6[n++] = v5_brick_a0[iz][iy][ix];
        buf3_6[n++] = v5_brick_a1[iz][iy][ix];
        buf3_6[n++] = v5_brick_a2[iz][iy][ix];
        buf3_6[n++] = v5_brick_a3[iz][iy][ix];
        buf3_6[n++] = v5_brick_a4[iz][iy][ix];
        buf3_6[n++] = v5_brick_a5[iz][iy][ix];
        buf3_6[n++] = v5_brick_a6[iz][iy][ix];
      }
      
  if (comm->procneigh[0][1] == me)
    for (i = 0; i < n; i++) buf4_6[i] = buf3_6[i];
  else {
    MPI_Irecv(buf4_6,7*nbuf_pa_6,MPI_FFT_SCALAR,comm->procneigh[0][0],0,world,&request);
    MPI_Send(buf3_6,n,MPI_FFT_SCALAR,comm->procneigh[0][1],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzlo_out_6; iz <= nzhi_out_6; iz++)
    for (iy = nylo_out_6; iy <= nyhi_out_6; iy++)
      for (ix = nxlo_out_6; ix < nxlo_in_6; ix++) {
        v0_brick_a0[iz][iy][ix] = buf4_6[n++];
        v0_brick_a1[iz][iy][ix] = buf4_6[n++];
        v0_brick_a2[iz][iy][ix] = buf4_6[n++];
        v0_brick_a3[iz][iy][ix] = buf4_6[n++];
        v0_brick_a4[iz][iy][ix] = buf4_6[n++];
        v0_brick_a5[iz][iy][ix] = buf4_6[n++];
        v0_brick_a6[iz][iy][ix] = buf4_6[n++];
        v1_brick_a0[iz][iy][ix] = buf4_6[n++];
        v1_brick_a1[iz][iy][ix] = buf4_6[n++];
        v1_brick_a2[iz][iy][ix] = buf4_6[n++];
        v1_brick_a3[iz][iy][ix] = buf4_6[n++];
        v1_brick_a4[iz][iy][ix] = buf4_6[n++];
        v1_brick_a5[iz][iy][ix] = buf4_6[n++];
        v1_brick_a6[iz][iy][ix] = buf4_6[n++];
        v2_brick_a0[iz][iy][ix] = buf4_6[n++];
        v2_brick_a1[iz][iy][ix] = buf4_6[n++];
        v2_brick_a2[iz][iy][ix] = buf4_6[n++];
        v2_brick_a3[iz][iy][ix] = buf4_6[n++];
        v2_brick_a4[iz][iy][ix] = buf4_6[n++];
        v2_brick_a5[iz][iy][ix] = buf4_6[n++];
        v2_brick_a6[iz][iy][ix] = buf4_6[n++];
        v3_brick_a0[iz][iy][ix] = buf4_6[n++];
        v3_brick_a1[iz][iy][ix] = buf4_6[n++];
        v3_brick_a2[iz][iy][ix] = buf4_6[n++];
        v3_brick_a3[iz][iy][ix] = buf4_6[n++];
        v3_brick_a4[iz][iy][ix] = buf4_6[n++];
        v3_brick_a5[iz][iy][ix] = buf4_6[n++];
        v3_brick_a6[iz][iy][ix] = buf4_6[n++];
        v4_brick_a0[iz][iy][ix] = buf4_6[n++];
        v4_brick_a1[iz][iy][ix] = buf4_6[n++];
        v4_brick_a2[iz][iy][ix] = buf4_6[n++];
        v4_brick_a3[iz][iy][ix] = buf4_6[n++];
        v4_brick_a4[iz][iy][ix] = buf4_6[n++];
        v4_brick_a5[iz][iy][ix] = buf4_6[n++];
        v4_brick_a6[iz][iy][ix] = buf4_6[n++];
        v5_brick_a0[iz][iy][ix] = buf4_6[n++];
        v5_brick_a1[iz][iy][ix] = buf4_6[n++];
        v5_brick_a2[iz][iy][ix] = buf4_6[n++];
        v5_brick_a3[iz][iy][ix] = buf4_6[n++];
        v5_brick_a4[iz][iy][ix] = buf4_6[n++];
        v5_brick_a5[iz][iy][ix] = buf4_6[n++];
        v5_brick_a6[iz][iy][ix] = buf4_6[n++];
      }
      
  // pack my real cells for -x processor
  // pass data to self or -x processor
  // unpack and sum recv data into my ghost cells

  n = 0;
  for (iz = nzlo_out_6; iz <= nzhi_out_6; iz++)
    for (iy = nylo_out_6; iy <= nyhi_out_6; iy++)
      for (ix = nxlo_in_6; ix < nxlo_in_6+nxlo_ghost_6; ix++) {
        buf3_6[n++] = v0_brick_a0[iz][iy][ix];
        buf3_6[n++] = v0_brick_a1[iz][iy][ix];
        buf3_6[n++] = v0_brick_a2[iz][iy][ix];
        buf3_6[n++] = v0_brick_a3[iz][iy][ix];
        buf3_6[n++] = v0_brick_a4[iz][iy][ix];
        buf3_6[n++] = v0_brick_a5[iz][iy][ix];
        buf3_6[n++] = v0_brick_a6[iz][iy][ix];
        buf3_6[n++] = v1_brick_a0[iz][iy][ix];
        buf3_6[n++] = v1_brick_a1[iz][iy][ix];
        buf3_6[n++] = v1_brick_a2[iz][iy][ix];
        buf3_6[n++] = v1_brick_a3[iz][iy][ix];
        buf3_6[n++] = v1_brick_a4[iz][iy][ix];
        buf3_6[n++] = v1_brick_a5[iz][iy][ix];
        buf3_6[n++] = v1_brick_a6[iz][iy][ix];
        buf3_6[n++] = v2_brick_a0[iz][iy][ix];
        buf3_6[n++] = v2_brick_a1[iz][iy][ix];
        buf3_6[n++] = v2_brick_a2[iz][iy][ix];
        buf3_6[n++] = v2_brick_a3[iz][iy][ix];
        buf3_6[n++] = v2_brick_a4[iz][iy][ix];
        buf3_6[n++] = v2_brick_a5[iz][iy][ix];
        buf3_6[n++] = v2_brick_a6[iz][iy][ix];
        buf3_6[n++] = v3_brick_a0[iz][iy][ix];
        buf3_6[n++] = v3_brick_a1[iz][iy][ix];
        buf3_6[n++] = v3_brick_a2[iz][iy][ix];
        buf3_6[n++] = v3_brick_a3[iz][iy][ix];
        buf3_6[n++] = v3_brick_a4[iz][iy][ix];
        buf3_6[n++] = v3_brick_a5[iz][iy][ix];
        buf3_6[n++] = v3_brick_a6[iz][iy][ix];
        buf3_6[n++] = v4_brick_a0[iz][iy][ix];
        buf3_6[n++] = v4_brick_a1[iz][iy][ix];
        buf3_6[n++] = v4_brick_a2[iz][iy][ix];
        buf3_6[n++] = v4_brick_a3[iz][iy][ix];
        buf3_6[n++] = v4_brick_a4[iz][iy][ix];
        buf3_6[n++] = v4_brick_a5[iz][iy][ix];
        buf3_6[n++] = v4_brick_a6[iz][iy][ix];
        buf3_6[n++] = v5_brick_a0[iz][iy][ix];
        buf3_6[n++] = v5_brick_a1[iz][iy][ix];
        buf3_6[n++] = v5_brick_a2[iz][iy][ix];
        buf3_6[n++] = v5_brick_a3[iz][iy][ix];
        buf3_6[n++] = v5_brick_a4[iz][iy][ix];
        buf3_6[n++] = v5_brick_a5[iz][iy][ix];
        buf3_6[n++] = v5_brick_a6[iz][iy][ix];
      }

  if (comm->procneigh[0][0] == me)
    for (i = 0; i < n; i++) buf4_6[i] = buf3_6[i];
  else {
    MPI_Irecv(buf4_6,7*nbuf_pa_6,MPI_FFT_SCALAR,comm->procneigh[0][1],0,world,&request);
    MPI_Send(buf3_6,n,MPI_FFT_SCALAR,comm->procneigh[0][0],0,world);
    MPI_Wait(&request,&status);
  }

  n = 0;
  for (iz = nzlo_out_6; iz <= nzhi_out_6; iz++)
    for (iy = nylo_out_6; iy <= nyhi_out_6; iy++)
      for (ix = nxhi_in_6+1; ix <= nxhi_out_6; ix++) {
        v0_brick_a0[iz][iy][ix] = buf4_6[n++];
        v0_brick_a1[iz][iy][ix] = buf4_6[n++];
        v0_brick_a2[iz][iy][ix] = buf4_6[n++];
        v0_brick_a3[iz][iy][ix] = buf4_6[n++];
        v0_brick_a4[iz][iy][ix] = buf4_6[n++];
        v0_brick_a5[iz][iy][ix] = buf4_6[n++];
        v0_brick_a6[iz][iy][ix] = buf4_6[n++];
        v1_brick_a0[iz][iy][ix] = buf4_6[n++];
        v1_brick_a1[iz][iy][ix] = buf4_6[n++];
        v1_brick_a2[iz][iy][ix] = buf4_6[n++];
        v1_brick_a3[iz][iy][ix] = buf4_6[n++];
        v1_brick_a4[iz][iy][ix] = buf4_6[n++];
        v1_brick_a5[iz][iy][ix] = buf4_6[n++];
        v1_brick_a6[iz][iy][ix] = buf4_6[n++];
        v2_brick_a0[iz][iy][ix] = buf4_6[n++];
        v2_brick_a1[iz][iy][ix] = buf4_6[n++];
        v2_brick_a2[iz][iy][ix] = buf4_6[n++];
        v2_brick_a3[iz][iy][ix] = buf4_6[n++];
        v2_brick_a4[iz][iy][ix] = buf4_6[n++];
        v2_brick_a5[iz][iy][ix] = buf4_6[n++];
        v2_brick_a6[iz][iy][ix] = buf4_6[n++];
        v3_brick_a0[iz][iy][ix] = buf4_6[n++];
        v3_brick_a1[iz][iy][ix] = buf4_6[n++];
        v3_brick_a2[iz][iy][ix] = buf4_6[n++];
        v3_brick_a3[iz][iy][ix] = buf4_6[n++];
        v3_brick_a4[iz][iy][ix] = buf4_6[n++];
        v3_brick_a5[iz][iy][ix] = buf4_6[n++];
        v3_brick_a6[iz][iy][ix] = buf4_6[n++];
        v4_brick_a0[iz][iy][ix] = buf4_6[n++];
        v4_brick_a1[iz][iy][ix] = buf4_6[n++];
        v4_brick_a2[iz][iy][ix] = buf4_6[n++];
        v4_brick_a3[iz][iy][ix] = buf4_6[n++];
        v4_brick_a4[iz][iy][ix] = buf4_6[n++];
        v4_brick_a5[iz][iy][ix] = buf4_6[n++];
        v4_brick_a6[iz][iy][ix] = buf4_6[n++];
        v5_brick_a0[iz][iy][ix] = buf4_6[n++];
        v5_brick_a1[iz][iy][ix] = buf4_6[n++];
        v5_brick_a2[iz][iy][ix] = buf4_6[n++];
        v5_brick_a3[iz][iy][ix] = buf4_6[n++];
        v5_brick_a4[iz][iy][ix] = buf4_6[n++];
        v5_brick_a5[iz][iy][ix] = buf4_6[n++];
        v5_brick_a6[iz][iy][ix] = buf4_6[n++];
      }
}

/* ----------------------------------------------------------------------
   find center grid pt for each of my particles
   check that full stencil for the particle will fit in my 3d brick
   store central grid pt indices in part2grid array 
------------------------------------------------------------------------- */

void PPPMDisp::particle_map(double delx, double dely, double delz,
                             double sft, int** p2g, int nup, int nlow,
                             int nxlo, int nylo, int nzlo,
                             int nxhi, int nyhi, int nzhi)
{
  int nx,ny,nz;

  double **x = atom->x;
  int nlocal = atom->nlocal;

  int flag = 0;
  for (int i = 0; i < nlocal; i++) {
    
    // (nx,ny,nz) = global coords of grid pt to "lower left" of charge
    // current particle coord can be outside global and local box
    // add/subtract OFFSET to avoid int(-0.75) = 0 when want it to be -1

    nx = static_cast<int> ((x[i][0]-boxlo[0])*delx+sft) - OFFSET;
    ny = static_cast<int> ((x[i][1]-boxlo[1])*dely+sft) - OFFSET;
    nz = static_cast<int> ((x[i][2]-boxlo[2])*delz+sft) - OFFSET;

    p2g[i][0] = nx;
    p2g[i][1] = ny;
    p2g[i][2] = nz;

    // check that entire stencil around nx,ny,nz will fit in my 3d brick

    if (nx+nlow < nxlo || nx+nup > nxhi ||
	ny+nlow < nylo || ny+nup > nyhi ||
	nz+nlow < nzlo || nz+nup > nzhi)
      flag = 1;
  }

  if (flag) error->one(FLERR,"Out of range atoms - cannot compute PPPMDisp");
}


void PPPMDisp::particle_map_c(double delx, double dely, double delz,
                               double sft, int** p2g, int nup, int nlow,
                               int nxlo, int nylo, int nzlo,
                               int nxhi, int nyhi, int nzhi)
{
  particle_map(delx, dely, delz, sft, p2g, nup, nlow,
               nxlo, nylo, nzlo, nxhi, nyhi, nzhi);
}

/* ----------------------------------------------------------------------
   create discretized "density" on section of global grid due to my particles
   density(x,y,z) = charge "density" at grid points of my 3d brick
   (nxlo:nxhi,nylo:nyhi,nzlo:nzhi) is extent of my brick (including ghosts)
   in global grid 
------------------------------------------------------------------------- */

void PPPMDisp::make_rho_c()
{
  int l,m,n,nx,ny,nz,mx,my,mz;
  FFT_SCALAR dx,dy,dz,x0,y0,z0;

  // clear 3d density array

  memset(&(density_brick[nzlo_out][nylo_out][nxlo_out]),0,
	 ngrid*sizeof(FFT_SCALAR));

  // loop over my charges, add their contribution to nearby grid points
  // (nx,ny,nz) = global coords of grid pt to "lower left" of charge
  // (dx,dy,dz) = distance to "lower left" grid pt
  // (mx,my,mz) = global coords of moving stencil pt

  double *q = atom->q;
  double **x = atom->x;
  int nlocal = atom->nlocal;

  for (int i = 0; i < nlocal; i++) {

    nx = part2grid[i][0];
    ny = part2grid[i][1];
    nz = part2grid[i][2];
    dx = nx+shiftone - (x[i][0]-boxlo[0])*delxinv;
    dy = ny+shiftone - (x[i][1]-boxlo[1])*delyinv;
    dz = nz+shiftone - (x[i][2]-boxlo[2])*delzinv;

    compute_rho1d(dx,dy,dz, order, rho_coeff, rho1d);

    z0 = delvolinv * q[i];
    for (n = nlower; n <= nupper; n++) {
      mz = n+nz;
      y0 = z0*rho1d[2][n];
      for (m = nlower; m <= nupper; m++) {
	my = m+ny;
	x0 = y0*rho1d[1][m];
	for (l = nlower; l <= nupper; l++) {
	  mx = l+nx;
	  density_brick[mz][my][mx] += x0*rho1d[0][l];
	}
      }
    }
  }
}

/* ----------------------------------------------------------------------
   create discretized "density" on section of global grid due to my particles
   density(x,y,z) = dispersion "density" at grid points of my 3d brick
   (nxlo:nxhi,nylo:nyhi,nzlo:nzhi) is extent of my brick (including ghosts)
   in global grid --- geometric mixing
------------------------------------------------------------------------- */

void PPPMDisp::make_rho_g()
{
  int l,m,n,nx,ny,nz,mx,my,mz;
  FFT_SCALAR dx,dy,dz,x0,y0,z0;

  // clear 3d density array

  memset(&(density_brick_g[nzlo_out_6][nylo_out_6][nxlo_out_6]),0,
	 ngrid_6*sizeof(FFT_SCALAR));

  // loop over my charges, add their contribution to nearby grid points
  // (nx,ny,nz) = global coords of grid pt to "lower left" of charge
  // (dx,dy,dz) = distance to "lower left" grid pt
  // (mx,my,mz) = global coords of moving stencil pt
  int type;
  double **x = atom->x;
  int nlocal = atom->nlocal;

  for (int i = 0; i < nlocal; i++) {

    nx = part2grid_6[i][0];
    ny = part2grid_6[i][1];
    nz = part2grid_6[i][2];
    dx = nx+shiftone_6 - (x[i][0]-boxlo[0])*delxinv_6;
    dy = ny+shiftone_6 - (x[i][1]-boxlo[1])*delyinv_6;
    dz = nz+shiftone_6 - (x[i][2]-boxlo[2])*delzinv_6;

    compute_rho1d(dx,dy,dz, order_6, rho_coeff_6, rho1d_6);
    type = atom->type[i];
    z0 = delvolinv_6 * B[type];
    for (n = nlower_6; n <= nupper_6; n++) {
      mz = n+nz;
      y0 = z0*rho1d_6[2][n];
      for (m = nlower_6; m <= nupper_6; m++) {
	my = m+ny;
	x0 = y0*rho1d_6[1][m];
	for (l = nlower_6; l <= nupper_6; l++) {
	  mx = l+nx;
	  density_brick_g[mz][my][mx] += x0*rho1d_6[0][l];
	}
      }
    }
  }
}


/* ----------------------------------------------------------------------
   create discretized "density" on section of global grid due to my particles
   density(x,y,z) = dispersion "density" at grid points of my 3d brick
   (nxlo:nxhi,nylo:nyhi,nzlo:nzhi) is extent of my brick (including ghosts)
   in global grid --- arithmetic mixing
------------------------------------------------------------------------- */

void PPPMDisp::make_rho_a()
{
  int l,m,n,nx,ny,nz,mx,my,mz;
  FFT_SCALAR dx,dy,dz,x0,y0,z0,w;

  // clear 3d density array

  memset(&(density_brick_a0[nzlo_out_6][nylo_out_6][nxlo_out_6]),0,
	 ngrid_6*sizeof(FFT_SCALAR));
  memset(&(density_brick_a1[nzlo_out_6][nylo_out_6][nxlo_out_6]),0,
	 ngrid_6*sizeof(FFT_SCALAR));
  memset(&(density_brick_a2[nzlo_out_6][nylo_out_6][nxlo_out_6]),0,
	 ngrid_6*sizeof(FFT_SCALAR));
  memset(&(density_brick_a3[nzlo_out_6][nylo_out_6][nxlo_out_6]),0,
	 ngrid_6*sizeof(FFT_SCALAR));
  memset(&(density_brick_a4[nzlo_out_6][nylo_out_6][nxlo_out_6]),0,
	 ngrid_6*sizeof(FFT_SCALAR));
  memset(&(density_brick_a5[nzlo_out_6][nylo_out_6][nxlo_out_6]),0,
	 ngrid_6*sizeof(FFT_SCALAR));
  memset(&(density_brick_a6[nzlo_out_6][nylo_out_6][nxlo_out_6]),0,
	 ngrid_6*sizeof(FFT_SCALAR));

  // loop over my particles, add their contribution to nearby grid points
  // (nx,ny,nz) = global coords of grid pt to "lower left" of charge
  // (dx,dy,dz) = distance to "lower left" grid pt
  // (mx,my,mz) = global coords of moving stencil pt
  int type;
  double **x = atom->x;
  int nlocal = atom->nlocal;
  
  for (int i = 0; i < nlocal; i++) {

    //do the following for all 4 grids
    nx = part2grid_6[i][0];
    ny = part2grid_6[i][1];
    nz = part2grid_6[i][2];
    dx = nx+shiftone_6 - (x[i][0]-boxlo[0])*delxinv_6;
    dy = ny+shiftone_6 - (x[i][1]-boxlo[1])*delyinv_6;
    dz = nz+shiftone_6 - (x[i][2]-boxlo[2])*delzinv_6;
    compute_rho1d(dx,dy,dz, order_6, rho_coeff_6, rho1d_6);
    type = atom->type[i];
    z0 = delvolinv_6;
    for (n = nlower_6; n <= nupper_6; n++) {
      mz = n+nz;
      y0 = z0*rho1d_6[2][n];
      for (m = nlower_6; m <= nupper_6; m++) {
	my = m+ny;
	x0 = y0*rho1d_6[1][m];
	for (l = nlower_6; l <= nupper_6; l++) {
	  mx = l+nx;
          w = x0*rho1d_6[0][l];
	  density_brick_a0[mz][my][mx] += w*B[7*type];
	  density_brick_a1[mz][my][mx] += w*B[7*type+1];
	  density_brick_a2[mz][my][mx] += w*B[7*type+2];
	  density_brick_a3[mz][my][mx] += w*B[7*type+3];
	  density_brick_a4[mz][my][mx] += w*B[7*type+4];
	  density_brick_a5[mz][my][mx] += w*B[7*type+5];
	  density_brick_a6[mz][my][mx] += w*B[7*type+6];
	}
      }
    }
  }
}


/* ----------------------------------------------------------------------
   FFT-based Poisson solver for ik differentiation
------------------------------------------------------------------------- */

void PPPMDisp::poisson_ik(FFT_SCALAR* wk1, FFT_SCALAR* wk2,
                           FFT_SCALAR* dfft, LAMMPS_NS::FFT3d* ft1,LAMMPS_NS::FFT3d* ft2, 
                           int nx_p, int ny_p, int nz_p, int nft,
                           int nxlo_ft, int nylo_ft, int nzlo_ft,
                           int nxhi_ft, int nyhi_ft, int nzhi_ft,
                           int nxlo_i, int nylo_i, int nzlo_i,
                           int nxhi_i, int nyhi_i, int nzhi_i,
                           double& egy, double* gfn,
                           double* kx, double* ky, double* kz,
                           double* kx2, double* ky2, double* kz2,
                           FFT_SCALAR*** vx_brick, FFT_SCALAR*** vy_brick, FFT_SCALAR*** vz_brick,
                           double* vir, double** vcoeff, double** vcoeff2,
                           FFT_SCALAR*** u_pa, FFT_SCALAR*** v0_pa, FFT_SCALAR*** v1_pa, FFT_SCALAR*** v2_pa,
                           FFT_SCALAR*** v3_pa, FFT_SCALAR*** v4_pa, FFT_SCALAR*** v5_pa)


{
  int i,j,k,n;
  double eng;

  // transform charge/dispersion density (r -> k) 
  n = 0;
  for (i = 0; i < nft; i++) {
    wk1[n++] = dfft[i];
    wk1[n++] = ZEROF;
  }

  ft1->compute(wk1,wk1,1);

  // if requested, compute energy and virial contribution

  double scaleinv = 1.0/(nx_p*ny_p*nz_p);
  double s2 = scaleinv*scaleinv;

  if (eflag_global || vflag_global) {
    if (vflag_global) {
      n = 0;
      for (i = 0; i < nft; i++) {
	eng = s2 * gfn[i] * (wk1[n]*wk1[n] + wk1[n+1]*wk1[n+1]);
	for (j = 0; j < 6; j++) vir[j] += eng*vcoeff[i][j];
	if (eflag_global) egy += eng;
	n += 2;
      }
    } else {
      n = 0;
      for (i = 0; i < nft; i++) {
	egy += 
	  s2 * gfn[i] * (wk1[n]*wk1[n] + wk1[n+1]*wk1[n+1]);
	n += 2;
      }
    }
  }

  // scale by 1/total-grid-pts to get rho(k)
  // multiply by Green's function to get V(k)

  n = 0;
  for (i = 0; i < nft; i++) {
    wk1[n++] *= scaleinv * gfn[i];
    wk1[n++] *= scaleinv * gfn[i];
  }

  // compute gradients of V(r) in each of 3 dims by transformimg -ik*V(k)
  // FFT leaves data in 3d brick decomposition
  // copy it into inner portion of vdx,vdy,vdz arrays

  // x & y direction gradient

  n = 0;
  for (k = nzlo_ft; k <= nzhi_ft; k++)
    for (j = nylo_ft; j <= nyhi_ft; j++)
      for (i = nxlo_ft; i <= nxhi_ft; i++) {
	wk2[n] = 0.5*(kx[i]-kx2[i])*wk1[n+1] + 0.5*(ky[j]-ky2[j])*wk1[n];
	wk2[n+1] = -0.5*(kx[i]-kx2[i])*wk1[n] + 0.5*(ky[j]-ky2[j])*wk1[n+1];
	n += 2;
      }

  ft2->compute(wk2,wk2,-1);

  n = 0;
  for (k = nzlo_i; k <= nzhi_i; k++)
    for (j = nylo_i; j <= nyhi_i; j++)
      for (i = nxlo_i; i <= nxhi_i; i++) {
	vx_brick[k][j][i] = wk2[n++];
	vy_brick[k][j][i] = wk2[n++];
      }

  if (!eflag_atom) {
    // z direction gradient only

    n = 0;
    for (k = nzlo_ft; k <= nzhi_ft; k++)
      for (j = nylo_ft; j <= nyhi_ft; j++)
        for (i = nxlo_ft; i <= nxhi_ft; i++) {
	  wk2[n] = kz[k]*wk1[n+1];
	  wk2[n+1] = -kz[k]*wk1[n];
	  n += 2;
        }

    ft2->compute(wk2,wk2,-1);


    n = 0;
    for (k = nzlo_i; k <= nzhi_i; k++)
      for (j = nylo_i; j <= nyhi_i; j++)
        for (i = nxlo_i; i <= nxhi_i; i++) {
	  vz_brick[k][j][i] = wk2[n];
	  n += 2;
        }

  }
  else {
    // z direction gradient & per-atom energy

    n = 0;
    for (k = nzlo_ft; k <= nzhi_ft; k++)
      for (j = nylo_ft; j <= nyhi_ft; j++)
        for (i = nxlo_ft; i <= nxhi_ft; i++) {
	  wk2[n] = 0.5*(kz[k]-kz2[k])*wk1[n+1] - wk1[n+1];
	  wk2[n+1] = -0.5*(kz[k]-kz2[k])*wk1[n] + wk1[n];
	  n += 2;
        }

    ft2->compute(wk2,wk2,-1);

    n = 0;
    for (k = nzlo_i; k <= nzhi_i; k++)
      for (j = nylo_i; j <= nyhi_i; j++)
        for (i = nxlo_i; i <= nxhi_i; i++) {
	  vz_brick[k][j][i] = wk2[n++];
	  u_pa[k][j][i] = wk2[n++];;
        }
  }

   
  if (vflag_atom) poisson_peratom(wk1, wk2, ft2, vcoeff, vcoeff2, nft,
                                  nxlo_i, nylo_i, nzlo_i, nxhi_i, nyhi_i, nzhi_i,
                                  v0_pa, v1_pa, v2_pa, v3_pa, v4_pa, v5_pa);
}

/* ----------------------------------------------------------------------
   FFT-based Poisson solver for ad differentiation
------------------------------------------------------------------------- */

void PPPMDisp::poisson_ad(FFT_SCALAR* wk1, FFT_SCALAR* wk2,
                           FFT_SCALAR* dfft, LAMMPS_NS::FFT3d* ft1,LAMMPS_NS::FFT3d* ft2, 
                           int nx_p, int ny_p, int nz_p, int nft,
                           int nxlo_ft, int nylo_ft, int nzlo_ft,
                           int nxhi_ft, int nyhi_ft, int nzhi_ft,
                           int nxlo_i, int nylo_i, int nzlo_i,
                           int nxhi_i, int nyhi_i, int nzhi_i,
                           double& egy, double* gfn,
                           double* vir, double** vcoeff, double** vcoeff2,
                           FFT_SCALAR*** u_pa, FFT_SCALAR*** v0_pa, FFT_SCALAR*** v1_pa, FFT_SCALAR*** v2_pa,
                           FFT_SCALAR*** v3_pa, FFT_SCALAR*** v4_pa, FFT_SCALAR*** v5_pa)


{
  int i,j,k,n;
  double eng;

  // transform charge/dispersion density (r -> k) 
  n = 0;
  for (i = 0; i < nft; i++) {
    wk1[n++] = dfft[i];
    wk1[n++] = ZEROF;
  }

  ft1->compute(wk1,wk1,1);
 
  // if requested, compute energy and virial contribution

  double scaleinv = 1.0/(nx_p*ny_p*nz_p);
  double s2 = scaleinv*scaleinv;

  if (eflag_global || vflag_global) {
    if (vflag_global) {
      n = 0;
      for (i = 0; i < nft; i++) {
	eng = s2 * gfn[i] * (wk1[n]*wk1[n] + wk1[n+1]*wk1[n+1]);
	for (j = 0; j < 6; j++) vir[j] += eng*vcoeff[i][j];
	if (eflag_global) egy += eng;
	n += 2;
      }
    } else {
      n = 0;
      for (i = 0; i < nft; i++) {
	egy += 
	  s2 * gfn[i] * (wk1[n]*wk1[n] + wk1[n+1]*wk1[n+1]);
	n += 2;
      }
    }
  }

  // scale by 1/total-grid-pts to get rho(k)
  // multiply by Green's function to get V(k)

  n = 0;
  for (i = 0; i < nft; i++) {
    wk1[n++] *= scaleinv * gfn[i];
    wk1[n++] *= scaleinv * gfn[i];
  }


  n = 0;
  for (k = nzlo_ft; k <= nzhi_ft; k++)
    for (j = nylo_ft; j <= nyhi_ft; j++)
      for (i = nxlo_ft; i <= nxhi_ft; i++) {
        wk2[n] = wk1[n];
	wk2[n+1] = wk1[n+1];
	n += 2;
      }

  ft2->compute(wk2,wk2,-1);

  n = 0;
  for (k = nzlo_i; k <= nzhi_i; k++)
    for (j = nylo_i; j <= nyhi_i; j++)
      for (i = nxlo_i; i <= nxhi_i; i++) {
	u_pa[k][j][i] = wk2[n++];
        n++;
      }

  if (vflag_atom) poisson_peratom(wk1, wk2, ft2, vcoeff, vcoeff2, nft,
                                  nxlo_i, nylo_i, nzlo_i, nxhi_i, nyhi_i, nzhi_i,
                                  v0_pa, v1_pa, v2_pa, v3_pa, v4_pa, v5_pa);
}

/* ----------------------------------------------------------------------
   Fourier Transform for per atom virial calculations
------------------------------------------------------------------------- */

void PPPMDisp:: poisson_peratom(FFT_SCALAR* wk1, FFT_SCALAR* wk2, LAMMPS_NS::FFT3d* ft2, 
                                 double** vcoeff, double** vcoeff2, int nft,
                                 int nxlo_i, int nylo_i, int nzlo_i,
                                 int nxhi_i, int nyhi_i, int nzhi_i,
                                 FFT_SCALAR*** v0_pa, FFT_SCALAR*** v1_pa, FFT_SCALAR*** v2_pa,
                                 FFT_SCALAR*** v3_pa, FFT_SCALAR*** v4_pa, FFT_SCALAR*** v5_pa)
{
 //v0 & v1 term
  int n, i, j, k;
  n = 0;
  for (i = 0; i < nft; i++) {
    wk2[n] = wk1[n]*vcoeff[i][0] - wk1[n+1]*vcoeff[i][1];
    wk2[n+1] = wk1[n+1]*vcoeff[i][0] +  wk1[n]*vcoeff[i][1];
    n += 2;
  }
    
  ft2->compute(wk2,wk2,-1); 
    
  n = 0;
  for (k = nzlo_i; k <= nzhi_i; k++)
    for (j = nylo_i; j <= nyhi_i; j++)
      for (i = nxlo_i; i <= nxhi_i; i++) {
        v0_pa[k][j][i] = wk2[n++];
        v1_pa[k][j][i] = wk2[n++];
      }

  //v2 & v3 term
   
  n = 0;
  for (i = 0; i < nft; i++) {
    wk2[n] = wk1[n]*vcoeff[i][2] - wk1[n+1]*vcoeff2[i][0];
    wk2[n+1] = wk1[n+1]*vcoeff[i][2] + wk1[n]*vcoeff2[i][0];
    n += 2;
  }
    
  ft2->compute(wk2,wk2,-1); 
    
  n = 0;
  for (k = nzlo_i; k <= nzhi_i; k++)
    for (j = nylo_i; j <= nyhi_i; j++)
      for (i = nxlo_i; i <= nxhi_i; i++) {
        v2_pa[k][j][i] = wk2[n++];
        v3_pa[k][j][i] = wk2[n++];
      }

  //v4 & v5 term
   
  n = 0;
  for (i = 0; i < nft; i++) {
    wk2[n] = wk1[n]*vcoeff2[i][1] - wk1[n+1]*vcoeff2[i][2];
    wk2[n+1] = wk1[n+1]*vcoeff2[i][1] + wk1[n]*vcoeff2[i][2];
    n += 2;
  }
    
  ft2->compute(wk2,wk2,-1); 

  n = 0;
  for (k = nzlo_i; k <= nzhi_i; k++)
    for (j = nylo_i; j <= nyhi_i; j++)
      for (i = nxlo_i; i <= nxhi_i; i++) {
        v4_pa[k][j][i] = wk2[n++];
        v5_pa[k][j][i] = wk2[n++];
      }	 
 
}

/* ----------------------------------------------------------------------
   Poisson solver for one mesh with 2 different dispersion densities 
   for ik scheme
------------------------------------------------------------------------- */

void PPPMDisp::poisson_2s_ik(FFT_SCALAR* dfft_1, FFT_SCALAR* dfft_2,
                              FFT_SCALAR*** vxbrick_1, FFT_SCALAR*** vybrick_1, FFT_SCALAR*** vzbrick_1,
                              FFT_SCALAR*** vxbrick_2, FFT_SCALAR*** vybrick_2, FFT_SCALAR*** vzbrick_2,
                              FFT_SCALAR*** u_pa_1, FFT_SCALAR*** v0_pa_1, FFT_SCALAR*** v1_pa_1, FFT_SCALAR*** v2_pa_1,
                              FFT_SCALAR*** v3_pa_1, FFT_SCALAR*** v4_pa_1, FFT_SCALAR*** v5_pa_1,
                              FFT_SCALAR*** u_pa_2, FFT_SCALAR*** v0_pa_2, FFT_SCALAR*** v1_pa_2, FFT_SCALAR*** v2_pa_2,
                              FFT_SCALAR*** v3_pa_2, FFT_SCALAR*** v4_pa_2, FFT_SCALAR*** v5_pa_2)

{
  int i,j,k,n;
  double eng;

  // transform charge/dispersion density (r -> k) 

  n = 0;
  for (i = 0; i < nfft_6; i++) {
    work1_6[n++] = dfft_1[i];
    work1_6[n++] = dfft_2[i];
  }
  
  fft1_6->compute(work1_6,work1_6,1);

  // if requested, compute energy and virial contribution
  double scaleinv = 1.0/(nx_pppm_6*ny_pppm_6*nz_pppm_6);
  double s2 = scaleinv*scaleinv;
  if (eflag_global || vflag_global) {
    //split the work1_6 vector into its even an odd parts!
    split_fourier();
    if (vflag_global) {
      n = 0;
      for (i = 0; i < nfft_6; i++) {
	eng = 2 * s2 * greensfn_6[i] * (split_1[n]*split_2[n+1] + split_1[n+1]*split_2[n]);
	for (j = 0; j < 6; j++) virial_6[j] += eng*vg_6[i][j];
	if (eflag_global)energy_6 += eng;
	n += 2;
      }
    } else {
      n = 0;
      for (i = 0; i < nfft_6; i++) {
	energy_6 += 
	  2 * s2 * greensfn_6[i] * (split_1[n]*split_2[n+1] + split_1[n+1]*split_2[n]);
	n += 2;
      }
    }
  }


  n = 0;
  for (i = 0; i < nfft_6; i++) {
    work1_6[n++] *= scaleinv * greensfn_6[i];
    work1_6[n++] *= scaleinv * greensfn_6[i];
  }

  // compute gradients of V(r) in each of 3 dims by transformimg -ik*V(k)
  // FFT leaves data in 3d brick decomposition
  // copy it into inner portion of vdx,vdy,vdz arrays

  // x direction gradient

  n = 0;
  for (k = nzlo_fft_6; k <= nzhi_fft_6; k++)
    for (j = nylo_fft_6; j <= nyhi_fft_6; j++)
      for (i = nxlo_fft_6; i <= nxhi_fft_6; i++) {
	work2_6[n] = 0.5*(fkx_6[i]-fkx2_6[i])*work1_6[n+1];
	work2_6[n+1] = -0.5*(fkx_6[i]-fkx2_6[i])*work1_6[n];
	n += 2;
      }

  fft2_6->compute(work2_6,work2_6,-1);
  
  n = 0;
  for (k = nzlo_in_6; k <= nzhi_in_6; k++)
    for (j = nylo_in_6; j <= nyhi_in_6; j++)
      for (i = nxlo_in_6; i <= nxhi_in_6; i++) {
	vxbrick_1[k][j][i] = work2_6[n++];
        vxbrick_2[k][j][i] = work2_6[n++];
      }

  // y direction gradient

  n = 0;
  for (k = nzlo_fft_6; k <= nzhi_fft_6; k++)
    for (j = nylo_fft_6; j <= nyhi_fft_6; j++)
      for (i = nxlo_fft_6; i <= nxhi_fft_6; i++) {
	work2_6[n] = 0.5*(fky_6[j]-fky2_6[j])*work1_6[n+1];
	work2_6[n+1] = -0.5*(fky_6[j]-fky2_6[j])*work1_6[n];
	n += 2;
      }

  fft2_6->compute(work2_6,work2_6,-1);

  n = 0;
  for (k = nzlo_in_6; k <= nzhi_in_6; k++)
    for (j = nylo_in_6; j <= nyhi_in_6; j++)
      for (i = nxlo_in_6; i <= nxhi_in_6; i++) {
	vybrick_1[k][j][i] = work2_6[n++];
        vybrick_2[k][j][i] = work2_6[n++];
      }

  // z direction gradient

  n = 0;
  for (k = nzlo_fft_6; k <= nzhi_fft_6; k++)
    for (j = nylo_fft_6; j <= nyhi_fft_6; j++)
      for (i = nxlo_fft_6; i <= nxhi_fft_6; i++) {
	work2_6[n] = 0.5*(fkz_6[k]-fkz2_6[k])*work1_6[n+1];
	work2_6[n+1] = -0.5*(fkz_6[k]-fkz2_6[k])*work1_6[n];
	n += 2;
      }

  fft2_6->compute(work2_6,work2_6,-1);

  n = 0;
  for (k = nzlo_in_6; k <= nzhi_in_6; k++)
    for (j = nylo_in_6; j <= nyhi_in_6; j++)
      for (i = nxlo_in_6; i <= nxhi_in_6; i++) {
	vzbrick_1[k][j][i] = work2_6[n++];
	vzbrick_2[k][j][i] = work2_6[n++];
      }

  //Per-atom energy
    
  if (eflag_atom) {
    n = 0;
    for (i = 0; i < nfft_6; i++) {
      work2_6[n] = work1_6[n];
      work2_6[n+1] = work1_6[n+1];
      n += 2;
    }
    
    fft2_6->compute(work2_6,work2_6,-1); 
    
    n = 0;
    for (k = nzlo_in_6; k <= nzhi_in_6; k++)
      for (j = nylo_in_6; j <= nyhi_in_6; j++)
        for (i = nxlo_in_6; i <= nxhi_in_6; i++) {
          u_pa_1[k][j][i] = work2_6[n++];
          u_pa_2[k][j][i] = work2_6[n++];
        }
  } 

  if (vflag_atom) poisson_2s_peratom(v0_pa_1, v1_pa_1, v2_pa_1, v3_pa_1, v4_pa_1, v5_pa_1,
                                     v0_pa_2, v1_pa_2, v2_pa_2, v3_pa_2, v4_pa_2, v5_pa_2);
}


/* ----------------------------------------------------------------------
   Poisson solver for one mesh with 2 different dispersion densities 
   for ik scheme
------------------------------------------------------------------------- */

void PPPMDisp::poisson_2s_ad(FFT_SCALAR* dfft_1, FFT_SCALAR* dfft_2,
                              FFT_SCALAR*** u_pa_1, FFT_SCALAR*** v0_pa_1, FFT_SCALAR*** v1_pa_1, FFT_SCALAR*** v2_pa_1,
                              FFT_SCALAR*** v3_pa_1, FFT_SCALAR*** v4_pa_1, FFT_SCALAR*** v5_pa_1,
                              FFT_SCALAR*** u_pa_2, FFT_SCALAR*** v0_pa_2, FFT_SCALAR*** v1_pa_2, FFT_SCALAR*** v2_pa_2,
                              FFT_SCALAR*** v3_pa_2, FFT_SCALAR*** v4_pa_2, FFT_SCALAR*** v5_pa_2)

{
  int i,j,k,n;
  double eng;

  // transform charge/dispersion density (r -> k) 

  n = 0;
  for (i = 0; i < nfft_6; i++) {
    work1_6[n++] = dfft_1[i];
    work1_6[n++] = dfft_2[i];
  }
  
  fft1_6->compute(work1_6,work1_6,1);

  // if requested, compute energy and virial contribution
  double scaleinv = 1.0/(nx_pppm_6*ny_pppm_6*nz_pppm_6);
  double s2 = scaleinv*scaleinv;
  if (eflag_global || vflag_global) {
    //split the work1_6 vector into its even an odd parts!
    split_fourier();
    if (vflag_global) {
      n = 0;
      for (i = 0; i < nfft_6; i++) {
	eng = 2 * s2 * greensfn_6[i] * (split_1[n]*split_2[n+1] + split_1[n+1]*split_2[n]);
	for (j = 0; j < 6; j++) virial_6[j] += eng*vg_6[i][j];
	if (eflag_global)energy_6 += eng;
	n += 2;
      }
    } else {
      n = 0;
      for (i = 0; i < nfft_6; i++) {
	energy_6 += 
	  2 * s2 * greensfn_6[i] * (split_1[n]*split_2[n+1] + split_1[n+1]*split_2[n]);
	n += 2;
      }
    }
  }


  n = 0;
  for (i = 0; i < nfft_6; i++) {
    work1_6[n++] *= scaleinv * greensfn_6[i];
    work1_6[n++] *= scaleinv * greensfn_6[i];
  }


  n = 0;
  for (i = 0; i < nfft_6; i++) {
    work2_6[n] = work1_6[n];
    work2_6[n+1] = work1_6[n+1];
    n += 2;
  }
    
  fft2_6->compute(work2_6,work2_6,-1); 
    
  n = 0;
  for (k = nzlo_in_6; k <= nzhi_in_6; k++)
    for (j = nylo_in_6; j <= nyhi_in_6; j++)
      for (i = nxlo_in_6; i <= nxhi_in_6; i++) {
        u_pa_1[k][j][i] = work2_6[n++];
        u_pa_2[k][j][i] = work2_6[n++];
      } 

  if (vflag_atom) poisson_2s_peratom(v0_pa_1, v1_pa_1, v2_pa_1, v3_pa_1, v4_pa_1, v5_pa_1,
                                     v0_pa_2, v1_pa_2, v2_pa_2, v3_pa_2, v4_pa_2, v5_pa_2);
}

/* ----------------------------------------------------------------------
   Fourier Transform for per atom virial calculations
------------------------------------------------------------------------- */

void PPPMDisp::poisson_2s_peratom(FFT_SCALAR*** v0_pa_1, FFT_SCALAR*** v1_pa_1, FFT_SCALAR*** v2_pa_1,
                                   FFT_SCALAR*** v3_pa_1, FFT_SCALAR*** v4_pa_1, FFT_SCALAR*** v5_pa_1,
                                   FFT_SCALAR*** v0_pa_2, FFT_SCALAR*** v1_pa_2, FFT_SCALAR*** v2_pa_2,
                                   FFT_SCALAR*** v3_pa_2, FFT_SCALAR*** v4_pa_2, FFT_SCALAR*** v5_pa_2)
{
  //Compute first virial term v0
  int n, i, j, k;

  n = 0;
  for (i = 0; i < nfft_6; i++) {
    work2_6[n] = work1_6[n]*vg_6[i][0];
    work2_6[n+1] = work1_6[n+1]*vg_6[i][0];
    n += 2;
  }
   
  fft2_6->compute(work2_6,work2_6,-1); 
    
  n = 0;
  for (k = nzlo_in_6; k <= nzhi_in_6; k++)
    for (j = nylo_in_6; j <= nyhi_in_6; j++)
      for (i = nxlo_in_6; i <= nxhi_in_6; i++) {
        v0_pa_1[k][j][i] = work2_6[n++];
        v0_pa_2[k][j][i] = work2_6[n++];
      }
	 
  //Compute second virial term v1  
  
  n = 0;
  for (i = 0; i < nfft_6; i++) {
    work2_6[n] = work1_6[n]*vg_6[i][1];
    work2_6[n+1] = work1_6[n+1]*vg_6[i][1];
    n += 2;
  }
    
  fft2_6->compute(work2_6,work2_6,-1); 
  
  n = 0;
  for (k = nzlo_in_6; k <= nzhi_in_6; k++)
    for (j = nylo_in_6; j <= nyhi_in_6; j++)
      for (i = nxlo_in_6; i <= nxhi_in_6; i++) {
        v1_pa_1[k][j][i] = work2_6[n++];
        v1_pa_2[k][j][i] = work2_6[n++];
      }
	  
  //Compute third virial term v2
   
  n = 0;
  for (i = 0; i < nfft_6; i++) {
    work2_6[n] = work1_6[n]*vg_6[i][2];
    work2_6[n+1] = work1_6[n+1]*vg_6[i][2];
    n += 2;
  }
    
  fft2_6->compute(work2_6,work2_6,-1); 
    
  n = 0;
  for (k = nzlo_in_6; k <= nzhi_in_6; k++)
    for (j = nylo_in_6; j <= nyhi_in_6; j++)
      for (i = nxlo_in_6; i <= nxhi_in_6; i++) {
        v2_pa_1[k][j][i] = work2_6[n++];
        v2_pa_2[k][j][i] = work2_6[n++];
      }

  //Compute fourth virial term v3
   
  n = 0;
  for (i = 0; i < nfft_6; i++) {
    work2_6[n] = work1_6[n]*vg2_6[i][0];
    work2_6[n+1] = work1_6[n+1]*vg2_6[i][0];
    n += 2;
  }
    
  fft2_6->compute(work2_6,work2_6,-1); 
    
  n = 0;
  for (k = nzlo_in_6; k <= nzhi_in_6; k++)
    for (j = nylo_in_6; j <= nyhi_in_6; j++)
      for (i = nxlo_in_6; i <= nxhi_in_6; i++) {
        v3_pa_1[k][j][i] = work2_6[n++];
        v3_pa_2[k][j][i] = work2_6[n++];
      }

  //Compute fifth virial term v4
   
  n = 0;
  for (i = 0; i < nfft_6; i++) {
    work2_6[n] = work1_6[n]*vg2_6[i][1];
    work2_6[n+1] = work1_6[n+1]*vg2_6[i][1];
    n += 2;
  }
    
  fft2_6->compute(work2_6,work2_6,-1); 
    
  n = 0;
  for (k = nzlo_in_6; k <= nzhi_in_6; k++)
    for (j = nylo_in_6; j <= nyhi_in_6; j++)
      for (i = nxlo_in_6; i <= nxhi_in_6; i++) {
        v4_pa_1[k][j][i] = work2_6[n++];
        v4_pa_2[k][j][i] = work2_6[n++];
      }
   
  //Compute last virial term v5
   
  n = 0;
  for (i = 0; i < nfft_6; i++) {
    work2_6[n] = work1_6[n]*vg2_6[i][2];
    work2_6[n+1] = work1_6[n+1]*vg2_6[i][2];
    n += 2;
  }
    
  fft2_6->compute(work2_6,work2_6,-1); 
    
  n = 0;
  for (k = nzlo_in_6; k <= nzhi_in_6; k++)
    for (j = nylo_in_6; j <= nyhi_in_6; j++)
      for (i = nxlo_in_6; i <= nxhi_in_6; i++) {
        v5_pa_1[k][j][i] = work2_6[n++];
        v5_pa_2[k][j][i] = work2_6[n++];
      }
}

/* ----------------------------------------------------------------------
   determine the order of communication between the procs when
   splitting the fourier transform
   ------------------------------------------------------------------------- */

void PPPMDisp::split_order(int** com_matrix)
{
  // first element of com_order
  com_order[0] = me;
  //deleate diagonal elements of com_matrix
  int i,j;
  for (i = 0; i < nprocs; i++) com_matrix[i][i] = 0;

  int *busy;
  int *act_point = 0;
  int sum = 1;
  int curr_order = 1;
  memory->create(busy, nprocs, "pppm/disp:busy");
  memory->create(act_point, nprocs, "pppm/disp:actpoint");
  memset(act_point, 0, nprocs*sizeof(int));
  //repeate untill all entries in com_matrix are zero
  while (sum != 0) {
    memset(busy, 0, nprocs*sizeof(int));
    //loop over all procs
    for (i = 0; i < nprocs; i++) {
      //if current proc is not busy, search for a partner
      if (!busy[i]) {
        // move the current position of act_point;
        for (j = 0; j < 12; j++) {
          act_point[i]--;
          if (act_point[i] == -1) act_point[i] = nprocs-1;
          // if a partner is found:
          if (com_matrix[i][act_point[i]] && !busy[act_point[i]]) {
            busy[i] = busy[act_point[i]] = 1;
            com_matrix[i][act_point[i]] = com_matrix[act_point[i]][i] = 0;
            act_point[act_point[i]] = i;
	    break;
          }
        }
      }
    }
    if (busy[me]) com_order[curr_order++] = act_point[me];
    // calcualte the sum of all values of com_matrix
    sum = 0;
    for (i = 0; i <nprocs; i++)
      for (j = 0; j < nprocs; j++) sum += com_matrix[i][j];
  }

  memory->destroy(busy);
  memory->destroy(act_point);
}

/* ----------------------------------------------------------------------
   split the work vector into its real and imaginary parts
   ------------------------------------------------------------------------- */

void PPPMDisp::split_fourier()
{
  // add / substract half the value of work1 to split
  // fill work1 in splitbuf1 for communication
  int n,m,o;
  MPI_Request request;
  MPI_Status status;

  m = 0;
  for (n = 0; n < nfft_6; n++) {
    split_1[m] = 0.5*work1_6[m];
    split_2[m] = 0.5*work1_6[m];
    splitbuf1[dict_send[n][0]][dict_send[n][1]] = work1_6[m++];
    split_1[m] = -0.5*work1_6[m];
    split_2[m] = 0.5*work1_6[m];
    splitbuf1[dict_send[n][0]][dict_send[n][1]+1] = work1_6[m++];
  }
    
  // "exchange" points with yourself
  for (n = 0; n < com_each[0]; n++) splitbuf2[0][n] = splitbuf1[0][n];
  // exchange points with other procs
  for (n = 1; n < com_procs; n++) {
    MPI_Irecv(splitbuf2[n],com_each[n],MPI_FFT_SCALAR,com_order[n],0,world,&request);
    MPI_Send(splitbuf1[n],com_each[n],MPI_FFT_SCALAR,com_order[n],0,world);
    MPI_Wait(&request,&status);
  }

  // add received values to split_1 and split_2
  for (n = 0; n < com_procs; n++) {
    o = 0;
    for (m = 0; m < com_each[n]/2; m++) {
      split_1[dict_rec[n][m]] += 0.5*splitbuf2[n][o];
      split_2[dict_rec[n][m]] -= 0.5*splitbuf2[n][o++];
      split_1[dict_rec[n][m]+1] += 0.5*splitbuf2[n][o];
      split_2[dict_rec[n][m]+1] += 0.5*splitbuf2[n][o++];  
    }
  }
}
 
/* ----------------------------------------------------------------------
   interpolate from grid to get electric field & force on my particles 
   for ik scheme
------------------------------------------------------------------------- */

void PPPMDisp::fieldforce_c_ik()
{
  int i,l,m,n,nx,ny,nz,mx,my,mz;
  FFT_SCALAR dx,dy,dz,x0,y0,z0;
  FFT_SCALAR ekx,eky,ekz;

  // loop over my charges, interpolate electric field from nearby grid points
  // (nx,ny,nz) = global coords of grid pt to "lower left" of charge
  // (dx,dy,dz) = distance to "lower left" grid pt
  // (mx,my,mz) = global coords of moving stencil pt
  // ek = 3 components of E-field on particle

  double *q = atom->q;
  double **x = atom->x;
  double **f = atom->f;

  int nlocal = atom->nlocal;

  for (i = 0; i < nlocal; i++) {
    nx = part2grid[i][0];
    ny = part2grid[i][1];
    nz = part2grid[i][2];
    dx = nx+shiftone - (x[i][0]-boxlo[0])*delxinv;
    dy = ny+shiftone - (x[i][1]-boxlo[1])*delyinv;
    dz = nz+shiftone - (x[i][2]-boxlo[2])*delzinv;

    compute_rho1d(dx,dy,dz, order, rho_coeff, rho1d);

    ekx = eky = ekz = ZEROF;
    for (n = nlower; n <= nupper; n++) {
      mz = n+nz;
      z0 = rho1d[2][n];
      for (m = nlower; m <= nupper; m++) {
	my = m+ny;
	y0 = z0*rho1d[1][m];
	for (l = nlower; l <= nupper; l++) {
	  mx = l+nx;
	  x0 = y0*rho1d[0][l];
	  ekx -= x0*vdx_brick[mz][my][mx];
	  eky -= x0*vdy_brick[mz][my][mx];
	  ekz -= x0*vdz_brick[mz][my][mx];
	}
      }
    }

    // convert E-field to force

    const double qfactor = force->qqrd2e * scale * q[i];
    f[i][0] += qfactor*ekx;
    f[i][1] += qfactor*eky;
    f[i][2] += qfactor*ekz;
  }
}
/* ----------------------------------------------------------------------
   interpolate from grid to get electric field & force on my particles
   for ad scheme 
------------------------------------------------------------------------- */

void PPPMDisp::fieldforce_c_ad()
{
  int i,l,m,n,nx,ny,nz,mx,my,mz;
  FFT_SCALAR dx,dy,dz;
  FFT_SCALAR ekx,eky,ekz;
  double s1,s2,s3;
  double sf = 0.0;

  double *prd;

  if (triclinic == 0) prd = domain->prd;
  else prd = domain->prd_lamda;

  double xprd = prd[0];
  double yprd = prd[1];
  double zprd = prd[2];
  double zprd_slab = zprd*slab_volfactor;

  double hx_inv = nx_pppm/xprd;
  double hy_inv = ny_pppm/yprd;
  double hz_inv = nz_pppm/zprd_slab;

  // loop over my charges, interpolate electric field from nearby grid points
  // (nx,ny,nz) = global coords of grid pt to "lower left" of charge
  // (dx,dy,dz) = distance to "lower left" grid pt
  // (mx,my,mz) = global coords of moving stencil pt
  // ek = 3 components of E-field on particle

  double *q = atom->q;
  double **x = atom->x;
  double **f = atom->f;

  int nlocal = atom->nlocal;

  for (i = 0; i < nlocal; i++) {
    nx = part2grid[i][0];
    ny = part2grid[i][1];
    nz = part2grid[i][2];
    dx = nx+shiftone - (x[i][0]-boxlo[0])*delxinv;
    dy = ny+shiftone - (x[i][1]-boxlo[1])*delyinv;
    dz = nz+shiftone - (x[i][2]-boxlo[2])*delzinv;

    compute_rho1d(dx,dy,dz, order, rho_coeff, rho1d);
    compute_drho1d(dx,dy,dz, order, drho_coeff, drho1d);

    ekx = eky = ekz = ZEROF;
    for (n = nlower; n <= nupper; n++) {
      mz = n+nz;
      for (m = nlower; m <= nupper; m++) {
        my = m+ny;
        for (l = nlower; l <= nupper; l++) {
          mx = l+nx;
          ekx += drho1d[0][l]*rho1d[1][m]*rho1d[2][n]*u_brick[mz][my][mx];
          eky += rho1d[0][l]*drho1d[1][m]*rho1d[2][n]*u_brick[mz][my][mx];
          ekz += rho1d[0][l]*rho1d[1][m]*drho1d[2][n]*u_brick[mz][my][mx];
        }
      }
    }
    ekx *= hx_inv;
    eky *= hy_inv;
    ekz *= hz_inv;
    // convert E-field to force and substract self forces
    const double qfactor = force->qqrd2e * scale;

    s1 = x[i][0]*hx_inv;
    s2 = x[i][1]*hy_inv;
    s3 = x[i][2]*hz_inv;
    sf = sf_coeff[0]*sin(2*MY_PI*s1);
    sf += sf_coeff[1]*sin(4*MY_PI*s1);
    sf *= 2*q[i]*q[i];
    f[i][0] += qfactor*(ekx*q[i] - sf);

    sf = sf_coeff[2]*sin(2*MY_PI*s2);
    sf += sf_coeff[3]*sin(4*MY_PI*s2);
    sf *= 2*q[i]*q[i];
    f[i][1] += qfactor*(eky*q[i] - sf);


    sf = sf_coeff[4]*sin(2*MY_PI*s3);
    sf += sf_coeff[5]*sin(4*MY_PI*s3);
    sf *= 2*q[i]*q[i];
    if (slabflag != 2) f[i][2] += qfactor*(ekz*q[i] - sf);
  }
}

/* ----------------------------------------------------------------------
   interpolate from grid to get electric field & force on my particles 
------------------------------------------------------------------------- */

void PPPMDisp::fieldforce_c_peratom()
{
  int i,l,m,n,nx,ny,nz,mx,my,mz;
  FFT_SCALAR dx,dy,dz,x0,y0,z0;
  FFT_SCALAR u_pa,v0,v1,v2,v3,v4,v5;

  // loop over my charges, interpolate electric field from nearby grid points
  // (nx,ny,nz) = global coords of grid pt to "lower left" of charge
  // (dx,dy,dz) = distance to "lower left" grid pt
  // (mx,my,mz) = global coords of moving stencil pt
  // ek = 3 components of E-field on particle

  double *q = atom->q;
  double **x = atom->x;

  int nlocal = atom->nlocal;

  for (i = 0; i < nlocal; i++) {
    nx = part2grid[i][0];
    ny = part2grid[i][1];
    nz = part2grid[i][2];
    dx = nx+shiftone - (x[i][0]-boxlo[0])*delxinv;
    dy = ny+shiftone - (x[i][1]-boxlo[1])*delyinv;
    dz = nz+shiftone - (x[i][2]-boxlo[2])*delzinv;

    compute_rho1d(dx,dy,dz, order, rho_coeff, rho1d);

    u_pa = v0 = v1 = v2 = v3 = v4 = v5 = ZEROF;
    for (n = nlower; n <= nupper; n++) {
      mz = n+nz;
      z0 = rho1d[2][n];
      for (m = nlower; m <= nupper; m++) {
	my = m+ny;
	y0 = z0*rho1d[1][m];
	for (l = nlower; l <= nupper; l++) {
	  mx = l+nx;
	  x0 = y0*rho1d[0][l];
	  if (eflag_atom) u_pa += x0*u_brick[mz][my][mx];	
	  if (vflag_atom) {
            v0 += x0*v0_brick[mz][my][mx];
            v1 += x0*v1_brick[mz][my][mx];
            v2 += x0*v2_brick[mz][my][mx];
            v3 += x0*v3_brick[mz][my][mx];
            v4 += x0*v4_brick[mz][my][mx];
            v5 += x0*v5_brick[mz][my][mx];
          }
	}
      }
    }

    // convert E-field to force

    const double qfactor = 0.5*force->qqrd2e * scale * q[i];

    if (eflag_atom) eatom[i] += u_pa*qfactor;
    if (vflag_atom) {
      vatom[i][0] += v0*qfactor;
      vatom[i][1] += v1*qfactor;
      vatom[i][2] += v2*qfactor;
      vatom[i][3] += v3*qfactor;
      vatom[i][4] += v4*qfactor;
      vatom[i][5] += v5*qfactor;
    }
  }
}

/* ----------------------------------------------------------------------
   interpolate from grid to get dispersion field & force on my particles
   for geometric mixing rule 
------------------------------------------------------------------------- */

void PPPMDisp::fieldforce_g_ik()
{
  int i,l,m,n,nx,ny,nz,mx,my,mz;
  FFT_SCALAR dx,dy,dz,x0,y0,z0;
  FFT_SCALAR ekx,eky,ekz;

  // loop over my charges, interpolate electric field from nearby grid points
  // (nx,ny,nz) = global coords of grid pt to "lower left" of charge
  // (dx,dy,dz) = distance to "lower left" grid pt
  // (mx,my,mz) = global coords of moving stencil pt
  // ek = 3 components of dispersion field on particle

  double **x = atom->x;
  double **f = atom->f;
  int type;
  double lj;

  int nlocal = atom->nlocal;

  for (i = 0; i < nlocal; i++) {
    nx = part2grid_6[i][0];
    ny = part2grid_6[i][1];
    nz = part2grid_6[i][2];
    dx = nx+shiftone_6 - (x[i][0]-boxlo[0])*delxinv_6;
    dy = ny+shiftone_6 - (x[i][1]-boxlo[1])*delyinv_6;
    dz = nz+shiftone_6 - (x[i][2]-boxlo[2])*delzinv_6;

    compute_rho1d(dx,dy,dz, order_6, rho_coeff_6, rho1d_6);

    ekx = eky = ekz = ZEROF;
    for (n = nlower_6; n <= nupper_6; n++) {
      mz = n+nz;
      z0 = rho1d_6[2][n];
      for (m = nlower_6; m <= nupper_6; m++) {
	my = m+ny;
	y0 = z0*rho1d_6[1][m];
	for (l = nlower_6; l <= nupper_6; l++) {
	  mx = l+nx;
	  x0 = y0*rho1d_6[0][l];
	  ekx -= x0*vdx_brick_g[mz][my][mx];
	  eky -= x0*vdy_brick_g[mz][my][mx];
	  ekz -= x0*vdz_brick_g[mz][my][mx];
	}
      }
    }

    // convert E-field to force
    type = atom->type[i];
    lj = B[type];
    f[i][0] += lj*ekx;
    f[i][1] += lj*eky;
    f[i][2] += lj*ekz;
  }
}

/* ----------------------------------------------------------------------
   interpolate from grid to get dispersion field & force on my particles
   for geometric mixing rule for ad scheme
------------------------------------------------------------------------- */

void PPPMDisp::fieldforce_g_ad()
{
  int i,l,m,n,nx,ny,nz,mx,my,mz;
  FFT_SCALAR dx,dy,dz;
  FFT_SCALAR ekx,eky,ekz;
  double s1,s2,s3;
  double sf = 0.0;
  double *prd;

  if (triclinic == 0) prd = domain->prd;
  else prd = domain->prd_lamda;

  double xprd = prd[0];
  double yprd = prd[1];
  double zprd = prd[2];
  double zprd_slab = zprd*slab_volfactor;

  double hx_inv = nx_pppm_6/xprd;
  double hy_inv = ny_pppm_6/yprd;
  double hz_inv = nz_pppm_6/zprd_slab;

  // loop over my charges, interpolate electric field from nearby grid points
  // (nx,ny,nz) = global coords of grid pt to "lower left" of charge
  // (dx,dy,dz) = distance to "lower left" grid pt
  // (mx,my,mz) = global coords of moving stencil pt
  // ek = 3 components of dispersion field on particle

  double **x = atom->x;
  double **f = atom->f;
  int type;
  double lj;

  int nlocal = atom->nlocal;

 
  for (i = 0; i < nlocal; i++) {
    nx = part2grid_6[i][0];
    ny = part2grid_6[i][1];
    nz = part2grid_6[i][2];
    dx = nx+shiftone_6 - (x[i][0]-boxlo[0])*delxinv_6;
    dy = ny+shiftone_6 - (x[i][1]-boxlo[1])*delyinv_6;
    dz = nz+shiftone_6 - (x[i][2]-boxlo[2])*delzinv_6;

    compute_rho1d(dx,dy,dz, order_6, rho_coeff_6, rho1d_6);
    compute_drho1d(dx,dy,dz, order_6, drho_coeff_6, drho1d_6);


    ekx = eky = ekz = ZEROF;
    for (n = nlower_6; n <= nupper_6; n++) {
      mz = n+nz;
      for (m = nlower_6; m <= nupper_6; m++) {
        my = m+ny;
        for (l = nlower_6; l <= nupper_6; l++) {
          mx = l+nx;
          ekx += drho1d_6[0][l]*rho1d_6[1][m]*rho1d_6[2][n]*u_brick_g[mz][my][mx];
          eky += rho1d_6[0][l]*drho1d_6[1][m]*rho1d_6[2][n]*u_brick_g[mz][my][mx];
          ekz += rho1d_6[0][l]*rho1d_6[1][m]*drho1d_6[2][n]*u_brick_g[mz][my][mx];
        }
      }
    }
    ekx *= hx_inv;
    eky *= hy_inv;
    ekz *= hz_inv;

    // convert E-field to force
    type = atom->type[i];
    lj = B[type];

    s1 = x[i][0]*hx_inv;
    s2 = x[i][1]*hy_inv;
    s3 = x[i][2]*hz_inv;

    sf = sf_coeff_6[0]*sin(2*MY_PI*s1);
    sf += sf_coeff_6[1]*sin(4*MY_PI*s1);
    sf *= 2*lj*lj;
    f[i][0] += ekx*lj - sf;

    sf = sf_coeff_6[2]*sin(2*MY_PI*s2);
    sf += sf_coeff_6[3]*sin(4*MY_PI*s2);
    sf *= 2*lj*lj;
    f[i][1] += eky*lj - sf;


    sf = sf_coeff_6[4]*sin(2*MY_PI*s3);
    sf += sf_coeff_6[5]*sin(4*MY_PI*s3);
    sf *= 2*lj*lj;
    if (slabflag != 2) f[i][2] += ekz*lj - sf;

  }
}

/* ----------------------------------------------------------------------
   interpolate from grid to get dispersion field & force on my particles
   for geometric mixing rule for per atom quantities
------------------------------------------------------------------------- */

void PPPMDisp::fieldforce_g_peratom()
{
  int i,l,m,n,nx,ny,nz,mx,my,mz;
  FFT_SCALAR dx,dy,dz,x0,y0,z0;
  FFT_SCALAR u_pa,v0,v1,v2,v3,v4,v5;

  // loop over my charges, interpolate electric field from nearby grid points
  // (nx,ny,nz) = global coords of grid pt to "lower left" of charge
  // (dx,dy,dz) = distance to "lower left" grid pt
  // (mx,my,mz) = global coords of moving stencil pt
  // ek = 3 components of dispersion field on particle

  double **x = atom->x;
  int type;
  double lj;

  int nlocal = atom->nlocal;

  for (i = 0; i < nlocal; i++) {
    nx = part2grid_6[i][0];
    ny = part2grid_6[i][1];
    nz = part2grid_6[i][2];
    dx = nx+shiftone_6 - (x[i][0]-boxlo[0])*delxinv_6;
    dy = ny+shiftone_6 - (x[i][1]-boxlo[1])*delyinv_6;
    dz = nz+shiftone_6 - (x[i][2]-boxlo[2])*delzinv_6;

    compute_rho1d(dx,dy,dz, order_6, rho_coeff_6, rho1d_6);

    u_pa = v0 = v1 = v2 = v3 = v4 = v5 = ZEROF;
    for (n = nlower_6; n <= nupper_6; n++) {
      mz = n+nz;
      z0 = rho1d_6[2][n];
      for (m = nlower_6; m <= nupper_6; m++) {
	my = m+ny;
	y0 = z0*rho1d_6[1][m];
	for (l = nlower_6; l <= nupper_6; l++) {
	  mx = l+nx;
	  x0 = y0*rho1d_6[0][l];
	  if (eflag_atom) u_pa += x0*u_brick_g[mz][my][mx];	
	  if (vflag_atom) {
            v0 += x0*v0_brick_g[mz][my][mx];
            v1 += x0*v1_brick_g[mz][my][mx];
            v2 += x0*v2_brick_g[mz][my][mx];
            v3 += x0*v3_brick_g[mz][my][mx];
            v4 += x0*v4_brick_g[mz][my][mx];
            v5 += x0*v5_brick_g[mz][my][mx];
          }
	}
      }
    }

    // convert E-field to force
    type = atom->type[i];
    lj = B[type]*0.5;

    if (eflag_atom) eatom[i] += u_pa*lj;
    if (vflag_atom) {
      vatom[i][0] += v0*lj;
      vatom[i][1] += v1*lj;
      vatom[i][2] += v2*lj;
      vatom[i][3] += v3*lj;
      vatom[i][4] += v4*lj;
      vatom[i][5] += v5*lj;
    }
  }
}

/* ----------------------------------------------------------------------
   interpolate from grid to get dispersion field & force on my particles
   for arithmetic mixing rule and ik scheme
------------------------------------------------------------------------- */

void PPPMDisp::fieldforce_a_ik()
{
  int i,l,m,n,nx,ny,nz,mx,my,mz;
  FFT_SCALAR dx,dy,dz,x0,y0,z0;
  FFT_SCALAR ekx0, eky0, ekz0, ekx1, eky1, ekz1, ekx2, eky2, ekz2;
  FFT_SCALAR ekx3, eky3, ekz3, ekx4, eky4, ekz4, ekx5, eky5, ekz5;
  FFT_SCALAR ekx6, eky6, ekz6;

  // loop over my charges, interpolate electric field from nearby grid points
  // (nx,ny,nz) = global coords of grid pt to "lower left" of charge
  // (dx,dy,dz) = distance to "lower left" grid pt
  // (mx,my,mz) = global coords of moving stencil pt
  // ek = 3 components of dispersion field on particle

  double **x = atom->x;
  double **f = atom->f;
  int type;
  double lj0, lj1, lj2, lj3, lj4, lj5, lj6;

  int nlocal = atom->nlocal;

  for (i = 0; i < nlocal; i++) {

    nx = part2grid_6[i][0];
    ny = part2grid_6[i][1];
    nz = part2grid_6[i][2];
    dx = nx+shiftone_6 - (x[i][0]-boxlo[0])*delxinv_6;
    dy = ny+shiftone_6 - (x[i][1]-boxlo[1])*delyinv_6;
    dz = nz+shiftone_6 - (x[i][2]-boxlo[2])*delzinv_6;
    compute_rho1d(dx,dy,dz, order_6, rho_coeff_6, rho1d_6);
    ekx0 = eky0 = ekz0 = ZEROF;
    ekx1 = eky1 = ekz1 = ZEROF;
    ekx2 = eky2 = ekz2 = ZEROF;
    ekx3 = eky3 = ekz3 = ZEROF;
    ekx4 = eky4 = ekz4 = ZEROF;
    ekx5 = eky5 = ekz5 = ZEROF;
    ekx6 = eky6 = ekz6 = ZEROF;
    for (n = nlower_6; n <= nupper_6; n++) {
      mz = n+nz;
      z0 = rho1d_6[2][n];
      for (m = nlower_6; m <= nupper_6; m++) {
	my = m+ny;
	y0 = z0*rho1d_6[1][m];
	for (l = nlower_6; l <= nupper_6; l++) {
	  mx = l+nx;
	  x0 = y0*rho1d_6[0][l];
	  ekx0 -= x0*vdx_brick_a0[mz][my][mx];
	  eky0 -= x0*vdy_brick_a0[mz][my][mx];
	  ekz0 -= x0*vdz_brick_a0[mz][my][mx];
	  ekx1 -= x0*vdx_brick_a1[mz][my][mx];
	  eky1 -= x0*vdy_brick_a1[mz][my][mx];
	  ekz1 -= x0*vdz_brick_a1[mz][my][mx];
          ekx2 -= x0*vdx_brick_a2[mz][my][mx];
	  eky2 -= x0*vdy_brick_a2[mz][my][mx];
	  ekz2 -= x0*vdz_brick_a2[mz][my][mx];
	  ekx3 -= x0*vdx_brick_a3[mz][my][mx];
	  eky3 -= x0*vdy_brick_a3[mz][my][mx];
	  ekz3 -= x0*vdz_brick_a3[mz][my][mx];
	  ekx4 -= x0*vdx_brick_a4[mz][my][mx];
	  eky4 -= x0*vdy_brick_a4[mz][my][mx];
	  ekz4 -= x0*vdz_brick_a4[mz][my][mx];
          ekx5 -= x0*vdx_brick_a5[mz][my][mx];
	  eky5 -= x0*vdy_brick_a5[mz][my][mx];
	  ekz5 -= x0*vdz_brick_a5[mz][my][mx];
          ekx6 -= x0*vdx_brick_a6[mz][my][mx];
	  eky6 -= x0*vdy_brick_a6[mz][my][mx];
	  ekz6 -= x0*vdz_brick_a6[mz][my][mx];
	}
      }
    }
    // convert D-field to force
    type = atom->type[i];
    lj0 = B[7*type+6];
    lj1 = B[7*type+5];
    lj2 = B[7*type+4];
    lj3 = B[7*type+3];
    lj4 = B[7*type+2];
    lj5 = B[7*type+1];
    lj6 = B[7*type];
    f[i][0] += lj0*ekx0 + lj1*ekx1 + lj2*ekx2 + lj3*ekx3 + lj4*ekx4 + lj5*ekx5 + lj6*ekx6;
    f[i][1] += lj0*eky0 + lj1*eky1 + lj2*eky2 + lj3*eky3 + lj4*eky4 + lj5*eky5 + lj6*eky6;
    f[i][2] += lj0*ekz0 + lj1*ekz1 + lj2*ekz2 + lj3*ekz3 + lj4*ekz4 + lj5*ekz5 + lj6*ekz6;
  }
}

/* ----------------------------------------------------------------------
   interpolate from grid to get dispersion field & force on my particles
   for arithmetic mixing rule for the ad scheme
------------------------------------------------------------------------- */

void PPPMDisp::fieldforce_a_ad()
{
  int i,l,m,n,nx,ny,nz,mx,my,mz;
  FFT_SCALAR dx,dy,dz,x0,y0,z0;
  FFT_SCALAR ekx0, eky0, ekz0, ekx1, eky1, ekz1, ekx2, eky2, ekz2;
  FFT_SCALAR ekx3, eky3, ekz3, ekx4, eky4, ekz4, ekx5, eky5, ekz5;
  FFT_SCALAR ekx6, eky6, ekz6;

  double s1,s2,s3;
  double sf = 0.0;
  double *prd;

  if (triclinic == 0) prd = domain->prd;
  else prd = domain->prd_lamda;

  double xprd = prd[0];
  double yprd = prd[1];
  double zprd = prd[2];
  double zprd_slab = zprd*slab_volfactor;

  double hx_inv = nx_pppm_6/xprd;
  double hy_inv = ny_pppm_6/yprd;
  double hz_inv = nz_pppm_6/zprd_slab;

  // loop over my charges, interpolate electric field from nearby grid points
  // (nx,ny,nz) = global coords of grid pt to "lower left" of charge
  // (dx,dy,dz) = distance to "lower left" grid pt
  // (mx,my,mz) = global coords of moving stencil pt
  // ek = 3 components of dispersion field on particle

  double **x = atom->x;
  double **f = atom->f;
  int type;
  double lj0, lj1, lj2, lj3, lj4, lj5, lj6;

  int nlocal = atom->nlocal;

  for (i = 0; i < nlocal; i++) {

    nx = part2grid_6[i][0];
    ny = part2grid_6[i][1];
    nz = part2grid_6[i][2];
    dx = nx+shiftone_6 - (x[i][0]-boxlo[0])*delxinv_6;
    dy = ny+shiftone_6 - (x[i][1]-boxlo[1])*delyinv_6;
    dz = nz+shiftone_6 - (x[i][2]-boxlo[2])*delzinv_6;

    compute_rho1d(dx,dy,dz, order_6, rho_coeff_6, rho1d_6);
    compute_drho1d(dx,dy,dz, order_6, drho_coeff_6, drho1d_6);

    ekx0 = eky0 = ekz0 = ZEROF;
    ekx1 = eky1 = ekz1 = ZEROF;
    ekx2 = eky2 = ekz2 = ZEROF;
    ekx3 = eky3 = ekz3 = ZEROF;
    ekx4 = eky4 = ekz4 = ZEROF;
    ekx5 = eky5 = ekz5 = ZEROF;
    ekx6 = eky6 = ekz6 = ZEROF;
    for (n = nlower_6; n <= nupper_6; n++) {
      mz = n+nz;
      for (m = nlower_6; m <= nupper_6; m++) {
	my = m+ny;
	for (l = nlower_6; l <= nupper_6; l++) {
	  mx = l+nx;
          x0 = drho1d_6[0][l]*rho1d_6[1][m]*rho1d_6[2][n];
          y0 = rho1d_6[0][l]*drho1d_6[1][m]*rho1d_6[2][n];
          z0 = rho1d_6[0][l]*rho1d_6[1][m]*drho1d_6[2][n];

          ekx0 += x0*u_brick_a0[mz][my][mx];
          eky0 += y0*u_brick_a0[mz][my][mx];
          ekz0 += z0*u_brick_a0[mz][my][mx];

          ekx1 += x0*u_brick_a1[mz][my][mx];
          eky1 += y0*u_brick_a1[mz][my][mx];
          ekz1 += z0*u_brick_a1[mz][my][mx];

          ekx2 += x0*u_brick_a2[mz][my][mx];
          eky2 += y0*u_brick_a2[mz][my][mx];
          ekz2 += z0*u_brick_a2[mz][my][mx];

          ekx3 += x0*u_brick_a3[mz][my][mx];
          eky3 += y0*u_brick_a3[mz][my][mx];
          ekz3 += z0*u_brick_a3[mz][my][mx];

          ekx4 += x0*u_brick_a4[mz][my][mx];
          eky4 += y0*u_brick_a4[mz][my][mx];
          ekz4 += z0*u_brick_a4[mz][my][mx];

          ekx5 += x0*u_brick_a5[mz][my][mx];
          eky5 += y0*u_brick_a5[mz][my][mx];
          ekz5 += z0*u_brick_a5[mz][my][mx];

          ekx6 += x0*u_brick_a6[mz][my][mx];
          eky6 += y0*u_brick_a6[mz][my][mx];
          ekz6 += z0*u_brick_a6[mz][my][mx];
	}
      }
    }

    ekx0 *= hx_inv;
    eky0 *= hy_inv;
    ekz0 *= hz_inv;

    ekx1 *= hx_inv;
    eky1 *= hy_inv;
    ekz1 *= hz_inv;

    ekx2 *= hx_inv;
    eky2 *= hy_inv;
    ekz2 *= hz_inv;

    ekx3 *= hx_inv;
    eky3 *= hy_inv;
    ekz3 *= hz_inv;

    ekx4 *= hx_inv;
    eky4 *= hy_inv;
    ekz4 *= hz_inv;

    ekx5 *= hx_inv;
    eky5 *= hy_inv;
    ekz5 *= hz_inv;

    ekx6 *= hx_inv;
    eky6 *= hy_inv;
    ekz6 *= hz_inv;

    // convert D-field to force
    type = atom->type[i];
    lj0 = B[7*type+6];
    lj1 = B[7*type+5];
    lj2 = B[7*type+4];
    lj3 = B[7*type+3];
    lj4 = B[7*type+2];
    lj5 = B[7*type+1];
    lj6 = B[7*type];

    s1 = x[i][0]*hx_inv;
    s2 = x[i][1]*hy_inv;
    s3 = x[i][2]*hz_inv;

    sf = sf_coeff_6[0]*sin(2*MY_PI*s1);
    sf += sf_coeff_6[1]*sin(4*MY_PI*s1);
    sf *= 4*lj0*lj6 + 4*lj1*lj5 + 4*lj2*lj4 + 2*lj3*lj3;
    f[i][0] += lj0*ekx0 + lj1*ekx1 + lj2*ekx2 + lj3*ekx3 + lj4*ekx4 + lj5*ekx5 + lj6*ekx6 - sf;

    sf = sf_coeff_6[2]*sin(2*MY_PI*s2);
    sf += sf_coeff_6[3]*sin(4*MY_PI*s2);
    sf *= 4*lj0*lj6 + 4*lj1*lj5 + 4*lj2*lj4 + 2*lj3*lj3;
    f[i][1] += lj0*eky0 + lj1*eky1 + lj2*eky2 + lj3*eky3 + lj4*eky4 + lj5*eky5 + lj6*eky6 - sf;

    sf = sf_coeff_6[4]*sin(2*MY_PI*s3);
    sf += sf_coeff_6[5]*sin(4*MY_PI*s3);
    sf *= 4*lj0*lj6 + 4*lj1*lj5 + 4*lj2*lj4 + 2*lj3*lj3;
    if (slabflag != 2) f[i][2] += lj0*ekz0 + lj1*ekz1 + lj2*ekz2 + lj3*ekz3 + lj4*ekz4 + lj5*ekz5 + lj6*ekz6 - sf;
  }
}

/* ----------------------------------------------------------------------
   interpolate from grid to get dispersion field & force on my particles
   for arithmetic mixing rule for per atom quantities
------------------------------------------------------------------------- */

void PPPMDisp::fieldforce_a_peratom()
{
  int i,l,m,n,nx,ny,nz,mx,my,mz;
  FFT_SCALAR dx,dy,dz,x0,y0,z0;
  FFT_SCALAR u_pa0,v00,v10,v20,v30,v40,v50;
  FFT_SCALAR u_pa1,v01,v11,v21,v31,v41,v51;
  FFT_SCALAR u_pa2,v02,v12,v22,v32,v42,v52;
  FFT_SCALAR u_pa3,v03,v13,v23,v33,v43,v53;
  FFT_SCALAR u_pa4,v04,v14,v24,v34,v44,v54;
  FFT_SCALAR u_pa5,v05,v15,v25,v35,v45,v55;
  FFT_SCALAR u_pa6,v06,v16,v26,v36,v46,v56;

  // loop over my charges, interpolate electric field from nearby grid points
  // (nx,ny,nz) = global coords of grid pt to "lower left" of charge
  // (dx,dy,dz) = distance to "lower left" grid pt
  // (mx,my,mz) = global coords of moving stencil pt
  // ek = 3 components of dispersion field on particle

  double **x = atom->x;
  int type;
  double lj0, lj1, lj2, lj3, lj4, lj5, lj6;

  int nlocal = atom->nlocal;

  for (i = 0; i < nlocal; i++) {

    nx = part2grid_6[i][0];
    ny = part2grid_6[i][1];
    nz = part2grid_6[i][2];
    dx = nx+shiftone_6 - (x[i][0]-boxlo[0])*delxinv_6;
    dy = ny+shiftone_6 - (x[i][1]-boxlo[1])*delyinv_6;
    dz = nz+shiftone_6 - (x[i][2]-boxlo[2])*delzinv_6;
    compute_rho1d(dx,dy,dz, order_6, rho_coeff_6, rho1d_6);

    u_pa0 = v00 = v10 = v20 = v30 = v40 = v50 = ZEROF;
    u_pa1 = v01 = v11 = v21 = v31 = v41 = v51 = ZEROF;
    u_pa2 = v02 = v12 = v22 = v32 = v42 = v52 = ZEROF;
    u_pa3 = v03 = v13 = v23 = v33 = v43 = v53 = ZEROF;
    u_pa4 = v04 = v14 = v24 = v34 = v44 = v54 = ZEROF;
    u_pa5 = v05 = v15 = v25 = v35 = v45 = v55 = ZEROF;
    u_pa6 = v06 = v16 = v26 = v36 = v46 = v56 = ZEROF;
    for (n = nlower_6; n <= nupper_6; n++) {
      mz = n+nz;
      z0 = rho1d_6[2][n];
      for (m = nlower_6; m <= nupper_6; m++) {
	my = m+ny;
	y0 = z0*rho1d_6[1][m];
	for (l = nlower_6; l <= nupper_6; l++) {
	  mx = l+nx;
	  x0 = y0*rho1d_6[0][l];
          if (eflag_atom) {
            u_pa0 += x0*u_brick_a0[mz][my][mx];
            u_pa1 += x0*u_brick_a1[mz][my][mx];
            u_pa2 += x0*u_brick_a2[mz][my][mx];
            u_pa3 += x0*u_brick_a3[mz][my][mx];
            u_pa4 += x0*u_brick_a4[mz][my][mx];
            u_pa5 += x0*u_brick_a5[mz][my][mx];
            u_pa6 += x0*u_brick_a6[mz][my][mx];
	  }
          if (vflag_atom) {
            v00 += x0*v0_brick_a0[mz][my][mx];
            v10 += x0*v1_brick_a0[mz][my][mx];
            v20 += x0*v2_brick_a0[mz][my][mx];
            v30 += x0*v3_brick_a0[mz][my][mx];
            v40 += x0*v4_brick_a0[mz][my][mx];
            v50 += x0*v5_brick_a0[mz][my][mx];
            v01 += x0*v0_brick_a1[mz][my][mx];
            v11 += x0*v1_brick_a1[mz][my][mx];
            v21 += x0*v2_brick_a1[mz][my][mx];
            v31 += x0*v3_brick_a1[mz][my][mx];
            v41 += x0*v4_brick_a1[mz][my][mx];
            v51 += x0*v5_brick_a1[mz][my][mx];
            v02 += x0*v0_brick_a2[mz][my][mx];
            v12 += x0*v1_brick_a2[mz][my][mx];
            v22 += x0*v2_brick_a2[mz][my][mx];
            v32 += x0*v3_brick_a2[mz][my][mx];
            v42 += x0*v4_brick_a2[mz][my][mx];
            v52 += x0*v5_brick_a2[mz][my][mx];
            v03 += x0*v0_brick_a3[mz][my][mx];
            v13 += x0*v1_brick_a3[mz][my][mx];
            v23 += x0*v2_brick_a3[mz][my][mx];
            v33 += x0*v3_brick_a3[mz][my][mx];
            v43 += x0*v4_brick_a3[mz][my][mx];
            v53 += x0*v5_brick_a3[mz][my][mx];
            v04 += x0*v0_brick_a4[mz][my][mx];
            v14 += x0*v1_brick_a4[mz][my][mx];
            v24 += x0*v2_brick_a4[mz][my][mx];
            v34 += x0*v3_brick_a4[mz][my][mx];
            v44 += x0*v4_brick_a4[mz][my][mx];
            v54 += x0*v5_brick_a4[mz][my][mx];
            v05 += x0*v0_brick_a5[mz][my][mx];
            v15 += x0*v1_brick_a5[mz][my][mx];
            v25 += x0*v2_brick_a5[mz][my][mx];
            v35 += x0*v3_brick_a5[mz][my][mx];
            v45 += x0*v4_brick_a5[mz][my][mx];
            v55 += x0*v5_brick_a5[mz][my][mx];
            v06 += x0*v0_brick_a6[mz][my][mx];
            v16 += x0*v1_brick_a6[mz][my][mx];
            v26 += x0*v2_brick_a6[mz][my][mx];
            v36 += x0*v3_brick_a6[mz][my][mx];
            v46 += x0*v4_brick_a6[mz][my][mx];
            v56 += x0*v5_brick_a6[mz][my][mx];
          }
	}
      }
    }
    // convert D-field to force
    type = atom->type[i];
    lj0 = B[7*type+6]*0.5;
    lj1 = B[7*type+5]*0.5;
    lj2 = B[7*type+4]*0.5;
    lj3 = B[7*type+3]*0.5;
    lj4 = B[7*type+2]*0.5;
    lj5 = B[7*type+1]*0.5;
    lj6 = B[7*type]*0.5;

 
    if (eflag_atom) eatom[i] += u_pa0*lj0 + u_pa1*lj1 + u_pa2*lj2 + u_pa3*lj3 + u_pa4*lj4 + u_pa5*lj5 + u_pa6*lj6;
    if (vflag_atom) {
      vatom[i][0] += v00*lj0 + v01*lj1 + v02*lj2 + v03*lj3 + v04*lj4 + v05*lj5 + v06*lj6;
      vatom[i][1] += v10*lj0 + v11*lj1 + v12*lj2 + v13*lj3 + v14*lj4 + v15*lj5 + v16*lj6;
      vatom[i][2] += v20*lj0 + v21*lj1 + v22*lj2 + v23*lj3 + v24*lj4 + v25*lj5 + v26*lj6;
      vatom[i][3] += v30*lj0 + v31*lj1 + v32*lj2 + v33*lj3 + v34*lj4 + v35*lj5 + v36*lj6;
      vatom[i][4] += v40*lj0 + v41*lj1 + v42*lj2 + v43*lj3 + v44*lj4 + v45*lj5 + v46*lj6;
      vatom[i][5] += v50*lj0 + v51*lj1 + v52*lj2 + v53*lj3 + v54*lj4 + v55*lj5 + v56*lj6;
    }
  }
}


/* ----------------------------------------------------------------------
   map nprocs to NX by NY grid as PX by PY procs - return optimal px,py 
------------------------------------------------------------------------- */

void PPPMDisp::procs2grid2d(int nprocs, int nx, int ny, int *px, int *py)
{
  // loop thru all possible factorizations of nprocs
  // surf = surface area of largest proc sub-domain
  // innermost if test minimizes surface area and surface/volume ratio

  int bestsurf = 2 * (nx + ny);
  int bestboxx = 0;
  int bestboxy = 0;

  int boxx,boxy,surf,ipx,ipy;

  ipx = 1;
  while (ipx <= nprocs) {
    if (nprocs % ipx == 0) {
      ipy = nprocs/ipx;
      boxx = nx/ipx;
      if (nx % ipx) boxx++;
      boxy = ny/ipy;
      if (ny % ipy) boxy++;
      surf = boxx + boxy;
      if (surf < bestsurf || 
	  (surf == bestsurf && boxx*boxy > bestboxx*bestboxy)) {
	bestsurf = surf;
	bestboxx = boxx;
	bestboxy = boxy;
	*px = ipx;
	*py = ipy;
      }
    }
    ipx++;
  }
}

/* ----------------------------------------------------------------------
   charge assignment into rho1d
   dx,dy,dz = distance of particle from "lower left" grid point 
------------------------------------------------------------------------- */

void PPPMDisp::compute_rho1d(const FFT_SCALAR &dx, const FFT_SCALAR &dy,
			      const FFT_SCALAR &dz, int ord, 
                             FFT_SCALAR **rho_c, FFT_SCALAR **r1d)
{
  int k,l;
  FFT_SCALAR r1,r2,r3;

  for (k = (1-ord)/2; k <= ord/2; k++) {
    r1 = r2 = r3 = ZEROF;

    for (l = ord-1; l >= 0; l--) {
      r1 = rho_c[l][k] + r1*dx;
      r2 = rho_c[l][k] + r2*dy;
      r3 = rho_c[l][k] + r3*dz;
    }
    r1d[0][k] = r1;
    r1d[1][k] = r2;
    r1d[2][k] = r3;
  }
}

/* ----------------------------------------------------------------------
   charge assignment into drho1d
   dx,dy,dz = distance of particle from "lower left" grid point
------------------------------------------------------------------------- */

void PPPMDisp::compute_drho1d(const FFT_SCALAR &dx, const FFT_SCALAR &dy,
                          const FFT_SCALAR &dz, int ord, 
                              FFT_SCALAR **drho_c, FFT_SCALAR **dr1d)
{
  int k,l;
  FFT_SCALAR r1,r2,r3;

  for (k = (1-ord)/2; k <= ord/2; k++) {
    r1 = r2 = r3 = ZEROF;

    for (l = ord-2; l >= 0; l--) {
      r1 = drho_c[l][k] + r1*dx;
      r2 = drho_c[l][k] + r2*dy;
      r3 = drho_c[l][k] + r3*dz;
    }
    dr1d[0][k] = r1;
    dr1d[1][k] = r2;
    dr1d[2][k] = r3;
  }
}

/* ----------------------------------------------------------------------
   generate coeffients for the weight function of order n

              (n-1)
  Wn(x) =     Sum    wn(k,x) , Sum is over every other integer
           k=-(n-1)
  For k=-(n-1),-(n-1)+2, ....., (n-1)-2,n-1
      k is odd integers if n is even and even integers if n is odd
              ---
             | n-1
             | Sum a(l,j)*(x-k/2)**l   if abs(x-k/2) < 1/2
  wn(k,x) = <  l=0
             |
             |  0                       otherwise
              ---
  a coeffients are packed into the array rho_coeff to eliminate zeros
  rho_coeff(l,((k+mod(n+1,2))/2) = a(l,k) 
------------------------------------------------------------------------- */

void PPPMDisp::compute_rho_coeff(FFT_SCALAR **coeff , FFT_SCALAR **dcoeff, 
                                 int ord)
{
  int j,k,l,m;
  FFT_SCALAR s;

  FFT_SCALAR **a;
  memory->create2d_offset(a,ord,-ord,ord,"pppm/disp:a");

  for (k = -ord; k <= ord; k++) 
    for (l = 0; l < ord; l++)
      a[l][k] = 0.0;
        
  a[0][0] = 1.0;
  for (j = 1; j < ord; j++) {
    for (k = -j; k <= j; k += 2) {
      s = 0.0;
      for (l = 0; l < j; l++) {
	a[l+1][k] = (a[l][k+1]-a[l][k-1]) / (l+1);
#ifdef FFT_SINGLE
	s += powf(0.5,(float) l+1) *
	  (a[l][k-1] + powf(-1.0,(float) l) * a[l][k+1]) / (l+1);
#else
	s += pow(0.5,(double) l+1) * 
	  (a[l][k-1] + pow(-1.0,(double) l) * a[l][k+1]) / (l+1);
#endif
      }
      a[0][k] = s;
    }
  }

  m = (1-ord)/2;
  for (k = -(ord-1); k < ord; k += 2) {
    for (l = 0; l < ord; l++)
      coeff[l][m] = a[l][k];
    for (l = 1; l < ord; l++)
      dcoeff[l-1][m] = l*a[l][k];
    m++;
  }

  memory->destroy2d_offset(a,-ord);
}

/* ----------------------------------------------------------------------
   Slab-geometry correction term to dampen inter-slab interactions between
   periodically repeating slabs.  Yields good approximation to 2D Ewald if 
   adequate empty space is left between repeating slabs (J. Chem. Phys. 
   111, 3155).  Slabs defined here to be parallel to the xy plane. 
------------------------------------------------------------------------- */

void PPPMDisp::slabcorr(int eflag)
{
  // compute local contribution to global dipole moment

  double *q = atom->q;
  double **x = atom->x;
  int nlocal = atom->nlocal;

  double dipole = 0.0;
  for (int i = 0; i < nlocal; i++) dipole += q[i]*x[i][2];
  
  // sum local contributions to get global dipole moment

  double dipole_all;
  MPI_Allreduce(&dipole,&dipole_all,1,MPI_DOUBLE,MPI_SUM,world);

  // compute corrections
  
  const double e_slabcorr = 2.0*MY_PI*dipole_all*dipole_all/volume;
  const double qscale = force->qqrd2e * scale;
  
  if (eflag_global) energy_1 += qscale * e_slabcorr;

  // per-atom energy

  if (eflag_atom) {
    double efact = 2.0*MY_PI*dipole_all/volume; 
    for (int i = 0; i < nlocal; i++) eatom[i] += qscale * q[i]*x[i][2]*efact;
  }

  // add on force corrections

  double ffact = -4.0*MY_PI*dipole_all/volume; 
  double **f = atom->f;

  for (int i = 0; i < nlocal; i++) f[i][2] += qscale * q[i]*ffact;
}

/* ----------------------------------------------------------------------
   perform and time the 1d FFTs required for N timesteps
------------------------------------------------------------------------- */

int PPPMDisp::timing_1d(int n, double &time1d)
{
  double time1,time2;
  int mixing = 1;
  if (function[2]) mixing = 4;

  if (function[0]) for (int i = 0; i < 2*nfft_both; i++) work1[i] = ZEROF;
  if (function[1] + function[2])
    for (int i = 0; i < 2*nfft_both_6; i++) work1_6[i] = ZEROF;

  MPI_Barrier(world);
  time1 = MPI_Wtime();

  if (function[0]) {
    for (int i = 0; i < n; i++) {
      fft1->timing1d(work1,nfft_both,1);
      fft2->timing1d(work1,nfft_both,-1);
      if (differentiation_flag != 1){
        fft2->timing1d(work1,nfft_both,-1);
        fft2->timing1d(work1,nfft_both,-1);
      }
    }
  }

  MPI_Barrier(world);
  time2 = MPI_Wtime();
  time1d = time2 - time1;

  MPI_Barrier(world);
  time1 = MPI_Wtime();

  if (function[1] + function[2]) {
    for (int i = 0; i < n; i++) {
      fft1_6->timing1d(work1_6,nfft_both_6,1);
      fft2_6->timing1d(work1_6,nfft_both_6,-1);
      if (differentiation_flag != 1){
        fft2_6->timing1d(work1_6,nfft_both_6,-1);
        fft2_6->timing1d(work1_6,nfft_both_6,-1);
      }
    }
  }

  MPI_Barrier(world);
  time2 = MPI_Wtime();
  time1d += (time2 - time1)*mixing;

  if (differentiation_flag) return 2;
  return 4;
}

/* ----------------------------------------------------------------------
   perform and time the 3d FFTs required for N timesteps
------------------------------------------------------------------------- */

int PPPMDisp::timing_3d(int n, double &time3d)
{
  double time1,time2;
  int mixing = 1;
  if (function[2]) mixing = 4;

  if (function[0]) for (int i = 0; i < 2*nfft_both; i++) work1[i] = ZEROF;
  if (function[1] + function[2]) 
    for (int i = 0; i < 2*nfft_both_6; i++) work1_6[i] = ZEROF;



  MPI_Barrier(world);
  time1 = MPI_Wtime();

  if (function[0]) {
    for (int i = 0; i < n; i++) {
      fft1->compute(work1,work1,1);
      fft2->compute(work1,work1,-1);
      if (differentiation_flag != 1) {
        fft2->compute(work1,work1,-1);
        fft2->compute(work1,work1,-1);
      }
    }
  }

  MPI_Barrier(world);
  time2 = MPI_Wtime();
  time3d = time2 - time1;

  MPI_Barrier(world);
  time1 = MPI_Wtime();
  
  if (function[1] + function[2]) {
    for (int i = 0; i < n; i++) {
      fft1_6->compute(work1_6,work1_6,1);
      fft2_6->compute(work1_6,work1_6,-1);
      if (differentiation_flag != 1) {
        fft2_6->compute(work1_6,work1_6,-1);
        fft2_6->compute(work1_6,work1_6,-1);
      }
    }
  }

  MPI_Barrier(world);
  time2 = MPI_Wtime();
  time3d += (time2 - time1) * mixing;

  if (differentiation_flag) return 2;
  return 4;
}

/* ----------------------------------------------------------------------
   memory usage of local arrays 
------------------------------------------------------------------------- */

double PPPMDisp::memory_usage()
{
  double bytes = nmax*3 * sizeof(double);
  int mixing = 1;
  if (function[2]) mixing = 7;

  if (function[0]) {
    int nbrick = (nxhi_out-nxlo_out+1) * (nyhi_out-nylo_out+1) * 
      (nzhi_out-nzlo_out+1);
    bytes += (4 ) * nbrick * sizeof(FFT_SCALAR);     // density_brick + vd_brick + per atom bricks
    bytes += 6 * nfft_both * sizeof(double);      // vg
    bytes += nfft_both * sizeof(double);          // greensfn
    bytes += nfft_both * 3 * sizeof(FFT_SCALAR);    // density_FFT, work1, work2 
    bytes += (2 * nbuf ) * sizeof(FFT_SCALAR);       // buf1, buf2, buf3, buf4
  }

  if (function[1] + function[2]) {
    int nbrick = (nxhi_out_6-nxlo_out_6+1) * (nyhi_out_6-nylo_out_6+1) * 
      (nzhi_out_6-nzlo_out_6+1);
    bytes += (4 ) * nbrick * sizeof(FFT_SCALAR) * mixing;     // density_brick + vd_brick + per atom bricks
    bytes += 6 * nfft_both_6 * sizeof(double);      // vg
    bytes += nfft_both_6 * sizeof(double);          // greensfn
    bytes += nfft_both_6 * (mixing +2) * sizeof(FFT_SCALAR);    // density_FFT, work1, work2 
    bytes += (2 * nbuf_6 ) * sizeof(FFT_SCALAR) * mixing;       // buf1, buf2, buf3, buf4
  }
  return bytes;
}