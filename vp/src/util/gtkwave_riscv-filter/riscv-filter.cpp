#include <instr.h>

#include <string>
#include <iostream>
#include <iterator>
#include <sstream>
#include <inttypes.h>
#include <exception>

static Architecture ARCH = Architecture::RV32;
static bool USE_PRETTY_NAMES = false;

using namespace std;

string registerName(uint_fast16_t num) {
	if(USE_PRETTY_NAMES) {
		return Opcode::regnamePrettyStr[num];
	} else {
		return {"x" + to_string(num)};
	}
}

void printOpcode(Instruction& instr) {
	Opcode::Mapping op;
	if (instr.is_compressed()) {
		op = instr.decode_and_expand_compressed(ARCH);
	} else {
		op = instr.decode_normal(ARCH);
	}

	cout << Opcode::mappingStr.at(op) << " ";

	switch (Opcode::getType(op)) {
		case Opcode::Type::R:
			cout <<  registerName(instr.rd()) << ", " << registerName(instr.rs1()) << ", " << registerName(instr.rs2());
			break;
		case Opcode::Type::R4:	// only >= rv64
			cout <<  registerName(instr.rd()) << ", " << registerName(instr.rs1()) << ", " << registerName(instr.rs2()) << ", " << registerName(instr.rs3());
			break;
		case Opcode::Type::I:
			cout <<  registerName(instr.rd()) << ", " << registerName(instr.rs1()) << ", " << instr.I_imm();
			break;
		case Opcode::Type::S:
			cout <<  registerName(instr.rd()) << ", " << registerName(instr.rs1()) << ", " << instr.S_imm();
			break;
		case Opcode::Type::B:
			cout <<  registerName(instr.rd()) << ", " << registerName(instr.rs1()) << ", " << instr.B_imm();
			break;
		case Opcode::Type::U:
			cout <<  registerName(instr.rd()) << ", " << instr.U_imm();
			break;
		case Opcode::Type::J:
			cout <<  registerName(instr.rd()) << ", " << instr.J_imm();
			break;
		default:
			cout << "Unknown Opcode Type " << instr.opcode();
	}

	cout << endl;
}

int main(int argc, const char* argv[])
{
	string line;
	Instruction instr;
	cout << showbase << hex;

	for(unsigned i = 1; i < argc; i++) {
		if(argv[i] == string{"--use-pretty-names"})
			USE_PRETTY_NAMES = true;
		else if(argv[i] == string{"--rv64"})
			ARCH = Architecture::RV64;
		else {
			cout << argv[0] << " [--use-pretty-names] [--rv64]" << endl;
			return 0;
		}
	}

	while(std::getline(cin, line)) {
		try {
			instr = std::stoul(line, nullptr, 16);	// Base 16
		} catch (std::invalid_argument&) {
			cerr << "Not a parse-able hex number: '" << line << "'" << endl;
			continue;
		}
		printOpcode(instr);
	}
}
