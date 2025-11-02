file(GLOB_RECURSE HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

add_library(platform-basic
ethernet.cpp
display.cpp
${HEADERS})

target_link_libraries(platform-basic systemc)

set(SOURCES
	main.cpp)

add_executable(riscv32-vp ${SOURCES})
target_compile_definitions(riscv32-vp PUBLIC TARGET_RV32)
target_link_libraries(riscv32-vp rv32 platform-basic platform-common gdb-mc ${Boost_LIBRARIES} systemc pthread)

add_executable(riscv64-vp ${SOURCES})
target_compile_definitions(riscv64-vp PUBLIC TARGET_RV64)
target_link_libraries(riscv64-vp rv64 platform-basic platform-common gdb-mc ${Boost_LIBRARIES} systemc pthread)

add_executable(riscv64-cheriv9-vp ${SOURCES})
target_compile_definitions(riscv64-cheriv9-vp PUBLIC TARGET_RV64_CHERIV9)
target_link_libraries(riscv64-cheriv9-vp rv64_cheriv9 platform-basic platform-common gdb-mc ${Boost_LIBRARIES} systemc pthread)

add_executable(riscv-vp ALIAS riscv32-vp)

INSTALL(TARGETS
	riscv32-vp
	riscv64-vp
	riscv64-cheriv9-vp
	RUNTIME DESTINATION bin)

# riscv-vp = riscv32-vp (for compatibility; TODO: remove in future)
install(PROGRAMS $<TARGET_FILE:riscv32-vp> DESTINATION bin RENAME riscv-vp)
