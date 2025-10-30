file(GLOB_RECURSE HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
set(SOURCES
	linux_main.cpp
	${HEADERS})
set(LIBS platform-common gdb-mc
	${Boost_LIBRARIES}
	systemc
	pthread)

# Linux for RV32 with single worker core
add_executable(linux32-sc-vp ${SOURCES})
target_compile_definitions(linux32-sc-vp PUBLIC
	TARGET_RV32
	NUM_CORES=2)
target_link_libraries(linux32-sc-vp rv32 ${LIBS})

# Linux for RV32 with four worker cores
add_executable(linux32-mc-vp ${SOURCES})
target_compile_definitions(linux32-mc-vp PUBLIC
	TARGET_RV32
	NUM_CORES=5)
target_link_libraries(linux32-mc-vp rv32 ${LIBS})

# Linux for RV64 with single worker core
add_executable(linux64-sc-vp ${SOURCES})
target_compile_definitions(linux64-sc-vp PUBLIC
	TARGET_RV64
	NUM_CORES=2)
target_link_libraries(linux64-sc-vp rv64 ${LIBS})

# Linux for RV64 with four worker cores
add_executable(linux64-mc-vp ${SOURCES})
target_compile_definitions(linux64-mc-vp PUBLIC
	TARGET_RV64
	NUM_CORES=5)
target_link_libraries(linux64-mc-vp rv64 ${LIBS})

# Linux for RV64 CHERIv9 with single worker core
add_executable(linux64-cheriv9-sc-vp ${SOURCES})
target_compile_definitions(linux64-cheriv9-sc-vp PUBLIC
	TARGET_RV64_CHERIV9
	NUM_CORES=2)
target_link_libraries(linux64-cheriv9-sc-vp rv64_cheriv9 ${LIBS})

INSTALL(TARGETS
	linux32-mc-vp
	linux32-sc-vp
	linux64-mc-vp
	linux64-sc-vp
	linux64-cheriv9-sc-vp
	RUNTIME DESTINATION bin)

# Install the script from the "dt" subdirectory
install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/dt/linux-dt-gen.py
    DESTINATION bin)

# (for compatibility; TODO: removed in future)
# linux-vp = linux64-mc-vp
# linux-sc-vp = linux64-sc-vp
# linux32-vp = linux32-mc-vp
install(PROGRAMS $<TARGET_FILE:linux64-mc-vp> DESTINATION bin RENAME linux-vp)
install(PROGRAMS $<TARGET_FILE:linux64-sc-vp> DESTINATION bin RENAME linux-sc-vp)
install(PROGRAMS $<TARGET_FILE:linux32-mc-vp> DESTINATION bin RENAME linux32-vp)
