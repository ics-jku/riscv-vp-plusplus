#include <boost/program_options.hpp>
#include <systemc>

#include "iss.h"
#include "memory.h"
#include "platform/common/options.h"

using namespace rv32;
namespace po = boost::program_options;

class GD32Options : public Options {
   public:
	typedef unsigned int addr_t;

	addr_t flash_size = 1024 * 128;  // 128 KB flash
	addr_t flash_start_addr = 0x08000000;
	addr_t flash_end_addr = flash_start_addr + flash_size - 1;

	addr_t sram_size = 1024 * 32;  // 32 KB sram
	addr_t sram_start_addr = 0x20000000;
	addr_t sram_end_addr = sram_start_addr + sram_size - 1;

	GD32Options(void) {}
};

int sc_main(int argc, char **argv) {
	GD32Options opt;
	opt.parse(argc, argv);

	std::srand(std::time(nullptr));  // use current time as seed for random generator

	tlm::tlm_global_quantum::instance().set(sc_core::sc_time(opt.tlm_global_quantum, sc_core::SC_NS));

	ISS core(0);
	SimpleMemory sram("SRAM", opt.sram_size);
	SimpleMemory flash("Flash", opt.flash_size);

	return 0;
}