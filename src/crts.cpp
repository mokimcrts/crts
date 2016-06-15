#include <libconfig.h>
#include <stdio.h>
#include <stdlib.h>
#include <liquid/liquid.h>
#include <cstdint>
#include "crts.hpp"
#include "extensible_cognitive_radio.hpp"

#define DEBUG 0
#if DEBUG == 1
#define dprintf(...) printf(__VA_ARGS__)
#else
#define dprintf(...) /*__VA_ARGS__*/
#endif

////////////////////////////////////////////////////////////////////////
// Command-line style argument functions
////////////////////////////////////////////////////////////////////////

// Must call freeargcargv after calling this function to free memory
void str2argcargv(char *string, char *progName, int &argc, char (**&argv))    {
  char *stringCpy = (char *)malloc(sizeof(char)*(strlen(string)+1));
  strcpy(stringCpy, string);
  char* token = strtok(stringCpy, " ");
  // Get number of arguments
  argc = 1;
  while(token != NULL){
    (argc)++;
    token = strtok(NULL, " ");
  }
  
  argv = (char **) malloc(sizeof(char*) * (argc+1));
  argv[argc] = 0;
  // Set the name of the program in the first element
  *argv = (char *) malloc(sizeof(char) * (strlen(progName)+1));
  strcpy(*argv, progName); 
  
  if ((argc) > 1){
    free(stringCpy);
    stringCpy = (char *) malloc(sizeof(char)*strlen(string));
    strcpy(stringCpy, string);
    token = strtok(stringCpy, " ");
    for (int i=1; token!=NULL; i++)
    {
      argv[i] = (char *) malloc(sizeof(char)*strlen(token));
      strcpy( argv[i], token);
      token = strtok(NULL, " ");
    } 
  }

  //Debug
  for (int i=0; i<argc; i++){
    dprintf("argv[%d] = %s\n",i, argv[i]);
  }
}

// Call when done with argc argv arrays created by str2argcargv().
void freeargcargv(int &argc, char **&argv) {
  for (int i = 1; i<argc+1; i++){
    free( argv[i] );
  }
  free(argv);
  argv = NULL;
}



//////////////////////////////////////////////////////////////////
// Config files
//////////////////////////////////////////////////////////////////

void read_master_parameters(char *nameMasterScenFile, 
                            int *num_scenarios, 
                            bool *octave_log_summary) {
  config_t cfg; // Returns all parameters in this structure
  char config_str[30];
  const char *tmpS;
  int tmpI; // Stores the value of Integer Parameters from Config file

  config_init(&cfg);

  // Read the file. If there is an error, report it and exit.
  char scenario_master_file[100];
  sprintf(scenario_master_file, "%s.cfg", nameMasterScenFile);
  if (!config_read_file(&cfg, scenario_master_file)) {
    printf("Error reading master scenario file (%s) on line %i: %s\n",
           nameMasterScenFile, config_error_line(&cfg), config_error_text(&cfg));
    exit(EXIT_FAILURE);
  }

  // Read the number of scenarios for this execution
  if (config_lookup_int(&cfg, "num_scenarios", &tmpI))
    *num_scenarios = (int)tmpI;

  // Check that all scenarios are specified
  for (int i = 0; i < *num_scenarios; i++) {
    sprintf(config_str, "scenario_%d", i + 1);
    if (!config_lookup_string(&cfg, config_str, &tmpS)) {
      printf("Scenario %i is not specified!\n", i);
      exit(EXIT_FAILURE);
    }
  }

  // Read the octave log summary flag
  if (config_lookup_int(&cfg, "octave_log_summary", &tmpI))
    *octave_log_summary = (int)tmpI;
}

int read_master_scenario(char *nameMasterScenFile, int scenario_num,
                         char *scenario_name) {
  config_t cfg; // Returns all parameters in this structure
  char config_str[30];
  const char *tmpS;
  int tmpI; // Stores the value of Integer Parameters from Config file

  config_init(&cfg);

  // Read the file. If there is an error, report it and exit.
  char scenario_master_file[100];
  sprintf(scenario_master_file, "%s.cfg", nameMasterScenFile);
  if (!config_read_file(&cfg, scenario_master_file)) {
    printf("Error reading master scenario file (%s) on line %i\n",
           nameMasterScenFile, config_error_line(&cfg));
    exit(EXIT_FAILURE);
  }

  // Read the scenario name
  sprintf(config_str, "scenario_%d", scenario_num);
  if (config_lookup_string(&cfg, config_str, &tmpS))
    strcpy(scenario_name, tmpS);

  // Read the reps for all scenarios
  sprintf(config_str, "reps_all_scenarios");
  int reps;
  if (config_lookup_int(&cfg, config_str, &tmpI))
    reps = tmpI;
  else
    reps = 1;

  // Read the reps for this specific scenario
  sprintf(config_str, "reps_scenario_%d", scenario_num);
  if (config_lookup_int(&cfg, config_str, &tmpI))
    reps = tmpI;

  config_destroy(&cfg);
  return reps;
}

