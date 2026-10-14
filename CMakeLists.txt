#++
#	FACILITY: A yet another gateway for MODBUS (TCP to RTU)
#
#	AUTHOR: StarLet Squad and Ruslan R. Laishev (AKA: BadAss sysman)
#
#	CREATION DATE: 10-SEP-2025
#
#	MODIFICATION HISTORY:
#
#	25-AUG-2026	RRL	X.00-04: Diagnostic options are ON for any build type;
#				added ASAN build type;
#				fixed CMAKE_HOST_SYSTEM_PROCESSOR -> CMAKE_SYSTEM_PROCESSOR (cross-building);
#				fixed order of cmake_minimum_required()/project().
#
#	25-AUG-2026	RRL	X.00-06ECO01: The User Guides are delivered as the DEC/VSI-styled PDFs too
#				(docs/UserGuide_EN.pdf, docs/UserGuide_RU.pdf, the generator is
#				docs/make_userguide_pdf.py); both are installed with the docs.
#
#	25-AUG-2026	RRL	X.00-08: The musl C library builds are supported: added the toolchain file
#				cmake/musl.cmake (the kernel headers are appended by -idirafter, so the
#				musl ones always win); the library directory reported by pkg-config is
#				given to the linker explicitly - a libconfig installed to a non-standard
#				prefix was not found at the link time.
#
#	25-AUG-2026	RRL	X.00-07ECO04: README is delivered in two languages (README.md +
#				README_RU.md), both are installed with the docs.
#
#	25-AUG-2026	RRL	X.00-06: The bilingual (EN + RU) User Guide is installed with the docs.
#
#	25-AUG-2026	RRL	X.00-05ECO01: Removed the obsolete modbus-t2r_startup.sh (referenced the
#				pre-package layout); a service management is a matter of a systemd unit
#				of the target system.
#
#	25-AUG-2026	RRL	X.00-05: The 3rdparty/ submodules are gone - the StarLet set is consumed
#				as an installed package now: find_package(StarLet) + StarLet::starlet;
#				added the <install> and <uninstall> targets: the binary goes to sbin/,
#				the configuration examples - to etc/mbusgw-t2r/ (the existing files
#				are never overwritten), the docs - to share/doc/mbusgw-t2r/.
#
#	14-OCT-2026	RRL	X.00-09: The default installation prefix is /opt/mbusgw-t2r (a
#				self-contained tree: sbin/, etc/, share/doc/mbusgw-t2r/); an explicit
#				-DCMAKE_INSTALL_PREFIX=... still works as before. Under /opt/ the
#				configuration goes to <prefix>/etc/ - not to the GNUInstallDirs'
#				/etc/opt/<prefix>/. Fixed the doubled DESTDIR in the configuration
#				files path (file (INSTALL) prepends DESTDIR by itself).
#
#	14-OCT-2026	RRL	X.00-10: contrib/99-ftdi-latency.rules (the FTDI latency_timer = 1 msec udev
#				rule) is installed with the docs.
#
#	USAGE (see README.md for the whole installation sequence):
#		$ mkdir ./build && cd ./build
#		$ cmake ../ -DCMAKE_BUILD_TYPE=Release
#		$ make -s
#		$ sudo make install
#		$ sudo make uninstall
#--

cmake_minimum_required (VERSION 3.10)
cmake_policy(SET CMP0048 NEW)

include(CMakePrintHelpers)
include(CheckSymbolExists)



project(mbusgw-t2r C)

#
#   The default installation prefix is /opt/mbusgw-t2r; must precede GNUInstallDirs, which
#   derives the installation directories from it. An explicit -DCMAKE_INSTALL_PREFIX wins.
#
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
	set (CMAKE_INSTALL_PREFIX "/opt/${PROJECT_NAME}" CACHE PATH "Installation prefix" FORCE)
endif ()

include(GNUInstallDirs)							# Must follow project(): needs the compiler info
set ( CMAKE_VERBOSE_MAKEFILE ON )
set ( CMAKE_COLOR_MAKEFILE   ON )

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBCONFIG REQUIRED libconfig)

#
#   LIBCONFIG_LIBRARIES holds the bare "-lconfig" name, so the library directory reported by
#   pkg-config must be given to the linker explicitly - otherwise a libconfig installed to a
#   non-standard prefix (a musl sysroot, /opt/...) is not found at the link time.
#
link_directories(${LIBCONFIG_LIBRARY_DIRS})


find_package(StarLet REQUIRED)						# The installed UTILITY$ROUTINES package,
									# see README.md how to install it


