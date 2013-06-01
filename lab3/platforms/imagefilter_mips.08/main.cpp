const char *project_name="mips1";
const char *project_file="mips1.ac";
const char *archc_version="2.0beta1";
const char *archc_options="-abi -dy ";

#include  <systemc.h>
#include  "mips1.H"
#include  "ac_tlm_mem.h"
#include  "ac_tlm_lock.h"
#include  "ac_tlm_filter.h"
#include  "ac_tlm_router.h"

#define NUM_PROC 8

using user::ac_tlm_mem;
using user::ac_tlm_lock;
using user::ac_tlm_filter;
using user::ac_tlm_router;

int sc_main(int ac, char *av[])
{

  //!  ISA simulator
  char names[NUM_PROC][8];
  mips1 *processors[NUM_PROC];
  for (int i = 0; i < NUM_PROC; i++) {
    sprintf(names[i], "mips1_%d", i);
    processors[i] = new mips1(names[i]);
  }
  ac_tlm_mem mem("mem");
  ac_tlm_lock lock("lock");
  ac_tlm_filter filter("filter");
  ac_tlm_router router("router");

#ifdef AC_DEBUG
  ac_trace("mips1_proc.trace");
#endif

  // Link ports
  for (int i = 0; i < NUM_PROC; i++) {
    processors[i]->DM_port(router.target_export);
  }
  router.mem_port(mem.target_export);
  router.lock_port(lock.target_export);
  router.filter_port(filter.target_export);

  // Replicate arguments
  char **argvs[NUM_PROC];
  for (int i = 0; i < NUM_PROC; i++) {
    argvs[i] = (char **)malloc(ac * sizeof(char *));
  }
  for (int j = 0; j < ac; j++) {
    int len = strlen(av[j]);
    for (int i = 0; i < NUM_PROC; i++) {
      argvs[i][j] = (char *)malloc((len + 1) * sizeof(char));
      strcpy(argvs[i][j], av[j]);
    }
  }

  // Init processors
  for (int i = 0; i < NUM_PROC; i++ ) {
    processors[i]->init(ac, argvs[i]);
    processors[i]->set_instr_batch_size(1);
  }
  cerr << endl;

  sc_start();

  for (int i = 0; i < NUM_PROC; i++) {
    processors[i]->PrintStat();
  }
  cerr << endl;

#ifdef AC_STATS
  for (int i = 0; i < NUM_PROC; i++) {
    processors[i]->ac_sim_stats.time = sc_simulation_time();
    processors[i]->ac_sim_stats.print();
  }
#endif

#ifdef AC_DEBUG
  ac_close_trace();
#endif

  // Free, free, free!
  for (int i = 0; i < NUM_PROC; i++) {
    for (int j = 0; j < ac; j++) {
      free(argvs[i][j]);
    }
    free(argvs[i]);
  }
  for (int i = 0; i < NUM_PROC; i++) {
    processors[i]->~mips1();
  }

  return processors[0]->ac_exit_status;
}
