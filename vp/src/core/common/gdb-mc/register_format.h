#ifndef RISCV_GDB_REGISTER
#define RISCV_GDB_REGISTER

#include <stdint.h>

#include <sstream>

#include "core_defs.h"

class RegisterFormater {
   private:
	Architecture arch;
	std::ostringstream stream;

   public:
	RegisterFormater(Architecture);
	void formatRegister(uint64_t);
	std::string str(void);
};

#endif
