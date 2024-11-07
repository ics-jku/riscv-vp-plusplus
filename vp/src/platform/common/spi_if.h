#ifndef RISCV_VP_SPI_IF_H
#define RISCV_VP_SPI_IF_H

#include <map>
#include <queue>
#include <systemc>

#include "core/common/irq_if.h"
#include "util/tlm_map.h"

/* generic spi device interface */
class SPI_Device_IF {
	friend class SPI_IF;

   protected:
	virtual void select(bool ena) = 0;
	virtual uint8_t transfer(uint8_t mosi) = 0;
};

/*
 * simple spi device for spi transfer function mapping
 * (wrapper for hifive platform)
 */
class SPI_SimpleDevice : public SPI_Device_IF {
   public:
	typedef std::function<uint8_t(uint8_t)> TransferF_T;

   private:
	SPI_SimpleDevice::TransferF_T transferF;
	static uint8_t transferF_dummy(uint8_t) {
		return 0xff;
	}

	/* implementation of SPI_Device_IF */
   protected:
	void select(bool ena) override{};
	uint8_t transfer(uint8_t mosi) override {
		return transferF(mosi);
	};

   public:
	/* connect transfer function */
	void connect_transfer_function(TransferF_T transferF) {
		if (transferF == nullptr) {
			this->transferF = transferF_dummy;
		} else {
			this->transferF = transferF;
		}
	}

	SPI_SimpleDevice(TransferF_T transferF = transferF_dummy) {
		connect_transfer_function(transferF);
	}
};

/* spi host interface */
class SPI_IF {
	// TODO: use a simply a vector here
	std::map<unsigned int, SPI_Device_IF *> devices;
	SPI_Device_IF *selected_dev = nullptr;

   protected:
	void device_deselect() {
		if (selected_dev == nullptr) {
			return;
		}

		selected_dev->select(false);
		selected_dev = nullptr;
	}

	void device_select(unsigned int cs, SPI_Device_IF *device) {
		if (device_is_selected(device)) {
			/* device already selected -> nothing to do */
			return;
		}

		if (selected_dev != nullptr) {
			/* other device selected -> deselect */
			device_deselect();
		}

		selected_dev = device;
		selected_dev->select(true);
	}

	bool transfer(unsigned int cs, bool select, bool deselect, uint8_t mosi, uint8_t &miso) {
		SPI_Device_IF *device = get_device(cs);
		if (device == nullptr) {
			std::cerr << "SPI: WARNING: Transfer on unregistered Chip-Select " << cs << std::endl;
			miso = 0xff;
			return false;
		}

		if (select) {
			device_select(cs, device);
		}

		miso = device->transfer(mosi);

		if (deselect) {
			device_deselect();
		}

		return true;
	}

   public:
	/* interface */
	virtual bool is_chipselect_valid(unsigned int cs) = 0;

	bool device_is_selected(SPI_Device_IF *device) {
		return selected_dev == device;
	}

	SPI_Device_IF *get_device(unsigned int cs) {
		auto device = devices.find(cs);
		if (device == devices.end()) {
			return nullptr;
		}
		return device->second;
	}

	bool connect_device(unsigned int cs, SPI_Device_IF *device) {
		if (!is_chipselect_valid(cs)) {
			std::cerr << "SPI_IF: Unsupported chip select " << cs << std::endl;
			return false;
		}

		/* if there is already a device connected and it is selected -> deselect */
		SPI_Device_IF *olddev = get_device(cs);
		if (olddev != nullptr && device_is_selected(olddev)) {
			device_deselect();
		}

		devices.insert(std::pair<const uint32_t, SPI_Device_IF *>(cs, device));
		return true;
	}
};

#endif  // RISCV_VP_SPI_IF_H
