#include "vncsimpleinput.h"

#define PTR_EVENT_QUEUE_SIZE 10
#define REFRESH_RATE 30 /* Hz */

#define REG_CTRL_PTR_ENABLE_BIT (1 << 0)
#define REG_BUTTONMASK_PTR_DATA_AVAIL_BIT (1 << 31)
#define REG_BUTTONMASK_PTR_MOUSE_LEFT_BIT (1 << 0)
#define REG_BUTTONMASK_PTR_MOUSE_MIDDLE_BIT (1 << 1)
#define REG_BUTTONMASK_PTR_MOUSE_RIGHT_BIT (1 << 2)

#define REG_CTRL_PTR_ADDR 0x00
#define REG_WIDTH_PTR_ADDR 0x04
#define REG_HEIGHT_PTR_ADDR 0x08
#define REG_X_PTR_ADDR 0x0c
#define REG_Y_PTR_ADDR 0x10
#define REG_BUTTONMASK_PTR_ADDR 0x14

#define IS_ENABLED() ((reg_ctrl_ptr & REG_CTRL_PTR_ENABLE_BIT) == REG_CTRL_PTR_ENABLE_BIT)

VNCSimpleInput::VNCSimpleInput(sc_core::sc_module_name, VNCServer &vncServer, uint32_t irq)
    : vncServer(vncServer), irq(irq) {
	tsock.register_b_transport(this, &VNCSimpleInput::transport);

	router
	    .add_register_bank({
	        {REG_CTRL_PTR_ADDR, &reg_ctrl_ptr},
	        {REG_WIDTH_PTR_ADDR, &reg_width_ptr},
	        {REG_HEIGHT_PTR_ADDR, &reg_height_ptr},
	        {REG_X_PTR_ADDR, &reg_x_ptr},
	        {REG_Y_PTR_ADDR, &reg_y_ptr},
	        {REG_BUTTONMASK_PTR_ADDR, &reg_buttonmask_ptr},
	    })
	    .register_handler(this, &VNCSimpleInput::register_access_callback);

	vncServer.setVNCInput(this);

	SC_THREAD(updateProcess);
}

VNCSimpleInput::~VNCSimpleInput(void) {}

void VNCSimpleInput::doPtr(int buttonMask, int x, int y) {
	// printf("%s.%s.%i %ix%i 0x%X\n", __FILE__, __FUNCTION__, __LINE__,x,y,buttonMask);
	mutex.lock();
	if (IS_ENABLED() && (ptrEvents.size() < PTR_EVENT_QUEUE_SIZE)) {
		ptrEvents.push(std::make_tuple(buttonMask, x, y));
	}
	mutex.unlock();
}

void VNCSimpleInput::doKbd(rfbBool down, rfbKeySym key) {
	// printf("%s.%s.%i %i %i\n", __FILE__, __FUNCTION__, __LINE__, down, key);
}

void VNCSimpleInput::register_access_callback(const vp::map::register_access_t &r) {
	if (r.write) {
		if (r.vptr == &reg_width_ptr || r.vptr == &reg_height_ptr) {
			/* ignore */
			return;
		}
		if (r.vptr == &reg_ctrl_ptr) {
			/* reset fifo on any write to ctrl */
			mutex.lock();
			ptrEvents = {};
			reg_buttonmask_ptr = 0;
			mutex.unlock();
		}

	} else if (r.read) {
		if (r.vptr == &reg_buttonmask_ptr) {
			/* load next event on read of buttonmask */
			mutex.lock();
			int size = ptrEvents.size();
			if (size > 0) {
				/* get next event */
				std::tie(reg_buttonmask_ptr, reg_x_ptr, reg_y_ptr) = ptrEvents.front();
				ptrEvents.pop();
				/* still elements? */
				if (size > 1) {
					reg_buttonmask_ptr |= REG_BUTTONMASK_PTR_DATA_AVAIL_BIT;
					plic->gateway_trigger_interrupt(irq);
				} else {
					reg_buttonmask_ptr &= ~REG_BUTTONMASK_PTR_DATA_AVAIL_BIT;
				}
			}
			mutex.unlock();
		}
	}
	r.fn();
}

void VNCSimpleInput::transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
	router.transport(trans, delay);
}

void VNCSimpleInput::updateProcess() {
	reg_width_ptr = vncServer.getWidth();
	reg_height_ptr = vncServer.getHeight();

	while (vncServer.isActive()) {
		wait(1000000L / REFRESH_RATE, sc_core::SC_US);
		mutex.lock();
		bool notify = (IS_ENABLED() && ptrEvents.size());
		mutex.unlock();
		if (notify) {
			plic->gateway_trigger_interrupt(irq);
		}
	}
}
