#include <boost/program_options.hpp>
#include <systemc>

#include "afio.h"
#include "eclic.h"
#include "elf_loader.h"
#include "exti.h"
#include "gdb-mc/gdb_runner.h"
#include "gdb-mc/gdb_server.h"
#include "gpio.h"
#include "mem.h"
#include "memory.h"
#include "nuclei_core/nuclei_iss.h"
#include "platform/common/options.h"
#include "rcu.h"
#include "spi.h"
#include "timer.h"
#include "usart.h"

using namespace rv32;
namespace po = boost::program_options;

class GD32Options : public Options {
   public:
	typedef unsigned int addr_t;

	addr_t eclic_start_addr = 0xD2000000;
	addr_t eclic_end_addr = 0xD200FFFF;

	addr_t timer_start_addr = 0xD1000000;
	addr_t timer_end_addr = 0xD100D000;

	addr_t rcu_start_addr = 0x40021000;
	addr_t rcu_end_addr = 0x400213FF;

	addr_t usart0_start_addr = 0x40013800;
	addr_t usart0_end_addr = 0x40013BFF;

	addr_t spi_start_addr = 0x40013000;
	addr_t spi_end_addr = 0x400133FF;

	addr_t afio_start_addr = 0x40010000;
	addr_t afio_end_addr = 0x400103FF;

	addr_t exti_start_addr = 0x40010400;
	addr_t exti_end_addr = 0x400107FF;

	addr_t gpioa_start_addr = 0x40010800;
	addr_t gpioa_end_addr = 0x40010BFF;
	addr_t gpiob_start_addr = 0x40010C00;
	addr_t gpiob_end_addr = 0x40010FFF;
	addr_t gpioc_start_addr = 0x40011000;
	addr_t gpioc_end_addr = 0x400113FF;
	addr_t gpiod_start_addr = 0x40011400;
	addr_t gpiod_end_addr = 0x400117FF;
	addr_t gpioe_start_addr = 0x40011800;
	addr_t gpioe_end_addr = 0x40011BFF;

	addr_t sram_size = 1024 * 32;  // 32 KB sram
	addr_t sram_start_addr = 0x20000000;
	addr_t sram_end_addr = sram_start_addr + sram_size - 1;

	addr_t flash_size = 1024 * 128;  // 128 KB flash
	addr_t flash_start_addr = 0x08000000;
	addr_t flash_end_addr = flash_start_addr + flash_size - 1;

	GD32Options(void) {}
};

int sc_main(int argc, char **argv) {
	GD32Options opt;
	opt.parse(argc, argv);

	std::srand(std::time(nullptr));  // use current time as seed for random generator

	tlm::tlm_global_quantum::instance().set(sc_core::sc_time(opt.tlm_global_quantum, sc_core::SC_NS));

	NUCLEI_ISS core(0);
	SimpleMemory sram("SRAM", opt.sram_size);
	SimpleMemory flash("Flash", opt.flash_size);
	ELFLoader loader(opt.input_program.c_str());
	SimpleBus<2, 14> ahb("AHB");
	CombinedMemoryInterface iss_mem_if("MemoryInterface", core);

	RCU rcu("RCU");
	TIMER timer("TIMER");
	ECLIC<87, 15> eclic("ECLIC");
	USART usart0("USART0");
	AFIO afio("AFIO");
	EXTI exti("EXTI");
	GPIO gpioa("GPIOA", gpio::Port::A);
	GPIO gpiob("GPIOB", gpio::Port::B);
	GPIO gpioc("GPIOC", gpio::Port::C);
	GPIO gpiod("GPIOD", gpio::Port::D);
	GPIO gpioe("GPIOE", gpio::Port::E);
	SPI spi0("SPI0");

	spi0.connect(gpioa.getSPIwriteFunction(4));  // pass spi write calls through to gpio server (port A)

	DebugMemoryInterface dbg_if("DebugMemoryInterface");

	MemoryDMI sram_dmi = MemoryDMI::create_start_size_mapping(sram.data, opt.sram_start_addr, sram.size);
	MemoryDMI flash_dmi = MemoryDMI::create_start_size_mapping(flash.data, opt.flash_start_addr, flash.size);
	InstrMemoryProxy instr_mem(flash_dmi, core);

	std::shared_ptr<BusLock> bus_lock = std::make_shared<BusLock>();
	iss_mem_if.bus_lock = bus_lock;

	instr_memory_if *instr_mem_if = &iss_mem_if;
	data_memory_if *data_mem_if = &iss_mem_if;
	if (opt.use_instr_dmi)
		instr_mem_if = &instr_mem;
	if (opt.use_data_dmi)
		iss_mem_if.dmi_ranges.emplace_back(sram_dmi);

	{
		unsigned int it = 0;
		ahb.ports[it++] = new PortMapping(opt.flash_start_addr, opt.flash_end_addr);
		ahb.ports[it++] = new PortMapping(opt.sram_start_addr, opt.sram_end_addr);
		ahb.ports[it++] = new PortMapping(opt.rcu_start_addr, opt.rcu_end_addr);
		ahb.ports[it++] = new PortMapping(opt.timer_start_addr, opt.timer_end_addr);
		ahb.ports[it++] = new PortMapping(opt.eclic_start_addr, opt.eclic_end_addr);
		ahb.ports[it++] = new PortMapping(opt.usart0_start_addr, opt.usart0_end_addr);
		ahb.ports[it++] = new PortMapping(opt.afio_start_addr, opt.afio_end_addr);
		ahb.ports[it++] = new PortMapping(opt.exti_start_addr, opt.exti_end_addr);
		ahb.ports[it++] = new PortMapping(opt.gpioa_start_addr, opt.gpioa_end_addr);
		ahb.ports[it++] = new PortMapping(opt.gpiob_start_addr, opt.gpiob_end_addr);
		ahb.ports[it++] = new PortMapping(opt.gpioc_start_addr, opt.gpioc_end_addr);
		ahb.ports[it++] = new PortMapping(opt.gpiod_start_addr, opt.gpiod_end_addr);
		ahb.ports[it++] = new PortMapping(opt.gpioe_start_addr, opt.gpioe_end_addr);
		ahb.ports[it++] = new PortMapping(opt.spi_start_addr, opt.spi_end_addr);
	}

	loader.load_executable_image(flash, flash.size, opt.flash_start_addr, false);
	loader.load_executable_image(sram, sram.size, opt.sram_start_addr, false);

	core.init(instr_mem_if, data_mem_if, &timer, loader.get_entrypoint(), rv32_align_address(opt.sram_end_addr));

	// connect TLM sockets
	iss_mem_if.isock.bind(ahb.tsocks[0]);
	dbg_if.isock.bind(ahb.tsocks[1]);
	{
		unsigned int it = 0;
		ahb.isocks[it++].bind(flash.tsock);
		ahb.isocks[it++].bind(sram.tsock);
		ahb.isocks[it++].bind(rcu.tsock);
		ahb.isocks[it++].bind(timer.tsock);
		ahb.isocks[it++].bind(eclic.tsock);
		ahb.isocks[it++].bind(usart0.tsock);
		ahb.isocks[it++].bind(afio.tsock);
		ahb.isocks[it++].bind(exti.tsock);
		ahb.isocks[it++].bind(gpioa.tsock);
		ahb.isocks[it++].bind(gpiob.tsock);
		ahb.isocks[it++].bind(gpioc.tsock);
		ahb.isocks[it++].bind(gpiod.tsock);
		ahb.isocks[it++].bind(gpioe.tsock);
		ahb.isocks[it++].bind(spi0.tsock);
	}

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
