#
# This file is part of the ACT library
#
# Copyright (c) 2021 Fabian Posch
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy SIMLIB_FINAL_INSTALL_LOCATIONof the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor,
# Boston, MA  02110-1301, USA.
#

cmake_minimum_required(VERSION 3.15)

project(actsim)
set(SIMLIB_NAME "actsimext_sh")

message("Building project ${PROJECT_NAME} and SIMLIB lib${SIMLIB_NAME}.so")

# enable debug prints
set(DEBUG_CMAKE FALSE CACHE BOOL "enable / disable CMake configuration debug information")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

# always build into build/
set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/build)

# if you don't want to build the tracelib together with actsim, set this to false
set(BUILD_TRACELIB FALSE CACHE BOOL "enable / disable building tracelib with this project")
message("Build tracelib: ${BUILD_TRACELIB}")

# Build and install based on the ACT_HOME environment variable. Setting this to false
# will assume an OS standard installation scheme (including dependencies).
set(TARGET_ACT_HOME TRUE CACHE BOOL "if true, targets the ACT folder layout; target system standard folder layout otherwise")
message("Build for ACT folder structure: ${TARGET_ACT_HOME}")

if(DEFINED ENV{ACT_HOME})
    message("ACT_HOME is defined and points to $ENV{ACT_HOME}")
else(DEFINED ENV{ACT_HOME})
    message("ACT_HOME is not defined.")

    # if we want to build for the ACT_HOME folder structure, this is an issue
    if(TARGET_ACT_HOME)
        message(FATAL_ERROR "Instructed to build for ACT_HOME but ACT_HOME is not defined! Export your ACT_HOME location and try again.")
    endif(TARGET_ACT_HOME)
endif(DEFINED ENV{ACT_HOME})

# installation directory
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT AND TARGET_ACT_HOME)
    set(CMAKE_INSTALL_PREFIX $ENV{ACT_HOME} CACHE PATH "installation path" FORCE)
endif(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT AND TARGET_ACT_HOME)
message("Installing to ${CMAKE_INSTALL_PREFIX}")

# installation destination for sub-components
set(BIN_PATH "bin" CACHE PATH "sub-path for binaries")
set(LIB_PATH "lib" CACHE PATH "sub-path for libraries")
set(HEADER_PATH "include/actsim" CACHE PATH "sub-path for header files")
set(ACTRES_PATH "act/sim" CACHE PATH "sub-path for ACT resources")

# if we're on unix like we want the config in /etc
# if this is windows, just shove it into the program folder
if((TARGET_ACT_HOME) OR (CMAKE_SYSTEM_NAME STREQUAL "Windows"))
    set(CONF_PATH "conf/generic" CACHE PATH "sub-path for configuration files")
else((TARGET_ACT_HOME) OR (CMAKE_SYSTEM_NAME STREQUAL "Windows"))
    set(CONF_PATH "/etc/actsim" CACHE PATH "sub-path for configuration files")
endif((TARGET_ACT_HOME) OR (CMAKE_SYSTEM_NAME STREQUAL "Windows"))

# The final installation location for the simulation library is generated automatically
# on the assumption that the generated library is not moved after calling `make install`.
# Should you try to package this software, you want to override this.
if(TARGET_ACT_HOME)
    set(
        SIMLIB_FINAL_INSTALL_LOCATION 
        "\${ACT_HOME}/${LIB_PATH}/lib${SIMLIB_NAME}.so"
        CACHE 
        PATH 
        "final location of C++ component of simlib"
    )
else(TARGET_ACT_HOME)
    set(
        SIMLIB_FINAL_INSTALL_LOCATION 
        "${CMAKE_INSTALL_PREFIX}/${LIB_PATH}/lib${SIMLIB_NAME}.so"
        CACHE 
        PATH 
        "final location of C++ component of simlib"
    )
endif(TARGET_ACT_HOME)

if(DEBUG_CMAKE)
    message("Searching for libraries at system paths ${CMAKE_LIBRARY_PATH}")
endif(DEBUG_CMAKE)

