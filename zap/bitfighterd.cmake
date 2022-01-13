#
# Dedicated server build
# 
add_executable(bitfighterd
	EXCLUDE_FROM_ALL
	${SHARED_SOURCES}
	${EXTRA_SOURCES}
	main.cpp
)

# If certain system libs were not found, add the in-tree variants as dependencies
set(DEDICATED_EXTRA_DEPS "")
if(NOT TOMCRYPT_FOUND)
	list(APPEND DEDICATED_EXTRA_DEPS tomcrypt)
endif()
if(NOT CLIPPER_FOUND)
	list(APPEND DEDICATED_EXTRA_DEPS clipper)
endif()
if(NOT POLY2TRI_FOUND)
	list(APPEND DEDICATED_EXTRA_DEPS poly2tri)
endif()
if(NOT LUAJIT_FOUND)
	list(APPEND DEDICATED_EXTRA_DEPS ${LUA_LIB})
endif()

add_dependencies(bitfighterd
	tnl
	${DEDICATED_EXTRA_DEPS}
)

target_link_libraries(bitfighterd
	${SHARED_LIBS}
)

set_target_properties(bitfighterd
	PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/exe
)

get_property(DEDICATED_DEFS TARGET bitfighterd PROPERTY COMPILE_DEFINITIONS)
set_target_properties(bitfighterd
	PROPERTIES
	COMPILE_DEFINITIONS "${DEDICATED_DEFS};ZAP_DEDICATED"
)

set_target_properties(bitfighterd PROPERTIES COMPILE_DEFINITIONS_DEBUG "TNL_DEBUG")

BF_PLATFORM_SET_TARGET_PROPERTIES(bitfighterd)

BF_PLATFORM_POST_BUILD_INSTALL_RESOURCES(bitfighterd)

# BF_PLATFORM_INSTALL(bitfighterd)

# BF_PLATFORM_CREATE_PACKAGES(bitfighterd)
