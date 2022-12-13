#include "gpio.h"

using namespace std;
using namespace gpio;

static Pinstate getIOF(PinNumber pin, bool iofsel) {
	switch(pin) {
	case 16:	// UART0 RX
		return !iofsel ? Pinstate::IOF_UART_RX : Pinstate::UNSET;
	case 17:	// UART0 TX
		return !iofsel ? Pinstate::IOF_UART_TX : Pinstate::UNSET;
	case 19:	// PWM1_1
	case 20:	// PWM1_0
	case 21:	// PWM1_2
	case 22:	// PWM1_3
		return !iofsel ? Pinstate::UNSET : Pinstate::IOF_PWM;
	case  0:	// PWM0_0
	case  1:	// PWM0_1
		return !iofsel ? Pinstate::UNSET : Pinstate::IOF_PWM;
	case  2:	// SPI1 CS0, PWM0_2
	case  3:	// SPI1 OSI, PWM0_3
		return !iofsel ? Pinstate::IOF_SPI : Pinstate::IOF_PWM;
	case  4:	// SPI1 MISO
	case  5:	// SPI1 SCK
		return !iofsel ? Pinstate::IOF_SPI : Pinstate::UNSET;
	case  9:	// SPI1 CS2
		return !iofsel ? Pinstate::IOF_SPI : Pinstate::UNSET;
	case 10:	// SPI1 CS3, PWM2_0
		return !iofsel ? Pinstate::IOF_SPI : Pinstate::IOF_PWM;
	case 11:	// PWM2_1
	case 12:	// PWM2_2
	case 13:	// PWM2_3
		return !iofsel ? Pinstate::UNSET : Pinstate::IOF_PWM;
	case 18:	// UART1 (RX?)
		return !iofsel ? Pinstate::IOF_UART_RX : Pinstate::UNSET;
	case 23:	// UART1 (TX?)
		return !iofsel ? Pinstate::IOF_UART_TX : Pinstate::UNSET;
	default:
		return Pinstate::UNSET;
	}
}

static PinNumber getPinOffsFromSPIcs(PinNumber cs) {
	switch (cs) {
	case 0:
		return 2;
	case 1:
		assert(false && "[GPIO] On Fe310, CS 1 is not routable");
		return max_num_pins;
	case 2:
		return 9;
	case 3:
		return 10;
	default:
		assert(false && "[GPIO] Invalid CS pin given");
		return max_num_pins;
	}
}

GPIO::GPIO(sc_core::sc_module_name, unsigned int_gpio_base) : int_gpio_base(int_gpio_base) {
	tsock.register_b_transport(this, &GPIO::transport);

	router
	    .add_register_bank({
	        {PIN_VALUE_ADDR, &value},
	        {INPUT_EN_REG_ADDR, &input_en},
	        {OUTPUT_EN_REG_ADDR, &output_en},
	        {PORT_REG_ADDR, &port},
	        {PULLUP_EN_ADDR, &pullup_en},
	        {PIN_DRIVE_STNGTH, &pin_drive_strength},
	        {RISE_INTR_EN, &rise_intr_en},
	        {RISE_INTR_PEND, &rise_intr_pending},
	        {FALL_INTR_EN, &fall_intr_en},
	        {FALL_INTR_PEND, &fall_intr_pending},
	        {HIGH_INTR_EN, &high_intr_en},
	        {HIGH_INTR_PEND, &high_intr_pending},
	        {LOW_INTR_EN, &low_intr_en},
	        {LOW_INTR_PEND, &low_intr_pending},
	        {IOF_EN_REG_ADDR, &iof_en},
	        {IOF_SEL_REG_ADDR, &iof_sel},
	        {OUT_XOR_REG_ADDR, &out_xor},
	    })
	    .register_handler(this, &GPIO::register_access_callback);

	SC_METHOD(synchronousChange);
	sensitive << asyncEvent;
	dont_initialize();

	server.setupConnection(to_string(gpio::default_port).c_str());
	server.registerOnChange(bind(&GPIO::asyncOnchange, this, placeholders::_1, placeholders::_2));
	serverThread = new thread(bind(&GpioServer::startAccepting, &server));
}