# set up library locations and headers
# this can be overriden by command line arguments for different installation layout
if(DEFINED ENV{ACT_HOME})

    set(LIB_LOCATIONPREFIX $ENV{ACT_HOME}/lib)

    find_library(
        ACTBASELIB_LOCATION 
        NAMES "libact_sh" "act_sh" 
        HINTS ${LIB_LOCATIONPREFIX}/ 
        REQUIRED
    )
    find_library(
        ACTCOMMONLIB_LOCATION 
        NAMES "libvlsilib_sh" "vlsilib_sh" 
        HINTS ${LIB_LOCATIONPREFIX}/ 
        REQUIRED
    )
    find_library(
        ACTPASSLIB_LOCATION 
        NAMES "libactpass_sh" "actpass_sh" 
        HINTS ${LIB_LOCATIONPREFIX}/ 
        REQUIRED
    )
    find_library(
        ACTASIMLIB_LOCATION 
        NAMES "libasim" "asim" 
        HINTS ${LIB_LOCATIONPREFIX}/ 
        REQUIRED
    )
    find_library(
        ACTSCMLIB_LOCATION 
        NAMES "libactscm" "actscm" 
        HINTS ${LIB_LOCATIONPREFIX}/ 
        REQUIRED
    )
    find_library(
        ACTSCMCLILIB_LOCATION 
        NAMES "libactscmcli" "actscmcli" 
        HINTS ${LIB_LOCATIONPREFIX}/ 
        REQUIRED
    )
    find_library(
        TRACELIB_LOCATION 
        NAMES "libtracelib" "tracelib" 
        HINTS ${LIB_LOCATIONPREFIX}/ 
        REQUIRED
    )

else(DEFINED ENV{ACT_HOME})

    find_library(
        ACTBASELIB_LOCATION 
        NAMES "libact_sh" "libact_sh"
        REQUIRED
    )
    find_library(
        ACTCOMMONLIB_LOCATION 
        NAMES "libvlsilib_sh" "libvlsilib_sh"
        REQUIRED
    )
    find_library(
        ACTPASSLIB_LOCATION 
        NAMES "libactpass_sh" "libactpass_sh"
        REQUIRED
    )
    find_library(
        ACTASIMLIB_LOCATION 
        NAMES "libasim" "libasim"
        REQUIRED
    )
    find_library(
        ACTSCMLIB_LOCATION 
        NAMES "libactscm" "libactscm"
        REQUIRED
    )
    find_library(
        ACTSCMCLILIB_LOCATION 
        NAMES "libactscmcli" "libactscmcli"
        REQUIRED
    )
    find_library(
        TRACELIB_LOCATION 
        NAMES "libtracelib" "libtracelib"
        REQUIRED
    )

endif(DEFINED ENV{ACT_HOME})

# ACT library header location
if(DEFINED ENV{ACT_HOME})
    set(ACTHEADERS_LOCATION $ENV{ACT_HOME}/include/ CACHE PATH "import location for ACT library headers")
else(DEFINED ENV{ACT_HOME})
    # we want to search for everything in the act folder
    set(ACTHEADERS_LOCATION ${CMAKE_SYSTEM_INCLUDE_PATH} CACHE PATH "import location for ACT library headers")
    list(TRANSFORM ${ACTHEADERS_LOCATION} APPEND "act/")
endif(DEFINED ENV{ACT_HOME})

# look for XYCE in the most probable locations
find_library(XYCELIB_LOCATION "libxyce.a"
    $ENV{ACT_HOME}/lib
)

# compiler options
add_compile_options(
    -Wall
    -Wextra
    -Wconversion-null
    -Wmissing-declarations
    -Woverlength-strings
    -Wpointer-arith
    -Wunused-local-typedefs
    -Wunused-result
    -Wvarargs
    -Wvla
    -Wwrite-strings
    -Wformat-security
    -Wundef
)

add_compile_options(
    -O2
    -fPIC
)

# add the libraries
add_library(ACTLIB SHARED IMPORTED)
set_target_properties(ACTLIB PROPERTIES IMPORTED_LOCATION ${ACTBASELIB_LOCATION})

add_library(ACTCOMMON SHARED IMPORTED)
set_target_properties(ACTCOMMON PROPERTIES IMPORTED_LOCATION ${ACTCOMMONLIB_LOCATION})

add_library(ACTPASSLIB SHARED IMPORTED)
set_target_properties(ACTPASSLIB PROPERTIES IMPORTED_LOCATION ${ACTPASSLIB_LOCATION})

add_library(ACTASIMLIB STATIC IMPORTED)
set_target_properties(ACTASIMLIB PROPERTIES IMPORTED_LOCATION ${ACTASIMLIB_LOCATION})