struct scenario_parameters read_scenario_parameters(char *scenario_file) {
  // configuration variable
  config_t cfg;
  config_init(&cfg);

  // string pointing to scenario file
  char scenario[100];
  strcpy(scenario, "scenarios/");
  strcat(scenario, scenario_file);

  // Read the file. If there is an error, report it and exit.
  if (!config_read_file(&cfg, scenario)) {
    printf("Error reading %s on line %i\n", scenario, config_error_line(&cfg));
    printf("%s\n", config_error_text(&cfg));
    config_destroy(&cfg);
    exit(EXIT_FAILURE);
  }

  // Read scenario parameters
  struct scenario_parameters sp;
  int tmpI;
  double tmpD;
  const char *tmpS;
  
  config_lookup_int(&cfg, "num_nodes", &tmpI);
  sp.num_nodes = tmpI;
  config_lookup_float(&cfg, "run_time", &tmpD);
  // FIXME: Only integer number of seconds are allowed
  sp.run_time = (int64_t)tmpD;
  
  if (config_lookup_string(&cfg, "scenario_controller", &tmpS))
    strcpy(sp.SC, tmpS);
  else
    strcpy(sp.SC, "SC_Template");
  
  if (config_lookup_float(&cfg, "sc_timeout_ms", &tmpD))
    sp.sc_timeout_ms = tmpD;
  else
    sp.sc_timeout_ms = 1.0;

  if (config_lookup_string(&cfg, "sc_args", &tmpS))
    strcpy(sp.sc_args, tmpS);
  else
    sp.sc_args[0] = '\0';
  
  config_destroy(&cfg);

  return sp;
} // End readScConfigFile()

struct node_parameters read_node_parameters(int node, char *scenario_file) {
  // string pointing to scenario file
  char scenario[100];
  strcpy(scenario, "scenarios/");
  strcat(scenario, scenario_file);

  config_t cfg;
  config_init(&cfg);

  // Read the file. If there is an error, report it and exit.
  if (!config_read_file(&cfg, scenario)) {
    printf("Error reading config file on line %i\n", config_error_line(&cfg));
    config_destroy(&cfg);
    exit(EXIT_FAILURE);
  }

  int tmpI;
  double tmpD;
  const char *tmpS;

  // scenario info struct for node
  struct node_parameters np = {};
  char nodestr[100];
  std::string node_num;
  std::stringstream out;
  out << node;
  node_num = out.str();

  // lookup specific node
  strcpy(nodestr, "node");
  strcat(nodestr, node_num.c_str());
  config_setting_t *node_config = config_lookup(&cfg, nodestr);

  // read target IP address for the node
  if (config_setting_lookup_string(node_config, "target_ip", &tmpS))
    strcpy(np.target_ip, tmpS);
  else
    strcpy(np.target_ip, "10.0.0.3");

  // read server IP address for the node
  if (config_setting_lookup_string(node_config, "server_ip", &tmpS))
    strcpy(np.server_ip, tmpS);
  else
    strcpy(np.server_ip, "192.168.1.12");

  // read type of node
  if (config_setting_lookup_string(node_config, "node_type", &tmpS)) {
    if (strcmp(tmpS, "cognitive radio") == 0) {
      np.node_type = COGNITIVE_RADIO;
      np.cognitive_radio_type = EXTENSIBLE_COGNITIVE_RADIO;
      // If node is a CR, lookup whether is uses the ECR or python
      if (config_setting_lookup_string(node_config, "cognitive_radio_type", &tmpS)) {
        if (strcmp(tmpS, "python") == 0) {
          np.cognitive_radio_type = PYTHON;
          // python radios are specified by the "python_file" field in the
          // scenario file
          if (config_setting_lookup_string(node_config, "python_file", &tmpS)) {
            strcpy(np.python_file, tmpS);
            // check for optional command line arguments
            if (config_setting_lookup_string(node_config, "python_args", &tmpS)) {
              strcpy(np.python_args, tmpS);
            }
          } else {
            printf("A python radio requires a python file.\n");
          }
        }
        // Possibly add more types later, but for now if not python, then radio
        // must be ECR-based using the cognitive_engine
        else {
          if (config_setting_lookup_string(node_config, "cognitive_engine", &tmpS))
            strcpy(np.cognitive_engine, tmpS);
          else {
            printf(
                "Configuration of a node did not specify a cognitive engine");
            exit(EXIT_FAILURE);
          }
        }
      }
    } else if (strcmp(tmpS, "interferer") == 0)
      np.node_type = INTERFERER;
  }

