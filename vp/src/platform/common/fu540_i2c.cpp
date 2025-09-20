#include "fu540_i2c.h"

#define I2C_CTR_EN (1 << 7)
#define I2C_CTR_IEN (1 << 6)
#define I2C_TX_ADDR (0xFF ^ 1)
#define I2C_TX_WR 1
#define I2C_CR_STA (1 << 7)
#define I2C_CR_STO (1 << 6)
#define I2C_CR_RD (1 << 5)
#define I2C_CR_WR (1 << 4)
#define I2C_CR_ACK (1 << 3)
#define I2C_CR_IACK (1)
#define I2C_SR_RXACK (1 << 7)
#define I2C_SR_BUSY (1 << 6)
#define I2C_SR_AL (1 << 5)
#define I2C_SR_TIP (1 << 1)
#define I2C_SR_IF (1)

FU540_I2C::FU540_I2C(const sc_core::sc_module_name &name, const int interrupt)
    : sc_module(name), interrupt(interrupt), router("FU540_I2C") {
	tsock.register_b_transport(this, &FU540_I2C::transport);
	router
	    .add_register_bank({
	        {REG_PRER_LO, &reg_prer_lo},
	        {REG_PRER_HI, &reg_prer_hi},
	        {REG_CTR, &reg_ctr},
	        {REG_TXR_RXR, &reg_rxr},
	        {REG_CR_SR, &reg_sr},
	    })
	    .register_handler(this, &FU540_I2C::register_update_callback);
}

void FU540_I2C::transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
	router.transport(trans, delay);
}

void FU540_I2C::register_update_callback(const vp::map::register_access_t &r) {
	if (r.write) {
		switch (r.addr) {
			case REG_PRER_LO:  // Clock Prescale register lo-byte
				r.fn();
				break;
			case REG_PRER_HI:  // Clock Prescale register hi-byte
				r.fn();
				break;
			case REG_CTR:  // Control register
				r.fn();
				enabled = (reg_ctr & I2C_CTR_EN);             // i2c core enable bit
				interrupt_enabled = (reg_ctr & I2C_CTR_IEN);  // i2c interrupt enable bit
				break;
			case REG_TXR:  // Transmit register
				if (enabled) {
					reg_txr = r.nv;
				}
				break;
			case REG_CR:  // Command register
				if (enabled) {
					uint8_t command = r.nv;

					// interrupt acknowledge
					if (command & I2C_CR_IACK) {
						interruptFlag = false;
					}

					// Process I2C commands
					if (command & (I2C_CR_STA | I2C_CR_WR | I2C_CR_RD)) {
						transferInProgress = true;

						if (command & I2C_CR_STA) {
							// START condition
							busy = true;
							arbitrationLost = false;

							// get adress
							uint8_t addr = (reg_txr & I2C_TX_ADDR) >> 1;

							// Attempt to start communication with device
							bool ack = I2C_IF::start(addr);
							rxack = !ack;  // RxACK is inverted (1 = NACK, 0 = ACK)

							// If no device responds, clear busy immediately
							if (!ack) {
								busy = false;
							}

						} else if (command & I2C_CR_WR) {  // Write operation
							bool ack = I2C_IF::write(reg_txr);
							rxack = !ack;

						} else if (command & I2C_CR_RD) {  // read operation
							uint8_t data;
							bool ack = I2C_IF::read(data);
							if (ack) {
								reg_rxr = data;
							}
							rxack = !ack;

							// Send ACK/NACK based on ACK bit in command
							sendACK = !(command & I2C_CR_ACK);
						}

						transferInProgress = false;
						triggerInterrupt();
					}

					if (enabled && (command & I2C_CR_STO)) {  // STOP condition
						I2C_IF::stop();
						busy = false;
					}
				}
				break;
		}
	} else if (r.read) {
		reg_sr = getStatusRegister();
		r.fn();
	}
}

void FU540_I2C::triggerInterrupt() {
	if (plic != nullptr && interrupt_enabled && !interruptFlag) {
		plic->gateway_trigger_interrupt(interrupt);
	}
	interruptFlag = true;
}

uint8_t FU540_I2C::getStatusRegister() {
	uint8_t status = 0;
	if (rxack)
		status |= I2C_SR_RXACK;
	if (busy)
		status |= I2C_SR_BUSY;
	if (arbitrationLost)
		status |= I2C_SR_AL;
	if (transferInProgress)
		status |= I2C_SR_TIP;
	if (interruptFlag)
		status |= I2C_SR_IF;

	return status;
}
