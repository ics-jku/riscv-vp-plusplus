#include <cstdlib>
#include <ctime>

#include "aon.h"
#include "can.h"
#include "oled/oled.hpp"
#include "core/common/clint.h"
#include "core/rv32/syscall.h"
#include "elf_loader.h"
#include "fe310_plic.h"
#include "debug_memory.h"
#include "gpio.h"
#include "iss.h"
#include "maskROM.h"
#include "mem.h"
#include "memory.h"
#include "prci.h"
#include "slip.h"
#include "spi.h"
#include "uart.h"
#include "platform/common/options.h"

#include "gdb-mc/gdb_server.h"
#include "gdb-mc/gdb_runner.h"

#include <boost/io/ios_state.hpp>
#include <boost/program_options.hpp>
#include <iomanip>
#include <iostream>
#include <memory>
#include <functional>

// Interrupt numbers	(see platform.h)
#define INT_RESERVED 0
#define INT_WDOGCMP 1
#define INT_RTCCMP 2
#define INT_UART0_BASE 3
#define INT_UART1_BASE 4
#define INT_SPI0_BASE 5
#define INT_SPI1_BASE 6
#define INT_SPI2_BASE 7
#define INT_GPIO_BASE 8
#define INT_PWM0_BASE 40
#define INT_PWM1_BASE 44
#define INT_PWM2_BASE 48

using namespace rv32;
namespace po = boost::program_options;

class HifiveOptions : public Options {
public:
	typedef unsigned int addr_t;

	addr_t maskROM_start_addr = 0x00001000;
	addr_t maskROM_end_addr = 0x00001FFF;
	addr_t clint_start_addr = 0x02000000;
	addr_t clint_end_addr = 0x0200FFFF;
	addr_t sys_start_addr = 0x02010000;
	addr_t sys_end_addr = 0x020103ff;
	addr_t plic_start_addr = 0x0C000000;
	addr_t plic_end_addr = 0x0FFFFFFF;
	addr_t aon_start_addr = 0x10000000;
	addr_t aon_end_addr = 0x10007FFF;
	addr_t prci_start_addr = 0x10008000;
	addr_t prci_end_addr = 0x1000FFFF;
	addr_t uart0_start_addr = 0x10013000;
	addr_t uart0_end_addr = 0x10013FFF;
	addr_t uart1_start_addr = 0x10023000;
	addr_t uart1_end_addr = 0x10023FFF;		// not routed on Hifive1 Rev. A
	addr_t spi0_start_addr = 0x10014000;
	addr_t spi0_end_addr = 0x10014FFF;
	addr_t spi1_start_addr = 0x10024000;
	addr_t spi1_end_addr = 0x10024FFF;
	addr_t spi2_start_addr = 0x10034000;
	addr_t spi2_end_addr = 0x10034FFF;
	addr_t gpio0_start_addr = 0x10012000;
	addr_t gpio0_end_addr = 0x10012FFF;
	addr_t flash_size = 1024 * 1024 * 512;	// 512 MB flash
	addr_t flash_start_addr = 0x20000000;
	addr_t flash_end_addr = flash_start_addr + flash_size - 1;
	addr_t dram_size = 1024 * 16;			// 16 KB dram
	addr_t dram_start_addr = 0x80000000;
	addr_t dram_end_addr = dram_start_addr + dram_size - 1;

	bool enable_can = false;
	bool disable_inline_oled = false;
	bool wait_for_gpio_connection = false;
	bool forward_uart_0 = false;
	bool forward_uart_1 = false;
	std::string tun_device = "tun0";

	HifiveOptions(void) {
		// clang-format off
		add_options()
			("enable-inline-can",  po::bool_switch(&enable_can), "enable support for CAN SPI module")
			("disable-inline-oled", po::bool_switch(&disable_inline_oled), "enable support for OLED SPI module")
			("forward-uart0-to-vbb", po::bool_switch(&forward_uart_0), "forwards UART_0 TX/RX to virtual breadboard")
			("forward-uart1-to-vbb", po::bool_switch(&forward_uart_1), "forwards UART_1 TX/RX to virtual breadboard")
			("wait-for-gpio-connection", po::bool_switch(&wait_for_gpio_connection), "Waits for a GPIO-Connection before starting program")
			("tun-device", po::value<std::string>(&tun_device), "tun device used by SLIP");
		// clang-format on
	}

