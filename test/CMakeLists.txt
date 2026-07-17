# Test executables (centralised)
# Each links against its corresponding module library.

add_executable(test_delaunay2d test_delaunay2d.cpp)
target_link_libraries(test_delaunay2d PRIVATE delaunay2d)

add_executable(test_delaunay3d test_delaunay3d.cpp)
target_link_libraries(test_delaunay3d PRIVATE delaunay3d)

add_executable(test_mesher2d test_mesher2d.cpp)
target_link_libraries(test_mesher2d PRIVATE mesher2d)

add_executable(test_aft2d test_aft2d.cpp)
target_link_libraries(test_aft2d PRIVATE aft2d)

add_executable(test_meshutils test_meshutils.cpp)
target_link_libraries(test_meshutils PRIVATE aft2d meshutils)

add_executable(test_quadtree test_quadtree.cpp)
target_link_libraries(test_quadtree PRIVATE quadtree)
