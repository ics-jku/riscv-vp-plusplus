file(GLOB_RECURSE HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

set(SOURCES
	main.cpp)

add_executable(tiny32-vp ${SOURCES})
target_compile_definitions(tiny32-vp PUBLIC TARGET_RV32)
target_link_libraries(tiny32-vp rv32 platform-common gdb-mc ${Boost_LIBRARIES} systemc pthread)

add_executable(tiny64-vp ${SOURCES})
target_compile_definitions(tiny64-vp PUBLIC TARGET_RV64)
target_link_libraries(tiny64-vp rv64 platform-common gdb-mc ${Boost_LIBRARIES} systemc pthread)

add_executable(tiny64-cheriv9-vp ${SOURCES})
target_compile_definitions(tiny64-cheriv9-vp PUBLIC TARGET_RV64_CHERIV9)
target_link_libraries(tiny64-cheriv9-vp rv64_cheriv9 platform-common gdb-mc ${Boost_LIBRARIES} systemc pthread)

INSTALL(TARGETS
	tiny32-vp
	tiny64-vp
	tiny64-cheriv9-vp
	RUNTIME DESTINATION bin)
