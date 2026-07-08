# Module: mesher2d
# Produces: mesher2d.dll, test_mesher2d.exe
# Depends on: delaunay2d

add_library(mesher2d SHARED src/Mesher2D.cpp)
target_compile_definitions(mesher2d PRIVATE MESHER2D_EXPORTS)
target_include_directories(mesher2d
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(mesher2d PUBLIC delaunay2d)

# (test moved to top-level test/)
