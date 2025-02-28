#include <instr.h>
#include <inttypes.h>
#include <regfile.h>

#include <exception>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>

static Architecture ARCH = Architecture::RV32;
static bool USE_PRETTY_NAMES = false;

using namespace std;

string registerName(uint_fast16_t num) {
	if (USE_PRETTY_NAMES) {
		return RegFile_T<int64_t, uint64_t>::regnames[num];
	} else {
		return {"x" + to_string(num)};
	}
}

void printOperation(Instruction& instr, const RV_ISA_Config& isa_config) {
	Operation::OpId opId;
	if (instr.is_compressed()) {
		opId = instr.decode_and_expand_compressed(ARCH, isa_config);
	} else {
		opId = instr.decode_normal(ARCH, isa_config);
	}

	cout << Operation::opIdStr.at(opId) << " ";

	switch (Operation::getType(opId)) {
		case Operation::Type::R:
			cout << registerName(instr.rd()) << ", " << registerName(instr.rs1()) << ", " << registerName(instr.rs2());
			break;
		case Operation::Type::R4:  // only >= rv64
			cout << registerName(instr.rd()) << ", " << registerName(instr.rs1()) << ", " << registerName(instr.rs2())
			     << ", " << registerName(instr.rs3());
			break;
		case Operation::Type::I:
			cout << registerName(instr.rd()) << ", " << registerName(instr.rs1()) << ", " << instr.I_imm();
			break;
		case Operation::Type::S:
			cout << registerName(instr.rd()) << ", " << registerName(instr.rs1()) << ", " << instr.S_imm();
			break;
		case Operation::Type::B:
			cout << registerName(instr.rd()) << ", " << registerName(instr.rs1()) << ", " << instr.B_imm();
			break;
		case Operation::Type::U:
			cout << registerName(instr.rd()) << ", " << instr.U_imm();
			break;
		case Operation::Type::J:
			cout << registerName(instr.rd()) << ", " << instr.J_imm();
			break;
		default:
			cout << "Unknown Operation Type " << instr.opcode();
	}

	cout << endl;
}

int main(int argc, const char* argv[]) {
	string line;
	Instruction instr;
	RV_ISA_Config isa_config(false, false);
	cout << showbase << hex;

	for (unsigned i = 1; i < argc; i++) {
		if (argv[i] == string{"--use-pretty-names"})
			USE_PRETTY_NAMES = true;
		else if (argv[i] == string{"--rv64"})
			ARCH = Architecture::RV64;
		else {
			cout << argv[0] << " [--use-pretty-names] [--rv64]" << endl;
			return 0;
		}
	}

	while (std::getline(cin, line)) {
		try {
			instr = std::stoul(line, nullptr, 16);  // Base 16
		} catch (std::invalid_argument&) {
			cerr << "Not a parse-able hex number: '" << line << "'" << endl;
			continue;
		}
		printOperation(instr, &isa_config);
	}
}
