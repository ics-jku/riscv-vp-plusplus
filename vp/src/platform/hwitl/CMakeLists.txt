file(GLOB HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.h*)
file(GLOB SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.c*)

add_git_submodule(virtual-bus)

add_executable(hwitl-vp
	${SOURCES}
)

target_link_libraries(hwitl-vp rv32 virtual-bus platform-common gdb-mc ${Boost_LIBRARIES} systemc pthread)

INSTALL(TARGETS hwitl-vp RUNTIME DESTINATION bin)