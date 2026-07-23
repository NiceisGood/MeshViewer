// Test: read STL vertices → Delaunay2D triangulation (XY projection)
// Self-contained: parses STL directly, no dependency on Geometry/MeshReader.

#include "Delaunay2D.h"

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <iostream>
#include <vector>
#include <string>

static bool read_stl_vertices(const std::string& path,
                              std::vector<double>& xs,
                              std::vector<double>& ys)
{
    // Try binary first
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;

    // Read 80-byte header
    char header[80];
    if (std::fread(header, 1, 80, f) != 80) { std::fclose(f); return false; }

    // Read triangle count
    uint32_t num_tris;
    if (std::fread(&num_tris, sizeof(num_tris), 1, f) != 1) {
        std::fclose(f); return false;
    }

    // Quick sanity check on triangle count
    if (num_tris > 0 && num_tris < 10000000) {
        // Binary STL: each triangle = 3 floats normal + 3*3 floats verts + 2 bytes attr = 50 bytes
        // Check file size to confirm binary
        std::fseek(f, 0, SEEK_END);
        long file_size = std::ftell(f);
        long expected = 80 + 4 + static_cast<long>(num_tris) * 50;
        if (file_size == expected) {
            // Binary STL confirmed
            std::fseek(f, 84, SEEK_SET);  // skip header + count
            for (uint32_t i = 0; i < num_tris; ++i) {
                float normal[3], verts[3][3];
                uint16_t attr;
                if (std::fread(normal, sizeof(float), 3, f) != 3) break;
                if (std::fread(verts[0], sizeof(float), 3, f) != 3) break;
                if (std::fread(verts[1], sizeof(float), 3, f) != 3) break;
                if (std::fread(verts[2], sizeof(float), 3, f) != 3) break;
                if (std::fread(&attr, sizeof(attr), 1, f) != 1) break;
                xs.push_back(verts[0][0]); ys.push_back(verts[0][1]);
                xs.push_back(verts[1][0]); ys.push_back(verts[1][1]);
                xs.push_back(verts[2][0]); ys.push_back(verts[2][1]);
            }
            std::fclose(f);
            return !xs.empty();
        }
    }

    // Fallback: ASCII STL
    std::freopen(path.c_str(), "r", f);
    char line[1024];
    while (std::fgets(line, sizeof(line), f)) {
        char* s = line;
        while (*s == ' ' || *s == '\t') ++s;
        if (std::strncmp(s, "vertex", 6) == 0) {
            float x, y, z;
            if (std::sscanf(s, "vertex %f %f %f", &x, &y, &z) == 3) {
                xs.push_back(x);
                ys.push_back(y);
            }
        }
    }
    std::fclose(f);
    return !xs.empty();
}

int main(int argc, char* argv[])
{
    const char* path = (argc > 1) ? argv[1] : "data/sphere_spider.stl";

    // Read vertices from STL (XY projection)
    std::vector<double> xs, ys;
    if (!read_stl_vertices(path, xs, ys)) {
        std::cerr << "FAIL: could not read " << path << "\n";
        return 1;
    }

    // Deduplicate unique XY points
    // (STL stores vertices per-triangle, so many duplicates)
    struct DupPoint { double x, y; };
    std::vector<DupPoint> unique;
    for (size_t i = 0; i < xs.size(); ++i) {
        bool found = false;
        for (const auto& u : unique) {
            if (std::abs(u.x - xs[i]) < 1e-10 && std::abs(u.y - ys[i]) < 1e-10) {
                found = true;
                break;
            }
        }
        if (!found)
            unique.push_back({xs[i], ys[i]});
    }

    std::cout << "STL file: " << path << "\n";
    std::cout << "  Raw vertices (with duplicates): " << xs.size() << "\n";
    std::cout << "  Unique XY points:               " << unique.size() << "\n\n";

    if (unique.size() < 3) {
        std::cerr << "FAIL: need at least 3 unique points\n";
        return 1;
    }

    // Convert to Point2D (same as onDelaunay2D does)
    std::vector<Point2D> points;
    points.reserve(unique.size());
    for (const auto& p : unique)
        points.push_back({p.x, p.y});

    // Run Delaunay2D (same strategy as onDelaunay2D)
    Delaunay2D delaunay(std::move(points), Delaunay2D::Strategy::Optimized);
    auto triangles = delaunay.triangulate();

    if (triangles.empty()) {
        std::cerr << "FAIL: triangulation produced 0 triangles\n";
        return 1;
    }

    // Validate indices
    int np = static_cast<int>(delaunay.points().size());
    int errors = 0;
    for (size_t i = 0; i < triangles.size(); ++i) {
        const auto& t = triangles[i];
        if (t.v0 < 0 || t.v0 >= np || t.v1 < 0 || t.v1 >= np || t.v2 < 0 || t.v2 >= np) {
            std::cerr << "  ERROR: triangle " << i << " has out-of-range index\n";
            ++errors;
        }
    }

    std::cout << "Delaunay2D: " << delaunay.points().size()
              << " unique points → " << triangles.size() << " triangles\n";

    if (errors == 0) {
        std::cout << "PASS: all indices valid\n";
        return 0;
    } else {
        std::cerr << "FAIL: " << errors << " errors\n";
        return 1;
    }
}
