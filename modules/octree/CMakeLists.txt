# Module: octree
# Produces: octree.dll

add_library(octree SHARED src/Octree.cpp)
target_compile_definitions(octree PRIVATE OCTREE_EXPORTS)

target_include_directories(octree
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
