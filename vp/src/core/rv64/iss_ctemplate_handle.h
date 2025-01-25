/*
 * NEVER INCLUDE THIS FILE IN OTHER FILES THAN iss.h OR iss.cpp!!!
 *
 * NOTE RVxx.1:
 * See also "NOTE RVxx.1" in other files in this directory and in *especially* in RV32 ISS
 * (core/rv32/iss_ctemplate_handle.h)
 *
 * The RV64 ISS_CT template is used as
 *  *  1. final class for the classic RV64 ISS, and
 * ISS_CT is a template with an implementation located in iss.h and iss.cpp.
 * Every concrete class based on ISS_CT has to be *explicitly* specified below.
 *
 *
 * NOTE RVxx.2: C-style macros
 * See also "NOTE RVxx.2" in other files in this directory and in *especially* in RV32 ISS
 * (core/rv32/iss_ctemplate_handle.h) For RV64 we currently have only on concrete ISS definition and implementation.
 * However, we still using this c-template pattern here to keep differences between the RV32 and RV64 ISS as small as
 * possible.
 */

/*
 * Create definition / implementation from iss_template.h/cpp for the classic RV64 ISS
 *
 * The class is called "ISS" and uses the classic RV64 "csr_table".
 * "ISS" is used directly for all classic RV64 platforms. There are no derived classes from "ISS", therefore
 * ISS_CT_ENABLE_POLYMORPHISM is NOT set (see NOTE RVxx.1), which means:
 *  1. The class "ISS" is set to final -> not derivable
 *  2. There are no virtual methods -> no cost for dynamic dispatch
 */
#define ISS_CT ISS
#define ISS_CT_T_CSR_TABLE csr_table
#undef ISS_CT_ENABLE_POLYMORPHISM

#if defined(ISS_CT_CREATE_DEFINITION)
#include "iss_ctemplate.h"
#elif defined(ISS_CT_CREATE_IMPLEMENTATION)
#include "iss_ctemplate.cpp"
#else
#error "ISS_CT_CREATE_... invalid or not defined!"
#endif
/* undef all configurations */
#undef ISS_CT
#undef ISS_CT_T_CSR_TABLE

/* cleanup */
#undef ISS_CT_CREATE_DEFINITION
#undef ISS_CT_CREATE_IMPLEMENTATION
