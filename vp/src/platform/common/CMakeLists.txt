file(GLOB_RECURSE HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

add_library(platform-common
		fe310_plic.cpp
		fu540_plic.cpp
		uart_if.cpp
		fd_abstract_uart.cpp
		slip.cpp
		uart.cpp
		sifive_test.cpp
		vncsimplefb.cpp
		vncsimpleinputptr.cpp
		vncsimpleinputkbd.cpp
		options.cpp
		${HEADERS})

target_include_directories(platform-common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# only for libvncserver > 0.9.13
# find_package(LibVNCServer)
# if(LibVNCServer_FOUND)
# 	target_link_libraries(platform-common LibVNCServer::vncserver)
# endif()
target_link_libraries(platform-common systemc vncserver util)
