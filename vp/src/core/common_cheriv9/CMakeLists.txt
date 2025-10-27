file(GLOB_RECURSE HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

add_library(core-common_cheriv9
		instr.cpp
		cheri_cap_common.cpp
		../rv64_cheriv9/cheri_prelude.cpp # TODO: Check if this dependency is allowed
		iss_stats.cpp
		${HEADERS})

target_link_libraries(core-common_cheriv9 PRIVATE pthread systemc)
