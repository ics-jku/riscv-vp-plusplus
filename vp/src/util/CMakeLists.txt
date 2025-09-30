file(GLOB_RECURSE HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

add_library(util
	options.cpp
	vncserver.cpp
        ${HEADERS})

target_include_directories(util PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
