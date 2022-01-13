#
# Full client build
# 
set(BF_EXE_NAME "bitfighter")

BF_PLATFORM_SET_EXECUTABLE_NAME()

add_executable(${BF_EXE_NAME}
	$<TARGET_OBJECTS:bitfighter_client>
	${EXTRA_SOURCES}
	main.cpp
)

add_dependencies(${BF_EXE_NAME}
	bitfighter_client
)

target_link_libraries(${BF_EXE_NAME}
	${CLIENT_LIBS}
	${SHARED_LIBS}
)

# Where to put the executable
set_target_properties(${BF_EXE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/exe)

set_target_properties(${BF_EXE_NAME} PROPERTIES COMPILE_DEFINITIONS_DEBUG "TNL_DEBUG")

BF_PLATFORM_SET_TARGET_PROPERTIES(${BF_EXE_NAME})

BF_PLATFORM_SET_TARGET_OTHER_PROPERTIES(${BF_EXE_NAME})

BF_PLATFORM_POST_BUILD_INSTALL_RESOURCES(${BF_EXE_NAME})

BF_PLATFORM_INSTALL(${BF_EXE_NAME})

BF_PLATFORM_CREATE_PACKAGES(${BF_EXE_NAME})
