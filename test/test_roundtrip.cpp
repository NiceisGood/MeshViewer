// Round-trip test: write quadtree mesh to .qmesh, then read it back
#include "MeshUtils.h"
#include "Quadtree.h"
#include "MeshReader.h"
#include <cstdio>

int main()
{
    // 1. Generate a quadtree mesh
    Quadtree qt(0.0, 0.0, 2.0, 2.0, 10);
    qt.build([](double, double, double, int depth) {
        return depth < 4;  // uniform depth 4
    });

    std::vector<QPoint2D> pts;
    auto tris = qt.triangulate(pts);

    std::printf("Original quadtree mesh: %zu points, %zu triangles\n",
                pts.size(), tris.size());

    // 2. Write to .qmesh
    bool ok = write_qmesh(pts, tris, "data/test_quadtree.qmesh");
    std::printf("write_qmesh: %s\n", ok ? "OK" : "FAIL");
    if (!ok) return 1;

    // 3. Read back
    MeshData mesh;
    ok = read_qmesh("data/test_quadtree.qmesh", mesh);
    std::printf("read_qmesh:  %s\n", ok ? "OK" : "FAIL");
    if (!ok) return 1;

    // 4. Verify
    int nv = mesh.num_vertices();
    int nt = mesh.num_triangles();
    std::printf("Read back:     %d points, %d triangles\n", nv, nt);

    bool match = (nv == static_cast<int>(pts.size()) &&
                  nt == static_cast<int>(tris.size()));
    std::printf("Round-trip:    %s\n", match ? "PASS" : "FAIL");

    // Verify a few vertex coordinates
    bool coords_ok = true;
    for (int i = 0; i < nv && i < 10; ++i) {
        float dx = mesh.vertices[i * 3] - static_cast<float>(pts[i].x);
        float dy = mesh.vertices[i * 3 + 1] - static_cast<float>(pts[i].y);
        float dz = mesh.vertices[i * 3 + 2];  // should be 0
        if (std::abs(dx) > 1e-6f || std::abs(dy) > 1e-6f || std::abs(dz) > 1e-6f) {
            coords_ok = false;
            std::printf("  Mismatch at vertex %d: orig=(%.6f,%.6f) got=(%.6f,%.6f,%.6f)\n",
                        i, pts[i].x, pts[i].y,
                        mesh.vertices[i*3], mesh.vertices[i*3+1], mesh.vertices[i*3+2]);
        }
    }
    std::printf("Vertex coords: %s\n", coords_ok ? "PASS" : "FAIL");

    // Verify a few triangle indices
    bool indices_ok = true;
    for (size_t i = 0; i < tris.size() && i < 10; ++i) {
        if (mesh.indices[i*3]   != static_cast<unsigned>(tris[i].v0) ||
            mesh.indices[i*3+1] != static_cast<unsigned>(tris[i].v1) ||
            mesh.indices[i*3+2] != static_cast<unsigned>(tris[i].v2)) {
            indices_ok = false;
            std::printf("  Mismatch at tri %zu\n", i);
        }
    }
    std::printf("Tri indices:   %s\n", indices_ok ? "PASS" : "FAIL");

    std::printf("\nOverall: %s\n", (match && coords_ok && indices_ok) ? "ALL PASS" : "FAILED");
    return (match && coords_ok && indices_ok) ? 0 : 1;
}
