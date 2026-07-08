# Module: meshutils
# Header-only utility library — no .cpp to compile.
add_library(meshutils INTERFACE)
target_include_directories(meshutils INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(meshutils INTERFACE delaunay2d)
