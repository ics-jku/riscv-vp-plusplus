file(GLOB_RECURSE HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

add_executable(tiny32-vp
        main.cpp
        ${HEADERS})

target_link_libraries(tiny32-vp rv32 platform-common gdb-mc
	${Boost_LIBRARIES} systemc pthread)

INSTALL(TARGETS tiny32-vp RUNTIME DESTINATION bin)
