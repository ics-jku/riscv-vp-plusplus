/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018 Hesham Almatary <Hesham.Almatary@cl.cam.ac.uk>
 *
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory (Department of Computer Science and
 * Technology) under DARPA contract HR0011-18-C-0016 ("ECATS"), as part of th
 * DARPA SSITH research programme.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef AF_INET
#include <sys/socket.h>
#endif
#ifndef INADDR_ANY
#include <netinet/in.h>
#endif

#include <signal.h>

#include <algorithm>
#include <cassert>
#include <cstdio>

#include "rvfi_dii.h"

namespace cheriv9::rv64 {

rvfi_dii_t::rvfi_dii_t(uint16_t port, TaggedMemory &mem) : mem(mem), socket_fd(0), client_fd(0) {
	socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_fd == -1) {
		fprintf(stderr, "remote_rvfi_dii failed to make socket: %s (%d)\n", strerror(errno), errno);
		abort();
	}

	int reuseaddr = 1;
	if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(int)) == -1) {
		fprintf(stderr, "remote_rvfi_dii failed setsockopt: %s (%d)\n", strerror(errno), errno);
		abort();
	}

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = htons(port);

	if (bind(socket_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		fprintf(stderr, "remote_rvfi_dii failed to bind socket: %s (%d)\n", strerror(errno), errno);
		abort();
	}

	if (listen(socket_fd, 1) == -1) {
		fprintf(stderr, "remote_rvfi_dii failed to listen on socket: %s (%d)\n", strerror(errno), errno);
		abort();
	}

	socklen_t addrlen = sizeof(addr);
	if (getsockname(socket_fd, (struct sockaddr *)&addr, &addrlen) == -1) {
		fprintf(stderr, "remote_rvfi_dii getsockname failed: %s (%d)\n", strerror(errno), errno);
		abort();
	}

	printf("Listening for remote rvfi_dii connection on port %d.\n", ntohs(addr.sin_port));
	fflush(stdout);
}

void rvfi_dii_t::accept() {
	client_fd = ::accept(socket_fd, NULL, NULL);
	if (client_fd == -1) {
		if (errno == EAGAIN) {
			// No client waiting to connect right now.
		} else {
			fprintf(stderr, "failed to accept on socket: %s (%d)\n", strerror(errno), errno);
			abort();
		}
	}
}

void rvfi_dii_t::execute_command(ISS *s) {
	uint8_t command = 0;

	// Overwrite the processor's PC as the reset() writes it with default RSTVECTOR (0x10000)
	// s->get_core(0)->get_state()->pc = 0x80000000;
	s->pc = 0x80000000;

	while (1) {
		// Wait for a packet

		read_trace(&rvfi_dii_input);

		command = rvfi_dii_input.rvfi_dii_cmd;

		switch (command) {
			case 0: {
				s->rvfi_dii_output.rvfi_dii_halt = 1;
				write_trace(&(s->rvfi_dii_output));
				// // Zero the output trace for the next test
				s->rvfi_dii_output = {};
				// Reset the processor
				s->reset();
				mem.reset();
				s->pc = 0x80000000;
				s->dbbcache.set_pc(s->pc);
				s->dbbcache.set_last_pc(s->pc);
#ifdef ENABLE_CHERI
				// (static_cast<cheri_t *>(s->get_core(0)->get_extension()))->reset();
#endif  // ENABLE_CHERI
        //  Reset memories
        // std::vector<std::pair<reg_t, mem_t *>> mems = s->get_mems();
        // for (auto &x : mems)
        // {
        //   mem_t *mem = static_cast<mem_t *>(x.second);
        //   memset(mem->contents(), 0, mem->size());
        // }
        // Memory is reset in ISS::reset()
			} break;

			case 'B':
				command = 0;
				fprintf(stderr, "*BLINK*\n");
				break;
			case 1: {
				// s->step(1, sext32(rvfi_dii_input.rvfi_dii_insn));
				s->rvfi_dii_input = rvfi_dii_input;
				try {
					s->run_step();
				} catch (std::runtime_error &e) {
					fprintf(stderr, "Error: %s\n", e.what());
					s->rvfi_dii_output.rvfi_dii_pc_wdata = 0;
					s->rvfi_dii_output.rvfi_dii_trap = 1;  // TODO Could this be done in trap.h?
					s->rvfi_dii_output.rvfi_dii_pc_wdata =
					    0;  // TODO Could this be done in trap.h? // TODO Check if clearing this is the proper solution
					s->rvfi_dii_output.rvfi_dii_rs1_addr = 0;
					s->rvfi_dii_output.rvfi_dii_rs2_addr = 0;
					s->rvfi_dii_output.rvfi_dii_mem_addr = 0;
					s->pc = 0;
					// s->last_pc = 0; // TODO
					//  quit = true;
				}

				// /* Send back to client */
				// write_trace(&(s->get_core(0)->rvfi_dii_output));
				write_trace(&(s->rvfi_dii_output));
			} break;
			case 'Q':
				quit = true;
				break;
			default:
				fprintf(stderr, "rvfi_dii got unsupported command '%c'\n", command);
		}

		if (quit) {
			// The remote disconnected.
			fprintf(stderr, "Received a quit command. Quitting.\n");
			close(client_fd);
			client_fd = 0;
			exit(0);
		}
	}
}

void rvfi_dii_t::start(ISS *s) {
	if (client_fd > 0) {
		execute_command(s);
	} else {
		this->accept();
	}
}

void rvfi_dii_t::read_trace(rvfi_dii_command_t *input) {
	// int recv_start = 0;
	// int recv_end;
	// recv_start = 0;

	// Keep reading from socket until there is data to consume
	while ((read(client_fd, input, sizeof(rvfi_dii_command_t)) == -1) && errno == EAGAIN);

	if (errno == ECONNRESET) {
		// If a connection reset by peer, then exit
		printf("Clients closed sockets, exiting..\n");
		exit(0);
	}
}

void rvfi_dii_t::write_trace(rvfi_dii_trace_t *output) {
	sleep(0.001);
	// signal(SIGPIPE, SIG_IGN);
	// ssize_t bytes = write(client_fd, output, sizeof(rvfi_dii_trace_t));
	ssize_t bytes = send(client_fd, output, sizeof(rvfi_dii_trace_t), MSG_NOSIGNAL);
	if (bytes == -1) {
		sleep(1);
		bytes = send(client_fd, output, sizeof(rvfi_dii_trace_t), MSG_NOSIGNAL);
		if (bytes == -1) {
			// If a connection reset by peer, then exit
			printf("Clients closed sockets, exiting..\n");
			exit(0);
		}
	}
	// Additionally print trace to stdout for debugging
}
} /* namespace cheriv9::rv64 */
