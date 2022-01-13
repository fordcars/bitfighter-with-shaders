## Global project configuration

#
# Environment verification
#
set(OSX_DEPLOY_TARGET $ENV{MACOSX_DEPLOYMENT_TARGET})

message(STATUS "MACOSX_DEPLOYMENT_TARGET: ${OSX_DEPLOY_TARGET}")


# These mandatory variables should be set with cross-compiling
if(NOT XCOMPILE)
	# Detect current OSX version
	set(OSX_VERSION 0)
	find_program(sw_vers sw_vers)
	execute_process(COMMAND ${sw_vers} "-productVersion" OUTPUT_VARIABLE OSX_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
	string(REGEX REPLACE "([0-9]+.[0-9]+).*" "\\1" OSX_VERSION ${OSX_VERSION})
	
	message(STATUS "OSX Version: ${OSX_VERSION}")
    
	if(NOT OSX_DEPLOY_TARGET)
		# MACOSX_DEPLOYMENT_TARGET must be set in the environment to compile properly on OSX 10.6 and earlier
		if(${OSX_VERSION} VERSION_LESS "10.7")
			message(FATAL_ERROR "MACOSX_DEPLOYMENT_TARGET environment variable not set.  Set this like so: 'export MACOSX_DEPLOYMENT_TARGET=10.6'")
		else()
			set(OSX_DEPLOY_TARGET ${OSX_VERSION})
		endif()
	endif()


	# Make sure the compiling architecture is set
	if(OSX_DEPLOY_TARGET VERSION_LESS "10.5")
		message(FATAL_ERROR "Bitfighter cannot be compiled on OSX earlier than 10.5")
	elseif(OSX_DEPLOY_TARGET VERSION_LESS "10.6")
		set(CMAKE_OSX_ARCHITECTURES "i386")
	else()
		set(CMAKE_OSX_ARCHITECTURES "x86_64")
	endif()
	
	message(STATUS "CMAKE_OSX_SYSROOT: ${CMAKE_OSX_SYSROOT}")
endif()

message(STATUS "Compiling for OSX architectures: ${CMAKE_OSX_ARCHITECTURES}")


#
# Linker flags
# 

# 
# Compiler specific flags
# 

# c++11
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -std=gnu++11")

# OSX 10.7 and greater need this to find some dependencies
if(OSX_DEPLOY_TARGET VERSION_GREATER "10.6")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif()

# Enable more warnings in Debug build
if(CMAKE_COMPILER_IS_GNUCC)
	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wall")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall")
endif()


#
# Library searching and dependencies
#
set(BF_LIB_DIR ${CMAKE_SOURCE_DIR}/lib)
set(BF_LIB_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/lib/include)

# Set some search paths
set(SDL2_SEARCH_PATHS       ${BF_LIB_DIR} ${BF_LIB_INCLUDE_DIR}/libsdl)
set(OGG_SEARCH_PATHS        ${BF_LIB_DIR} ${BF_LIB_INCLUDE_DIR}/libogg)
set(VORBIS_SEARCH_PATHS     ${BF_LIB_DIR} ${BF_LIB_INCLUDE_DIR}/libvorbis)
set(VORBISFILE_SEARCH_PATHS ${BF_LIB_DIR} ${BF_LIB_INCLUDE_DIR}/libvorbis)
set(SPEEX_SEARCH_PATHS      ${BF_LIB_DIR} ${BF_LIB_INCLUDE_DIR}/libspeex)
set(MODPLUG_SEARCH_PATHS    ${BF_LIB_DIR} ${BF_LIB_INCLUDE_DIR}/libmodplug)
set(ALURE_SEARCH_PATHS      ${BF_LIB_DIR} ${BF_LIB_INCLUDE_DIR}/alure)

# Directly set include dirs for some libraries
set(OPENAL_INCLUDE_DIR "${BF_LIB_INCLUDE_DIR}/openal/include")
set(ZLIB_INCLUDE_DIR "${BF_LIB_INCLUDE_DIR}/zlib")
# libpng needs two for some weird reason
set(PNG_INCLUDE_DIR "${BF_LIB_INCLUDE_DIR}/libpng")
set(PNG_PNG_INCLUDE_DIR "${BF_LIB_INCLUDE_DIR}/libpng")

