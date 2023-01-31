file(GLOB_RECURSE HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

add_executable(tiny64-mc
        mc_main.cpp
        ${HEADERS})

target_link_libraries(tiny64-mc rv64 platform-common gdb-mc
	${Boost_LIBRARIES} systemc pthread)

INSTALL(TARGETS tiny64-mc RUNTIME DESTINATION bin)
