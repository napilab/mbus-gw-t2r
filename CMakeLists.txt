#++
#	FACILITY: A yet another gateway for MODBUS (TCP to RTU)
#
#	AUTHOR: Ruslan R. Laishev (AKA "BadAss SysMan")
#
#	CREATION DATE: 10-SEP-2025
#
#	USAGE:
#		$ mkdir ./build && cd ./build
#		$ cmake ../ -DCMAKE_BUILD_TYPE=Debug
#		$ make -s
#
#	MODIFICATION HISTORY:
#
#--

cmake_policy(SET CMP0048 NEW)

include(CMakePrintHelpers)
include(CheckSymbolExists)



project(mbusgw-t2r C)
cmake_minimum_required (VERSION 3.10 )
set ( CMAKE_VERBOSE_MAKEFILE ON )
set ( CMAKE_COLOR_MAKEFILE   ON )

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBCONFIG REQUIRED libconfig)


set (3RDPARTY_ROOT      3rdparty/ )

set (STARLET_DIR       ${3RDPARTY_ROOT}/utility_routines/ )
add_subdirectory(${STARLET_DIR})					# UTILITY$ROUTINES


set ( T2R_TARGET        "mbusgw-t2r" )

add_definitions( -D__ARCH__NAME__="${CMAKE_HOST_SYSTEM_PROCESSOR}" )
add_definitions( -D_REENTRANT=1 )
cmake_print_variables(CMAKE_CURRENT_SOURCE_DIR)


string(TOLOWER ${CMAKE_BUILD_TYPE} build_type)                          # Additional definitons for DEBUG builds
if (build_type STREQUAL debug)
	add_definitions( -D_DEBUG=1 -D__TRACE__=1  -pg )			# Add "-w" to disable warnings
else ()
	add_definitions( -w  )						# Add "-w" to disable warnings
endif()


check_symbol_exists(TIOCSRS485 sys/ioctl.h HAVE_TIOCRS485)
if(HAVE_TIOCRS485)
    add_definitions(-DHAVE_TIOCRS485)
endif()


include_directories (
	${3RDPARTY_ROOT}/uthash/include/
	${STARLET_DIR}
	src/
	${LIBCONFIG_DIR}/lib/
)

set ( T2R_SOURCES                                               # TOOTOO Router
	src/t2r_defs.h
	src/t2r_msgs.h
	src/t2r_net.c
	src/t2r_tty.c
	src/t2r_main.c
	src/t2r_modbus.h
	src/t2r_modbus.c
)


add_executable ( ${T2R_TARGET} ${T2R_SOURCES})
target_link_libraries( ${T2R_TARGET} starlet pthread rt config)


#------------------------------------------------------------------------------------------------------
add_custom_target(files_config SOURCES
	modbus-t2r.conf
	modbus-t2r_startup.sh
	modbus-t2r_settings.conf

	README.md
	BLOCKNOTE.md
)