  // read all possible node settings
  if (config_setting_lookup_string(node_config, "crts_ip", &tmpS))
    strcpy(np.crts_ip, tmpS);
  else
    strcpy(np.crts_ip, "10.0.0.2");

  if (config_setting_lookup_int(node_config, "print_rx_frame_metrics", &tmpI))
    np.print_rx_frame_metrics = (int)tmpI;

  if (config_setting_lookup_int(node_config, "log_phy_rx", &tmpI))
    np.log_phy_rx = (int)tmpI;

  if (config_setting_lookup_int(node_config, "log_phy_tx", &tmpI))
    np.log_phy_tx = (int)tmpI;

  if (config_setting_lookup_int(node_config, "log_net_rx", &tmpI))
    np.log_net_rx = (int)tmpI;

  if (config_setting_lookup_int(node_config, "log_net_tx", &tmpI))
    np.log_net_tx = (int)tmpI;

  if (config_setting_lookup_string(node_config, "phy_rx_log_file", &tmpS))
    strcpy(np.phy_rx_log_file, tmpS);

  if (config_setting_lookup_string(node_config, "phy_tx_log_file", &tmpS))
    strcpy(np.phy_tx_log_file, tmpS);

  if (config_setting_lookup_string(node_config, "net_rx_log_file", &tmpS))
    strcpy(np.net_rx_log_file, tmpS);

  if (config_setting_lookup_string(node_config, "net_tx_log_file", &tmpS))
    strcpy(np.net_tx_log_file, tmpS);

  if (config_setting_lookup_int(node_config, "generate_octave_logs", &tmpI))
    np.generate_octave_logs = (int)tmpI;

  if (config_setting_lookup_int(node_config, "generate_python_logs", &tmpI))
    np.generate_python_logs = (int)tmpI;

  if (config_setting_lookup_float(node_config, "ce_timeout_ms", &tmpD))
    np.ce_timeout_ms = tmpD;

  if (config_setting_lookup_float(node_config, "net_mean_throughput", &tmpD)) {
    np.net_mean_throughput = tmpD;
  } else
    np.net_mean_throughput = 1e3;

  // look up network traffic type
  np.net_burst_length = 1;
  if (config_setting_lookup_string(node_config, "net_traffic_type", &tmpS)) {
    if (!strcmp(tmpS, "stream")) {
      np.net_traffic_type = NET_TRAFFIC_STREAM;
    } else if (!strcmp(tmpS, "burst")) {
      np.net_traffic_type = NET_TRAFFIC_BURST;
      // look up the burst length if traffic type is burst
      if (config_setting_lookup_int(node_config, "net_burst_length", &tmpI))
        np.net_burst_length = tmpI;
    } else if (!strcmp(tmpS, "poisson"))
      np.net_traffic_type = NET_TRAFFIC_POISSON;
    else
      np.net_traffic_type = NET_TRAFFIC_STREAM;
  }

  if (config_setting_lookup_float(node_config, "rx_freq", &tmpD))
    np.rx_freq = tmpD;

  if (config_setting_lookup_float(node_config, "rx_rate", &tmpD))
    np.rx_rate = tmpD;
  else
    np.rx_rate = 1e6;

  if (config_setting_lookup_float(node_config, "rx_gain", &tmpD))
    np.rx_gain = tmpD;
  else
    np.rx_gain = 20.0;

  if (config_setting_lookup_int(node_config, "rx_subcarriers", &tmpI))
    np.rx_subcarriers = (int)tmpI;
  else
    np.rx_subcarriers = 64;

