# Module: delaunay2dshow
# Interactive 2D Delaunay triangulation demonstration with live
# incremental updates, circumcircle and Voronoi overlay.
#
# Can be built as a standalone executable or linked as a library.
# Dependencies: Qt5::Widgets, geometry (Point2D), delaunay2d (Delaunay2D)

set(QT_PATH "/d/Qt/Qt5.14.2/5.14.2/msvc2017_64")
set(CMAKE_PREFIX_PATH "${QT_PATH}")
set(CMAKE_AUTOMOC ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets)

# ── Library target (for meshviewer integration) ──
add_library(delaunay2dshow STATIC
    src/DelaunayCanvas.cpp
    src/Delaunay2DShow.cpp
    include/DelaunayCanvas.h
    include/Delaunay2DShow.h
)

target_include_directories(delaunay2dshow PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/modules/delaunay2d/include
    ${CMAKE_SOURCE_DIR}/modules/geometry/include
)

target_link_libraries(delaunay2dshow PUBLIC
    Qt5::Widgets
    delaunay2d
    geometry
)

target_compile_definitions(delaunay2dshow PRIVATE _CRT_SECURE_NO_WARNINGS)

# ── Standalone executable target (for independent use) ──
add_executable(delaunay2dshow_standalone
    src/main.cpp
)
target_link_libraries(delaunay2dshow_standalone PRIVATE delaunay2dshow)
target_compile_definitions(delaunay2dshow_standalone PRIVATE DELAUNAY2DSHOW_STANDALONE)

set_target_properties(delaunay2dshow_standalone PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib/$<CONFIG>"
)
