# Module: delaunay2d
# Produces: delaunay2d.dll

add_library(delaunay2d SHARED src/Delaunay2D.cpp)
target_compile_definitions(delaunay2d PRIVATE DELAUNAY2D_EXPORTS)

target_include_directories(delaunay2d
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
