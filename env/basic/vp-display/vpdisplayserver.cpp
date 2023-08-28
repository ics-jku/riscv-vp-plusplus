#include "vpdisplayserver.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>

#include <iostream>

VPDisplayserver::VPDisplayserver(unsigned int sharedMemoryKey) : mSharedMemoryKey(sharedMemoryKey), stop(false) {}

VPDisplayserver::~VPDisplayserver() {
	stop.store(true);
	active_watch.join();
}

Framebuffer* VPDisplayserver::createSM() {
	int shmid;
	// TODO: Dont create, but check if exists
	if ((shmid = shmget(mSharedMemoryKey, sizeof(Framebuffer), 0666)) < 0) {
		perror("shmget");
		exit(1);
	}
	std::cout << "SHMID: " << shmid << std::endl;
	framebuffer = reinterpret_cast<Framebuffer*>(shmat(shmid, nullptr, 0));
	if (framebuffer == (Framebuffer*)-1) {
		perror("shmat");
		exit(1);
	}
	return framebuffer;
}

void VPDisplayserver::startListening(std::function<void(bool)> notifyChange) {
	// TODO: While loop around buffer changer
	active_watch = std::thread([=]() {
		uint8_t lastFrame = -1;
		while (!stop.load()) {
			if (framebuffer->activeFrame != lastFrame) {
				notifyChange(true);
				lastFrame = framebuffer->activeFrame;
			}
			// std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	});
	notifyChange(true);
}