# Directly specify some libs
set(OPENAL_LIBRARY "${BF_LIB_DIR}/libopenal.1.dylib")
set(PNG_LIBRARY "${BF_LIB_DIR}/libpng.framework")

set(SPARKLE_SEARCH_PATHS ${BF_LIB_DIR})
# OSX doesn't use vorbisfile (or it's built-in to normal vorbis, I think)
set(VORBISFILE_LIBRARIES "")


find_package(Sparkle)


## End Global project configuration


## Sub-project configuration
#
# Note that any variable adjustment from the parent CMakeLists.txt will
# need to be re-set with the PARENT_SCOPE option

function(BF_PLATFORM_SET_EXTRA_SOURCES)
	list(APPEND SHARED_SOURCES Directory.mm)
	set(SHARED_SOURCES ${SHARED_SOURCES} PARENT_SCOPE)
endfunction()


function(BF_PLATFORM_SET_EXTRA_LIBS)
	set(EXTRA_LIBS dl m PARENT_SCOPE)
endfunction()


function(BF_PLATFORM_APPEND_LIBS)
	list(APPEND CLIENT_LIBS ${SPARKLE_LIBRARIES})
	set(CLIENT_LIBS ${CLIENT_LIBS} PARENT_SCOPE)
endfunction()


function(BF_PLATFORM_ADD_DEFINITIONS)
	add_definitions(-iquote ${CMAKE_SOURCE_DIR}/zap)
endfunction()


function(BF_PLATFORM_SET_EXECUTABLE_NAME)
	set(BF_EXE_NAME "Bitfighter" PARENT_SCOPE)
endfunction()


