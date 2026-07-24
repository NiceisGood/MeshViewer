# Module: meshviewer
# Qt5 + OpenGL mesh viewer application
# Requires: Qt5 (Widgets + OpenGL modules)

set(QT_PATH "/d/Qt/Qt5.14.2/5.14.2/msvc2017_64")
set(CMAKE_PREFIX_PATH "${QT_PATH}")
set(CMAKE_AUTOMOC ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets OpenGL)

add_executable(meshviewer
    src/main.cpp
    src/MeshViewer.cpp
    src/MeshRenderer.cpp
    src/MeshReader.cpp
    src/PointCloudDialog.cpp
    include/MeshViewer.h
    include/MeshRenderer.h
    include/MeshReader.h
    include/PointCloudDialog.h
)

target_include_directories(meshviewer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/modules/delaunay2d/include
    ${CMAKE_SOURCE_DIR}/modules/delaunay3d/include
    ${CMAKE_SOURCE_DIR}/modules/meshutils/include
    ${CMAKE_SOURCE_DIR}/modules/quadtree/include
    ${CMAKE_SOURCE_DIR}/modules/octree/include
    ${CMAKE_SOURCE_DIR}/modules/geometry/include
)

target_link_libraries(meshviewer PRIVATE
    Qt5::Widgets
    Qt5::OpenGL
    octree
    geometry
    delaunay2d
    delaunay3d
    delaunay2dshow
)

# Place the executable alongside other test binaries
set_target_properties(meshviewer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib/$<CONFIG>"
)
