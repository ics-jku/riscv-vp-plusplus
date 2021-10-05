#ifndef RISCV_VP_OPTIONS_H
#define RISCV_VP_OPTIONS_H

#include <boost/program_options.hpp>

class Options : public boost::program_options::options_description {
public:
	Options(void);
	virtual ~Options();
	virtual void parse(int argc, char **argv);

	std::string input_program;

	bool intercept_syscalls = false;
	bool use_debug_runner = false;
	unsigned int debug_port = 5005;
	bool trace_mode = false;
	unsigned int tlm_global_quantum = 10;
	bool use_instr_dmi = false;
	bool use_data_dmi = false;

	friend std::ostream& operator<<(std::ostream& os, const Options& o);

private:

	boost::program_options::positional_options_description pos;
	boost::program_options::variables_map vm;
};

std::ostream& operator<<(std::ostream& os, const Options& o);

#endif
