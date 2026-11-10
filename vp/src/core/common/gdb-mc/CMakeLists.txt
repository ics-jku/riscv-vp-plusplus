add_subdirectory(libgdb)

# GDB target descriptions: the XML files in xml/ are embedded into gdb_xml_data.cpp at build time
file(GLOB GDB_XML_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/xml/*.xml")
list(SORT GDB_XML_FILES)
set(GDB_XML_DATA "${CMAKE_CURRENT_BINARY_DIR}/gdb_xml_data.cpp")
add_custom_command(
	OUTPUT "${GDB_XML_DATA}"
	COMMAND ${CMAKE_COMMAND} "-DOUTPUT=${GDB_XML_DATA}" "-DINPUTS=${GDB_XML_FILES}"
	        -P "${CMAKE_CURRENT_SOURCE_DIR}/embed_xml.cmake"
	DEPENDS ${GDB_XML_FILES} "${CMAKE_CURRENT_SOURCE_DIR}/embed_xml.cmake"
	COMMENT "Embedding GDB target description XML files"
	VERBATIM)

add_library(gdb-mc
	gdb_server.cpp
	gdb_runner.cpp
	gdb_regs.cpp
	gdb_xml.cpp
	"${GDB_XML_DATA}"
	handler.cpp)

target_link_libraries(gdb-mc gdb core-common systemc)
