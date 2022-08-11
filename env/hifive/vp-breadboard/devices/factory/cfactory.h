#pragma once

#include "devices/interface/cdevice.h"

class CFactory {

public:
	void printAvailableDevices();
	bool deviceExists(DeviceClass classname);
	CDevice* instantiateDevice(DeviceID id, DeviceClass classname);
};
