#include <instr.h>

#include <string>
#include <iostream>
#include <iterator>
#include <sstream>
#include <inttypes.h>
#include <exception>

using namespace std;

const char *regnames[] = {
    "zero (x0)", "ra   (x1)", "sp   (x2)", "gp   (x3)", "tp   (x4)", "t0   (x5)", "t1   (x6)", "t2   (x7)",
    "s0/fp(x8)", "s1   (x9)", "a0  (x10)", "a1  (x11)", "a2  (x12)", "a3  (x13)", "a4  (x14)", "a5  (x15)",
    "a6  (x16)", "a7  (x17)", "s2  (x18)", "s3  (x19)", "s4  (x20)", "s5  (x21)", "s6  (x22)", "s7  (x23)",
    "s8  (x24)", "s9  (x25)", "s10 (x26)", "s11 (x27)", "t3  (x28)", "t4  (x29)", "t5  (x30)", "t6  (x31)",
};

void printOpcode(Instruction& instr) {
	Opcode::Mapping op;
	if (instr.is_compressed()) {
		op = instr.decode_and_expand_compressed(RV32);
	} else {
		op = instr.decode_normal(RV32);
	}

	cout << Opcode::mappingStr.at(op) << " ";

	switch (Opcode::getType(op)) {
		case Opcode::Type::R:
			cout <<  regnames[instr.rd()] << ", " << regnames[instr.rs1()] << ", " << regnames[instr.rs2()];
			break;
		case Opcode::Type::R4:	// only >= rv64
			cout <<  regnames[instr.rd()] << ", " << regnames[instr.rs1()] << ", " << regnames[instr.rs2()] << ", " << regnames[instr.rs3()];
			break;
		case Opcode::Type::I:
			cout <<  regnames[instr.rd()] << ", " << regnames[instr.rs1()] << ", " << instr.I_imm();
			break;
		case Opcode::Type::S:
			cout <<  regnames[instr.rd()] << ", " << regnames[instr.rs1()] << ", " << instr.S_imm();
			break;
		case Opcode::Type::B:
			cout <<  regnames[instr.rd()] << ", " << regnames[instr.rs1()] << ", " << instr.B_imm();
			break;
		case Opcode::Type::U:
			cout <<  regnames[instr.rd()] << ", " << instr.U_imm();
			break;
		case Opcode::Type::J:
			cout <<  regnames[instr.rd()] << ", " << instr.J_imm();
			break;
		default:
			cout << "Unknown Opcode Type " << instr.opcode();
	}

	cout << endl;
}

int main()
{
	string line;
	Instruction instr;
	cout << showbase << hex;

	while(std::getline(cin, line)) {
		try {
			instr = std::stoul(line, nullptr, 16);	// Base 16
		} catch (std::invalid_argument e) {
			cerr << "Invalid hex number " << line << endl;
			continue;
		}
		printOpcode(instr);
	}
}
