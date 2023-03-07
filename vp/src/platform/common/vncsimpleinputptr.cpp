#include "vncsimpleinputptr.h"

#define PTR_EVENT_QUEUE_SIZE 10
#define REFRESH_RATE 30 /* Hz */

/* Registers and Bits */
#define REG_CTRL_ADDR 0x00
#define REG_WIDTH_ADDR 0x04
#define REG_HEIGHT_ADDR 0x08
#define REG_X_ADDR 0x0c
#define REG_Y_ADDR 0x10
#define REG_BUTTONMASK_ADDR 0x14

#define REG_CTRL_ENABLE_BIT (1 << 0)
#define REG_BUTTONMASK_DATA_AVAIL_BIT (1 << 31)
#define REG_BUTTONMASK_MOUSE_LEFT_BIT (1 << 0)
#define REG_BUTTONMASK_MOUSE_MIDDLE_BIT (1 << 1)
#define REG_BUTTONMASK_MOUSE_RIGHT_BIT (1 << 2)

#define IS_ENABLED() ((reg_ctrl & REG_CTRL_ENABLE_BIT) == REG_CTRL_ENABLE_BIT)

VNCSimpleInputPtr::VNCSimpleInputPtr(sc_core::sc_module_name, VNCServer &vncServer, uint32_t irq)
    : vncServer(vncServer), irq(irq) {
	tsock.register_b_transport(this, &VNCSimpleInputPtr::transport);

	router
	    .add_register_bank({
	        {REG_CTRL_ADDR, &reg_ctrl},
	        {REG_WIDTH_ADDR, &reg_width},
	        {REG_HEIGHT_ADDR, &reg_height},
	        {REG_X_ADDR, &reg_x},
	        {REG_Y_ADDR, &reg_y},
	        {REG_BUTTONMASK_ADDR, &reg_buttonmask},
	    })
	    .register_handler(this, &VNCSimpleInputPtr::register_access_callback);

	interrupt = false;
	vncServer.setVNCInputPtr(this);

	SC_THREAD(updateProcess);
}

void VNCSimpleInputPtr::doPtr(int buttonMask, int x, int y) {
	// printf("%s.%s.%i %ix%i 0x%X\n", __FILE__, __FUNCTION__, __LINE__,x,y,buttonMask);
	mutex.lock();
	if (IS_ENABLED() && (ptrEvents.size() < PTR_EVENT_QUEUE_SIZE)) {
		ptrEvents.push(std::make_tuple(REG_BUTTONMASK_DATA_AVAIL_BIT | buttonMask, x, y));
		interrupt = true;
	}
	mutex.unlock();
}

void VNCSimpleInputPtr::register_access_callback(const vp::map::register_access_t &r) {
	if (r.write) {
		if (r.vptr == &reg_width || r.vptr == &reg_height) {
			/* ignore */
			return;

		} else if (r.vptr == &reg_ctrl) {
			/* reset fifo on any write to ctrl */
			mutex.lock();
			ptrEvents = {};
			interrupt = false;
			reg_buttonmask = 0;
			mutex.unlock();
		}

	} else if (r.read) {
		if (r.vptr == &reg_buttonmask) {
			/* load next event on read of buttonmask */
			bool notify = false;
			mutex.lock();
			int size = ptrEvents.size();
			if (size > 0) {
				/* get next event */
				std::tie(reg_buttonmask, reg_x, reg_y) = ptrEvents.front();
				ptrEvents.pop();
				/* still elements? */
				if (size > 1) {
					notify = true;
				} else {
				}
			} else {
				reg_buttonmask &= ~REG_BUTTONMASK_DATA_AVAIL_BIT;
			}
			mutex.unlock();

			if (notify) {
				plic->gateway_trigger_interrupt(irq);
			}
		}
	}

	r.fn();
}

void VNCSimpleInputPtr::transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
	router.transport(trans, delay);
}

void VNCSimpleInputPtr::updateProcess() {
	reg_width = vncServer.getWidth();
	reg_height = vncServer.getHeight();

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
