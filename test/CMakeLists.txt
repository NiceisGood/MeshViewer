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

add_executable(test_octree test_octree.cpp)
target_link_libraries(test_octree PRIVATE octree)

# Standalone test executables (not built by default — require manual cmake rebuild)
add_executable(test_export test_export.cpp)
target_link_libraries(test_export PRIVATE meshutils delaunay2d delaunay3d)

add_executable(test_roundtrip test_roundtrip.cpp)
target_link_libraries(test_roundtrip PRIVATE meshutils quadtree)

add_executable(test_octree_diagnostic test_octree_diagnostic.cpp)
target_link_libraries(test_octree_diagnostic PRIVATE octree meshutils)

add_executable(test_octree_from_stl test_octree_from_stl.cpp)
target_link_libraries(test_octree_from_stl PRIVATE octree meshutils)

# Qt5 minimal smoke test (verifies MSVC Qt5 toolchain works)
find_package(Qt5 COMPONENTS Widgets QUIET)
if(Qt5_FOUND)
  add_executable(test_qt_minimal test_qt_minimal.cpp)
  target_link_libraries(test_qt_minimal PRIVATE Qt5::Widgets)

  find_package(Qt5 COMPONENTS OpenGL QUIET)
  if(Qt5OpenGL_FOUND)
    add_executable(test_qt_gl_minimal test_qt_gl_minimal.cpp)
    target_link_libraries(test_qt_gl_minimal PRIVATE Qt5::Widgets Qt5::OpenGL)
  endif()
endif()
