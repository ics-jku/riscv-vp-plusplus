add_executable(hifive-vp
        hifive_main.cpp
        can.cpp
        oled/common.cpp
        oled/oled.cpp
        gpio.cpp

        # currently not supported - TODO: tunnel-uart must be reworked to channel_tunnel (derived from Channel_IF)
        #tunnel-uart.cpp # currently not supported

        ../../util/elegantEnums.cpp #sorry :(
        ${HEADERS})

add_git_submodule(vbb-protocol)

target_link_libraries(hifive-vp virtual-breadboard-server rv32 platform-common gdb-mc ${Boost_LIBRARIES} systemc pthread)

INSTALL(TARGETS hifive-vp RUNTIME DESTINATION bin)
