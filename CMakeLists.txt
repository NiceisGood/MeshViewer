cmake_minimum_required(VERSION 3.14)
project(delaunay LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MSVC: force UTF-8 source encoding (✓/✗ etc.)
if(MSVC)
    add_compile_options(/utf-8)
endif()

# All binaries organised by build config under lib/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib/$<CONFIG>)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib/$<CONFIG>)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib/$<CONFIG>)
set(CMAKE_DEBUG_POSTFIX _d)

# ── Modules ────────────────────────────────────────────────────────────
add_subdirectory(modules/delaunay2d)
add_subdirectory(modules/delaunay3d)
add_subdirectory(modules/aft2d)
add_subdirectory(modules/mesher2d)
add_subdirectory(modules/benchmark)
add_subdirectory(modules/meshutils)
add_subdirectory(modules/quadtree)
add_subdirectory(modules/octree)
add_subdirectory(modules/geometry)
add_subdirectory(modules/delaunay2dshow)
add_subdirectory(modules/meshviewer)
add_subdirectory(test)
