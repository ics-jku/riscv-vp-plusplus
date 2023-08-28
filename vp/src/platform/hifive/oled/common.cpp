/*
 * common.cpp
 *
 *  Created on: 24 Sep 2019
 *      Author: dwd
 */

#include "common.hpp"

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>

namespace ss1106 {

IMPL_ENUM(Operator);

State* getSharedState() {
	int shmid;
	for (unsigned i = 0; i < 2; i++) {
		if ((shmid = shmget(shm_key, sizeof(State), IPC_CREAT | 0660)) >= 0)
			continue;

		std::cerr << "Could not get " << sizeof(State) << " Byte of shared Memory " << int(shm_key)
		          << " for oled display" << std::endl;
		perror("shmget");
		std::cerr << "Trying again...\n" << std::endl;

		if (-1 == (shmctl(shm_key, IPC_RMID, nullptr))) {
			std::cerr << "Could not destroy SHM" << std::endl;
			perror("shmctl");
			return nullptr;
		}
	}

	void* addr = shmat(shmid, nullptr, 0);
	if (addr == nullptr) {
		perror("shmat");
		return nullptr;
	}

	return reinterpret_cast<State*>(addr);
}

};  // namespace ss1106
