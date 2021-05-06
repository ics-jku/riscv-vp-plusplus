#include <cstdlib>
#include <ctime>

#include "core/common/clint.h"
#include "elf_loader.h"
#include "debug_memory.h"
#include "iss.h"
#include "mem.h"
#include "memory.h"
#include "syscall.h"
#include "microrv32_uart.h"
#include "microrv32_led.h"
#include "microrv32_gpio.h"
#include "util/options.h"
#include "platform/common/options.h"

#include "gdb-mc/gdb_server.h"
#include "gdb-mc/gdb_runner.h"

#include <boost/io/ios_state.hpp>
#include <boost/program_options.hpp>
#include <iomanip>
#include <iostream>

using namespace rv32;
namespace po = boost::program_options;

class BasicOptions : public Options {
public:
	typedef unsigned int addr_t;

	addr_t clint_start_addr = 0x2000000;
	addr_t clint_end_addr = 0x200ffff;
	addr_t sys_start_addr = 0x02010000;
	addr_t sys_end_addr = 0x020103ff;
	addr_t mem_start_addr = 0x80000000;
	addr_t mem_end_addr = 0x80ffffff;
	addr_t led_start_addr = 0x81000000;
	addr_t led_end_addr = 0x810000ff;
	addr_t uart_start_addr = 0x82000000;
	addr_t uart_end_addr = 0x820000ff;
	addr_t gpio_a_start_addr = 0x83000000;
	addr_t gpio_a_end_addr = 0x830000ff;
	
	addr_t mem_size = mem_end_addr - mem_start_addr;

	bool use_E_base_isa = false;

	OptionValue<unsigned long> entry_point;

	BasicOptions(void) {
        	// clang-format off
		add_options()
			("use-E-base-isa", po::bool_switch(&use_E_base_isa), "use the E instead of the I integer base ISA")
			("entry-point", po::value<std::string>(&entry_point.option),"set entry point address (ISS program counter)");
        	// clang-format on
	}

	void parse(int argc, char **argv) override {
		Options::parse(argc, argv);

		entry_point.finalize(parse_ulong_option);
	}
};

int sc_main(int argc, char **argv) {
	BasicOptions opt;
	opt.parse(argc, argv);

	std::srand(std::time(nullptr));  // use current time as seed for random generator

	tlm::tlm_global_quantum::instance().set(sc_core::sc_time(opt.tlm_global_quantum, sc_core::SC_NS));

	ISS core(0, opt.use_E_base_isa);
	SimpleMemory mem("SimpleMemory", opt.mem_size);
	ELFLoader loader(opt.input_program.c_str());
	SimpleBus<2, 6> bus("SimpleBus");
	CombinedMemoryInterface iss_mem_if("MemoryInterface", core);
	SyscallHandler sys("SyscallHandler");
	CLINT<1> clint("CLINT");
	DebugMemoryInterface dbg_if("DebugMemoryInterface");
	MicroRV32UART uart("MicroRV32UART");
	MicroRV32LED led("MicroRV32LED");
	MicroRV32GPIO gpio_a("MicroRV32GPIO");

	MemoryDMI dmi = MemoryDMI::create_start_size_mapping(mem.data, opt.mem_start_addr, mem.size);
	InstrMemoryProxy instr_mem(dmi, core);

	std::shared_ptr<BusLock> bus_lock = std::make_shared<BusLock>();
	iss_mem_if.bus_lock = bus_lock;

	instr_memory_if *instr_mem_if = &iss_mem_if;
	data_memory_if *data_mem_if = &iss_mem_if;
	if (opt.use_instr_dmi)
		instr_mem_if = &instr_mem;
	if (opt.use_data_dmi) {
		iss_mem_if.dmi_ranges.emplace_back(dmi);
	}

	uint64_t entry_point = loader.get_entrypoint();
	if (opt.entry_point.available)
		entry_point = opt.entry_point.value;

	loader.load_executable_image(mem, mem.size, opt.mem_start_addr);
	core.init(instr_mem_if, data_mem_if, &clint, entry_point, rv32_align_address(opt.mem_end_addr));
	sys.init(mem.data, opt.mem_start_addr, loader.get_heap_addr());
	sys.register_core(&core);

	if (opt.intercept_syscalls)
		core.sys = &sys;

	// address mapping
	bus.ports[0] = new PortMapping(opt.mem_start_addr, opt.mem_end_addr);
	bus.ports[1] = new PortMapping(opt.clint_start_addr, opt.clint_end_addr);
	bus.ports[2] = new PortMapping(opt.uart_start_addr, opt.uart_end_addr);
	bus.ports[3] = new PortMapping(opt.sys_start_addr, opt.sys_end_addr);
	bus.ports[4] = new PortMapping(opt.led_start_addr, opt.led_end_addr);
	bus.ports[5] = new PortMapping(opt.gpio_a_start_addr, opt.gpio_a_end_addr);

	// connect TLM sockets
	iss_mem_if.isock.bind(bus.tsocks[0]);
	dbg_if.isock.bind(bus.tsocks[1]);

	bus.isocks[0].bind(mem.tsock);
	bus.isocks[1].bind(clint.tsock);
	bus.isocks[2].bind(uart.tsock);
	bus.isocks[3].bind(sys.tsock);
	bus.isocks[4].bind(led.tsock);
	bus.isocks[5].bind(gpio_a.tsock);

	// connect interrupt signals/communication
	clint.target_harts[0] = &core;

	std::vector<debug_target_if *> threads;
	threads.push_back(&core);

	core.trace = opt.trace_mode;  // switch for printing instructions
	if (opt.use_debug_runner) {
		auto server = new GDBServer("GDBServer", threads, &dbg_if, opt.debug_port);
		new GDBServerRunner("GDBRunner", server, &core);
	} else {
		new DirectCoreRunner(core);
	}

	sc_core::sc_start();

	core.show();

	return 0;
}
