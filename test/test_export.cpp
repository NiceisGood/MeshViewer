// Quick test for the new export functions
#include "MeshUtils.h"
#include "Delaunay2D.h"
#include "Delaunay3D.h"
#include <cstdio>

int main()
{
    // 1. Test 2D exports (OBJ already tested, now STL + VTK)
    {
        std::vector<Point2D> pts = {{0,0}, {1,0}, {0,1}, {1,1}};
        std::vector<Triangle2D> tris = {{0,1,2}, {1,3,2}};

        bool ok;
        ok = write_stl_ascii(pts, tris, "data/test_2d_ascii.stl");
        std::printf("write_stl_ascii: %s\n", ok ? "OK" : "FAIL");
        ok = write_stl_binary(pts, tris, "data/test_2d_binary.stl");
        std::printf("write_stl_binary: %s\n", ok ? "OK" : "FAIL");
        ok = write_vtk_2d(pts, tris, "data/test_2d.vtk");
        std::printf("write_vtk_2d:     %s\n", ok ? "OK" : "FAIL");
    }

    // 2. Test 3D exports
    {
        std::vector<Point3D> pts = {
            {0,0,0}, {1,0,0}, {0,1,0}, {0,0,1},  // one tetra
            {1,1,0}, {1,0,1}, {0,1,1}             // more points
        };
        std::vector<Tetrahedron> tets = {
            {0,1,2,3},   // first tetra
            {1,4,5,3},   // second (shares face 1,3 with first? no)
            {2,3,6,5}    // third
        };

        bool ok;
        ok = write_obj_3d(pts, tets, "data/test_3d.obj");
        std::printf("write_obj_3d:       %s\n", ok ? "OK" : "FAIL");
        ok = write_stl_ascii_3d(pts, tets, "data/test_3d_ascii.stl");
        std::printf("write_stl_ascii_3d: %s\n", ok ? "OK" : "FAIL");
        ok = write_stl_binary_3d(pts, tets, "data/test_3d_binary.stl");
        std::printf("write_stl_binary_3d:%s\n", ok ? "OK" : "FAIL");
        ok = write_vtk_3d(pts, tets, "data/test_3d.vtk");
        std::printf("write_vtk_3d:       %s\n", ok ? "OK" : "FAIL");
    }

    std::printf("\nAll export tests done.\n");
    return 0;
}
