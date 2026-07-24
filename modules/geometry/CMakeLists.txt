# Module: geometry
# Discrete geometry model I/O and data representation.
# Provides Geometry class for STL/OBJ import/export and cross-module data exchange.

add_library(geometry STATIC
    src/Geometry.cpp
    include/Geometry.h
    src/PointCloud.cpp
    include/PointCloud.h
    src/PointCloudUtils.cpp
    include/PointCloudUtils.h
)

target_include_directories(geometry PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(geometry PRIVATE
    ${CMAKE_SOURCE_DIR}/modules/meshviewer/include  # for MeshData
)

target_compile_definitions(geometry PRIVATE _CRT_SECURE_NO_WARNINGS)
