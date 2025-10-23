file(GLOB_RECURSE HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

add_library(util
	options.cpp
	vncserver.cpp
	propertymap.cpp
	${HEADERS})

target_link_libraries(util PRIVATE systemc)
