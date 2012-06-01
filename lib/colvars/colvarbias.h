#ifndef COLVARBIAS_H
#define COLVARBIAS_H

#include "colvar.h"
#include "colvarparse.h"


/// \brief Collective variable bias, base class
class colvarbias : public colvarparse {
public:

  /// Numeric id of this bias
  int            id;

  /// Name of this bias
  std::string    name;
  
  /// Add a new collective variable to this bias
  void add_colvar (std::string const &cv_name);

  /// Retrieve colvar values and calculate their biasing forces
  /// Return bias energy
  virtual cvm::real update() = 0;

  /// Load new configuration - force constant and/or centers only
  virtual void change_configuration(std::string const &conf);

  /// Calculate change in energy from using alternate configuration
  virtual cvm::real energy_difference(std::string const &conf);

  /// Perform analysis tasks
  virtual inline void analyse() {}

  /// Send forces to the collective variables
  void communicate_forces();

  /// \brief Constructor
  /// 
  /// The constructor of the colvarbias base class is protected, so
  /// that it can only be called from inherited classes
  colvarbias (std::string const &conf, char const *key);

  /// Default constructor
  colvarbias();

  /// Destructor
  virtual inline ~colvarbias() {}

  /// Read the bias configuration from a restart file
  virtual std::istream & read_restart (std::istream &is) = 0;

  /// Write the bias configuration to a restart file
  virtual std::ostream & write_restart (std::ostream &os) = 0;

protected:

  /// \brief Pointers to collective variables to which the bias is
  /// applied; current values and metric functions will be obtained
  /// through each colvar object
  std::vector<colvar *>    colvars;

  /// \brief Current forces from this bias to the colvars
  std::vector<colvarvalue> colvar_forces;

  /// \brief Current energy of this bias (colvar_forces should be
  /// obtained by deriving this)
  cvm::real                bias_energy;

  /// \brief Whether this bias has already accumulated information
  /// (when relevant)
  bool                     has_data;

};


/// \brief Harmonic restraint, optionally moving towards a target
/// (implementation of \link colvarbias \endlink)
class colvarbias_harmonic : public colvarbias {

public:

  /// Retrieve colvar values and calculate their biasing forces
  virtual cvm::real update();

  /// Load new configuration - force constant and/or centers only
  virtual void change_configuration(std::string const &conf);

  /// Calculate change in energy from using alternate configuration
  virtual cvm::real energy_difference(std::string const &conf);

  /// Read the bias configuration from a restart file
  virtual std::istream & read_restart (std::istream &is);

  /// Write the bias configuration to a restart file
  virtual std::ostream & write_restart (std::ostream &os);

  /// \brief Constructor
  colvarbias_harmonic (std::string const &conf, char const *key);

  /// Destructor
  virtual inline ~colvarbias_harmonic() {}


protected:

  /// \brief Restraint centers
  std::vector<colvarvalue> colvar_centers;

  /// \brief Restraint centers without wrapping or constraints applied
  std::vector<colvarvalue> colvar_centers_raw;

  /// \brief Restraint force constant
  cvm::real force_k;

  /// \brief Moving target?
  bool b_chg_centers;

  /// \brief Changing force constant?
  bool b_chg_force_k;

  /// \brief Restraint force constant (target value)
  cvm::real target_force_k;

  /// \brief Equilibration steps for restraint FE calculation through TI
  cvm::real target_equil_steps;

  /// \brief Restraint force constant (starting value)
  cvm::real starting_force_k;

  /// \brief Lambda-schedule for custom varying force constant
  std::vector<cvm::real> lambda_schedule;

  /// \brief New restraint centers
  std::vector<colvarvalue> target_centers;

  /// \brief Amplitude of the restraint centers' increment at each step
  /// (or stage) towards the new values (calculated from target_nsteps)
  std::vector<colvarvalue> centers_incr;

  /// \brief Exponent for varying the force constant
  cvm::real force_k_exp;

  /// \brief Number of steps required to reach the target force constant
  /// or restraint centers
  size_t target_nsteps;

  /// \brief Number of stages over which to perform the change
  /// If zero, perform a continuous change
  size_t target_nstages;

  /// \brief Number of current stage of the perturbation
  size_t stage;
  
  /// \brief Intermediate quantity to compute the restraint free energy
  /// (in TI, would be the accumulating FE derivative)
  cvm::real restraint_FE;
};


#endif



// Emacs
// Local Variables:
// mode: C++
// End: