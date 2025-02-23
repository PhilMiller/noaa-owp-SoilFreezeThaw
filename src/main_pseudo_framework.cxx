#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "../bmi/bmi.hxx"
#include "../include/bmi_soil_freeze_thaw.hxx"
#include "../include/soil_freeze_thaw.hxx"

// include extern models
#include "cfe/include/cfe.h"
#include "cfe/include/bmi.h"
#include "cfe/include/bmi_cfe.h"
#include "evapotranspiration/include/pet.h"
#include "evapotranspiration/include/bmi_pet.h"
#include "aorc_bmi/include/aorc.h"
#include "aorc_bmi/include/bmi_aorc.h"
#include "SoilMoistureProfiles/include/bmi_soil_moisture_profile.hxx"
#include "SoilMoistureProfiles/include/soil_moisture_profile.hxx"

#define FrozenFraction true

std::vector<double> ReadForcingData(std::string config_file);

/***************************************************************
    Function to pass PET to CFE using BMI.
***************************************************************/
void pass_pet_to_cfe(Bmi *cfe_bmi_model, Bmi *pet_bmi_model) {
    double var_val;
    double *var_ptr = &var_val;

    pet_bmi_model->get_value(pet_bmi_model, "water_potential_evaporation_flux", &var_ptr);
    cfe_bmi_model->set_value(cfe_bmi_model, "water_potential_evaporation_flux", &var_ptr);
}

/***************************************************************
    Function to pass the forcing data from AORC to CFE using BMI.
    This requires a lot of getters and setters, 
    so no need to clutter up main program
***************************************************************/
void pass_forcing_from_aorc_to_cfe(Bmi *cfe_bmi_model, Bmi *aorc_bmi_model) {

    /********************************************************************
        TODO: Get variable names through BMI, then loop through those
              so we don't re-write the get/set functions over and over
    ********************************************************************/

    double var_val;
    double *var_ptr = &var_val;
    
    aorc_bmi_model->get_value(aorc_bmi_model, "atmosphere_water__liquid_equivalent_precipitation_rate", var_ptr);
    cfe_bmi_model->set_value(cfe_bmi_model, "atmosphere_water__liquid_equivalent_precipitation_rate", var_ptr);

}

/***************************************************************
    Function to pass the ice fraction from Freeze-thaw model to CFE using BMI.
***************************************************************/

void pass_icefraction_from_sft_to_cfe(Bmi *cfe_bmi_model, BmiSoilFreezeThaw sft_bmi_model) {
  
  /********************************************************************
        TODO: Get variable names through BMI, then loop through those
              so we don't re-write the get/set functions over and over
  ********************************************************************/
  enum {Schaake=1, Xinanjiang=2};
  
  double ice_frac_v=0.0;
  double *ice_frac_ptr = &ice_frac_v;

  int *sf_runoff_scheme = new int[1];

  cfe_bmi_model->get_value(cfe_bmi_model, "SURF_RUNOFF_SCHEME", &sf_runoff_scheme[0]);
  sft_bmi_model.SetValue("ice_fraction_scheme_bmi", &(sf_runoff_scheme[0]));
  
  if (*sf_runoff_scheme == Schaake) {
    sft_bmi_model.GetValue("ice_fraction_schaake", ice_frac_ptr);
    cfe_bmi_model->set_value(cfe_bmi_model, "ice_fraction_schaake", ice_frac_ptr);
  }
  else if (*sf_runoff_scheme == Xinanjiang) {
    sft_bmi_model.GetValue("ice_fraction_xinan", ice_frac_ptr);
    cfe_bmi_model->set_value(cfe_bmi_model, "ice_fraction_xinan", ice_frac_ptr);
  }
 
}

/***************************************************************
    Function to pass the parameters from CFE to SMP and get SMC from the Coupler to set in Freeze-thaw model.
***************************************************************/
void pass_smc_from_smp_to_sft(Bmi *cfe_bmi_model, BmiSoilFreezeThaw *sft_bmi_model,BmiSoilMoistureProfile *smp_bmi_model) {
  
  enum {Constant=1, Linear=2};
  
  int nz = 0;
  int *nz_ptr = &nz;
  sft_bmi_model->GetValue("num_cells", nz_ptr);

  double storage = 0.0;
  double storage_change = 0.0;
  double *storage_ptr = &storage;
  double *storage_change_ptr = &storage_change;
  int smc_option_bmi=-1;
  int *smc_option_bmi_ptr = &smc_option_bmi;
  
  cfe_bmi_model->get_value(cfe_bmi_model, "SOIL_STORAGE", storage_ptr);
  cfe_bmi_model->get_value(cfe_bmi_model, "SOIL_STORAGE_CHANGE", storage_change_ptr);
  
  smp_bmi_model->SetValue("soil_storage",storage_ptr);
  smp_bmi_model->SetValue("soil_storage_change",storage_change_ptr);
  smp_bmi_model->GetValue("soil_storage_model",smc_option_bmi_ptr);

  if (smc_option_bmi == Constant)
    smp_bmi_model->Update();
  else if (smc_option_bmi == Linear) {
    double smc_layers[] = {0.25, 0.15, 0.1, 0.12};
    //double smc_layers[] = {0.4, 0.4, 0.4, 0.43};
    smp_bmi_model->SetValue("soil_moisture_layered",&smc_layers[0]);
    smp_bmi_model->Update();
  }
  else {
    std::cout<<"Not a valid option for the SMC profile!! "<<smc_option_bmi<<"\n";
    abort();
  }
  
  double *smct = new double[nz];
    
  smp_bmi_model->GetValue("soil_moisture_profile",&smct[0]);
  sft_bmi_model->SetValue("soil_moisture_profile", &smct[0]);
  
}

/***************************************************************
    Function to pass the forcing data from AORC to PET using BMI.
    This requires a lot of getters and setters, 
    so no need to clutter up main program
***************************************************************/
void pass_forcing_from_aorc_to_pet(Bmi *pet_bmi_model, Bmi *aorc_bmi_model){

    /********************************************************************
        TODO: Get variable names through BMI, then loop through those
              so we don't re-write the get/set functions over and over
    ********************************************************************/

    double var1_val;
    double *var1_ptr = &var1_val;
    aorc_bmi_model->get_value(aorc_bmi_model, "land_surface_air__temperature", var1_ptr);
    pet_bmi_model->set_value(pet_bmi_model, "land_surface_air__temperature", var1_ptr);

    double var2_val;
    double *var2_ptr = &var2_val;
    aorc_bmi_model->get_value(aorc_bmi_model, "land_surface_air__pressure", var2_ptr);
    pet_bmi_model->set_value(pet_bmi_model, "land_surface_air__pressure", var2_ptr);

    double var3_val;
    double *var3_ptr = &var3_val;
    aorc_bmi_model->get_value(aorc_bmi_model, "atmosphere_air_water~vapor__relative_saturation", var3_ptr);
    pet_bmi_model->set_value(pet_bmi_model, "atmosphere_air_water~vapor__relative_saturation", var3_ptr);

    double var4_val;
    double *var4_ptr = &var4_val;
    aorc_bmi_model->get_value(aorc_bmi_model, "atmosphere_water__liquid_equivalent_precipitation_rate", var4_ptr);
    pet_bmi_model->set_value(pet_bmi_model, "atmosphere_water__liquid_equivalent_precipitation_rate", var4_ptr);

    double var5_val;
    double *var5_ptr = &var5_val;
    aorc_bmi_model->get_value(aorc_bmi_model, "land_surface_radiation~incoming~shortwave__energy_flux", var5_ptr);
    pet_bmi_model->set_value(pet_bmi_model, "land_surface_radiation~incoming~shortwave__energy_flux", var5_ptr);

    double var6_val;
    double *var6_ptr = &var6_val;
    aorc_bmi_model->get_value(aorc_bmi_model, "land_surface_radiation~incoming~longwave__energy_flux", var6_ptr);
    pet_bmi_model->set_value(pet_bmi_model, "land_surface_radiation~incoming~longwave__energy_flux", var6_ptr);

    double var7_val;
    double *var7_ptr = &var7_val;
    aorc_bmi_model->get_value(aorc_bmi_model, "land_surface_wind__x_component_of_velocity", var7_ptr);
    pet_bmi_model->set_value(pet_bmi_model, "land_surface_wind__x_component_of_velocity", var7_ptr);

    double var8_val;
    double *var8_ptr = &var8_val;
    aorc_bmi_model->get_value(aorc_bmi_model, "land_surface_wind__y_component_of_velocity", var8_ptr);
    pet_bmi_model->set_value(pet_bmi_model, "land_surface_wind__y_component_of_velocity", var8_ptr);
}



/************************************************************************
    This main program is a mock framwork.
    This is not part of BMI, but acts as the driver that calls the model.
************************************************************************/
int
 main(int argc, const char *argv[])
{

  /************************************************************************
      A configuration file is required for running this model through BMI
  ************************************************************************/
  if(argc<=4){
    printf("make sure to include a path to the CFE config file\n");
    exit(1);
  }

  /************************************************************************
      allocating memory to store the entire BMI structure for CFE and AORC
  ************************************************************************/
  printf("allocating memory to store entire BMI structure for CFE\n");
  Bmi *cfe_bmi_model = (Bmi *) malloc(sizeof(Bmi));
  printf("allocating memory to store entire BMI structure for AORC\n");
  Bmi *aorc_bmi_model = (Bmi *) malloc(sizeof(Bmi));
  printf("allocating memory to store entire BMI structure for PET\n");
  Bmi *pet_bmi_model = (Bmi *) malloc(sizeof(Bmi));
  
  BmiSoilFreezeThaw sft_bmi_model;

  BmiSoilMoistureProfile smp_bmi_model;
  
  /************************************************************************
      Registering the BMI model for CFE and AORC
  ************************************************************************/
  printf("Registering BMI CFE model\n");
  register_bmi_cfe(cfe_bmi_model);
  printf("Registering BMI AORC model\n");
  register_bmi_aorc(aorc_bmi_model);
  printf("Registering BMI PET model\n");
  register_bmi_pet(pet_bmi_model);
  
  /************************************************************************
      Initializing the BMI model for CFE and AORC and Freeze-thaw model
  ************************************************************************/
  printf("Initializeing BMI CFE model\n");
  const char *cfg_file_cfe = argv[1];
  cfe_bmi_model->initialize(cfe_bmi_model, cfg_file_cfe);

  printf("Initializeing BMI AORC model\n");
  const char *cfg_file_aorc = argv[2];
  printf("AORC config file %s\n", cfg_file_aorc);
  aorc_bmi_model->initialize(aorc_bmi_model, cfg_file_aorc);

  printf("Initializeing BMI PET model\n");
  const char *cfg_file_pet = argv[3];
  pet_bmi_model->initialize(pet_bmi_model, cfg_file_pet);

  printf("Initializeing BMI SFT model\n");
  const char *cfg_file_sft = argv[4];
  sft_bmi_model.Initialize(cfg_file_sft);

  //Read ground temperature data for SFT
  std::vector<double> ground_temp = ReadForcingData(cfg_file_sft);
  
  printf("Initializeing BMI SMP model\n");
  const char *cfg_file_smp = argv[5];
  smp_bmi_model.Initialize(cfg_file_smp);
  
  /************************************************************************
    Get the information from the configuration here in Main
  ************************************************************************/
  printf("Get the information from the configuration here in Main\n");
  cfe_state_struct *cfe;
  cfe = (cfe_state_struct *) cfe_bmi_model->data;

  printf("forcing file for the CFE module %s\n", cfe->forcing_file);
  pet_model *pet;
  pet = (pet_model *) pet_bmi_model->data;

  printf("forcing file for the PET module %s\n", pet->forcing_file);
  aorc_model *aorc;
  aorc = (aorc_model *) aorc_bmi_model->data;
  printf("forcing file for the AORC module %s\n", aorc->forcing_file);

  /************************************************************************
    This is the basic process for getting the four things to talk through BMI
    1. Update the AORC forcing data
    2. Getting forcing from AORC and setting forcing for PET
    3. Update the PET model
    3. Getting forcing from AORC and setting forcing for CFE
    4. Getting PET from PET and setting for CFE
    5. Get ice fraction from freeze-thaw model
    5. Update the CFE model.
    6. Update BMI SMP to get updated soil storage/change for the SFT model
    7. Update the Freeze-thaw model (soil temperature/ice content update)
  ************************************************************************/

  
  /************************************************************************
    Now loop through time and call the models with the intermediate get/set
  ************************************************************************/
  printf("looping through and calling updata\n");
  if (cfe->verbosity > 0)
    print_cfe_flux_header();

  // get time steps
  double endtime  = sft_bmi_model.GetEndTime();
  double timestep = sft_bmi_model.GetTimeStep();
  int nsteps = int(endtime/timestep); // total number of time steps

  assert (nsteps <= int(ground_temp.size()) ); // assertion to ensure that nsteps are less or equal than the input data
  
  for (int i = 0; i < nsteps; i++){

    std::cout<<"------------------------------------------------------ \n";
    std::cout<<"Timestep | "<< i <<", ground temp = "<< ground_temp[i] <<"\n";
    std::cout<<"------------------------------------------------------ \n";
    
    aorc_bmi_model->update(aorc_bmi_model);                         // Update model 1

    pass_forcing_from_aorc_to_pet(pet_bmi_model, aorc_bmi_model);   // Get and Set values

    pet_bmi_model->update(pet_bmi_model);
    
    pass_forcing_from_aorc_to_cfe(cfe_bmi_model, aorc_bmi_model);   // Get and Set values

    pass_pet_to_cfe(cfe_bmi_model, pet_bmi_model);   // Get and Set values

    if (FrozenFraction)
      pass_icefraction_from_sft_to_cfe(cfe_bmi_model, sft_bmi_model);

    sft_bmi_model.SetValue("ground_temperature", &ground_temp[i]);
    
    if (pet->aorc.air_temperature_2m_K != aorc->aorc.air_temperature_2m_K){
      printf("ERROR: Temperature values do not match from AORC and PET\n");
      printf("Temperature value from AORC is %lf\n", aorc->aorc.air_temperature_2m_K);
      printf("Tempterature value from PET is %lf\n", pet->aorc.air_temperature_2m_K);
    }
    
    if (cfe->aorc.precip_kg_per_m2 != aorc->aorc.precip_kg_per_m2){
      printf("Precip values do not match\n");
      printf("precip value from AORC is %lf\n", aorc->aorc.precip_kg_per_m2);
      printf("precip value from CFE is %lf\n", cfe->aorc.precip_kg_per_m2);
    }
    
    if (cfe->et_struct.potential_et_m_per_s != pet->pet_m_per_s){
      printf("ERROR: PET values do not match from PET and CFE\n");
      printf("PET value from PET is %8.9lf\n", pet->pet_m_per_s);
      printf("PET value from CFE is %8.9lf\n", cfe->et_struct.potential_et_m_per_s);
    }

    if (cfe->verbosity > 2){
      printf("PET value from PET is %8.9lf\n", pet->pet_m_per_s);
      printf("PET value from CFE is %8.9lf\n", cfe->et_struct.potential_et_m_per_s);
    }
    
    cfe_bmi_model->update(cfe_bmi_model);                           // Update model 2
  
    if (cfe->verbosity > 0)
      print_cfe_flux_at_timestep(cfe);
    

    pass_smc_from_smp_to_sft(cfe_bmi_model, &sft_bmi_model,&smp_bmi_model);

    sft_bmi_model.Update(); // Update model 3
  }

  // Run the Mass Balance check
  mass_balance_check(cfe);

  /************************************************************************
    Finalize both the CFE and AORC bmi models
  ************************************************************************/
  printf("Finalizing CFE and AORC models\n");
  cfe_bmi_model->finalize(cfe_bmi_model);
  aorc_bmi_model->finalize(aorc_bmi_model);
  sft_bmi_model.Finalize();
  return 0;
}


std::vector<double>
ReadForcingData(std::string config_file)
{
  // get the forcing file from the config file

  std::ifstream file;
  file.open(config_file);

  if (!file) {
    std::stringstream errMsg;
    errMsg << config_file << " does not exist";
    throw std::runtime_error(errMsg.str());
  }

  std::string forcing_file;
  bool is_forcing_file_set=false;
  
  while (file) {
    std::string line;
    std::string param_key, param_value;

    std::getline(file, line);

    int loc_eq = line.find("=") + 1;
    param_key = line.substr(0, line.find("="));
    param_value = line.substr(loc_eq,line.length());

    if (param_key == "forcing_file") {
      forcing_file = param_value;
      is_forcing_file_set = true;
      break;
    }
  }

  if (!is_forcing_file_set) {
    std::stringstream errMsg;
    errMsg << config_file << " does not provide forcing_file";
    throw std::runtime_error(errMsg.str());
  }
  
  std::ifstream fp;
  fp.open(forcing_file);
  if (!fp) {
    cout<<"file "<<forcing_file<<" doesn't exist. \n";
    abort();
  }
  
  std::vector<double> Time_v(0.0);
  std::vector<double> GT_v(0.0);
  std::vector<string> vars;
  std::string line, cell;
  
  //read first line of strings which contains forcing variables names.
  std::getline(fp, line);
  std::stringstream lineStream(line);
  int ground_temp_index=-1;
  
  while(std::getline(lineStream,cell, ',')) {
    vars.push_back(cell);
  }

  for (unsigned int i=0; i<vars.size();i++) {
    if (vars[i] ==  "TMP_ground_surface")
      ground_temp_index = i;
  }

  if (ground_temp_index <0)
    ground_temp_index = 6; // 6 is the air temperature column, if not coupled and ground temperatgure is not provided
    
  int len_v = vars.size(); // number of forcing variables + time

  int count = 0;
  while (fp) {
    std::getline(fp, line);
    std::stringstream lineStream(line);
    while(std::getline(lineStream,cell, ',')) {
      
      if (count % len_v == 0) {
	Time_v.push_back(stod(cell));
	count +=1;
	continue;
      }

      if (count % len_v == ground_temp_index) {
	GT_v.push_back(stod(cell));
	count +=1;
	continue;
      }
      count +=1;
    }

  }

  return GT_v;
 
}
