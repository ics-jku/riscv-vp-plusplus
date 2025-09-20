#ifndef RISCV_VP_FU540_I2C_H
#define RISCV_VP_FU540_I2C_H

#include <systemc>

#include "i2c_if.h"
#include "util/tlm_map.h"

class FU540_I2C : public sc_core::sc_module, public I2C_IF {
   public:
	const int interrupt = 0;
	interrupt_gateway *plic = nullptr;

	tlm_utils::simple_target_socket<FU540_I2C> tsock;

	FU540_I2C(const sc_core::sc_module_name &name, const int interrupt);

	SC_HAS_PROCESS(FU540_I2C);

   private:
	bool enabled = false;
	bool interrupt_enabled = false;

	uint32_t reg_prer_lo = 0xFF;
	uint32_t reg_prer_hi = 0xFF;
	uint32_t reg_ctr = 0x00;
	uint32_t reg_txr = 0x00;
	uint32_t reg_rxr = 0x00;
	uint32_t reg_sr = 0x00;

	bool sendACK = false;
	bool rxack = false;
	bool busy = false;
	bool arbitrationLost = false;
	bool transferInProgress = false;
	bool interruptFlag = false;

	vp::map::LocalRouter router = {"FU540_I2C"};

	enum {
		// Clock Prescale register lo-byte
		REG_PRER_LO = 0x00,
		// Clock Prescale register hi-byte
		REG_PRER_HI = 0x04,
		// Control register
		REG_CTR = 0x08,
		// Transmit & Receive register
		REG_TXR = 0x0C,
		REG_RXR = 0x0C,
		REG_TXR_RXR = 0x0C,
		// Command & Status register
		REG_CR = 0x10,
		REG_SR = 0x10,
		REG_CR_SR = 0x10,
	};

	void transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay);
	void register_update_callback(const vp::map::register_access_t &r);
	void triggerInterrupt();
	uint8_t getStatusRegister();
};

#endif  // RISCV_VP_FU540_I2C_H
