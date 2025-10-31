/* if not defined externally fall back to TARGET_RV64 */
#if !defined(TARGET_RV32) && !defined(TARGET_RV64) && !defined(TARGET_RV64_CHERIV9)
#define TARGET_RV64
#endif

/* if not defined externally fall back to one worker core */
#if !defined(NUM_CORES)
#define NUM_CORES 1
#endif

#include <termios.h>
#include <unistd.h>

#include <boost/io/ios_state.hpp>
#include <boost/program_options.hpp>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <iostream>

#include "core/common/clint.h"
#include "core/common/debug.h"
#include "core/common/debug_memory.h"
#include "core/common/gdb-mc/gdb_runner.h"
#include "core/common/gdb-mc/gdb_server.h"
#include "core/common/lwrt_clint.h"

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
#elif defined(TARGET_RV64)
#include "core/rv64/elf_loader.h"
#include "core/rv64/iss.h"
#include "core/rv64/mem.h"
#include "core/rv64/mmu.h"
#elif defined(TARGET_RV64_CHERIV9)
#include "core/rv64_cheriv9/elf_loader.h"
#include "core/rv64_cheriv9/iss.h"
#include "core/rv64_cheriv9/mem.h"
#include "core/rv64_cheriv9/mmu.h"
#endif

#include "platform/common/channel_console.h"
#include "platform/common/dummy_tlm_target.h"
#include "platform/common/memory.h"
#include "platform/common/ns16550a_uart.h"
#include "platform/common/options.h"
#include "platform/common/sifive_plic.h"
#include "platform/common/sifive_test.h"
#include "platform/common/tagged_memory.h"
#include "util/options.h"
#include "util/propertymap.h"

#define MEM_SIZE_MB 2048  // MB ram

#if defined(TARGET_RV32)
using namespace rv32;
/* address to load raw (not elf) images provided via --kernel-file */
#define KERNEL_LOAD_ADDR 0x80400000

#elif defined(TARGET_RV64)
using namespace rv64;
/* address to load raw (not elf) images provided via --kernel-file */
#define KERNEL_LOAD_ADDR 0x80200000

#elif defined(TARGET_RV64_CHERIV9)
using namespace cheriv9::rv64;
/* address to load raw (not elf) images provided via --kernel-file */
#define KERNEL_LOAD_ADDR 0x80200000

#endif /* TARGET_RVxx */

namespace po = boost::program_options;

struct LinuxOptions : public Options {
   public:
	typedef uint64_t addr_t;

	addr_t dtb_rom_start_addr = 0x00001000;
	addr_t dtb_rom_size = 0x2000;
	addr_t dtb_rom_end_addr = dtb_rom_start_addr + dtb_rom_size - 1;

	addr_t fwcfg_start_addr = 0x10100000;
	addr_t fwcfg_end_addr = fwcfg_start_addr + 0x18 - 1;
	addr_t flash0_start_addr = 0x20000000;
	addr_t flash0_end_addr = flash0_start_addr + 0x2000000 - 1;
	addr_t flash1_start_addr = 0x22000000;
	addr_t flash1_end_addr = flash1_start_addr + 0x2000000 - 1;
	addr_t platform_bus_start_addr = 0x4000000;
	addr_t platform_bus_end_addr = platform_bus_start_addr + 0x2000000 - 1;
	addr_t mem_size = 1024ul * 1024ul * (uint64_t)(MEM_SIZE_MB);
	addr_t mem_start_addr = 0x80000000;
	addr_t mem_end_addr = mem_start_addr + mem_size - 1;
	addr_t rtc_start_addr = 0x101000;
	addr_t rtc_end_addr = rtc_start_addr + 0x1000 - 1;
	addr_t uart0_start_addr = 0x10000000;
	addr_t uart0_end_addr = uart0_start_addr + 0x100 - 1;
	addr_t sifive_test_start_addr = 0x100000;
	addr_t sifive_test_end_addr = sifive_test_start_addr + 0x1000 - 1;
	addr_t pci_start_addr = 0x30000000;
	addr_t pci_end_addr = pci_start_addr + 0x10000000 - 1;
	addr_t virtio_mmio_start_addr = 0x10001000;
	addr_t virtio_mmio_end_addr = virtio_mmio_start_addr + 0x8000 - 1;
	addr_t plic_start_addr = 0xc000000;
	addr_t plic_end_addr = virtio_mmio_start_addr + 0x600000 - 1;
	addr_t clint_start_addr = 0x02000000;
	addr_t clint_end_addr = clint_start_addr + 0x10000 - 1;

