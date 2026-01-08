#ifndef RISCV_VP_OPTIONS_H
#define RISCV_VP_OPTIONS_H

#include <boost/program_options.hpp>
#include <iostream>

class Options : public boost::program_options::options_description {
   public:
	Options(void);
	virtual ~Options();
	virtual void parse(int argc, char** argv);

	std::string input_program;

	bool use_E_base_isa = false;
	bool en_ext_Zfh = false;
	bool intercept_syscalls = false;
	bool error_on_zero_traphandler = false;
	bool use_debug_runner = false;
	unsigned int debug_port = 5005;
	bool debug_cont_sim_on_wait = false;
	bool trace_mode = false;
	unsigned int tlm_global_quantum = 10;
	bool use_dbbcache = false;
	bool use_lscache = false;
	bool use_instr_dmi = false;
	bool use_data_dmi = false;
	bool use_debug_bus = false;
	unsigned int debug_bus_port = 5006;
	bool break_on_transaction = false;

	std::string property_tree_file;
	bool property_tree_export = false;
	bool property_tree_is_loaded = false;

	virtual void printValues(std::ostream& os = std::cout) const;

	void handle_property_export_and_exit();

   private:
	boost::program_options::positional_options_description pos;
	boost::program_options::variables_map vm;
};

#endif
