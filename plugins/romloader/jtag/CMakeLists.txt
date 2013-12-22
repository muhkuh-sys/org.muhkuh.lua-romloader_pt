cmake_minimum_required(VERSION 2.8)

ADD_SUBDIRECTORY(openocd)

MESSAGE("OPENOCD_LIBRARIES: ${OPENOCD_LIBRARIES}")
MESSAGE("OPENOCD_DEFINES: ${OPENOCD_DEFINES}")
MESSAGE("OPENOCD_INCLUDE_DIRECTORIES: ${OPENOCD_INCLUDE_DIRECTORIES}")


PROJECT("romloader_jtag")

SET(romloader_jtag_VERSION 0.0.1)


FIND_PACKAGE(SWIG REQUIRED)
INCLUDE(${SWIG_USE_FILE})

if(${LUA51_FOUND})
	include_directories(${LUA_INCLUDE_DIR})
endif(${LUA51_FOUND})

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})
INCLUDE_DIRECTORIES(${SWIG_RUNTIME_OUTPUT_PATH})


#----------------------------------------------------------------------------
#
# Set the platform specific parts.
#

# Append the include directories for openocd.
INCLUDE_DIRECTORIES(${OPENOCD_INCLUDE_DIRECTORIES})

# Append the openocd defines.
ADD_DEFINITIONS(${OPENOCD_DEFINES})


SET(romloader_jtag_SOURCES
	romloader_jtag_main.cpp
	romloader_jtag_openocd.c
)

SET(romloader_jtag_HEADERS
	romloader_jtag_main.h
	romloader_jtag_openocd.h
)


ADD_LIBRARY(TARGET_romloader_jtag_lib STATIC ${romloader_jtag_SOURCES})
# This static library will be linked with a shared object later. Add "-fPIC" on GCC systems or X64 builds break.
IF("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU"  AND  "${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
	SET_TARGET_PROPERTIES(TARGET_romloader_jtag_lib PROPERTIES COMPILE_FLAGS -fPIC)
ENDIF("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU"  AND  "${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")

# The library depends on the openocd header.
ADD_DEPENDENCIES(TARGET_romloader_jtag_lib TARGET_openocd)



#----------------------------------------------------------------------------
#
# Build the plugins.
#

SET_SOURCE_FILES_PROPERTIES(romloader_jtag.i PROPERTIES CPLUSPLUS ON)
SET_PROPERTY(SOURCE romloader_jtag.i PROPERTY SWIG_FLAGS ${OPENOCD_DEFINES} -I${CMAKE_HOME_DIRECTORY}/modules/typemaps)


IF(${LUA51_FOUND})
	# Build the Lua module.
	SWIG_ADD_MODULE(TARGET_plugins_romloader_romloader_jtag_LUA lua romloader_jtag.i)
	SWIG_LINK_LIBRARIES(TARGET_plugins_romloader_romloader_jtag_LUA TARGET_romloader_jtag_lib ${LUA_LIBRARIES} ${OPENOCD_LIBRARIES} TARGET_plugins_romloader_LIB)
	
	# Set the name of the output file to "romloader_jtag".
	SET_TARGET_PROPERTIES(TARGET_plugins_romloader_romloader_jtag_LUA PROPERTIES PREFIX "" OUTPUT_NAME "romloader_jtag")
	
	IF((${CMAKE_SYSTEM_NAME} STREQUAL "Windows") AND (${CMAKE_COMPILER_IS_GNUCC}))
		# On minGW link all compiler libraries static.
		SET_PROPERTY(TARGET TARGET_plugins_romloader_romloader_jtag_LUA PROPERTY LINK_FLAGS "-static-libgcc -static-libstdc++")
	ENDIF((${CMAKE_SYSTEM_NAME} STREQUAL "Windows") AND (${CMAKE_COMPILER_IS_GNUCC}))
	
	# The module depends on the openocd libraries.
	ADD_DEPENDENCIES(TARGET_plugins_romloader_romloader_jtag_LUA TARGET_openocd)
	
	# Install the lua module.
	INSTALL(TARGETS TARGET_plugins_romloader_romloader_jtag_LUA DESTINATION ${INSTALL_DIR_LUA_MODULES})
ENDIF(${LUA51_FOUND})
