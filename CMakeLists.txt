project(boxdotool)
cmake_minimum_required(VERSION 3.0.0)

include(FindPkgConfig)
include_directories(.)

file(GLOB boxdotool_SOURCES cmd_*.c)

if(WITH_DEBUG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g3 -O0")
endif()

find_package(
    X11
    COMPONENTS
        Xext
        Xinerama
        Xkb
        Xtst
)

add_library(boxdo_obj OBJECT xdo.c xdo_search.c xdo_select.c)
add_library(boxdo SHARED $<TARGET_OBJECTS:boxdo_obj>)
target_link_libraries(
    boxdo_obj
    m
    X11
    Xext
    Xinerama
    xkbcommon
    Xtst
)

# -fPIC is needed for it to work properly as a shared library.
set_target_properties(boxdo_obj PROPERTIES COMPILE_FLAGS "-fPIC")

add_executable(boxdotool xdotool.c ${boxdotool_SOURCES})
target_link_libraries(
    boxdotool
    boxdo_obj
)

if(WITH_DOCS)
    add_custom_target(
        xdotool.1
        ALL
        COMMAND
            pod2man -c "" -r "" xdotool.pod > xdotool.1
        VERBATIM
    )
endif(WITH_DOCS)
