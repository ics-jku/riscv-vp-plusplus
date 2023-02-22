#include <cstdlib>
#include <ctime>

#include "core/common/clint.h"
#include "core/common/real_clint.h"
#include "elf_loader.h"
#include "fe310_plic.h"
#include "debug_memory.h"
#include "iss.h"
#include "mem.h"
#include "memory.h"
#include "syscall.h"
#include "util/options.h"
#include "platform/common/options.h"
#include "platform/common/terminal.h"
#include "virtual_bus_tlm_connector.hpp"

#include "gdb-mc/gdb_server.h"
#include "gdb-mc/gdb_runner.h"

#include <boost/io/ios_state.hpp>
#include <boost/program_options.hpp>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <termios.h>


using namespace rv32;
namespace po = boost::program_options;

class HwitlOptions : public Options {
public:
	typedef unsigned int addr_t;

	std::string virtual_bus_device;
	unsigned virtual_bus_baudrate = 0;
	std::string test_signature;
	bool use_real_clint = false;

	addr_t mem_size = 1024 * 1024 * 32;  // 32 MB ram, to place it before the CLINT and run the base examples (assume
	                                     // memory start at zero) without modifications
	addr_t mem_start_addr         = 0x00000000;
	addr_t mem_end_addr = mem_start_addr + mem_size - 1;
	addr_t clint_start_addr       = 0x02000000;
	addr_t clint_end_addr         = 0x0200ffff;
	addr_t sys_start_addr         = 0x02010000;
	addr_t sys_end_addr           = 0x020103ff;
	addr_t plic_start_addr        = 0x40000000;
	addr_t plic_end_addr          = 0x41000000;
	addr_t term_start_addr        = 0x20000000;
	addr_t term_end_addr          = term_start_addr + 16;
	addr_t virtual_bus_start_addr = 0x50000000;
	addr_t virtual_bus_end_addr   = 0x5FFFFFFF;

	OptionValue<unsigned long> entry_point;

	HwitlOptions(void) {
        	// clang-format off
		add_options()
			("use-real-clint", po::bool_switch(&use_real_clint),"Lock clint to wall-clock time")
			("memory-start", po::value<unsigned int>(&mem_start_addr),"set memory start address")
			("memory-size", po::value<unsigned int>(&mem_size), "set memory size")
			("entry-point", po::value<std::string>(&entry_point.option),"set entry point address (ISS program counter)")
			("virtual-bus-device",  po::value<std::string>(&virtual_bus_device)->required(),"tty to virtual bus responder")
			("virtual-bus-baudrate",  po::value<unsigned int>(&virtual_bus_baudrate),"If set, change baudrate of tty device")
			("virtual-device-start",  po::value<unsigned int>(&virtual_bus_start_addr),"start of virtual peripheral")
			("virtual-device-end",  po::value<unsigned int>(&virtual_bus_end_addr),"end of virtual peripheral");
        	// clang-format on
	}

	void parse(int argc, char **argv) override {
		Options::parse(argc, argv);
		entry_point.finalize(parse_ulong_option);
	}
};

std::ostream& operator<<(std::ostream& os, const HwitlOptions& o) {
	os << "virtual-bus-device:\t" << o.virtual_bus_device << std::endl;
	os << static_cast <const Options&>( o );
	return os;
}