  if (config_setting_lookup_string(node_config, "rx_subcarrier_alloc_method",
                                   &tmpS)) {
    // subcarrier allocation is being defined in a standard way
    if (!strcmp(tmpS, "standard")) {
      np.rx_subcarrier_alloc_method = STANDARD_SUBCARRIER_ALLOC;

      int rx_guard_subcarriers;
      int rx_central_nulls;
      int rx_pilot_freq;
      if (config_setting_lookup_int(node_config, "rx_guard_subcarriers", &tmpI))
        rx_guard_subcarriers = tmpI;

      if (config_setting_lookup_int(node_config, "rx_central_nulls", &tmpI))
        rx_central_nulls = tmpI;

      if (config_setting_lookup_int(node_config, "rx_pilot_freq", &tmpI))
        rx_pilot_freq = tmpI;

      for (int i = 0; i < np.rx_subcarriers; i++) {
        // central band nulls
        if (i < rx_central_nulls / 2 ||
            np.rx_subcarriers - i - 1 < rx_central_nulls / 2)
          np.rx_subcarrier_alloc[i] = OFDMFRAME_SCTYPE_NULL;
        // guard band nulls
        else if (i + 1 > np.rx_subcarriers / 2 - rx_guard_subcarriers &&
                 i < np.rx_subcarriers / 2 + rx_guard_subcarriers)
          np.rx_subcarrier_alloc[i] = OFDMFRAME_SCTYPE_NULL;
        // pilot subcarriers (based on distance from center)
        else if (abs((int)((float)np.rx_subcarriers / 2.0 - (float)i - 0.5)) %
                     rx_pilot_freq ==
                 0)
          np.rx_subcarrier_alloc[i] = OFDMFRAME_SCTYPE_PILOT;
        // data subcarriers
        else
          np.rx_subcarrier_alloc[i] = OFDMFRAME_SCTYPE_DATA;
      }
    }

    // subcarrier allocation is completely custom
    else if (!strcmp(tmpS, "custom")) {
      np.rx_subcarrier_alloc_method = CUSTOM_SUBCARRIER_ALLOC;
      config_setting_t *rx_subcarrier_alloc =
          config_setting_get_member(node_config, "rx_subcarrier_alloc");

      char type_str[9] = "sc_type_";
      char num_str[8] = "sc_num_";
      char sc_type[16];
      char sc_num[16];
      int i = 1;
      int j = 0;
      int offset = np.rx_subcarriers / 2;
      sprintf(sc_type, "%s%d", type_str, i);
      // read in a custom initial subcarrier allocation
      while (
          config_setting_lookup_string(rx_subcarrier_alloc, sc_type, &tmpS)) {
        // read the number of subcarriers into tmpI
        sprintf(sc_num, "%s%d", num_str, i);
        tmpI = 1;
        config_setting_lookup_int(rx_subcarrier_alloc, sc_num, &tmpI);
        // set the subcarrier type based on the number specified
        if (!strcmp(tmpS, "null")) {
          for (int k = 0; k < tmpI; k++) {
            if (j >= (np.rx_subcarriers) / 2)
              offset = -(np.rx_subcarriers / 2);
            np.rx_subcarrier_alloc[j + offset] = OFDMFRAME_SCTYPE_NULL;
            j++;
          }
        }
        if (!strcmp(tmpS, "pilot")) {
          for (int k = 0; k < tmpI; k++) {
            if (j >= (np.rx_subcarriers) / 2)
              offset = -(np.rx_subcarriers / 2);
            np.rx_subcarrier_alloc[j + offset] = OFDMFRAME_SCTYPE_PILOT;
            j++;
          }
        }
        if (!strcmp(tmpS, "data")) {
          for (int k = 0; k < tmpI; k++) {
            if (j >= (np.rx_subcarriers) / 2)
              offset = -(np.rx_subcarriers / 2);
            np.rx_subcarrier_alloc[j + offset] = OFDMFRAME_SCTYPE_DATA;
            j++;
          }
        }
        if (j > 2048) {
          printf("The number of subcarriers specified was too high!\n");
          exit(EXIT_FAILURE);
        }
        i++;
        sprintf(sc_type, "%s%d", type_str, i);
      }
    } else
      np.rx_subcarrier_alloc_method = LIQUID_DEFAULT_SUBCARRIER_ALLOC;
  }

  if (config_setting_lookup_int(node_config, "rx_cp_len", &tmpI))
    np.rx_cp_len = (int)tmpI;
  else
    np.rx_cp_len = 16;

  if (config_setting_lookup_int(node_config, "rx_taper_len", &tmpI))
    np.rx_taper_len = (int)tmpI;
  else
    np.rx_taper_len = 4;

  if (config_setting_lookup_float(node_config, "tx_freq", &tmpD))
    np.tx_freq = tmpD;

  if (config_setting_lookup_float(node_config, "tx_rate", &tmpD))
    np.tx_rate = tmpD;
  else
    np.rx_rate = 1e6;

  if (config_setting_lookup_float(node_config, "tx_gain_soft", &tmpD))
    np.tx_gain_soft = tmpD;
  else
    np.tx_gain_soft = -12.0;

