# Module: quadtree
# Produces: quadtree.dll

add_library(quadtree SHARED src/Quadtree.cpp)
target_compile_definitions(quadtree PRIVATE QUADTREE_EXPORTS)

target_include_directories(quadtree
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