	OptionValue<unsigned long> entry_point;
	std::string dtb_file;
	std::string kernel_file;
	bool dummy_tlm_target_debug = false;
	bool cheri_purecap = false;

	LinuxOptions(void) {
		// clang-format off
		add_options()
			("memory-start", po::value<uint64_t>(&mem_start_addr),"set memory start address")
			("memory-size", po::value<uint64_t>(&mem_size), "set memory size")
			("entry-point", po::value<std::string>(&entry_point.option),"set entry point address (ISS program counter)")
			("dtb-file", po::value<std::string>(&dtb_file)->required(), "dtb file for boot loading")
			("kernel-file", po::value<std::string>(&kernel_file), "optional kernel file to load (supports ELF or RAW files)")
			("dummy-tlm-target-debug", po::bool_switch(&dummy_tlm_target_debug), "print debug messages on dummy-tlm-target peripheral accesses")
#ifdef TARGET_RV64_CHERIV9
			("cheri-purecap", po::bool_switch(&cheri_purecap), "start in cheri purecap mode")
#endif
			;
		// clang-format on
	}

	void parse(int argc, char **argv) override {
		Options::parse(argc, argv);
		entry_point.finalize(parse_uint64_option);
		mem_end_addr = mem_start_addr + mem_size - 1;
	}
};

class Core {
   public:
	ISS iss;
	MMU mmu;
#ifdef TARGET_RV64_CHERIV9
	CombinedTaggedMemoryInterface memif;
#else
	CombinedMemoryInterface memif;
#endif
	InstrMemoryProxy imemif;

	Core(RV_ISA_Config *isa_config, unsigned int id, MemoryDMI dmi, uint64_t mem_start_addr, uint64_t mem_end_addr)
	    : iss(isa_config, id),
	      mmu(iss),
#ifdef TARGET_RV64_CHERIV9
	      memif(("MemoryInterface" + std::to_string(id)).c_str(), iss, &mmu, mem_start_addr, mem_end_addr),
#else
	      memif(("MemoryInterface" + std::to_string(id)).c_str(), iss, &mmu),
#endif
	      imemif(dmi, iss) {
		return;
	}

	void init(bool use_data_dmi, bool use_instr_dmi, bool use_dbbcache, bool use_lscache, clint_if *clint,
	          uint64_t entry, uint64_t sp_base, bool cheri_purecap = false) {
		if (use_data_dmi)
			memif.dmi_ranges.emplace_back(imemif.dmi);

#ifdef TARGET_RV64_CHERIV9
		iss.init(get_instr_memory_if(use_instr_dmi), use_dbbcache, &memif, use_lscache, clint, entry, sp_base,
		         cheri_purecap);
#else
		iss.init(get_instr_memory_if(use_instr_dmi), use_dbbcache, &memif, use_lscache, clint, entry, sp_base);
#endif
	}

   private:
	instr_memory_if *get_instr_memory_if(bool use_instr_dmi) {
		if (use_instr_dmi)
			return &imemif;
		else
			return &memif;
	}
};

void handle_kernel_file(const LinuxOptions opt, load_if &mem) {
	if (opt.kernel_file.size() == 0) {
		return;
	}

	std::cout << "Info: load kernel file \"" << opt.kernel_file << "\" ";
	ELFLoader elf(opt.kernel_file.c_str());
	if (elf.is_elf()) {
		/* load elf (use physical addresses) */
		std::cout << "as ELF file (to physical addresses defined in ELF)" << std::endl;
		elf.load_executable_image(mem, mem.get_size(), opt.mem_start_addr, false);
	} else {
		/* load raw to KERNEL_LOAD_ADDR */
		std::cout << "as RAW file (to 0x" << std::hex << KERNEL_LOAD_ADDR << std::dec << ")" << std::endl;
		mem.load_binary_file(opt.kernel_file, KERNEL_LOAD_ADDR - opt.mem_start_addr);
	}
}

