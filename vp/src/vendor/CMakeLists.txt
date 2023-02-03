subdirs(softfloat)
if(NOT USE_SYSTEM_SYSTEMC)
    include(ExternalProject)
    ExternalProject_Add(
        systemc_project
        URL ${CMAKE_CURRENT_SOURCE_DIR}/systemc
        CMAKE_ARGS
          -DCMAKE_CXX_FLAGS_NO_WARNINGS="-w"
          -DCMAKE_BUILD_TYPE=NO_WARNINGS
          -DCMAKE_INSTALL_LIBDIR=lib #don't use lib64
          -DCMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD}
          -DBUILD_SHARED_LIBS=OFF
        INSTALL_COMMAND ""
        BINARY_DIR ${CMAKE_BINARY_DIR}/systemc
        BUILD_BYPRODUCTS ${CMAKE_BINARY_DIR}/systemc/src/libsystemc.a
    )

    add_library(systemc STATIC IMPORTED GLOBAL)
    set_target_properties(systemc PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/systemc/src
        IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/systemc/src/libsystemc.a)
    add_dependencies(systemc systemc_project)
endif()