  if (config_setting_lookup_float(node_config, "tx_gain", &tmpD))
    np.tx_gain = tmpD;
  else
    np.tx_gain = 20.0;

  if (config_setting_lookup_int(node_config, "tx_subcarriers", &tmpI))
    np.tx_subcarriers = (int)tmpI;
  else
    np.tx_subcarriers = 64;

  if (config_setting_lookup_string(node_config, "tx_subcarrier_alloc_method",
                                   &tmpS)) {
    // subcarrier allocation is being defined in a standard way
    if (!strcmp(tmpS, "standard")) {
      np.tx_subcarrier_alloc_method = STANDARD_SUBCARRIER_ALLOC;

      int tx_guard_subcarriers;
      int tx_central_nulls;
      int tx_pilot_freq;
      if (config_setting_lookup_int(node_config, "tx_guard_subcarriers", &tmpI))
        tx_guard_subcarriers = tmpI;

      if (config_setting_lookup_int(node_config, "tx_central_nulls", &tmpI))
        tx_central_nulls = tmpI;

      if (config_setting_lookup_int(node_config, "tx_pilot_freq", &tmpI))
        tx_pilot_freq = tmpI;

      for (int i = 0; i < np.tx_subcarriers; i++) {
        // central band nulls
        if (i < tx_central_nulls / 2 ||
            np.tx_subcarriers - i - 1 < tx_central_nulls / 2)
          np.tx_subcarrier_alloc[i] = OFDMFRAME_SCTYPE_NULL;
        // guard band nulls
        else if (i + 1 > np.tx_subcarriers / 2 - tx_guard_subcarriers &&
                 i < np.tx_subcarriers / 2 + tx_guard_subcarriers)
          np.tx_subcarrier_alloc[i] = OFDMFRAME_SCTYPE_NULL;
        // pilot subcarriers
        else if (abs((int)((float)np.tx_subcarriers / 2.0 - (float)i - 0.5)) %
                     tx_pilot_freq ==
                 0)
          np.tx_subcarrier_alloc[i] = OFDMFRAME_SCTYPE_PILOT;
        // data subcarriers
        else
          np.tx_subcarrier_alloc[i] = OFDMFRAME_SCTYPE_DATA;
      }
    }

    // subcarrier allocation is completely custom
    else if (!strcmp(tmpS, "custom")) {
      np.tx_subcarrier_alloc_method = CUSTOM_SUBCARRIER_ALLOC;
      config_setting_t *tx_subcarrier_alloc =
          config_setting_get_member(node_config, "tx_subcarrier_alloc");

      char type_str[9] = "sc_type_";
      char num_str[8] = "sc_num_";
      char sc_type[16];
      char sc_num[16];
      int i = 1;
      int j = 0;
      int offset = np.tx_subcarriers / 2;
      sprintf(sc_type, "%s%d", type_str, i);
      // read in a custom initial subcarrier allocation
      while (
          config_setting_lookup_string(tx_subcarrier_alloc, sc_type, &tmpS)) {
        // read the number of subcarriers into tmpI
        sprintf(sc_num, "%s%d", num_str, i);
        tmpI = 1;
        config_setting_lookup_int(tx_subcarrier_alloc, sc_num, &tmpI);
        // set the subcarrier type based on the number specified
        if (!strcmp(tmpS, "null")) {
          for (int k = 0; k < tmpI; k++) {
            if (j >= (np.tx_subcarriers) / 2)
              offset = -(np.tx_subcarriers / 2);
            np.tx_subcarrier_alloc[j + offset] = OFDMFRAME_SCTYPE_NULL;
            j++;
          }
        }
        if (!strcmp(tmpS, "pilot")) {
          for (int k = 0; k < tmpI; k++) {
            if (j >= (np.tx_subcarriers) / 2)
              offset = -(np.tx_subcarriers / 2);
            np.tx_subcarrier_alloc[j + offset] = OFDMFRAME_SCTYPE_PILOT;
            j++;
          }
        }
        if (!strcmp(tmpS, "data")) {
          for (int k = 0; k < tmpI; k++) {
            if (j >= (np.tx_subcarriers) / 2)
              offset = -(np.tx_subcarriers / 2);
            np.tx_subcarrier_alloc[j + offset] = OFDMFRAME_SCTYPE_DATA;
            j++;
          }
        }
        if (j > 2048) {
          printf("The number of subcarriers specified was too high!\n");
          exit(EXIT_FAILURE);
        }
        i++;
        sprintf(sc_type, "%s%d", type_str, i);
      }
    } else
      np.tx_subcarrier_alloc_method = LIQUID_DEFAULT_SUBCARRIER_ALLOC;
  }

