cmake_minimum_required(VERSION 3.10.0)
project(gdb-tests)

# gdb test cases do not require a libc
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -nostdlib")

# Generate debug information relative to gdb/
string(APPEND CMAKE_ASM_FLAGS_INIT " -g")
string(APPEND CMAKE_ASM_FLAGS_INIT " -fdebug-prefix-map=${CMAKE_SOURCE_DIR}=../gdb")

# All gdb test cases are currently written in ASM
enable_language(ASM)

file(GLOB files "${CMAKE_CURRENT_SOURCE_DIR}/*")
foreach(file ${files})
	if(EXISTS "${file}/CMakeLists.txt")
		add_subdirectory(${file})
	endif()
endforeach()
