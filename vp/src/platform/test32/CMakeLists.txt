file(GLOB_RECURSE HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

add_executable(test32-vp
        test32_main.cpp
        ${HEADERS})

target_link_libraries(test32-vp rv32 platform-common gdb-mc ${Boost_LIBRARIES} systemc pthread)

INSTALL(TARGETS test32-vp RUNTIME DESTINATION bin)