int sc_main(int argc, char **argv) {
	// PropertyMap::global()->set_debug(true);

	LinuxOptions opt;
	opt.parse(argc, argv);

	if (!opt.property_map_is_loaded) {
		/*
		 * property map was not loaded by Options -> use default model properties
		 * and values
		 */

		/* set global clock explicitly to 100 MHz */
		VPPP_PROPERTY_SET("", "clock_cycle_period", sc_core::sc_time, sc_core::sc_time(10, sc_core::SC_NS));
	}

	if (opt.use_E_base_isa) {
		std::cerr << "Error: The Linux VP does not support RV32E/RV64E!" << std::endl;
		return -1;
	}
	RV_ISA_Config isa_config(false, opt.en_ext_Zfh);
#ifdef TARGET_RV64_CHERIV9
	isa_config.set_misa_extension(csr_misa::X);    // enable X extension (custom extension bit, marks CHERI is used)
	isa_config.clear_misa_extension(csr_misa::V);  // not supported with cheriv9
	isa_config.clear_misa_extension(csr_misa::N);  // not supported with cheriv9
#endif

	std::srand(std::time(nullptr));  // use current time as seed for random generator

	tlm::tlm_global_quantum::instance().set(sc_core::sc_time(opt.tlm_global_quantum, sc_core::SC_NS));

	ELFLoader loader(opt.input_program.c_str());
	NetTrace *debug_bus = nullptr;
	if (opt.use_debug_bus) {
		debug_bus = new NetTrace(opt.debug_bus_port);
	}
	SimpleBus<NUM_CORES + 1, 13> bus("SimpleBus", debug_bus, opt.break_on_transaction);

	SimpleMemory dtb_rom("DTB_ROM", opt.dtb_rom_size);
	DUMMY_TLM_TARGET fwcfg("fwcfg", opt.fwcfg_start_addr, opt.dummy_tlm_target_debug);
	DUMMY_TLM_TARGET flash0("flash0", opt.flash0_start_addr, opt.dummy_tlm_target_debug);
	DUMMY_TLM_TARGET flash1("flash1", opt.flash1_start_addr, opt.dummy_tlm_target_debug);
	DUMMY_TLM_TARGET platform_bus("platform_bus", opt.platform_bus_start_addr, opt.dummy_tlm_target_debug);
#ifdef TARGET_RV64_CHERIV9
	TaggedMemory mem("SimpleTaggedMemory", opt.mem_size);
#else
	SimpleMemory mem("SimpleMemory", opt.mem_size);
#endif
	DUMMY_TLM_TARGET rtc("rtc", opt.rtc_start_addr, opt.dummy_tlm_target_debug);

	Channel_Console channel_console;
	NS16550A_UART uart0_ns16550a("uart0", &channel_console, 10);
	SIFIVE_Test sifive_test("SIFIVE_Test");
	DUMMY_TLM_TARGET pci("pci", opt.pci_start_addr, opt.dummy_tlm_target_debug);
	DUMMY_TLM_TARGET virtio_mmio("virtio_mmio", opt.virtio_mmio_start_addr, opt.dummy_tlm_target_debug);
	SIFIVE_PLIC plic("PLIC", false, NUM_CORES, 96);
	LWRT_CLINT<NUM_CORES> clint("CLINT");

	DebugMemoryInterface dbg_if("DebugMemoryInterface");
#ifdef TARGET_RV64_CHERIV9
	MemoryDMI dmi = MemoryDMI::create_start_size_mapping(mem.data, opt.mem_start_addr, mem.get_size(), &mem.tag_bits);
#else
	MemoryDMI dmi = MemoryDMI::create_start_size_mapping(mem.data, opt.mem_start_addr, mem.get_size());
#endif

	Core *cores[NUM_CORES];
	std::shared_ptr<BusLock> bus_lock = std::make_shared<BusLock>();
	for (unsigned i = 0; i < NUM_CORES; i++) {
		cores[i] = new Core(&isa_config, i, dmi, opt.mem_start_addr, opt.mem_end_addr);

		cores[i]->memif.bus_lock = bus_lock;
		cores[i]->mmu.mem = &cores[i]->memif;

		// enable interactive debug via console
		channel_console.debug_targets_add(&cores[i]->iss);
	}

	uint64_t entry_point = loader.get_entrypoint();
	if (opt.entry_point.available)
		entry_point = opt.entry_point.value;

	loader.load_executable_image(mem, mem.get_size(), opt.mem_start_addr);
	for (size_t i = 0; i < NUM_CORES; i++) {
		cores[i]->init(opt.use_data_dmi, opt.use_instr_dmi, opt.use_dbbcache, opt.use_lscache, &clint, entry_point,
		               opt.mem_end_addr, opt.cheri_purecap);
		cores[i]->iss.error_on_zero_traphandler = opt.error_on_zero_traphandler;
	}

	// setup port mapping
	int i = 0;
	bus.ports[i++] = new PortMapping(opt.dtb_rom_start_addr, opt.dtb_rom_end_addr, dtb_rom);
	bus.ports[i++] = new PortMapping(opt.fwcfg_start_addr, opt.fwcfg_end_addr, fwcfg);
	bus.ports[i++] = new PortMapping(opt.flash0_start_addr, opt.flash0_end_addr, flash0);
	bus.ports[i++] = new PortMapping(opt.flash1_start_addr, opt.flash1_end_addr, flash1);
	bus.ports[i++] = new PortMapping(opt.platform_bus_start_addr, opt.platform_bus_end_addr, platform_bus);
	bus.ports[i++] = new PortMapping(opt.mem_start_addr, opt.mem_end_addr, mem);
	bus.ports[i++] = new PortMapping(opt.rtc_start_addr, opt.rtc_end_addr, rtc);
	bus.ports[i++] = new PortMapping(opt.uart0_start_addr, opt.uart0_end_addr, uart0_ns16550a);
	bus.ports[i++] = new PortMapping(opt.sifive_test_start_addr, opt.sifive_test_end_addr, sifive_test);
	bus.ports[i++] = new PortMapping(opt.pci_start_addr, opt.pci_end_addr, pci);
	bus.ports[i++] = new PortMapping(opt.virtio_mmio_start_addr, opt.virtio_mmio_end_addr, virtio_mmio);
	bus.ports[i++] = new PortMapping(opt.plic_start_addr, opt.plic_end_addr, plic);
	bus.ports[i++] = new PortMapping(opt.clint_start_addr, opt.clint_end_addr, clint);
	bus.mapping_complete();

	// connect TLM sockets
	for (size_t i = 0; i < NUM_CORES; i++) {
		cores[i]->memif.isock.bind(bus.tsocks[i]);
	}
	dbg_if.isock.bind(bus.tsocks[NUM_CORES]);
	i = 0;
	bus.isocks[i++].bind(dtb_rom.tsock);
	bus.isocks[i++].bind(fwcfg.tsock);
	bus.isocks[i++].bind(flash0.tsock);
	bus.isocks[i++].bind(flash1.tsock);
	bus.isocks[i++].bind(platform_bus.tsock);
	bus.isocks[i++].bind(mem.tsock);
	bus.isocks[i++].bind(rtc.tsock);
	bus.isocks[i++].bind(uart0_ns16550a.tsock);
	bus.isocks[i++].bind(sifive_test.tsock);
	bus.isocks[i++].bind(pci.tsock);
	bus.isocks[i++].bind(virtio_mmio.tsock);
	bus.isocks[i++].bind(plic.tsock);
	bus.isocks[i++].bind(clint.tsock);

	// connect interrupt signals/communication
	for (size_t i = 0; i < NUM_CORES; i++) {
		plic.target_harts[i] = &cores[i]->iss;
		clint.target_harts[i] = &cores[i]->iss;
	}
	uart0_ns16550a.plic = &plic;

	for (size_t i = 0; i < NUM_CORES; i++) {
		// switch for printing instructions
		cores[i]->iss.enable_trace(opt.trace_mode);

		// emulate RISC-V core boot loader
		cores[i]->iss.regs[RegFile::a0] = cores[i]->iss.get_hart_id();
		cores[i]->iss.regs[RegFile::a1] = opt.dtb_rom_start_addr;
	}

	// load DTB (Device Tree Binary) file
	dtb_rom.load_binary_file(opt.dtb_file, 0);

	// load kernel
	handle_kernel_file(opt, mem);

	std::vector<mmu_memory_if *> mmus;
	std::vector<debug_target_if *> dharts;
	if (opt.use_debug_runner) {
		for (size_t i = 0; i < NUM_CORES; i++) {
			dharts.push_back(&cores[i]->iss);
			mmus.push_back(&cores[i]->memif);
		}

		auto server = new GDBServer("GDBServer", dharts, &dbg_if, opt.debug_port, mmus);
		for (size_t i = 0; i < dharts.size(); i++)
			new GDBServerRunner(("GDBRunner" + std::to_string(i)).c_str(), server, dharts[i]);
	} else {
		for (size_t i = 0; i < NUM_CORES; i++) {
			new DirectCoreRunner(cores[i]->iss);
		}
	}

	/* may not return (exit) */
	opt.handle_property_export_and_exit();

	sc_core::sc_start();
	for (size_t i = 0; i < NUM_CORES; i++) {
		cores[i]->iss.show();
	}

	return 0;
}