int sc_main(int argc, char **argv) {
	HwitlOptions opt;
	opt.parse(argc, argv);

	std::srand(std::time(nullptr));  // use current time as seed for random generator

	tlm::tlm_global_quantum::instance().set(sc_core::sc_time(opt.tlm_global_quantum, sc_core::SC_NS));

	ISS core(0);
	SimpleMemory mem("SimpleMemory", opt.mem_size);
	SimpleTerminal term("SimpleTerminal");
	ELFLoader loader(opt.input_program.c_str());
	SimpleBus<2, 6> bus("SimpleBus");
	CombinedMemoryInterface iss_mem_if("MemoryInterface", core);
	SyscallHandler sys("SyscallHandler");
	FE310_PLIC<1, 64, 96, 32> plic("PLIC");
	DebugMemoryInterface dbg_if("DebugMemoryInterface");

	std::shared_ptr<CLINT<1>> sim_clint;
	std::shared_ptr<RealCLINT> real_clint;
	std::vector<clint_interrupt_target*> real_clint_targets {&core};
	clint_if* one_clint;
	if(opt.use_real_clint) {
		real_clint = std::make_shared<RealCLINT>("REAL_CLINT", real_clint_targets);
		one_clint = real_clint.get();
	} else {
		sim_clint = std::make_shared<CLINT<1>>("SIM_CLINT");
		one_clint = sim_clint.get();
	}


	int virtual_bus_device_handle = -1;
	virtual_bus_device_handle = open(opt.virtual_bus_device.c_str(), O_RDWR| O_NOCTTY);
	if(virtual_bus_device_handle < 0) {
		std::cerr << "[hwitl-vp] Device " << opt.virtual_bus_device << " could not be opened: "
				<< strerror(errno) << std::endl;
		return -1;
	}
	if(opt.virtual_bus_baudrate > 0) {
		if(!setBaudrate(virtual_bus_device_handle, opt.virtual_bus_baudrate)) {
			std::cerr << "[hwitl-vp] WARN: Could not set baudrate of " << opt.virtual_bus_baudrate << "!" << std::endl;
		}
	}
	setTTYRawmode(virtual_bus_device_handle);	// ignore return

	Initiator virtual_bus_connector(virtual_bus_device_handle);
	VirtualBusMember virtual_bus_member("virtual_bus_member", virtual_bus_connector, opt.virtual_bus_start_addr);
	virtual_bus_member.setInterruptRoutine([&plic](){plic.gateway_trigger_interrupt(2);});

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
	try {
		loader.load_executable_image(mem, mem.size, opt.mem_start_addr);
	} catch(ELFLoader::load_executable_exception& e) {
		std::cerr << e.what() << std::endl;
		std::cerr << "Memory map: " << std::endl;
		std::cerr << opt << std::endl;
		return -1;
	}
	/*
	 * The rv32 calling convention defaults to 32 bit, but as this Config is
	 * mainly used together with the syscall handler, this helps for certain floats.
	 * https://github.com/riscv-non-isa/riscv-elf-psabi-doc/blob/master/riscv-elf.adoc
	 */
	core.init(instr_mem_if, data_mem_if, one_clint, entry_point, rv64_align_address(opt.mem_end_addr));
	sys.init(mem.data, opt.mem_start_addr, loader.get_heap_addr());
	sys.register_core(&core);

	if (opt.intercept_syscalls)
		core.sys = &sys;

	// address mapping
	{
		unsigned it = 0;
		bus.ports[it++] = new PortMapping(opt.mem_start_addr, opt.mem_end_addr);
		bus.ports[it++] = new PortMapping(opt.clint_start_addr, opt.clint_end_addr);
		bus.ports[it++] = new PortMapping(opt.plic_start_addr, opt.plic_end_addr);
		bus.ports[it++] = new PortMapping(opt.term_start_addr, opt.term_end_addr);
		bus.ports[it++] = new PortMapping(opt.sys_start_addr, opt.sys_end_addr);
		bus.ports[it++] = new PortMapping(opt.virtual_bus_start_addr, opt.virtual_bus_end_addr);
	}

	// connect TLM sockets
	iss_mem_if.isock.bind(bus.tsocks[0]);
	dbg_if.isock.bind(bus.tsocks[1]);

	{
		unsigned it = 0;
		bus.isocks[it++].bind(mem.tsock);
		if(opt.use_real_clint)
			bus.isocks[it++].bind(real_clint->tsock);
		else
			bus.isocks[it++].bind(sim_clint->tsock);
		bus.isocks[it++].bind(plic.tsock);
		bus.isocks[it++].bind(term.tsock);
		bus.isocks[it++].bind(sys.tsock);
		bus.isocks[it++].bind(virtual_bus_member.tsock);
	}

	// connect interrupt signals/communication
	plic.target_harts[0] = &core;
	if(sim_clint)
		sim_clint->target_harts[0] = &core;

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

	if (opt.test_signature != "") {
		auto begin_sig = loader.get_begin_signature_address();
		auto end_sig = loader.get_end_signature_address();

		{
			std::cout << std::hex;
			std::cout << "begin_signature: " << begin_sig << std::endl;
			std::cout << "end_signature: " << end_sig << std::endl;
			std::cout << "signature output file: " << opt.test_signature << std::endl;
			std::cout << std::dec;
		}

		assert(end_sig >= begin_sig);
		assert(begin_sig >= opt.mem_start_addr);

		auto begin = begin_sig - opt.mem_start_addr;
		auto end = end_sig - opt.mem_start_addr;

		std::ofstream sigfile(opt.test_signature, std::ios::out);

		auto n = begin;
		while (n < end) {
			sigfile << std::hex << std::setw(2) << std::setfill('0') << (unsigned)mem.data[n];
			++n;
		}
	}

	return 0;
}
