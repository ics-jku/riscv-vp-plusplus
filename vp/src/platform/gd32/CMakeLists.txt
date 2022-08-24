add_executable(gd32-vp
        gd32_main.cpp
        gpio.cpp
        timer.cpp
        nuclei_core/nuclei_iss.cpp
        gpio/gpio-server.cpp
        gpio/gpio-client.cpp
        gpio/gpiocommon.cpp
        oled/oled.cpp
        oled/common.cpp
        ../../util/elegantEnums.cpp) # TODO

target_link_libraries(gd32-vp rv32 platform-common gdb-mc ${Boost_LIBRARIES} ${SystemC_LIBRARIES} pthread)

INSTALL(TARGETS gd32-vp RUNTIME DESTINATION bin)
