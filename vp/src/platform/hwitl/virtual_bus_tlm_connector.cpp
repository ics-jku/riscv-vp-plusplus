/*
 * virtual_bus_tlm_connector.cpp
 *
 *  Created on: Oct 20, 2022
 *      Author: dwd
 */


#include "virtual_bus_tlm_connector.hpp"

using namespace std;
using namespace sc_core;
using namespace tlm_utils;
using namespace tlm;

VirtualBusMember::VirtualBusMember(sc_module_name name,
		Initiator& virtual_bus_connector,
		hwitl::Address base_address)
    : sc_module(name), virtual_bus(virtual_bus_connector), base_address(base_address)  {
	tsock.register_b_transport(this, &VirtualBusMember::transport);
	m_read_delay = sc_time(SC_ZERO_TIME);
	m_write_delay = sc_time(SC_ZERO_TIME);

	// TODO: Issue Reset command to remote device

	// Zero time: Special case, dont update
	m_interrupt_polling_delay = SC_ZERO_TIME;
	SC_THREAD(interrupt_service);
}

void VirtualBusMember::setDelayTimes(sc_time read_delay, sc_time write_delay) {
	m_read_delay = read_delay;
	m_write_delay = write_delay;
}

void VirtualBusMember::setInterruptRoutine(std::function<void(void)> trigger_target, sc_core::sc_time polling_time) {
	trigger_interrupt_callback = trigger_target;
	m_interrupt_polling_delay = polling_time;
	m_interrupt_event.notify(m_interrupt_polling_delay);
}

void VirtualBusMember::transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
	tlm::tlm_command cmd = trans.get_command();
	unsigned addr = trans.get_address();
	auto len = trans.get_data_length();

	if(len > sizeof(hwitl::Payload)) {
		cerr << "[virtual_bus_tlm_connector] accesses of length > " << sizeof(hwitl::Payload) <<
				" currently unsupported (was " << len << ")" << endl;
		trans.set_response_status(tlm_response_status::TLM_ADDRESS_ERROR_RESPONSE);
		return;
	}

	hwitl::Payload temp;
	hwitl::Payload* data = &temp;
	const bool unaligned = len != sizeof(hwitl::Payload);
	if(!unaligned) {
		data = reinterpret_cast<hwitl::Payload*>(trans.get_data_ptr());
	} else {
		temp = 0;
		if(cmd == tlm::TLM_WRITE_COMMAND) {
			// This just ignores word size and writes to unaligned addresses.
			// WARN: This lets the actual peripheral handle this
			assert(len <= sizeof(hwitl::Payload));	// should always be the case
			memcpy(data, trans.get_data_ptr(), len);
		}
	}

	if (cmd == tlm::TLM_WRITE_COMMAND) {
		const auto response = virtual_bus.write(base_address + addr, *data);
		switch(response) {
		case hwitl::ResponseStatus::Ack::ok:
			break;
		case hwitl::ResponseStatus::Ack::not_mapped:
			trans.set_response_status(tlm_response_status::TLM_ADDRESS_ERROR_RESPONSE);
			cerr << "[virtual_bus_tlm_connector] Write Address " << showbase << hex << base_address + addr << dec << " not mapped" << endl;
			break;
		default:
			trans.set_response_status(tlm_response_status::TLM_GENERIC_ERROR_RESPONSE);
			cerr << "[virtual_bus_tlm_connector] Write Error '" << static_cast<unsigned>(response) << "' at address " << showbase << hex << base_address + addr << dec << endl;
		}
		delay += m_write_delay;
	} else if (cmd == tlm::TLM_READ_COMMAND) {
		const auto response = virtual_bus.read(base_address + addr);
		if(!response) {
			trans.set_response_status(tlm_response_status::TLM_GENERIC_ERROR_RESPONSE);
			cerr << "[virtual_bus_tlm_connector] Could not read from remote" << endl;
			return;
		}
		switch(response->getStatus().ack) {
		case hwitl::ResponseStatus::Ack::ok:
			*data = response->getPayload();
			break;
		case hwitl::ResponseStatus::Ack::not_mapped:
			trans.set_response_status(tlm_response_status::TLM_ADDRESS_ERROR_RESPONSE);
			cerr << "[virtual_bus_tlm_connector] Read Address " << showbase << hex << base_address + addr << dec << " not mapped" << endl;
			break;
		default:
			trans.set_response_status(tlm_response_status::TLM_GENERIC_ERROR_RESPONSE);
			cerr << "[virtual_bus_tlm_connector] Read Error " << static_cast<unsigned>(response->getStatus().ack) << " at address " << showbase << hex << base_address + addr << dec << endl;
		}
		if(unaligned) {
			assert(len <= sizeof(hwitl::Payload));	// should always be the case
			memcpy(trans.get_data_ptr(), data, len);
		}
		delay += m_read_delay;
	} else {
		sc_assert(false && "unsupported tlm command");
	}

}

void VirtualBusMember::interrupt_service() {
	while (true) {
		if(m_interrupt_polling_delay > SC_ZERO_TIME)
			m_interrupt_event.notify(m_interrupt_polling_delay);
		sc_core::wait(m_interrupt_event);

		if(trigger_interrupt_callback && virtual_bus.getInterrupt())
			trigger_interrupt_callback();
	}
}
