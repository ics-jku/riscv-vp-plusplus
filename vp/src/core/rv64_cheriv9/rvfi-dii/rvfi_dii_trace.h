#pragma once
#include <cstdint>
typedef struct {
	// uint64_t        rvfi_dii_valid;     // Valid signal:                        Instruction was committed properly.
	uint64_t rvfi_dii_order;     // [000 - 063] Instruction number:      INSTRET value after completion.
	uint64_t rvfi_dii_pc_rdata;  // [237 - 300] PC before instr:         PC for current instruction
	uint64_t rvfi_dii_pc_wdata;  // [301 - 364] PC after instr:          Following PC - either PC + 4 or jump target.
	uint64_t rvfi_dii_insn;      // [067 - 098] Instruction word:        32-bit command value.

	uint64_t rvfi_dii_rs1_data;  // [109 - 172] Read register values:    Values as read from registers named
	uint64_t rvfi_dii_rs2_data;  // [173 - 236]                          above. Must be 0 if register ID is 0.
	uint64_t rvfi_dii_rd_wdata;  // [434 - 497] Write register value:    MUST be 0 if rd_ is 0.

	uint64_t rvfi_dii_mem_addr;  // [498 - 561] Memory access addr:      Points to byte address (aligned if define
	                             //                                      is set). *Should* be straightforward.
	// XXX: SC writes something other than read value, but the value that would be read is unimportant.
	// Unsure what the point of this is, it's only relevant when the value is going to be in rd anyway.
	uint64_t rvfi_dii_mem_rdata;  // [578 - 641] Read data:               Data read from mem_addr (i.e. before write)
	// PROBLEM: LR/SC, if SC fails then value is not written. Indicate as wmask = 0.
	uint64_t rvfi_dii_mem_wdata;  // [365 - 428] Write data:              Data written to memory by this command.

	// Not explicitly given, but calculable from opcode/funct3 from ISA_Decls.
	uint8_t rvfi_dii_mem_rmask;  // [562 - 569] Read mask:               Indicates valid bytes read. 0 if unused.
	uint8_t rvfi_dii_mem_wmask;  // [570 - 577] Write mask:              Indicates valid bytes written. 0 if unused.

	uint8_t rvfi_dii_rs1_addr;  // [099 - 103] Read register addresses: Can be arbitrary when not used,
	uint8_t rvfi_dii_rs2_addr;  // [104 - 108]                          otherwise set as decoded.

	// Found in ALU if used, not 0'd if not used. Check opcode/op_stage2.
	// PROBLEM: LD/AMO - then found in stage 2.
	uint8_t rvfi_dii_rd_addr;  // [429 - 433] Write register address:  MUST be 0 if not used.

	uint8_t rvfi_dii_trap;  // [064 - 064] Trap indicator:          Invalid decode, misaligned access or
	                        //                                      jump command to misaligned address.

	uint8_t rvfi_dii_halt;  // [065 - 065] Halt indicator:          Marks the last instruction retired
	                        //                                      before halting execution.

	uint8_t rvfi_dii_intr;  // [066 - 066] Trap handler:            Set for first instruction in trap handler.
} rvfi_dii_trace_t;         // 88 Bytes

typedef struct {
	uint32_t rvfi_dii_insn;  // [0 - 31] Instruction word: 32-bit instruction or command. The lower 16-bits
	                         // may decode to a 16-bit compressed instruction.
	uint16_t rvfi_dii_time;  // [62 - 53] Time to inject token.  The difference between this and the previous
	                         // instruction time gives a delay before injecting this instruction.
	                         // This can be ignored for models but gives repeatability for implementations
	                         // while shortening counterexamples.
	uint8_t rvfi_dii_cmd;    // [63] This token is a trace command.  For example, reset device under test.
	uint8_t padding;
} rvfi_dii_command_t;  // 8 bytes
