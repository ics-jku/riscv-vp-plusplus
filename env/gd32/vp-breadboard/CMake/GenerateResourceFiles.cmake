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
    find_program(RCC_exe NAMES rcc rcc-qt5 ${QT_CMAKE_EXPORT_NAMESPACE}::rcc)

    set(generatedSourceCode "${CMAKE_CURRENT_BINARY_DIR}/qrc_${resourceName}.cpp")
    add_custom_command(OUTPUT "${generatedSourceCode}"
                       COMMAND ${RCC_exe}
                       ARGS --name "${resourceName}"
                           --output "${generatedSourceCode}" "${generatedResourceFile}"
                       DEPENDS ${rcc_FILES}
                       COMMENT "RCC ${resourceName}"
                       VERBATIM)
    target_sources(${target} PRIVATE "${generatedSourceCode}")
    
    #message("${target} with ${generatedSourceCode} depends on ${rcc_FILES}")
endfunction()