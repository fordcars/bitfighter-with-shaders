add_library(poly2tri STATIC
	${CMAKE_CURRENT_SOURCE_DIR}/common/shapes.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/sweep/advancing_front.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/sweep/cdt.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/sweep/sweep.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/sweep/sweep_context.cpp
)
