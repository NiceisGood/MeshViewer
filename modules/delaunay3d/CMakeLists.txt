# Module: delaunay3d
# Produces: delaunay3d.dll

add_library(delaunay3d SHARED src/Delaunay3D.cpp)
target_compile_definitions(delaunay3d PRIVATE DELAUNAY3D_EXPORTS)

target_include_directories(delaunay3d
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
