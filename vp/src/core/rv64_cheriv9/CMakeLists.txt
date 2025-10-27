file(GLOB_RECURSE HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

add_library(rv64_cheriv9
		iss.cpp
		syscall.cpp
		cheri_prelude.cpp
        ${HEADERS})

target_link_libraries(rv64_cheriv9 systemc core-common core-common_cheriv9 softfloat)


if(COLOR_THEME STREQUAL "LIGHT")
	message("> using color theme LIGHT")
	target_compile_definitions(rv64 PRIVATE COLOR_THEME_LIGHT)
elseif(COLOR_THEME STREQUAL "DARK")
	message("> using color theme DARK")
	target_compile_definitions(rv64 PRIVATE COLOR_THEME_DARK)
endif()

subdirs(rvfi-dii)
