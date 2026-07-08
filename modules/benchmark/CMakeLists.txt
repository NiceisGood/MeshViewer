# Module: benchmark (cross-module validation + performance)
# Links against both delaunay2d and delaunay3d

add_executable(benchmark benchmark.cpp)
target_link_libraries(benchmark PRIVATE delaunay2d delaunay3d)
