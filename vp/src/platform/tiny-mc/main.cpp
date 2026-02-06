/* if not defined externally fall back to TARGET_RV32 */
#if !defined(TARGET_RV32) && !defined(TARGET_RV64)
#define TARGET_RV32
#endif

#include <boost/io/ios_state.hpp>
#include <boost/program_options.hpp>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <iostream>

#include "core/common/clint.h"
#include "core/common/gdb-mc/gdb_runner.h"
#include "core/common/gdb-mc/gdb_server.h"

/*
 * It should be possible to remove the ifdefs here and include all files
 * without any conflicts, and indeed: If we remove the ifdefs we get no
 * compilation errors. However, when we start the resulting VP (especially
 * with CHERI) we get a lot of errors like:
 * [ISS] Error: Multiple implementations for operation 955 (AMOSWAP_C)
 * -> TODO: find/fix cause and remove ifdefs (not critical)
 */
#if defined(TARGET_RV32)
#include "core/rv32/elf_loader.h"
#include "core/rv32/iss.h"
#include "core/rv32/mem.h"
#include "core/rv32/mmu.h"
#include "core/rv32/syscall.h"
#elif defined(TARGET_RV64)
#include "core/rv64/elf_loader.h"
#include "core/rv64/iss.h"
#include "core/rv64/mem.h"
#include "core/rv64/mmu.h"
#include "core/rv64/syscall.h"
#endif

#include "platform/common/memory.h"
#include "platform/common/options.h"
#include "util/propertytree.h"

#if defined(TARGET_RV32)
using namespace rv32;
#elif defined(TARGET_RV64)
using namespace rv64;
#endif

namespace po = boost::program_options;

struct TinyOptions : public Options {
   public:
	typedef uint64_t addr_t;

	addr_t mem_size = 1024 * 1024 * 32;  // 32 MB ram, to place it before the CLINT and run the base examples (assume
	                                     // memory start at zero) without modifications
	addr_t mem_start_addr = 0x00000000;
	addr_t mem_end_addr = mem_start_addr + mem_size - 1;
	addr_t clint_start_addr = 0x02000000;
	addr_t clint_end_addr = 0x0200ffff;
	addr_t sys_start_addr = 0x02010000;
	addr_t sys_end_addr = 0x020103ff;

	bool quiet = false;
	bool use_E_base_isa = false;

	TinyOptions(void) {
		// clang-format off
		add_options()
			("quiet", po::bool_switch(&quiet), "do not output register values on exit")
			("memory-start", po::value<uint64_t>(&mem_start_addr), "set memory start address")
			("memory-size", po::value<uint64_t>(&mem_size), "set memory size")
			("use-E-base-isa", po::bool_switch(&use_E_base_isa), "use the E instead of the I integer base ISA");
		// clang-format on
	}

	void parse(int argc, char **argv) override {
		Options::parse(argc, argv);
		mem_end_addr = mem_start_addr + mem_size - 1;
	}
};

