# Module: aft2d
# Produces: aft2d.dll
# Depends on: delaunay2d (for Point2D / Triangle2D types)

add_library(aft2d SHARED src/AFT2D.cpp)
target_compile_definitions(aft2d PRIVATE AFT2D_EXPORTS)

target_include_directories(aft2d
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(aft2d PUBLIC delaunay2d)

# (test moved to top-level test/)
