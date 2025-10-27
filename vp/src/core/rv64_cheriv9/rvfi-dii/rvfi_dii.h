/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018 Hesham Almatary <Hesham.Almatary@cl.cam.ac.uk>
 * Copyright (c) 2018 Jack Deely
 * Copyright (c) 2018 Jonathan Woodruff
 *
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory (Department of Computer Science and
 * Technology) under DARPA contract HR0011-18-C-0016 ("ECATS"), as part of the
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

#ifndef _RISCV_RVFI_DII_H
#define _RISCV_RVFI_DII_H

#include <stdint.h>

#include "core/rv64_cheriv9/iss.h"
#include "platform/common/tagged_memory.h"
#include "rvfi_dii_trace.h"

namespace cheriv9::rv64 {

class rvfi_dii_interface_t {
   public:
	virtual ~rvfi_dii_interface_t() = default;

	// This function must be implemented by a RVDI-DII-supported platform to
	// read a RVDI-DII-supported execution command (also known as exceution trace)
	// It is considered the "input" interface of the RVDI-DII module
	virtual void read_trace(rvfi_dii_command_t *input) = 0;

	// This function must be implemented by a RVDI-DII-supported platform to
	// write a RVDI-DII-supported output trace.
	// It is considered the "output" interface of the RVDI-DII module
	virtual void write_trace(rvfi_dii_trace_t *output) = 0;

	virtual void init() {};
	virtual void reset() {};
};

class rvfi_dii_t : public rvfi_dii_interface_t {
   public:
	// Create a new server, listening for connections from localhost on the given
	// port.
	rvfi_dii_t(uint16_t port, TaggedMemory &mem);

	void read_trace(rvfi_dii_command_t *input);
	void write_trace(rvfi_dii_trace_t *output);

	// Start RVFI-DII to handle commands
	void start(ISS *p);

	bool quit = false;

	TaggedMemory &mem;

   private:
	int socket_fd;
	int client_fd;

	rvfi_dii_command_t rvfi_dii_input;

	// Check for a client connecting, and accept if there is one.
	void accept();

	void execute_command(ISS *s);
};

} /* namespace cheriv9::rv64 */

#endif
