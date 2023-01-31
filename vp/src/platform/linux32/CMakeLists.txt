file(GLOB_RECURSE HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

add_executable(linux32-vp
		linux32_main.cpp
        ${HEADERS})

target_link_libraries(linux32-vp rv32 platform-common gdb-mc
	${Boost_LIBRARIES} systemc pthread)

INSTALL(TARGETS linux32-vp RUNTIME DESTINATION bin)
