file(GLOB_RECURSE HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

add_library(core-common
		timer.cpp
		real_clint.cpp
		instr.cpp
		debug_memory.cpp
		rawmode.cpp
		${HEADERS})

target_include_directories(core-common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(core-common PRIVATE pthread systemc)

add_subdirectory(gdb-mc)
