cmake_minimum_required(VERSION 3.16)
project(gamingcpu-vp
    VERSION 1.0
    LANGUAGES CXX
    DESCRIPTION "GamingCPU Virtual Platform - SystemC/TLM-2.0 RV32IMAC SoC Model"
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find SystemC!
# I think SYSTEMC_HOME can be set via -DSYSTEMC_HOME=... or an environment variable
if(NOT DEFINED SYSTEMC_HOME)
    if(DEFINED ENV{SYSTEMC_HOME})
        set(SYSTEMC_HOME $ENV{SYSTEMC_HOME})
    else()
        # Default: look in deps/systemc-install (built by deps/setup.sh)
        set(SYSTEMC_HOME "${CMAKE_SOURCE_DIR}/deps/systemc-install")
    endif()
endif()

message(STATUS "SYSTEMC_HOME = ${SYSTEMC_HOME}")

# Find SystemC headers
find_path(SYSTEMC_INCLUDE_DIR
    NAMES systemc systemc.h
    PATHS ${SYSTEMC_HOME}/include
    NO_DEFAULT_PATH
)

# Find SystemC library (may be in lib/ or lib-linux64/ idfk)
find_library(SYSTEMC_LIBRARY
    NAMES systemc
    PATHS ${SYSTEMC_HOME}/lib ${SYSTEMC_HOME}/lib-linux64
    NO_DEFAULT_PATH
)

if(NOT SYSTEMC_INCLUDE_DIR OR NOT SYSTEMC_LIBRARY)
    message(FATAL_ERROR
        "SystemC not found. Run 'bash deps/setup.sh' first, or set -DSYSTEMC_HOME=<path>"
    )
endif()

message(STATUS "SystemC include: ${SYSTEMC_INCLUDE_DIR}")
message(STATUS "SystemC library: ${SYSTEMC_LIBRARY}")

# Create imported target for SystemC
add_library(SystemC::SystemC STATIC IMPORTED)
set_target_properties(SystemC::SystemC PROPERTIES
    IMPORTED_LOCATION ${SYSTEMC_LIBRARY}
    INTERFACE_INCLUDE_DIRECTORIES ${SYSTEMC_INCLUDE_DIR}
)

# SystemC requires pthreads
find_package(Threads REQUIRED)

# VP source file
# Built incrementally: add source files as modules are implemented

set(VP_SOURCES
    # Memory subsystem
    src/mem/memory.cpp
    src/mem/bootrom.cpp

    # Entry point
    src/main.cpp
)

set(VP_HEADERS
    src/mem/memory.h
    src/mem/bootrom.h
    src/platform/platform_config.h
)

# Main executable
add_executable(gamingcpu-vp ${VP_SOURCES})

target_include_directories(gamingcpu-vp PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(gamingcpu-vp PRIVATE
    SystemC::SystemC
    Threads::Threads
)

# SystemC uses dlopen on some platforms so I have to add this slop
target_link_libraries(gamingcpu-vp PRIVATE ${CMAKE_DL_LIBS})