	void printValues(std::ostream& os) const override {
		os << std::hex;
		os << "flash_start_addr:\t" << +flash_start_addr << std::endl;
		os << "flash_end_addr:\t"   << +flash_end_addr   << std::endl;
		os << "dram_start_addr:\t" << +dram_start_addr << std::endl;
		os << "dram_end_addr:\t"   << +dram_end_addr   << std::endl;
		static_cast <const Options&>( *this ).printValues(os);
	}
};

int sc_main(int argc, char **argv) {
	HifiveOptions opt;
	opt.parse(argc, argv);

	std::srand(std::time(nullptr));  // use current time as seed for random generator

	tlm::tlm_global_quantum::instance().set(sc_core::sc_time(opt.tlm_global_quantum, sc_core::SC_NS));

	ISS core(0);
	SimpleMemory dram("DRAM", opt.dram_size);
	SimpleMemory flash("Flash", opt.flash_size);
	ELFLoader loader(opt.input_program.c_str());
	SimpleBus<2, 14> bus("SimpleBus");
	CombinedMemoryInterface iss_mem_if("MemoryInterface", core);
	SyscallHandler sys("SyscallHandler");

	FE310_PLIC<1, 53, 64, 7> plic("PLIC");
	CLINT<1> clint("CLINT");
	AON aon("AON");
	PRCI prci("PRCI");
	GPIO gpio0("GPIO0", INT_GPIO_BASE);
	SPI spi0("SPI0");
	SPI spi1("SPI1");
	std::shared_ptr<CAN> can;
	if (opt.enable_can) {
		std::cout << "using internal CAN controller on SPI CS 0" << std::endl;
		can = std::make_shared<CAN>();
		spi1.connect(0, std::bind(&CAN::write, can, std::placeholders::_1));
	} else {
		// pass through to gpio server
		spi1.connect(0, gpio0.getSPIwriteFunction(0));
	}
	std::shared_ptr<SS1106> oled;
	if(!opt.disable_inline_oled) {
		std::cout << "[hifive_main] using internal SS1106 oled controller on SPI CS 2 (with DC as Pin 16, Bit 10)" << std::endl;
		oled = std::make_shared<SS1106>([&gpio0]{return gpio0.value & (1 << 10);});		// custom pin 16 is offset 10
		spi1.connect(2, std::bind(&SS1106::write, oled, std::placeholders::_1));
	} else {
		// pass through to gpio server
		spi1.connect(2, gpio0.getSPIwriteFunction(2));
	}

	spi1.connect(3, gpio0.getSPIwriteFunction(3));
	SPI spi2("SPI2");
	std::shared_ptr<UART> uart0;
	std::shared_ptr<Tunnel_UART> uart0_tunnel;
	if(opt.forward_uart_0) {
		std::cout << "[hifive_main] tunneling UART0 over virtual breadboard protocol" << std::endl;
		uart0_tunnel = std::make_shared<Tunnel_UART>("UART0", 3);
		uart0_tunnel->register_transmit_function(gpio0.getUartTransmitFunction(17));
		gpio0.registerUartReceiveFunction(16, std::bind(&Tunnel_UART::nonblock_receive, uart0_tunnel, std::placeholders::_1));
	} else {
		uart0 = std::make_shared<UART>("UART0", 3);
	}
	std::shared_ptr<SLIP> slip;
	std::shared_ptr<Tunnel_UART> uart1_tunnel;
	if(opt.forward_uart_1) {
		std::cout << "[hifive_main] tunneling UART1 over virtual breadboard protocol" << std::endl;
		uart1_tunnel = std::make_shared<Tunnel_UART>("UART1", 4);
		// following pins only connected on RevB
		uart1_tunnel->register_transmit_function(gpio0.getUartTransmitFunction(23));
		gpio0.registerUartReceiveFunction(18, std::bind(&Tunnel_UART::nonblock_receive, uart1_tunnel, std::placeholders::_1));
	} else {
		slip = std::make_shared<SLIP>("SLIP", 4, opt.tun_device);
	}
	MaskROM maskROM("MASKROM");
	DebugMemoryInterface dbg_if("DebugMemoryInterface");

	MemoryDMI dram_dmi = MemoryDMI::create_start_size_mapping(dram.data, opt.dram_start_addr, dram.size);
	MemoryDMI flash_dmi = MemoryDMI::create_start_size_mapping(flash.data, opt.flash_start_addr, flash.size);
	InstrMemoryProxy instr_mem(flash_dmi, core);

	std::shared_ptr<BusLock> bus_lock = std::make_shared<BusLock>();
	iss_mem_if.bus_lock = bus_lock;

	instr_memory_if *instr_mem_if = &iss_mem_if;
	data_memory_if *data_mem_if = &iss_mem_if;
	if (opt.use_instr_dmi)
		instr_mem_if = &instr_mem;
	if (opt.use_data_dmi)
		iss_mem_if.dmi_ranges.emplace_back(dram_dmi);

	bus.ports[ 0] = new PortMapping(opt.flash_start_addr,  opt.flash_end_addr);
	bus.ports[ 1] = new PortMapping(opt.dram_start_addr,   opt.dram_end_addr);
	bus.ports[ 2] = new PortMapping(opt.plic_start_addr,   opt.plic_end_addr);
	bus.ports[ 3] = new PortMapping(opt.clint_start_addr,  opt.clint_end_addr);
	bus.ports[ 4] = new PortMapping(opt.aon_start_addr,    opt.aon_end_addr);
	bus.ports[ 5] = new PortMapping(opt.prci_start_addr,   opt.prci_end_addr);
	bus.ports[ 6] = new PortMapping(opt.spi0_start_addr,   opt.spi0_end_addr);
	bus.ports[ 7] = new PortMapping(opt.uart0_start_addr,  opt.uart0_end_addr);
	bus.ports[ 8] = new PortMapping(opt.maskROM_start_addr,opt.maskROM_end_addr);
	bus.ports[ 9] = new PortMapping(opt.gpio0_start_addr,  opt.gpio0_end_addr);
	bus.ports[10] = new PortMapping(opt.sys_start_addr,    opt.sys_end_addr);
	bus.ports[11] = new PortMapping(opt.spi1_start_addr,   opt.spi1_end_addr);
	bus.ports[12] = new PortMapping(opt.spi2_start_addr,   opt.spi2_end_addr);
	bus.ports[13] = new PortMapping(opt.uart1_start_addr,  opt.uart1_end_addr);

	loader.load_executable_image(flash, flash.size, opt.flash_start_addr, false);
	loader.load_executable_image(dram, dram.size, opt.dram_start_addr, false);

	core.init(instr_mem_if, data_mem_if, &clint, loader.get_entrypoint(), rv32_align_address(opt.dram_end_addr));
	sys.init(dram.data, opt.dram_start_addr, loader.get_heap_addr());
	sys.register_core(&core);

	if (opt.intercept_syscalls)
		core.sys = &sys;

	// connect TLM sockets
	iss_mem_if.isock.bind(bus.tsocks[0]);
	dbg_if.isock.bind(bus.tsocks[1]);
	bus.isocks[0].bind(flash.tsock);
	bus.isocks[1].bind(dram.tsock);
	bus.isocks[2].bind(plic.tsock);
	bus.isocks[3].bind(clint.tsock);
	bus.isocks[4].bind(aon.tsock);
	bus.isocks[5].bind(prci.tsock);
	bus.isocks[6].bind(spi0.tsock);
	if(uart0) {
		bus.isocks[7].bind(uart0->tsock);
	} else if (uart0_tunnel) {
		bus.isocks[7].bind(uart0_tunnel->tsock);
	}
	bus.isocks[8].bind(maskROM.tsock);
	bus.isocks[9].bind(gpio0.tsock);
	bus.isocks[10].bind(sys.tsock);
	bus.isocks[11].bind(spi1.tsock);
	bus.isocks[12].bind(spi2.tsock);
	if(slip) {
		bus.isocks[13].bind(slip->tsock);
	} else if (uart1_tunnel) {
		bus.isocks[13].bind(uart1_tunnel->tsock);
	}

	// connect interrupt signals/communication
	plic.target_harts[0] = &core;
	clint.target_harts[0] = &core;
	gpio0.plic = &plic;
	if(uart0)
		uart0->plic = &plic;
	if(uart0_tunnel)
		uart0_tunnel->plic = &plic;
	if(slip)
		slip->plic = &plic;
	if(uart1_tunnel)
		uart1_tunnel->plic = &plic;

	std::vector<debug_target_if *> threads;
	threads.push_back(&core);

	core.trace = opt.trace_mode;  // switch for printing instructions
	if (opt.use_debug_runner) {
		auto server = new GDBServer("GDBServer", threads, &dbg_if, opt.debug_port);
		new GDBServerRunner("GDBRunner", server, &core);
	} else {
		new DirectCoreRunner(core);
	}

	if(opt.wait_for_gpio_connection) {
		std::cout << "[hifive_main] Waiting for virtual breadboard protocol connection" << std::endl;
		while(!gpio0.isServerConnected()) {
			usleep(2000);
		}
	}

	sc_core::sc_start();

	core.show();

	return 0;
}
