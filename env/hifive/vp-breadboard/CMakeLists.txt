project(vp-breadboard) # Your project name

set(CMAKE_CXX_STANDARD 17)

cmake_minimum_required(VERSION 3.1.0)

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
cmake_policy(SET CMP0071 NEW)	# not required, will be standard in some time

# A version of cmake_parse_arguments that makes sure all arguments are processed and errors out
# with a message about ${type} having received unknown arguments.
macro(qt_parse_all_arguments result type flags options multiopts)
    cmake_parse_arguments(${result} "${flags}" "${options}" "${multiopts}" ${ARGN})
    if(DEFINED ${result}_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unknown arguments were passed to ${type} (${${result}_UNPARSED_ARGUMENTS}).")
    endif()
endmacro()

function(add_qt_resource target resourceName)
    qt_parse_all_arguments(rcc "add_qt_resource" "" "PREFIX;LANG;BASE" "FILES" ${ARGN})

    # Generate .qrc file:

    # <RCC><qresource ...>
    set(qrcContents "<RCC>\n  <qresource")
    if (rcc_PREFIX)
        string(APPEND qrcContents " prefix=\"${rcc_PREFIX}\"")
    endif()
    if (rcc_LANG)
        string(APPEND qrcContents " lang=\"${rcc_LANG}\"")
    endif()
    string(APPEND qrcContents ">\n")

    foreach(file ${rcc_FILES})
        if(rcc_BASE)
            set(based_file "${rcc_BASE}/${file}")
        else()
            set(based_file "${file}")
        endif()
        get_property(alias SOURCE ${based_file} PROPERTY alias)
        if (NOT alias)
            set(alias "${file}")
        endif()
        ### FIXME: escape file paths to be XML conform
        # <file ...>...</file>
        string(APPEND qrcContents "    <file alias=\"${alias}\">")
        string(APPEND qrcContents "${CMAKE_CURRENT_SOURCE_DIR}/${based_file}</file>\n")
    endforeach()

    # </qresource></RCC>
    string(APPEND qrcContents "  </qresource>\n</RCC>\n")

    set(generatedResourceFile "${CMAKE_CURRENT_BINARY_DIR}/${resourceName}.qrc")
    file(GENERATE OUTPUT "${generatedResourceFile}" CONTENT "${qrcContents}")

    # Process .qrc file:

    set(generatedSourceCode "${CMAKE_CURRENT_BINARY_DIR}/qrc_${resourceName}.cpp")
    add_custom_command(OUTPUT "${generatedSourceCode}"
                       #COMMAND "${QT_CMAKE_EXPORT_NAMESPACE}::rcc"
                       COMMAND "rcc"
                       ARGS --name "${resourceName}"
                           --output "${generatedSourceCode}" "${generatedResourceFile}"
                       DEPENDS ${files}
                       COMMENT "RCC ${resourceName}"
                       VERBATIM)
    target_sources(${target} PRIVATE "${generatedSourceCode}")
endfunction()

find_package(Qt5Widgets CONFIG REQUIRED)

include_directories(
    ../../../vp/src/platform/hifive/
    ../../../vp/src/
)

set(HEADERS mainwindow.h)

set(UI mainwindow.ui)

file(GLOB CONFIGS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ./conf/*.json)
set(IMAGES 
	"img/sevensegment.jpg"
	"img/oled_shield.JPG"
)

file(GLOB DEVICES ./devices/*.cpp)
set(SOURCES
    main.cpp
    mainwindow.cpp
    ../../../vp/src/platform/hifive/gpio/gpiocommon.cpp
    ../../../vp/src/platform/hifive/gpio/gpio-client.cpp
    ../../../vp/src/util/elegantEnums.cpp
    ../../../vp/src/platform/hifive/oled/common.cpp
    ../../../vp/src/platform/hifive/oled/oled.cpp
    ${DEVICES}
)

add_executable(vp-breadboard ${SOURCES} ${HEADERS} ${UI})

add_qt_resource(vp-breadboard configs FILES ${CONFIGS})
add_qt_resource(vp-breadboard images FILES ${IMAGES})

target_link_libraries(vp-breadboard Qt5::Widgets pthread)

