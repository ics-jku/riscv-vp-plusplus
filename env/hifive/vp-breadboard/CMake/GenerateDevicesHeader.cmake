function(createCDeviceHeader)
	set(multiValueArgs CDEVICES)
	cmake_parse_arguments(CREATECDEVICEHEADER "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

	set(headerContent "#pragma once\n\n")
	foreach(file ${CREATECDEVICEHEADER_CDEVICES})
		string(APPEND headerContent "#include \"${file}\"\n")
	endforeach()

    set(generatedResourceFile "${CMAKE_CURRENT_BINARY_DIR}/all_devices.hpp")
    file(GENERATE OUTPUT "${generatedResourceFile}" CONTENT "${headerContent}")
endfunction()