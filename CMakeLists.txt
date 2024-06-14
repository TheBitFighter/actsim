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
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor,
# Boston, MA  02110-1301, USA.
#

cmake_minimum_required(VERSION 3.15)

project(actsim)
set(SIMLIB_NAME "actsimext_sh")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

# if you don't want to build the tracelib together with actsim, set this to false
set(BUILD_TRACELIB FALSE)

# installation directory
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX $ENV{ACT_HOME} CACHE PATH "installation path" FORCE)
endif(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)

# installation destination for sub-components
set(BIN_PATH "bin")
set(LIB_PATH "lib")
set(HEADER_PATH "include/actsim")
set(CONF_PATH "conf/generic")
set(ACTRES_PATH "act/sim")

# set up library locations
# this can be overriden by command line arguments for different installation layout
set(LIB_LOCATIONPREFIX $ENV{ACT_HOME}/lib)
set(ACTBASELIB_LOCATION ${LIB_LOCATIONPREFIX}/libact_sh.so)
set(ACTCOMMONLIB_LOCATION ${LIB_LOCATIONPREFIX}/libvlsilib_sh.so)
set(ACTPASSLIB_LOCATION ${LIB_LOCATIONPREFIX}/libactpass_sh.so)
set(ACTASIMLIB_LOCATION ${LIB_LOCATIONPREFIX}/libasim.a)
set(ACTSCMLIB_LOCATION ${LIB_LOCATIONPREFIX}/libactscm.a)
set(ACTSCMCLILIB_LOCATION ${LIB_LOCATIONPREFIX}/libactscmcli.a)
set(TRACELIB_LOCATION ${LIB_LOCATIONPREFIX}/libtracelib.a)

# look in the most probably locations for XYCE
find_library(XYCELIB_LOCATION "libxyce.a"
    $ENV{ACT_HOME}/lib
    "/usr/lib"
    "/usr/local/lib"
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

add_library(ACTCOMMON STATIC IMPORTED)
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
    FILES "simlib/actsim.conf"
    DESTINATION ${CONF_PATH}/
    COMPONENT SIMLIB
)

install(
    DIRECTORY simlib/act
    DESTINATION ${ACTRES_PATH}/
    COMPONENT SIMLIB
)

# FLI headers
install(
    DIRECTORY simlib/
    DESTINATION ${HEADER_PATH}/
    COMPONENT FLI
    FILES_MATCHING
    PATTERN "actsim_ext.h"
)