GPIO::~GPIO() {
	server.quit();
	if (serverThread) {
		if(serverThread->joinable()){
			serverThread->join();
		}
		delete serverThread;
	}
}

void GPIO::register_access_callback(const vp::map::register_access_t &r) {
	if (r.write) {
		switch (r.addr) {
		case PIN_VALUE_ADDR:
			cerr << "[GPIO] write to value register is ignored!" << endl;
			return;
		case PORT_REG_ADDR:
			// cout << "[GPIO] new Port value: ";
			// bitPrint(reinterpret_cast<unsigned char*>(&r.nv), sizeof(uint32_t));

			// value and server.state might differ, if a bit is changed by
			// client and the synchronous_change was not fired yet.
			// Server wins in this scenario.
			{
				const auto prv_output_enabled_port = (port & output_en) & ~iof_en;
				const auto new_output_enabled_port = (r.nv & output_en) & ~iof_en;
				const auto changed_output_pins = new_output_enabled_port ^ prv_output_enabled_port;

				if(!changed_output_pins)
					break;

				value = (value & ~output_en) | new_output_enabled_port;

				for(PinNumber i = 0; i < available_pins; i++) {
					const auto bitoffs = (1l << i);
					if(bitoffs & changed_output_pins) {
						server.state.pins[i] = new_output_enabled_port & bitoffs ? Pinstate::HIGH : Pinstate::LOW;
						server.pushPin(i, toTristate(server.state.pins[i]));
					}
				}
			}
			break;

		case PULLUP_EN_ADDR:
			// cout << "[GPIO] pullup changed" << endl;
			// bitPrint(reinterpret_cast<unsigned char*>(&pullup_en), sizeof(uint32_t));
			{
				const auto newly_pulled_up_bits = (r.nv ^ pullup_en) & r.nv;
				value |= newly_pulled_up_bits;
				for(PinNumber i = 0; i < available_pins; i++) {
					if((1l << i) & newly_pulled_up_bits) {
						server.state.pins[i] = Pinstate::HIGH;
						// TODO: Notify pin subscriber
					}
				}
			}
			break;
		case OUTPUT_EN_REG_ADDR:
			{
				const auto& old_output_en = output_en;
				const auto& new_output_en = r.nv;

				const auto newly_output_disabled_bits =  old_output_en & ~new_output_en;
				const auto newly_output_enabled_bits  = ~old_output_en &  new_output_en;
				// Implicit zero when output disabled
				value = (value & ~old_output_en) | (port & new_output_en);

				for(PinNumber i = 0; i < available_pins; i++) {
					const auto bitoffs = (1l << i);
					if(bitoffs & newly_output_disabled_bits) {
						// we can't differentiate between "driven by server" or "by client".
						// Server wins, so override.
						server.state.pins[i] = Pinstate::UNSET;
						server.pushPin(i, Tristate::UNSET);
					}
					else if (bitoffs & newly_output_enabled_bits) {
						server.state.pins[i] = value & bitoffs ? Pinstate::HIGH : Pinstate::LOW;
						server.pushPin(i, toTristate(server.state.pins[i]));
					}
				}
			}
			break;
		default:
			break;
		}
	}
	r.fn();
	if (r.write) {
		switch (r.addr) {
		case IOF_EN_REG_ADDR:
		case IOF_SEL_REG_ADDR:
			for (PinNumber i = 0; i < available_pins; i++) {
				if((1l << i) & iof_en) {
					{
					const auto iof = getIOF(i, iof_sel & (1l << i));
					//cout << "IOF for pin " << (int)i << " is " << (int) iof << endl;
					if(iof == Pinstate::UNSET)
						cerr << "[GPIO] Set invalid iof to pin " << (int)i << endl;
					else
						server.state.pins[i] = iof;
					}
				}
				// TODO: Upon IOF disable, set value and state according to (port & output_en)
			}
			break;
		case FALL_INTR_EN:
			// cout << "[GPIO] set fall_intr_en to ";
			// bitPrint(reinterpret_cast<unsigned char*>(&fall_intr_en), sizeof(uint32_t));
			break;
		case FALL_INTR_PEND:
			// cout << "[GPIO] set fall_intr_pending to ";
			// bitPrint(reinterpret_cast<unsigned char*>(&fall_intr_pending),sizeof(uint32_t));
			break;
		default:
			break;
		}
	}
}

