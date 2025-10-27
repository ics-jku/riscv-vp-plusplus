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
 *
 * There are following parameters:
 *  * ISS_CT .. the name of the class to define/implement
 *  * ISS_CT_T_CSR_TABLE .. the csr_table type to use
 *  * ISS_CT_ENABLE_POLYMORPHISM ..
 *    * see NOTE RVxx.1 in iss_ctemplate.h
 *    * If not defined: The class will be set to final and there will be no virtual methods.
 *      -> Not derivable. No overhead for dynamic dispatch. -> Used for the classic RV32 ISS
 *    * If defined: The class will *NOT* be set to final and some methods (annotated with NOTE RVxx.1) will be set as
 * virtual.
 *      -> Derivable, but overhead for dynamic dispatch. -> Used to realize the derived nuclei_core.
 *  * ISS_CT_STATS_ENABLED .. Enable ISS statistics (see common/iss_stats.h)
 *  * ISS_CT_OP_TAIL_FAST_FDD_ENABLED ..
 *    related to DBBCache based optimization. If this is defined it enables tail dispatch (threaded code)
 *    instead of global dispatch for operations
 *    Longer compilation, increased size, but faster execution
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

#define ISS_CT_ARCH RV64

#define ISS_CT ISS
#define ISS_CT_T_CSR_TABLE csr_table
#undef ISS_CT_ENABLE_POLYMORPHISM
#undef ISS_CT_STATS_ENABLED
#define ISS_CT_OP_TAIL_FAST_FDD_ENABLED

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
#undef ISS_CT_ENABLE_POLYMORPHISM
#undef ISS_CT_STATS_ENABLED
#undef ISS_CT_OP_TAIL_FAST_FDD_ENABLED

/* cleanup */
#undef ISS_CT_CREATE_DEFINITION
#undef ISS_CT_CREATE_IMPLEMENTATION
#undef ISS_CT_ARCH