  if (config_setting_lookup_int(node_config, "tx_cp_len", &tmpI))
    np.tx_cp_len = (int)tmpI;
  else
    np.tx_cp_len = 16;

  if (config_setting_lookup_int(node_config, "tx_taper_len", &tmpI))
    np.tx_taper_len = (int)tmpI;
  else
    np.tx_taper_len = 4;

  // default tx modulation is BPSK
  np.tx_modulation = LIQUID_MODEM_BPSK;
  if (config_setting_lookup_string(node_config, "tx_modulation", &tmpS)) {

    // Iterate through every liquid modulation scheme
    // and if the string matches, then assign that scheme.
    // See liquid soruce: src/modem/src/modem_utilities.c
    // for definition of modulation_types
    for (int k = 0; k < LIQUID_MODEM_NUM_SCHEMES; k++) {
      if (!strcmp(tmpS, modulation_types[k].name))
        np.tx_modulation = modulation_types[k].scheme;
    }
  }

  // default tx CRC32
  np.tx_crc = LIQUID_CRC_32;
  if (config_setting_lookup_string(node_config, "tx_crc", &tmpS)) {

    // Iterate through every liquid CRC
    // and if the string matches, then assign that CRC.
    // See liquid soruce: src/fec/src/crc.c
    // for definition of crc_scheme_str
    for (int k = 0; k < LIQUID_CRC_NUM_SCHEMES; k++) {
      if (!strcmp(tmpS, crc_scheme_str[k][0]))
        np.tx_crc = k;
    }
  }

  // default tx FEC0 is Hamming 12/8
  np.tx_fec0 = LIQUID_FEC_HAMMING128;
  if (config_setting_lookup_string(node_config, "tx_fec0", &tmpS)) {

    // Iterate through every liquid FEC
    // and if the string matches, then assign that FEC.
    // See liquid soruce: src/fec/src/fec.c
    // for definition of fec_scheme_str
    for (int k = 0; k < LIQUID_FEC_NUM_SCHEMES; k++) {
      if (!strcmp(tmpS, fec_scheme_str[k][0]))
        np.tx_fec0 = k;
    }
  }

  // default rx FEC1 is none
  np.tx_fec1 = LIQUID_FEC_NONE;
  if (config_setting_lookup_string(node_config, "tx_fec1", &tmpS)) {

    // Iterate through every liquid FEC
    // and if the string matches, then assign that FEC.
    // See liquid soruce: src/fec/src/fec.c
    // for definition of fec_scheme_str
    for (int k = 0; k < LIQUID_FEC_NUM_SCHEMES; k++) {
      if (!strcmp(tmpS, fec_scheme_str[k][0]))
        np.tx_fec1 = k;
    }
  }

  if (config_setting_lookup_string(node_config, "interference_type", &tmpS)) {
    if (!strcmp(tmpS, "CW"))
      np.interference_type = CW;
    if (!strcmp(tmpS, "NOISE"))
      np.interference_type = NOISE;
    if (!strcmp(tmpS, "GMSK"))
      np.interference_type = GMSK;
    if (!strcmp(tmpS, "RRC"))
      np.interference_type = RRC;
    if (!strcmp(tmpS, "OFDM"))
      np.interference_type = OFDM;
    if (!strcmp(tmpS, "AWGN"))
      np.interference_type = AWGN; 
  }

  if (config_setting_lookup_float(node_config, "period", &tmpD))
    np.period = tmpD;

  if (config_setting_lookup_float(node_config, "duty_cycle", &tmpD))
    np.duty_cycle = tmpD;

  // ======================================================
  // process frequency hopping parameters
  // ======================================================
  np.tx_freq_behavior = FIXED;
  if (config_setting_lookup_string(node_config, "tx_freq_behavior", &tmpS)) {
    if (!strcmp(tmpS, "FIXED"))
      np.tx_freq_behavior = FIXED;
    if (!strcmp(tmpS, "SWEEP"))
      np.tx_freq_behavior = SWEEP;
    if (!strcmp(tmpS, "RANDOM"))
      np.tx_freq_behavior = RANDOM;
  }

  if (config_setting_lookup_float(node_config, "tx_freq_min", &tmpD))
    np.tx_freq_min = tmpD;

  if (config_setting_lookup_float(node_config, "tx_freq_max", &tmpD))
    np.tx_freq_max = tmpD;