set ( T2R_TARGET        "mbusgw-t2r" )

add_definitions( -D__ARCH__NAME__="${CMAKE_SYSTEM_PROCESSOR}" )		# NOT ..._HOST_..., see cross-building
add_definitions( -D_REENTRANT=1 )
cmake_print_variables(CMAKE_CURRENT_SOURCE_DIR)


add_compile_options(							# Diagnostic is ON for any build type
	-Wall -Wextra -Wformat=2
	-Werror=format
	-Werror=return-type
	-Werror=implicit-function-declaration
	-Wno-unused-parameter
	-Wno-missing-braces						# The StarLet's $ASCINI/OPTS_NULL macro style
)

string(TOLOWER "${CMAKE_BUILD_TYPE}" build_type)			# Additional definitons for DEBUG builds
if (build_type STREQUAL debug)
	add_definitions( -D_DEBUG=1 -D__TRACE__=1 )
	add_compile_options( -g3 -O0 -fno-omit-frame-pointer )
endif()

#
#   An additional build type with the run-time checking of memory access and UB:
#	$ cmake ../ -DCMAKE_BUILD_TYPE=Asan
#
if (build_type STREQUAL asan)
	add_definitions( -D_DEBUG=1 -D__TRACE__=1 )
	add_compile_options( -g3 -O1 -fsanitize=address,undefined -fno-omit-frame-pointer )
	add_link_options( -fsanitize=address,undefined )
endif()


check_symbol_exists(TIOCSRS485 sys/ioctl.h HAVE_TIOCRS485)
if(HAVE_TIOCRS485)
    add_definitions(-DHAVE_TIOCRS485)
endif()


include_directories (
	src/
	${LIBCONFIG_INCLUDE_DIRS}
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
target_link_libraries( ${T2R_TARGET} StarLet::starlet pthread rt ${LIBCONFIG_LIBRARIES})


#------------------------------------------------------------------------------------------------------
#	Installation:	sbin/mbusgw-t2r
#			etc/mbusgw-t2r/*.conf	- the existing configuration files are never overwritten
#						  (etc/*.conf under /opt/: the tree is self-contained)
#			share/doc/mbusgw-t2r/	- README, BLOCKNOTE, the reference copies of the configs,
#						  the FTDI udev rule
#
if (CMAKE_INSTALL_PREFIX MATCHES "^/opt/")
	set (T2R_CONFDIR "${CMAKE_INSTALL_PREFIX}/etc")
else ()
	set (T2R_CONFDIR "${CMAKE_INSTALL_FULL_SYSCONFDIR}/mbusgw-t2r")
endif ()

install (TARGETS ${T2R_TARGET}
	RUNTIME DESTINATION ${CMAKE_INSTALL_SBINDIR}
)

install (FILES
	README.md
	README_RU.md
	BLOCKNOTE.md
	modbus-t2r.conf
	modbus-t2r_settings.conf
	docs/UserGuide_EN.md
	docs/UserGuide_RU.md
	docs/UserGuide_EN.pdf
	docs/UserGuide_RU.pdf
	contrib/99-ftdi-latency.rules
	DESTINATION ${CMAKE_INSTALL_DOCDIR}
)

#	The configuration files are put to etc/ only when they are absent: an update must not
#	destroy the has been tuned working configuration.
install (CODE "
	file (MAKE_DIRECTORY \"\$ENV{DESTDIR}${T2R_CONFDIR}\")
	foreach (l_conf modbus-t2r.conf modbus-t2r_settings.conf)
		set (l_dst \"\$ENV{DESTDIR}${T2R_CONFDIR}/\${l_conf}\")
		if (NOT EXISTS \"\${l_dst}\")
			file (INSTALL \"${CMAKE_CURRENT_SOURCE_DIR}/\${l_conf}\"
				DESTINATION \"${T2R_CONFDIR}\")	# file (INSTALL) adds DESTDIR itself
		else ()
			message (STATUS \"Keeping the existing \${l_dst}\")
		endif ()
	endforeach ()
")

#	'make uninstall': removes everything from install_manifest.txt; the configuration files
#	which were kept (not overwritten) at the install time are not touched.
add_custom_target (uninstall
	COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/uninstall.cmake
	COMMENT "Uninstalling by install_manifest.txt"
)


add_custom_target(files_config SOURCES
	modbus-t2r.conf
	modbus-t2r_settings.conf

	README.md
	BLOCKNOTE.md
)
