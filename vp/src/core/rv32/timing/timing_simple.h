#pragma once

#include "../iss.h"

struct SimpleTimingDecorator : public timing_if {
	std::array<sc_core::sc_time, Operations::OpId::NUMBER_OF_OPERATIONS> instr_cycles;
	sc_core::sc_time cycle_time = sc_core::sc_time(10, sc_core::SC_NS);

	SimpleTimingDecorator() {
		for (int i = 0; i < Operations::OpId::NUMBER_OF_OPERATIONS; ++i) instr_cycles[i] = cycle_time;

		const sc_core::sc_time memory_access_cycles = 4 * cycle_time;
		const sc_core::sc_time mul_div_cycles = 8 * cycle_time;

		instr_cycles[Operations::OpId::LB] = memory_access_cycles;
		instr_cycles[Operations::OpId::LBU] = memory_access_cycles;
		instr_cycles[Operations::OpId::LH] = memory_access_cycles;
		instr_cycles[Operations::OpId::LHU] = memory_access_cycles;
		instr_cycles[Operations::OpId::LW] = memory_access_cycles;
		instr_cycles[Operations::OpId::SB] = memory_access_cycles;
		instr_cycles[Operations::OpId::SH] = memory_access_cycles;
		instr_cycles[Operations::OpId::SW] = memory_access_cycles;
		instr_cycles[Operations::OpId::MUL] = mul_div_cycles;
		instr_cycles[Operations::OpId::MULH] = mul_div_cycles;
		instr_cycles[Operations::OpId::MULHU] = mul_div_cycles;
		instr_cycles[Operations::OpId::MULHSU] = mul_div_cycles;
		instr_cycles[Operations::OpId::DIV] = mul_div_cycles;
		instr_cycles[Operations::OpId::DIVU] = mul_div_cycles;
		instr_cycles[Operations::OpId::REM] = mul_div_cycles;
		instr_cycles[Operations::OpId::REMU] = mul_div_cycles;
	}

	void on_begin_exec_step(Instruction instr, Operations::OpId::OpId op, ISS &iss) override {
		auto new_cycles = instr_cycles[op];

		iss.quantum_keeper.inc(new_cycles);
	}
};