void GPIO::transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
	router.transport(trans, delay);
}

bool GPIO::isServerConnected() {
	return server.isConnected();
}

void GPIO::asyncOnchange(PinNumber bit, Tristate val) {
	const auto state_prev = server.state.pins[bit];

	switch(val){
	case Tristate::UNSET:
		/*
		 * TODO: Maybe do pullup/down decision here or emplace into a queue
		 * Queue would be better for sync, but unsure about performance.
		 * May be less overhead as only changed pins can be handled.
		 * But moving a bitmask unconditionally for 32 Pins is pretty fast, too.
		 */

	case Tristate::LOW:
	case Tristate::HIGH:
		server.state.pins[bit] = toPinstate(val);
		break;
	default:
		cout << "[GPIO] Invalid pin value " << (int) val << " on pin " << bit << endl;
	}

	if(state_prev != server.state.pins[bit]){
		// cout << "[GPIO] Bit " << (unsigned) bit << " changed to " << (unsigned)
		// val << endl;
		asyncEvent.notify();
	}
}

void GPIO::synchronousChange() {
	// cout << "[GPIO] might have changed!" << endl;

	gpio::State serverSnapshot = server.state;

	// This is seriously more complicated just handling the last updated bit
	// from asyncChange. But because we have to wait until the update phase, and
	// until then there may fire multiple bits!

	for (PinNumber i = 0; i < available_pins; i++) {
		const auto bitmask = 1l << i;
		if (input_en & bitmask) {

			// Small optimization: If not set as input, unset will stay unset even if not pullup enabled.
			if (serverSnapshot.pins[i] == Pinstate::UNSET) {
				if(pullup_en & bitmask)
					serverSnapshot.pins[i] = Pinstate::HIGH;
				else
					serverSnapshot.pins[i] = Pinstate::LOW;
			}

			// cout << "bit " << (unsigned) i << " is input enabled ";
			if (!(value & bitmask) && serverSnapshot.pins[i] == Pinstate::HIGH) {
				// cout << " changed to 1 ";
				if (rise_intr_en & bitmask) {
					// cout << "and interrupt is enabled ";
					// interrupt pending is inverted
					if (~rise_intr_pending & bitmask) {
						// cout << "but not yet consumed" << endl;
					} else {
						// cout << "and is being fired at " << int_gpio_base + i
						// << endl;
						rise_intr_pending &= ~bitmask;
						plic->gateway_trigger_interrupt(int_gpio_base + i);
					}
				} else {
					// cout << "but no interrupt is registered." << endl;
				}
				// transfer to value register
				value |= bitmask;
			} else if ((value & bitmask) && serverSnapshot.pins[i] == Pinstate::LOW){
				// cout << " changed to 0 ";
				if (fall_intr_en & bitmask) {
					// cout << "and interrupt is enabled ";
					// interrupt pending is inverted
					if (~fall_intr_pending & bitmask) {
						// cout << "but not yet consumed" << endl;
					} else {
						// cout << "and is being fired at " << int_gpio_base + i
						// << endl;
						fall_intr_pending &= ~bitmask;
						plic->gateway_trigger_interrupt(int_gpio_base + i);
					}
				} else {
					// cout << "but no interrupt is registered." << endl;
				}
				// transfer to value register
				value &= ~bitmask;
			} else {
				// This pin did not change
			}
		}
	}

	// if something changed between snapshot and now, change is discarded. "Yeet"
	server.state = serverSnapshot;
}

UartTXFunction GPIO::getUartTransmitFunction(const gpio::PinNumber tx){
	return bind(&GpioServer::pushUART, &server, tx, placeholders::_1);
}
void GPIO::registerUartReceiveFunction(const gpio::PinNumber rx, UartRXFunction fun){
	server.registerUARTRX(rx, fun);
}


SpiWriteFunction GPIO::getSPIwriteFunction(gpio::PinNumber cs) {
	const auto pin = getPinOffsFromSPIcs(cs);
	return bind(&GpioServer::pushSPI, &server, pin, placeholders::_1);
}
