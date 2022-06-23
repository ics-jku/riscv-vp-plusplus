project(vp-breadboard) # Your project name

set(CMAKE_CXX_STANDARD 17)

cmake_minimum_required(VERSION 3.1.0)

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
cmake_policy(SET CMP0071 NEW)	# not required, will be standard in some time

find_package(Qt5Widgets CONFIG REQUIRED)

find_library(LUA_LIB lua)
if(NOT LUA_LIB)
    find_library(LUA_LIB lua5.3)
    if(NOT LUA_LIB)
      message(FATAL_ERROR "lua library not found")
    endif()
endif()

if(NOT DONT_INCLUDE_LUA_DEVICES)
    set(DONT_INCLUDE_LUA_DEVICES false)
endif()
if(NOT DONT_INCLUDE_CONFIGS)
    set(DONT_INCLUDE_CONFIGS false)
endif()

include(CMake/AddGitSubmodule.cmake)
include(CMake/GenerateResourceFiles.cmake)

add_git_submodule(LuaBridge3)
	
include_directories(
    ../../../vp/src/platform/hifive/
    ../../../vp/src/
)

set(HEADERS mainwindow.h)

set(UI mainwindow.ui)

if(NOT DONT_INCLUDE_CONFIGS)
	file(GLOB CONFIGS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ./conf/*.json)
endif()
if(NOT DONT_INCLUDE_LUA_DEVICES)
	file(GLOB SCRIPTS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ./devices/lua/*.lua)
endif()
set(SCRIPTS ${SCRIPTS} devices/loadscript.lua)
file(GLOB IMAGES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ./img/*.jpg)
file(GLOB DEVICES ./devices/c/*.cpp)

set(SOURCES
    main.cpp
    mainwindow.cpp
    ../../../vp/src/platform/hifive/gpio/gpiocommon.cpp
    ../../../vp/src/platform/hifive/gpio/gpio-client.cpp
    ../../../vp/src/util/elegantEnums.cpp
    ../../../vp/src/platform/hifive/oled/common.cpp
    ../../../vp/src/platform/hifive/oled/oled.cpp
    devices/device.cpp
    devices/luaEngine.cpp
    ${DEVICES}
)

add_executable(vp-breadboard ${SOURCES} ${HEADERS} ${UI})

add_qt_resource(vp-breadboard configs FILES ${CONFIGS})
add_qt_resource(vp-breadboard scripts FILES ${SCRIPTS})
add_qt_resource(vp-breadboard images FILES ${IMAGES})

target_link_libraries(vp-breadboard Qt5::Widgets pthread LuaBridge ${LUA_LIB})

