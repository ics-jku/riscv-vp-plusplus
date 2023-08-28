#ifndef RISCV_GDB_RUNNER
#define RISCV_GDB_RUNNER

#include <systemc>

#include "debug.h"
#include "gdb_server.h"

SC_MODULE(GDBServerRunner) {
   public:
	SC_HAS_PROCESS(GDBServerRunner);

	GDBServerRunner(sc_core::sc_module_name, GDBServer *, debug_target_if *);

   private:
	GDBServer *server;
	debug_target_if *hart;
	sc_core::sc_event run_event;
	sc_core::sc_event *stop_event;

	void run(void);
};

#endif
