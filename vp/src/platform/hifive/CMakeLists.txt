add_executable(hifive-vp
        hifive_main.cpp
        can.cpp
        oled/common.cpp
        oled/oled.cpp
        gpio.cpp
        gpio/gpiocommon.cpp
        gpio/gpio-server.cpp
        gpio/gpio-client.cpp
        ../../util/elegantEnums.cpp #sorry :(
        ${HEADERS})

target_link_libraries(hifive-vp rv32 platform-common gdb-mc ${Boost_LIBRARIES} ${SystemC_LIBRARIES} pthread)

INSTALL(TARGETS hifive-vp RUNTIME DESTINATION bin)
