cmake_minimum_required(VERSION 3.10.0)
project(integration-tests)

enable_language(C ASM)

file(GLOB files "${CMAKE_CURRENT_SOURCE_DIR}/*")
foreach(file ${files})
	if(EXISTS "${file}/CMakeLists.txt")
		add_subdirectory(${file})
	endif()
endforeach()
