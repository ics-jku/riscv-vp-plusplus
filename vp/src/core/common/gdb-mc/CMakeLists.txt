add_subdirectory(libgdb)

add_library(gdb-mc
	gdb_server.cpp
	gdb_runner.cpp
	register_format.cpp
	handler.cpp)

target_link_libraries(gdb-mc gdb core-common systemc)
