#pragma once

#include "devices/factory/cFactory.h"

/* ToDo
const uint8_t COL_LOW= 0;
const uint8_t COMMANDS [1] = {COL_LOW};
*/

class TFT : public CDevice {
	bool is_data = false;
	State state;

   public:
	TFT(DeviceID id);
	~TFT();

	inline static DeviceClass classname = "tft";
	const DeviceClass getClass() const;

	class TFT_EXMC : public CDevice::EXMC_Interface_C {
	   public:
		TFT_EXMC(CDevice* device);
	};

	class TFT_Graph : public CDevice::Graphbuf_Interface_C {
	   public:
		TFT_Graph(CDevice* device);
		void initializeBufferMaybe();
	};
};

static const bool registeredTFT = getCFactory().registerDeviceType<TFT>();
