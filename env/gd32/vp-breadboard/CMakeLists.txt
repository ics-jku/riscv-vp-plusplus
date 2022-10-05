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
    ../../../vp/src/platform/gd32/
    ../../../vp/src/
)

set(HEADERS 
	window/window.h
)

if(NOT DONT_INCLUDE_CONFIGS)
	file(GLOB CONFIGS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ./conf/*.json)
endif()
if(NOT DONT_INCLUDE_LUA_DEVICES)
	file(GLOB SCRIPTS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ./devices/lua/*.lua)
endif()
set(SCRIPTS ${SCRIPTS} devices/factory/loadscript.lua)
file(GLOB IMAGES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "./img/*.jpg" "./img/*.png")
file(GLOB DEVICES ./devices/c/*.cpp)

set(SOURCES
    main.cpp
    window/window.cpp
    window/central.cpp
    window/actions/json_entry.cpp
    window/actions/get_dir.cpp
    embedded/embedded.cpp
    breadboard/breadboard.cpp
    breadboard/breadboard-json.cpp
    breadboard/raster.cpp
    ../../../vp/src/platform/gd32/gpio/gpiocommon.cpp
    ../../../vp/src/platform/gd32/gpio/gpio-client.cpp
    ../../../vp/src/util/elegantEnums.cpp
    ../../../vp/src/platform/gd32/oled/common.cpp
    ../../../vp/src/platform/gd32/oled/oled.cpp
    devices/interface/luaDevice.cpp
    devices/factory/luaFactory.cpp
    devices/device.cpp
    devices/factory/factory.cpp
    devices/interface/cDevice.cpp
    devices/factory/cFactory.cpp
    ${DEVICES}
)

add_executable(vp-breadboard ${SOURCES} ${HEADERS})

add_qt_resource(vp-breadboard configs FILES ${CONFIGS})
add_qt_resource(vp-breadboard scripts FILES ${SCRIPTS})
add_qt_resource(vp-breadboard images FILES ${IMAGES})

target_link_libraries(vp-breadboard Qt5::Widgets pthread LuaBridge ${LUA_LIB})

