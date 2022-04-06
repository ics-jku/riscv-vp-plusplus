project(vp-breadboard) # Your project name

set(CMAKE_CXX_STANDARD 17)

cmake_minimum_required(VERSION 3.1.0)

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5Widgets CONFIG REQUIRED)

include_directories(
    ../../../vp/src/platform/hifive/
    ../../../vp/src/
)

set(HEADERS mainwindow.h)

set(UI mainwindow.ui)

set(RESOURCES resources.qrc)

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

add_executable(vp-breadboard ${SOURCES} ${HEADERS} ${RESOURCES} ${UI})

target_link_libraries(vp-breadboard Qt5::Widgets pthread)

