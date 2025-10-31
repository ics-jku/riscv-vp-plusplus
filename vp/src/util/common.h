#pragma once

#include <stdexcept>

/*
 * preprocessor magic
 * The cascading (with _INNER) is needed to ensure expansion of macro parameters
 * Do not use the _INNER variants directly!
 */
// convert parameter to string
#define M_DEFINE2STR(_def) M_DEFINE2STR_INNER(_def)
#define M_DEFINE2STR_INNER(_def) #_def
// join macro parameters
#define M_JOIN(x, y) M_JOIN_AGAIN(x, y)
#define M_JOIN_AGAIN(x, y) x##y

/* bit manipulation helpers */
#define BIT_RANGE(instr, upper, lower) (instr & (((1 << (upper - lower + 1)) - 1) << lower))
#define BIT_SLICE(instr, upper, lower) (BIT_RANGE(instr, upper, lower) >> lower)
#define BIT_SINGLE(instr, pos) (instr & (1 << pos))
#define BIT_SINGLE_P1(instr, pos) (BIT_SINGLE(instr, pos) >> pos)
#define BIT_SINGLE_PN(instr, pos, new_pos) ((BIT_SINGLE(instr, pos) >> pos) << new_pos)
#define EXTRACT_SIGN_BIT(instr, pos, new_pos) ((BIT_SINGLE_P1(instr, pos) << 31) >> (31 - new_pos))

#define likely(x) __builtin_expect((x), 1)
#define unlikely(x) __builtin_expect((x), 0)

#define UNUSED(x) (void)(x)

inline void ensure(bool cond) {
	if (unlikely(!cond))
		throw std::runtime_error("runtime assertion failed");
}

inline void ensure(bool cond, const std::string &reason) {
	if (unlikely(!cond))
		throw std::runtime_error(reason);
}

/* Allow to provide a custom function name for a SystemC thread to avoid duplicate name warning in case the same
 * SystemC module is instantiated multiple times.
 * Compatible with SystemC 3.0.1 (see systemc-3.0.1/src/sysc/kernel/sc_module.h line 477ff)
 */
#define SC_NAMED_THREAD(func, name) \
	SC_PROCESS_MACRO_BEGIN_         \
	this->declare_thread_process(SC_MAKE_FUNC_PTR(SC_CURRENT_USER_MODULE_TYPE, func), name) SC_PROCESS_MACRO_END_

#if defined(COLOR_THEME_LIGHT) || defined(COLOR_THEME_DARK)
#define COLORFRMT "\e[38;5;%um%s\e[39m"
#define COLORPRINT(fmt, data) fmt, data
#else
#define COLORFRMT "%s"
#define COLORPRINT(fmt, data) data
#endif
