// Octree from STL surface test
// Reads an ASCII STL file, computes bounding box, builds an adaptive octree,
// tetrahedralizes, and exports to multiple formats.
#include "Octree.h"
#include "MeshUtils.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>
#include <cstring>

// -----------------------------------------------------------------------
// Simple ASCII STL reader (standalone, no external deps)
// -----------------------------------------------------------------------
struct STLFace { float v[3][3]; };  // [vertex][x,y,z]

static bool read_stl_ascii(const std::string& path, std::vector<STLFace>& faces)
{
    FILE* f = std::fopen(path.c_str(), "r");
    if (!f) { std::printf("  ERROR: cannot open %s\n", path.c_str()); return false; }

    char line[256];
    float verts[3][3];
    int vi = 0;

    while (std::fgets(line, sizeof(line), f)) {
        // Trim leading whitespace
        char* s = line;
        while (*s == ' ' || *s == '\t') ++s;

        if (std::strncmp(s, "vertex", 6) == 0) {
            float x, y, z;
            if (std::sscanf(s + 6, "%f %f %f", &x, &y, &z) == 3) {
                verts[vi][0] = x; verts[vi][1] = y; verts[vi][2] = z;
                ++vi;
                if (vi == 3) {
                    STLFace face;
                    for (int i = 0; i < 3; ++i)
                        for (int j = 0; j < 3; ++j)
                            face.v[i][j] = verts[i][j];
                    faces.push_back(face);
                    vi = 0;
                }
            }
        }
    }

    std::fclose(f);
    return true;
}

// -----------------------------------------------------------------------
// Convert OctPoint3D/OctTetrahedron → Point3D/Tetrahedron for the
// non-template export functions (write_obj_3d, write_stl_*, write_vtk_3d).
// -----------------------------------------------------------------------
static void convert_mesh(const std::vector<OctPoint3D>& src_pts,
                         const std::vector<OctTetrahedron>& src_tets,
                         std::vector<Point3D>& dst_pts,
                         std::vector<Tetrahedron>& dst_tets)
{
    dst_pts.clear(); dst_pts.reserve(src_pts.size());
    for (const auto& p : src_pts)
        dst_pts.push_back({p.x, p.y, p.z});

    dst_tets.clear(); dst_tets.reserve(src_tets.size());
    for (const auto& t : src_tets)
        dst_tets.push_back({t.v0, t.v1, t.v2, t.v3});
}

// -----------------------------------------------------------------------
int main()
{
    std::printf("=== Octree from STL ===\n\n");

    // 1. Read STL surface mesh
    std::vector<STLFace> faces;
    std::string stl_path = "data/sphere_spider.stl";

    if (!read_stl_ascii(stl_path, faces)) {
        std::printf("FAILED to read %s\n", stl_path.c_str());
        return 1;
    }
    std::printf("Read %s: %zu triangles\n", stl_path.c_str(), faces.size());

    // 2. Compute bounding box from all triangle vertices
    float xmin = 1e30f, ymin = 1e30f, zmin = 1e30f;
    float xmax = -1e30f, ymax = -1e30f, zmax = -1e30f;

    for (const auto& f : faces) {
        for (int i = 0; i < 3; ++i) {
            if (f.v[i][0] < xmin) xmin = f.v[i][0];
            if (f.v[i][0] > xmax) xmax = f.v[i][0];
            if (f.v[i][1] < ymin) ymin = f.v[i][1];
            if (f.v[i][1] > ymax) ymax = f.v[i][1];
            if (f.v[i][2] < zmin) zmin = f.v[i][2];
            if (f.v[i][2] > zmax) zmax = f.v[i][2];
        }
    }

    // Add 10% padding
    float dx = xmax - xmin, dy = ymax - ymin, dz = zmax - zmin;
    float pad = std::max({dx, dy, dz}) * 0.1f;
    xmin -= pad; ymin -= pad; zmin -= pad;
    xmax += pad; ymax += pad; zmax += pad;

    std::printf("Bounding box: [%.3f, %.3f] x [%.3f, %.3f] x [%.3f, %.3f]\n",
                xmin, xmax, ymin, ymax, zmin, zmax);
    std::printf("Domain size:  %.3f x %.3f x %.3f\n", dx + 2*pad, dy + 2*pad, dz + 2*pad);

    // 3. Build adaptive octree
    //    Subdivision criterion: subdivide a cell if the STL surface is within
    //    1.5× cell width of the cell centre (surface proximity refinement).
    Octree oct(xmin, ymin, zmin, xmax, ymax, zmax, 6);

    oct.build([&](double cx, double cy, double cz, double half_size, int depth) -> bool {
        if (depth >= 5) return false;   // hard limit

        double cell_size = half_size * 2.0;
        double threshold = cell_size * 1.5;

        // Check min distance from cell centre to any STL vertex
        double min_dist2 = 1e30;
        for (const auto& f : faces) {
            for (int i = 0; i < 3; ++i) {
                double d2 = (cx - f.v[i][0]) * (cx - f.v[i][0]) +
                            (cy - f.v[i][1]) * (cy - f.v[i][1]) +
                            (cz - f.v[i][2]) * (cz - f.v[i][2]);
                if (d2 < min_dist2) min_dist2 = d2;
            }
        }

        return std::sqrt(min_dist2) < threshold;
    });

    std::printf("Octree built: %d nodes, %d leaves, max depth %d\n",
                oct.num_nodes(), oct.num_leaves(), oct.tree_depth());

    // 4. Tetrahedralize
    std::vector<OctPoint3D> pts;
    std::vector<OctTetrahedron> tets;
    tets = oct.tetrahedralize(pts);

    std::printf("Tetrahedral mesh: %zu points, %zu tetrahedra\n",
                pts.size(), tets.size());

    if (tets.empty()) {
        std::printf("ERROR: tetrahedralization produced no tetrahedra.\n");
        return 1;
    }

    // 5. Convert for non-template export functions
    std::vector<Point3D> pts3d;
    std::vector<Tetrahedron> tets3d;
    convert_mesh(pts, tets, pts3d, tets3d);

    // 6. Export to all formats
    bool ok;

    // VTK — full tetrahedral mesh
    ok = write_vtk_3d(pts3d, tets3d, "data/octree_sphere_spider.vtk");
    std::printf("VTK export:         %s\n", ok ? "OK" : "FAIL");

    // OBJ — surface only (extracted from tetrahedra)
    ok = write_obj_3d(pts3d, tets3d, "data/octree_sphere_spider.obj");
    std::printf("OBJ surface export: %s\n", ok ? "OK" : "FAIL");

    // STL ASCII — surface only
    ok = write_stl_ascii_3d(pts3d, tets3d, "data/octree_sphere_spider.stl");
    std::printf("STL ASCII export:   %s\n", ok ? "OK" : "FAIL");

    // STL binary — surface only
    ok = write_stl_binary_3d(pts3d, tets3d, "data/octree_sphere_spider_binary.stl");
    std::printf("STL binary export:  %s\n", ok ? "OK" : "FAIL");

    // QMesh3D — full tetrahedral mesh (template function, works with OctPoint3D directly)
    ok = write_qmesh_3d(pts, tets, "data/octree_sphere_spider.qmesh3d");
    std::printf("QMesh3D export:     %s\n", ok ? "OK" : "FAIL");

    std::printf("\n=== Done ===\n");
    return 0;
}
