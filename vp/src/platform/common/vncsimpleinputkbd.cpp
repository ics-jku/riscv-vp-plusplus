#include "vncsimpleinputkbd.h"

#define KBD_EVENT_QUEUE_SIZE 10
#define REFRESH_RATE 30 /* Hz */

#define REG_CTRL_ADDR 0x00
#define REG_KEY_ADDR 0x04

#define REG_CTRL_ENABLE_BIT (1 << 0)
#define REG_KEY_DATA_AVAIL_BIT (1 << 31)
#define REG_KEY_PRESSED_BIT (1 << 0)
#define REG_KEY_CODE_MASK (0x7FFFFFFE)
#define REG_KEY_CODE_SHIFT (1)

#define IS_ENABLED() ((reg_ctrl & REG_CTRL_ENABLE_BIT) == REG_CTRL_ENABLE_BIT)

VNCSimpleInputKbd::VNCSimpleInputKbd(sc_core::sc_module_name, VNCServer &vncServer, uint32_t irq)
    : vncServer(vncServer), irq(irq) {
	tsock.register_b_transport(this, &VNCSimpleInputKbd::transport);

	router
	    .add_register_bank({
	        {REG_CTRL_ADDR, &reg_ctrl},
	        {REG_KEY_ADDR, &reg_key},
	    })
	    .register_handler(this, &VNCSimpleInputKbd::register_access_callback);

	interrupt = false;
	vncServer.setVNCInputKbd(this);

	SC_THREAD(updateProcess);
}

void VNCSimpleInputKbd::doKbd(rfbBool down, rfbKeySym key) {
	uint32_t lkey = VNCServer::keySymToLinuxKeyCode(key);
	if (lkey == 0) {
		/* NOT SUPPORTED */
		return;
	}

	mutex.lock();
	if (IS_ENABLED() && (kbdEvents.size() < KBD_EVENT_QUEUE_SIZE)) {
		uint32_t key = REG_KEY_DATA_AVAIL_BIT | ((lkey << REG_KEY_CODE_SHIFT) & REG_KEY_CODE_MASK) |
		               (down ? REG_KEY_PRESSED_BIT : 0);
		kbdEvents.push(key);
		interrupt = true;
	}
	mutex.unlock();
}

void VNCSimpleInputKbd::register_access_callback(const vp::map::register_access_t &r) {
	if (r.write && (r.vptr == &reg_ctrl)) {
		/* reset fifo on any write to ctrl */
		mutex.lock();
		kbdEvents = {};
		interrupt = false;
		reg_key = 0;
		mutex.unlock();

	} else if (r.read && (r.vptr == &reg_key)) {
		/* load next event on read of key */
		bool notify = false;
		mutex.lock();
		int size = kbdEvents.size();
		if (size > 0) {
			/* get next event */
			reg_key = kbdEvents.front();
			kbdEvents.pop();
			/* still elements? */
			if (size > 1) {
				notify = true;
			}
		} else {
			reg_key &= ~REG_KEY_DATA_AVAIL_BIT;
		}
		mutex.unlock();

		if (notify) {
			plic->gateway_trigger_interrupt(irq);
		}
	}

	r.fn();
}

void VNCSimpleInputKbd::transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
	router.transport(trans, delay);
}

void VNCSimpleInputKbd::updateProcess() {
	while (vncServer.isActive()) {
		wait(1000000L / REFRESH_RATE, sc_core::SC_US);

		mutex.lock();
		bool notify = interrupt;
		interrupt = false;
		mutex.unlock();

		if (notify) {
			plic->gateway_trigger_interrupt(irq);
		}
	}
}