  if (config_setting_lookup_float(node_config, "tx_freq_dwell_time", &tmpD))
    np.tx_freq_dwell_time = tmpD;

  if (config_setting_lookup_float(node_config, "tx_freq_resolution", &tmpD))
    np.tx_freq_resolution = tmpD;

  // Read CE arguments (A getopt style string for sending custom paramters to the CE)
  if (config_setting_lookup_string(node_config, "ce_args", &tmpS))
    strcpy(np.ce_args, tmpS);
  else
    np.ce_args[0] = '\0';

  return np;
}

//////////////////////////////////////////////////////////////////
// Node parameters
//////////////////////////////////////////////////////////////////

void print_node_parameters(struct node_parameters *np) {
  printf("\n");
  printf("--------------------------------------------------------------\n");
  printf("-                    node parameters                         -\n");
  printf("--------------------------------------------------------------\n");
  printf("General Settings:\n");
  char node_type[15] = "UNKNOWN";
  if (np->node_type == COGNITIVE_RADIO) {
    strcpy(node_type, "Cognitive Radio");
  } else if (np->node_type == INTERFERER) {
    strcpy(node_type, "Interferer");
  }

  printf("    Node type:                         %-s\n", node_type);
  if (np->node_type == COGNITIVE_RADIO) {
    char cr_type[15] = "ECR";
    if (np->cognitive_radio_type == PYTHON)
      strcpy(cr_type, "python");
    printf("    Cognitive Radio type:            %-s\n", cr_type);
  }
  printf("    Server IP:                         %-s\n", np->server_ip);
  //
  if (np->node_type != INTERFERER) {
    printf("\nVirtual Network Interface Settings:\n");
    printf("    CRTS IP:                           %-s\n", np->crts_ip);
    printf("    Target IP:                         %-s\n", np->target_ip);
    char traffic[15];
    switch (np->net_traffic_type) {
    case NET_TRAFFIC_STREAM:
      strcpy(traffic, "stream");
      break;
    case NET_TRAFFIC_BURST:
      strcpy(traffic, "burst");
      break;
    case NET_TRAFFIC_POISSON:
      strcpy(traffic, "poisson");
      break;
    }
    printf("    Traffic pattern:                   %-s\n", traffic);
    printf("    Average throughput:                %-.2f\n",
           np->net_mean_throughput);
    if (np->net_traffic_type == NET_TRAFFIC_BURST)
      printf("    Burst length:                      %i\n",
             np->net_burst_length);
    //
    printf("\nCognitive Engine Settings:\n");
    printf("    Cognitive Engine:                  %-s\n", np->cognitive_engine);
    printf("    CE timeout:                        %-.2f\n", np->ce_timeout_ms);
  }
  //
  printf("\nLog/Report Settings:\n");
  if (np->node_type != INTERFERER)
    printf("    PHY Rx log file:                   %-s\n", np->phy_rx_log_file);
  printf("    PHY Tx log file:                   %-s\n", np->phy_tx_log_file);
  if (np->node_type != INTERFERER)
    printf("    NET Rx log file:                   %-s\n", np->net_rx_log_file);
  if (np->node_type != INTERFERER)
    printf("    NET Tx log file:                   %-s\n", np->net_tx_log_file);
  printf("    Generate octave logs:              %i\n",
         np->generate_octave_logs);
  printf("    Generate python logs:              %i\n",
         np->generate_python_logs);
  //
  printf("\nInitial USRP Settings:\n");
  if (np->node_type != INTERFERER) {
    printf("    Receive frequency:                 %-.2e\n", np->rx_freq);
    printf("    Receive rate:                      %-.2e\n", np->rx_rate);
    printf("    Receive gain:                      %-.2f\n", np->rx_gain);
  }
  printf("    Transmit frequency:                %-.2e\n", np->tx_freq);
  printf("    Transmit rate:                     %-.2e\n", np->tx_rate);
  printf("    Transmit gain:                     %-.2f\n", np->tx_gain);
  //
  if (np->node_type != INTERFERER && np->cognitive_radio_type == EXTENSIBLE_COGNITIVE_RADIO) {
    printf("\nInitial Liquid OFDM Settings\n");
    printf("    Receive subcarriers:               %i\n", np->tx_subcarriers);
    printf("    Receive cyclic prefix length:      %i\n", np->tx_cp_len);
    printf("    Receive taper length:              %i\n", np->tx_taper_len);
    printf("    Transmit subcarriers:              %i\n", np->tx_subcarriers);
    printf("    Transmit cyclic prefix length:     %i\n", np->tx_cp_len);
    printf("    Transmit taper length:             %i\n", np->tx_taper_len);
    printf("    Transmit soft gain:                %-.2f\n", np->tx_gain_soft);
    printf("    Transmit modulation:               %s\n",
           modulation_types[np->tx_modulation].name);
    printf("    Transmit CRC:                      %s\n",
           crc_scheme_str[np->tx_crc][0]);
    printf("    Transmit FEC0:                     %s\n",
           fec_scheme_str[np->tx_fec0][0]);
    printf("    Transmit FEC1:                     %s\n",
           fec_scheme_str[np->tx_fec1][0]);
  }

  if (np->node_type == INTERFERER) {
    printf("\nInitial Interference Settings:\n");
    char interference_type[5] = "NONE";
    char tx_freq_behavior[6] = "NONE";
    switch (np->interference_type) {
    case (CW):
      strcpy(interference_type, "CW");
      break;
    case (NOISE):
      strcpy(interference_type, "NOISE");
      break;
    case (GMSK):
      strcpy(interference_type, "GMSK");
      break;
    case (RRC):
      strcpy(interference_type, "RRC");
      break;
    case (OFDM):
      strcpy(interference_type, "OFDM");
      break;
    case (AWGN):
      strcpy(interference_type, "AWGN");
      break;
    }
    switch (np->tx_freq_behavior) {
    case (FIXED):
      strcpy(tx_freq_behavior, "FIXED");
      break;
    case (SWEEP):
      strcpy(tx_freq_behavior, "SWEEP");
      break;
    case (RANDOM):
      strcpy(tx_freq_behavior, "RANDOM");
      break;
    }
    printf("    Interference type:                 %-s\n", interference_type);
    printf("    Interference period:               %-.2f\n", np->period);
    printf("    Interference duty cycle:           %-.2f\n", np->duty_cycle);
    printf("\n");
    printf("    tx freq behavior:                  %-s\n", tx_freq_behavior);
    printf("    tx freq min:                       %-.2e\n", np->tx_freq_min);
    printf("    tx freq max:                       %-.2e\n", np->tx_freq_max);
    printf("    tx freq dwell time:                %-.2f\n",
           np->tx_freq_dwell_time);
    printf("    tx freq resolution:                %-.2e\n",
           np->tx_freq_resolution);

    printf("\n");
  }
  printf("--------------------------------------------------------------\n");
}