add_library(ACTSCMLIB STATIC IMPORTED)
set_target_properties(ACTSCMLIB PROPERTIES IMPORTED_LOCATION ${ACTSCMLIB_LOCATION})

add_library(ACTSCMCLILIB STATIC IMPORTED)
set_target_properties(ACTSCMCLILIB PROPERTIES IMPORTED_LOCATION ${ACTSCMCLILIB_LOCATION})

add_library(TRACELIB STATIC IMPORTED)
set_target_properties(TRACELIB PROPERTIES IMPORTED_LOCATION ${TRACELIB_LOCATION})

# check if XYCE is installed on the system
if(DEFINED XYCELIB_LOCATION AND NOT XYCELIB_LOCATION STREQUAL "XYCELIB_LOCATION-NOTFOUND")
    # prepare everything for XYCE support
    message("Compiling with XYCE support.")
    set(LINK_XYCE)
    add_library(XYCE_LIB SHARED IMPORTED)
    set_target_properties(XYCE_LIB PROPERTIES IMPORTED_LOCATION ${XYCELIB_LOCATION})
else()
    message("XYCE was not found on the system. Compiling without XYCE support.")
endif()

# Set the output directory of libraries
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Set the output directory of executables
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# include source
add_subdirectory(src)
# include simlib
add_subdirectory(simlib)

# if we need to build the tracelib as well, include it
if(${BUILD_TRACELIB})
    add_subdirectory(tracelib)
endif()

# specify install targets
# main program
install(
    TARGETS ${PROJECT_NAME}
    DESTINATION ${BIN_PATH}/
)

# simlib components
install(
    TARGETS ${SIMLIB_NAME}
    DESTINATION ${LIB_PATH}/
    COMPONENT SIMLIB
)

install(
    FILES "simlib/simlib_file.h"
    DESTINATION ${HEADER_PATH}/
    COMPONENT SIMLIB
)

install(
    FILES "${CMAKE_BINARY_DIR}/simlib/actsim.conf"
    DESTINATION ${CONF_PATH}/
    COMPONENT SIMLIB
)

install(
    DIRECTORY simlib/act/
    DESTINATION ${ACTRES_PATH}/
    COMPONENT SIMLIB
)

# FLI headers
install(
    FILES "simlib/actsim_ext.h"
    DESTINATION ${HEADER_PATH}/
    COMPONENT FLI
)

# debug prints
if(DEBUG_CMAKE)
    message("")
    message("Debug information:")
    message("Full binary installation path is ${CMAKE_INSTALL_PREFIX}/${BIN_PATH}")
    message("Full library installation path is ${CMAKE_INSTALL_PREFIX}/${LIB_PATH}")
    message("Full header installation path is ${CMAKE_INSTALL_PREFIX}/${HEADER_PATH}")
    message("Full ACT ressources installation path is ${CMAKE_INSTALL_PREFIX}/${ACTRES_PATH}")

    if((TARGET_ACT_HOME) OR (CMAKE_SYSTEM_NAME STREQUAL "Windows"))
        message("Full configuration installation path is ${CMAKE_INSTALL_PREFIX}/${CONF_PATH}")
    else((TARGET_ACT_HOME) OR (CMAKE_SYSTEM_NAME STREQUAL "Windows"))
        message("Full configuration installation path is ${CONF_PATH}")
    endif((TARGET_ACT_HOME) OR (CMAKE_SYSTEM_NAME STREQUAL "Windows"))

    message("ACTBASELIB_LOCATION is ${ACTBASELIB_LOCATION}")
    message("ACTCOMMONLIB_LOCATION is ${ACTCOMMONLIB_LOCATION}")
    message("ACTPASSLIB_LOCATION is ${ACTPASSLIB_LOCATION}")
    message("ACTASIMLIB_LOCATION is ${ACTASIMLIB_LOCATION}")
    message("ACTSCMLIB_LOCATION is ${ACTSCMLIB_LOCATION}")
    message("ACTSCMCLILIB_LOCATION is ${ACTSCMCLILIB_LOCATION}")
    message("TRACELIB_LOCATION is ${TRACELIB_LOCATION}")

    if(LINK_XYCE)
        message("XYCELIB_LOCATION is ${XYCELIB_LOCATION}")
    endif(LINK_XYCE)

    message("ACT library headers are imported from ${ACTHEADERS_LOCATION}")
    message("Final destination for simlib according to config: ${SIMLIB_FINAL_INSTALL_LOCATION}")
endif(DEBUG_CMAKE)
