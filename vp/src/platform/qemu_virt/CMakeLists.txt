file(GLOB_RECURSE HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
set(SOURCES
	main.cpp
	${HEADERS})
set(LIBS platform-common gdb-mc
	${Boost_LIBRARIES}
	systemc
	pthread)

add_executable(qemu_virt32-sc-vp ${SOURCES})
target_compile_definitions(qemu_virt32-sc-vp PUBLIC
	TARGET_RV32
	NUM_CORES=1)
target_link_libraries(qemu_virt32-sc-vp rv32 ${LIBS})

add_executable(qemu_virt32-mc-vp ${SOURCES})
target_compile_definitions(qemu_virt32-mc-vp PUBLIC
	TARGET_RV32
	NUM_CORES=4)
target_link_libraries(qemu_virt32-mc-vp rv32 ${LIBS})

add_executable(qemu_virt64-sc-vp ${SOURCES})
target_compile_definitions(qemu_virt64-sc-vp PUBLIC
	TARGET_RV64
	NUM_CORES=1)
target_link_libraries(qemu_virt64-sc-vp rv64 ${LIBS})

add_executable(qemu_virt64-mc-vp ${SOURCES})
target_compile_definitions(qemu_virt64-mc-vp PUBLIC
	TARGET_RV64
	NUM_CORES=4)
target_link_libraries(qemu_virt64-mc-vp rv64 ${LIBS})

add_executable(qemu_virt64-cheriv9-sc-vp ${SOURCES})
target_compile_definitions(qemu_virt64-cheriv9-sc-vp PUBLIC
	TARGET_RV64_CHERIV9
	NUM_CORES=1)
target_link_libraries(qemu_virt64-cheriv9-sc-vp rv64_cheriv9 ${LIBS})

INSTALL(TARGETS
	qemu_virt32-mc-vp
	qemu_virt32-sc-vp
	qemu_virt64-mc-vp
	qemu_virt64-sc-vp
	qemu_virt64-cheriv9-sc-vp
	RUNTIME DESTINATION bin)

# Install the script from the "dt" subdirectory
install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/dt/qemu_virt-dt-gen.py
    DESTINATION bin)