int sc_main(int argc, char **argv) {
	// PropertyTree::global()->set_debug(true);

	TinyOptions opt;
	opt.parse(argc, argv);

	std::srand(std::time(nullptr));  // use current time as seed for random generator

	if (!opt.property_tree_is_loaded) {
		/*
		 * property tree was not loaded by Options -> use default model properties
		 * and values
		 */

		/* set global clock explicitly to 100 MHz */
		VPPP_PROPERTY_SET("", "clock_cycle_period", sc_core::sc_time, sc_core::sc_time(10, sc_core::SC_NS));
	}

	tlm::tlm_global_quantum::instance().set(sc_core::sc_time(opt.tlm_global_quantum, sc_core::SC_NS));

	RV_ISA_Config isa_config(opt.use_E_base_isa, opt.en_ext_Zfh);
	ISS core0(&isa_config, 0);
	ISS core1(&isa_config, 1);
	MMU mmu0(core0);
	MMU mmu1(core1);

	CombinedMemoryInterface core0_mem_if("MemoryInterface0", core0, &mmu0);
	CombinedMemoryInterface core1_mem_if("MemoryInterface1", core1, &mmu1);

	SimpleMemory mem("SimpleMemory", opt.mem_size);
	ELFLoader loader(opt.input_program.c_str());
	NetTrace *debug_bus = nullptr;
	if (opt.use_debug_bus) {
		debug_bus = new NetTrace(opt.debug_bus_port);
	}
	SimpleBus<3, 3> bus("SimpleBus", debug_bus, opt.break_on_transaction);
	SyscallHandler sys("SyscallHandler");
	DebugMemoryInterface dbg_if("DebugMemoryInterface");

	CLINT<2> clint("CLINT");

	std::shared_ptr<BusLock> bus_lock = std::make_shared<BusLock>();
	core0_mem_if.bus_lock = bus_lock;
	core1_mem_if.bus_lock = bus_lock;
	mmu0.mem = &core0_mem_if;
	mmu1.mem = &core1_mem_if;

	loader.load_executable_image(mem, mem.get_size(), opt.mem_start_addr);

	core0.init(&core0_mem_if, opt.use_dbbcache, &core0_mem_if, opt.use_lscache, &clint, loader.get_entrypoint(),
	           opt.mem_end_addr);
	core1.init(&core1_mem_if, opt.use_dbbcache, &core1_mem_if, opt.use_lscache, &clint, loader.get_entrypoint(),
	           opt.mem_end_addr - (32 * 1024));  // start stack 32KiB below stack for core0

	sys.init(mem.data, opt.mem_start_addr, loader.get_heap_addr(mem.get_size(), opt.mem_start_addr));
	sys.register_core(&core0);
	sys.register_core(&core1);

	if (opt.intercept_syscalls) {
		core0.sys = &sys;
		core1.sys = &sys;
	}
	core0.error_on_zero_traphandler = opt.error_on_zero_traphandler;
	core1.error_on_zero_traphandler = opt.error_on_zero_traphandler;

	// setup port mapping
	bus.ports[0] = new PortMapping(opt.mem_start_addr, opt.mem_end_addr, mem);
	bus.ports[1] = new PortMapping(opt.clint_start_addr, opt.clint_end_addr, clint);
	bus.ports[2] = new PortMapping(opt.sys_start_addr, opt.sys_end_addr, sys);
	bus.mapping_complete();

	// connect TLM sockets
	core0_mem_if.isock.bind(bus.tsocks[0]);
	core1_mem_if.isock.bind(bus.tsocks[1]);
	dbg_if.isock.bind(bus.tsocks[2]);
	bus.isocks[0].bind(mem.tsock);
	bus.isocks[1].bind(clint.tsock);
	bus.isocks[2].bind(sys.tsock);

	// connect interrupt signals/communication
	clint.target_harts[0] = &core0;
	clint.target_harts[1] = &core1;

	// switch for printing instructions
	core0.enable_trace(opt.trace_mode);
	core1.enable_trace(opt.trace_mode);

	std::vector<debug_target_if *> threads;
	threads.push_back(&core0);
	threads.push_back(&core1);

	if (opt.use_debug_runner) {
		auto server = new GDBServer("GDBServer", threads, &dbg_if, opt.debug_port);
		new GDBServerRunner("GDBRunner0", server, &core0);
		new GDBServerRunner("GDBRunner1", server, &core1);
	} else {
		new DirectCoreRunner(core0);
		new DirectCoreRunner(core1);
	}

	/* may not return (exit) */
	opt.handle_property_export_and_exit();

	if (opt.quiet)
		sc_core::sc_report_handler::set_verbosity_level(sc_core::SC_NONE);

	sc_core::sc_start();
	if (!opt.quiet) {
		core0.show();
		core1.show();
	}

	return 0;
}
