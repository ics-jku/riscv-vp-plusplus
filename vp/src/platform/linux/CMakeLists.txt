file(GLOB_RECURSE HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

add_executable(linux-vp
        linux_main.cpp
        ${HEADERS})

target_link_libraries(linux-vp rv64 platform-common gdb-mc
	${Boost_LIBRARIES} systemc pthread)

INSTALL(TARGETS linux-vp RUNTIME DESTINATION bin)
