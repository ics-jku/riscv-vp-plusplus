#pragma once

#include "devices/interface/cdevice.h"

class CFactory {
public:
	typedef std::function<CDevice*(DeviceID id)> Creator;
	bool registerDeviceType(DeviceClass classname, Creator creator);

	void printAvailableDevices();
	bool deviceExists(DeviceClass classname);
	CDevice* instantiateDevice(DeviceID id, DeviceClass classname);
private:
	std::unordered_map<DeviceClass, Creator> devices;
};

CFactory& getCFactory();
template <typename Derived>
CDevice* deviceCreator(DeviceID id) { return new Derived(id); }
