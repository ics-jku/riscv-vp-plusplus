#include "gpio.h"

using namespace std;
using namespace gpio;

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
		if (r.vptr == &value) {
			cerr << "[GPIO] write to value register is ignored!" << endl;
			return;
		} else if (r.vptr == &pullup_en) {
			// cout << "[GPIO] pullup changed" << endl;
			// bitPrint(reinterpret_cast<unsigned char*>(&pullup_en),
			// sizeof(uint32_t));
			const auto newly_pulled_up_bits = (r.nv ^ pullup_en) & r.nv;
			value |= newly_pulled_up_bits;
			for(PinNumber i = 0; i < available_pins; i++) {
				if((1l << i) & newly_pulled_up_bits) {
					server.state.pins[i] = Tristate::HIGH;
				}
			}
		} else if(r.vptr == &output_en) {
			const auto newly_output_disabled_bits = (r.nv ^ output_en) & output_en;
			value &= ~(newly_output_disabled_bits);
			for(PinNumber i = 0; i < available_pins; i++) {
				if((1l << i) & newly_output_disabled_bits) {
					server.state.pins[i] = Tristate::UNSET;
				}
			}
		}
	}
	r.fn();
	if (r.write) {
		if (r.vptr == &port) {
			// cout << "[GPIO] new Port value: ";
			// bitPrint(reinterpret_cast<unsigned char*>(&port),

			// value and server.state might differ, if a bit is changed by
			// client and the synchronous_change was not fired yet.
			const auto valid_output = (port & output_en);
			value = (value & ~output_en) | valid_output;

			for(PinNumber i = 0; i < available_pins; i++) {
				if((1l << i) & output_en) {
					server.state.pins[i] = valid_output & (1l << i) ? Tristate::HIGH : Tristate::LOW;
				}
			}
		} else if (r.vptr == &fall_intr_en) {
			// cout << "[GPIO] set fall_intr_en to ";
			// bitPrint(reinterpret_cast<unsigned char*>(&fall_intr_en),
			// sizeof(uint32_t));
		} else if (r.vptr == &fall_intr_pending) {
			// cout << "[GPIO] set fall_intr_pending to ";
			// bitPrint(reinterpret_cast<unsigned char*>(&fall_intr_pending),
			// sizeof(uint32_t));
		}
	}
}

void GPIO::transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
	router.transport(trans, delay);
}

void GPIO::asyncOnchange(PinNumber bit, Tristate val) {
	const auto state_prev = server.state.pins[bit];

	switch(val){
	case Tristate::LOW:
	case Tristate::HIGH:
		server.state.pins[bit] = val;
		break;
	default:
		cout << "[GPIO] Ignoring other tristates for now" << endl;
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
			// cout << "bit " << (unsigned) i << " is input enabled ";
			if (!(value & bitmask) && serverSnapshot.pins[i] == Tristate::HIGH) {
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
			} else if ((value & bitmask) && serverSnapshot.pins[i] == Tristate::LOW){
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
				cerr << "[GPIO] This branch should not yet be reachable" << endl;
				// TODO: If Tristate::Unset, determine if pullup or pulldown is set and decide port value state
			}
		}
	}
	// TODO: Should routine recheck if something was changed in the meantime?
}
