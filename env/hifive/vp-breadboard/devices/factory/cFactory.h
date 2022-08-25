#pragma once

#include "devices/interface/cDevice.h"

class CFactory {
public:
	typedef std::function<std::unique_ptr<CDevice>(DeviceID id)> Creator;

	template <typename Derived>
	bool registerDeviceType() {
		return devices.insert(std::make_pair(Derived::classname, [](DeviceID id) {return std::make_unique<Derived>(id);})).second;
	}

	void printAvailableDevices();
	bool deviceExists(DeviceClass classname);
	std::unique_ptr<CDevice> instantiateDevice(DeviceID id, DeviceClass classname);
private:
	std::unordered_map<DeviceClass, Creator> devices;
};

CFactory& getCFactory();