function(BF_PLATFORM_SET_TARGET_PROPERTIES targetName)
	
	# We need this variable in both scopes
	set(OSX_BUILD_RESOURCE_DIR "${CMAKE_SOURCE_DIR}/build/osx/")
	set(OSX_BUILD_RESOURCE_DIR "${OSX_BUILD_RESOURCE_DIR}" PARENT_SCOPE)
	
	# Special flags needed because of LuaJIT on 64 bit OSX
	if(CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64")
		set_target_properties(${targetName} PROPERTIES LINK_FLAGS "-pagezero_size 10000 -image_base 100000000")
	endif()
endfunction()


function(BF_PLATFORM_SET_TARGET_OTHER_PROPERTIES targetName)
	# Setup OSX Bundle; specify output to be a .app
	set_target_properties(${targetName} PROPERTIES MACOSX_BUNDLE TRUE)
	
	# Set up our bundle plist variables
	set(MACOSX_BUNDLE_NAME ${targetName})
	set(MACOSX_BUNDLE_EXECUTABLE ${targetName})
	
	# Fill out a plist template with CMake variables
	configure_file(${OSX_BUILD_RESOURCE_DIR}/Bitfighter-Info.plist.cmake ${OSX_BUILD_RESOURCE_DIR}/Bitfighter-Info.plist)
	
	# Use the custom plist
	set_target_properties(${targetName} PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${OSX_BUILD_RESOURCE_DIR}/Bitfighter-Info.plist)
endfunction()


function(BF_PLATFORM_POST_BUILD_INSTALL_RESOURCES targetName)
	# The trailing slash is necessary to do here for proper native path translation
	file(TO_NATIVE_PATH ${CMAKE_SOURCE_DIR}/resource/ resDir)
	file(TO_NATIVE_PATH ${CMAKE_SOURCE_DIR}/lib/ libDir)
	file(TO_NATIVE_PATH ${CMAKE_SOURCE_DIR}/lua/luajit/src/ luaLibDir)
	file(TO_NATIVE_PATH ${CMAKE_SOURCE_DIR}/exe exeDir)
	
	# Create extra dirs in the .app
	set(frameworksDir "${exeDir}/${targetName}.app/Contents/Frameworks")
	set(resourcesDir "${exeDir}/${targetName}.app/Contents/Resources")
	execute_process(COMMAND mkdir -p ${frameworksDir})
	execute_process(COMMAND mkdir -p ${resourcesDir})
	
	set(RES_COPY_CMD cp -rp ${resDir}* ${resourcesDir})
	set(LIB_COPY_CMD rsync -av --exclude=*.h ${libDir}*.framework ${frameworksDir})
	set(LIB_COPY_CMD_2 rsync -av ${libDir}*.dylib ${frameworksDir})
	
	# Icon file
	set(COPY_RES_1 cp -rp ${OSX_BUILD_RESOURCE_DIR}/Bitfighter.icns ${resourcesDir})
	# Public key for Sparkle
	set(COPY_RES_2 cp -rp ${OSX_BUILD_RESOURCE_DIR}/dsa_pub.pem ${resourcesDir})
	# Notifier
	set(COPY_RES_3 cp -rp ${exeDir}/../notifier/bitfighter_notifier.py ${resourcesDir})
	set(COPY_RES_4 cp -rp ${exeDir}/../notifier/redship18.png ${resourcesDir})
	
	add_custom_command(TARGET ${targetName} POST_BUILD 
		COMMAND ${COPY_RES_1}
		COMMAND ${COPY_RES_2}
		COMMAND ${COPY_RES_3}
		COMMAND ${COPY_RES_4}
	)
	
	# 64-bit OSX needs to use shared LuaJIT library
	if(CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64")
		add_custom_command(TARGET ${targetName} POST_BUILD
			COMMAND cp -rp ${luaLibDir}libluajit.dylib ${frameworksDir}
		)
	endif()
	
	# Copy resources
	add_custom_command(TARGET ${targetName} POST_BUILD 
		COMMAND ${RES_COPY_CMD}
		COMMAND ${LIB_COPY_CMD}
		COMMAND ${LIB_COPY_CMD_2}
	)
	
	# Thin out our installed frameworks by running 'lipo' to clean out the unwanted 
	# architectures and removing any header files
	if(NOT LIPO_COMMAND)
		set(LIPO_COMMAND lipo)
	endif()
	
	# This can happen when cross-compiling x86_64
	if(NOT CMAKE_OSX_ARCHITECTURES)
		set(CMAKE_OSX_ARCHITECTURES "x86_64")
	endif()
	
	set(THIN_FRAMEWORKS ${CMAKE_SOURCE_DIR}/build/osx/tools/thin_frameworks.sh ${LIPO_COMMAND} ${CMAKE_OSX_ARCHITECTURES} ${exeDir}/${targetName}.app)
	set(DO_RPATH_THING ${CMAKE_INSTALL_NAME_TOOL} -add_rpath "@executable_path/../Frameworks/" ${exeDir}/${targetName}.app/Contents/MacOS/${targetName})
	
	add_custom_command(TARGET ${targetName} POST_BUILD
		COMMAND ${THIN_FRAMEWORKS}
		COMMAND ${DO_RPATH_THING}
	)
endfunction()


function(BF_PLATFORM_INSTALL targetName)
	set(CMAKE_INSTALL_PREFIX "/Applications")
	
	# This will install the .app.  The .app should have already been built
	# in the post-build section
	install(TARGETS ${targetName} BUNDLE DESTINATION ./)

endfunction()


function(BF_PLATFORM_CREATE_PACKAGES targetName)
	set(CPACK_GENERATOR "DragNDrop")
	set(CPACK_SYSTEM_NAME "OSX")
	
	if(CMAKE_OSX_ARCHITECTURES STREQUAL "i386")
		set(DMG_ARCH "32bit-Intel")
	else()
		set(DMG_ARCH "64bit-Intel")
	endif()
	
	set(CPACK_PACKAGE_FILE_NAME "Bitfighter-${BF_VERSION}-OSX-${DMG_ARCH}")
	set(CPACK_DMG_FORMAT "UDBZ")
	set(CPACK_DMG_VOLUME_NAME "Bitfighter ${BF_VERSION}")
	set(CPACK_DMG_DS_STORE "${OSX_BUILD_RESOURCE_DIR}/bitfighter.dsstore")
	set(CPACK_DMG_BACKGROUND_IMAGE "${OSX_BUILD_RESOURCE_DIR}/bf_dmg_background.png")
	
	#set(CPACK_PACKAGE_ICON "${ICONS_DIR}/DMG.icns")
	
	include(CPack)
endfunction()
