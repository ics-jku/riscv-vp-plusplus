file(GLOB_RECURSE HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

add_executable(microrv32-vp
        ${HEADERS}
        main.cpp)

target_link_libraries(microrv32-vp rv32 platform-common gdb-mc ${Boost_LIBRARIES} systemc pthread)

INSTALL(TARGETS microrv32-vp RUNTIME DESTINATION bin)
