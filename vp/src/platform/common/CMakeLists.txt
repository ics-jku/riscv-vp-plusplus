file(GLOB_RECURSE HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

add_library(platform-common
		fe310_plic.cpp
		sifive_plic.cpp
		sifive_test.cpp
		channel_if.cpp
		channel_fd_if.cpp
		channel_console.cpp
		channel_slip.cpp
		channel_tcp.cpp
		ns16550a_uart.cpp
		fu540_uart.cpp
		fu540_gpio.cpp
		vncsimplefb.cpp
		vncsimpleinputptr.cpp
		vncsimpleinputkbd.cpp
		spi_sd_card.cpp
		options.cpp
		net_trace.cpp
		${HEADERS})

target_include_directories(platform-common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# only for libvncserver > 0.9.13
# find_package(LibVNCServer)
# if(LibVNCServer_FOUND)
# 	target_link_libraries(platform-common LibVNCServer::vncserver)
# endif()
target_link_libraries(platform-common systemc vncserver util)
