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
}

void VirtualBusMember::set_delay(sc_time read_delay, sc_time write_delay) {
	m_read_delay = read_delay;
	m_write_delay = write_delay;
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
		// todo
		assert(false && "[virtual_bus_tlm_connector] unaligned access currently unsupported");
	}

	if (cmd == tlm::TLM_WRITE_COMMAND) {
		const auto response = virtual_bus.write(base_address + addr, *data);
		switch(response) {
		case hwitl::ResponseStatus::Ack::ok:
			break;
		case hwitl::ResponseStatus::Ack::not_mapped:
			trans.set_response_status(tlm_response_status::TLM_ADDRESS_ERROR_RESPONSE);
			break;
		default:
			trans.set_response_status(tlm_response_status::TLM_GENERIC_ERROR_RESPONSE);
		}
		delay += m_write_delay;
	} else if (cmd == tlm::TLM_READ_COMMAND) {
		const auto response = virtual_bus.read(base_address + addr);
		switch(response.status.ack) {
		case hwitl::ResponseStatus::Ack::ok:
			*data = response.payload;
			break;
		case hwitl::ResponseStatus::Ack::not_mapped:
			trans.set_response_status(tlm_response_status::TLM_ADDRESS_ERROR_RESPONSE);
			break;
		default:
			trans.set_response_status(tlm_response_status::TLM_GENERIC_ERROR_RESPONSE);
		}
		delay += m_read_delay;
	} else {
		sc_assert(false && "unsupported tlm command");
	}

}
