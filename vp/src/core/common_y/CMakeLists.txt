file(GLOB_RECURSE HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

add_library(core-common_y
		instr.cpp
		../rv64y/cheri_prelude.cpp # TODO: Check if this dependency is allowed
		iss_stats.cpp
		${HEADERS})

target_link_libraries(core-common_y PRIVATE pthread systemc)