//////////////////////////////////////////////////////////////////
// Control and feedback
//////////////////////////////////////////////////////////////////

int get_control_arg_len(int control_type){
  
  int len;
  switch(control_type){
    case CRTS_TX_STATE:
    case CRTS_TX_MOD:
    case CRTS_TX_FEC0:
    case CRTS_TX_FEC1:
    case CRTS_RX_STATE:
    case CRTS_NET_MODEL:
    case CRTS_FB_EN:
    case CRTS_TX_FREQ_BEHAVIOR:
      len = sizeof(int);
      break;
    case CRTS_TX_FREQ:
    case CRTS_TX_RATE:
    case CRTS_TX_GAIN:
    case CRTS_RX_FREQ:
    case CRTS_RX_RATE:
    case CRTS_RX_GAIN:
    case CRTS_RX_STATS:
    case CRTS_RX_STATS_FB:
    case CRTS_NET_THROUGHPUT:
    case CRTS_TX_DUTY_CYCLE:
    case CRTS_TX_PERIOD:
    case CRTS_TX_FREQ_MIN:
    case CRTS_TX_FREQ_MAX:
    case CRTS_TX_FREQ_DWELL_TIME:
    case CRTS_TX_FREQ_RES:
      len = sizeof(double);
      break;
    case CRTS_RX_STATS_RESET:
    default:
      len = 0;
  }

  return len;
}

int get_feedback_arg_len(int fb_type){
  
  int len;
  switch(fb_type){
    case CRTS_TX_STATE:
    case CRTS_TX_MOD:
    case CRTS_TX_FEC0:
    case CRTS_TX_FEC1:
    case CRTS_RX_STATE:
      len = sizeof(int);
      break;
    case CRTS_TX_FREQ:
    case CRTS_TX_RATE:
    case CRTS_TX_GAIN:
    case CRTS_RX_FREQ:
    case CRTS_RX_RATE:
    case CRTS_RX_GAIN:
      len = sizeof(double);
      break;
    case CRTS_RX_STATS:
      len = sizeof(struct ExtensibleCognitiveRadio::rx_statistics);
      break;
    default:
      len = 0;
  }

  return len;
}