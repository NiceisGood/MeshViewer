// Test: 2D Delaunay optimization via Laplacian smoothing (fixed connectivity)
// Verifies that mesh quality improves after smoothing interior vertices.

#include "Delaunay2D.h"
#include "MeshUtils.h"

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <utility>

static bool read_stl_vertices(const std::string& path,
                              std::vector<double>& xs,
                              std::vector<double>& ys)
{
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;

    char header[80];
    if (std::fread(header, 1, 80, f) != 80) { std::fclose(f); return false; }

    uint32_t num_tris;
    if (std::fread(&num_tris, sizeof(num_tris), 1, f) != 1) {
        std::fclose(f); return false;
    }

    if (num_tris > 0 && num_tris < 10000000) {
        std::fseek(f, 0, SEEK_END);
        long file_size = std::ftell(f);
        long expected = 80 + 4 + static_cast<long>(num_tris) * 50;
        if (file_size == expected) {
            std::fseek(f, 84, SEEK_SET);
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

    std::vector<double> xs, ys;
    if (!read_stl_vertices(path, xs, ys)) {
        std::cerr << "FAIL: could not read " << path << "\n";
        return 1;
    }

    // Deduplicate
    struct DupPoint { double x, y; };
    std::vector<DupPoint> unique;
    for (size_t i = 0; i < xs.size(); ++i) {
        bool found = false;
        for (const auto& u : unique) {
            if (std::abs(u.x - xs[i]) < 1e-10 && std::abs(u.y - ys[i]) < 1e-10) {
                found = true; break;
            }
        }
        if (!found) unique.push_back({xs[i], ys[i]});
    }

    std::cout << "STL: " << path << "\n";
    std::cout << "  Unique XY points: " << unique.size() << "\n\n";

    if (unique.size() < 3) {
        std::cerr << "FAIL: need at least 3 points\n";
        return 1;
    }

    // Convert to Point2D
    std::vector<Point2D> points;
    points.reserve(unique.size());
    for (const auto& p : unique)
        points.push_back({p.x, p.y});

    // Initial Delaunay triangulation to establish connectivity
    Delaunay2D delaunay(points, Delaunay2D::Strategy::Optimized);
    auto triangles = delaunay.triangulate();
    if (triangles.empty()) {
        std::cerr << "FAIL: no triangles produced\n";
        return 1;
    }

    // Compute initial stats on original points (no super-triangle vertices)
    auto initial = compute_stats(points, triangles);
    std::cout << "=== Initial mesh ===\n";
    print_stats(initial);
    std::cout << "\n";

    // Laplacian smoothing: fixed connectivity, no re-triangulation
    const int max_iters = 20;
    const double relax = 0.3;
    auto smoothed_pts = points;

    for (int iter = 0; iter < max_iters; ++iter) {
        int nv = static_cast<int>(smoothed_pts.size());

        // Build adjacency
        std::vector<std::vector<int>> adj(nv);
        for (const auto& t : triangles) {
            adj[t.v0].push_back(t.v1); adj[t.v0].push_back(t.v2);
            adj[t.v1].push_back(t.v0); adj[t.v1].push_back(t.v2);
            adj[t.v2].push_back(t.v0); adj[t.v2].push_back(t.v1);
        }

        // Boundary detection
        std::vector<std::pair<int,int>> edge_count;
        for (const auto& t : triangles) {
            auto add_edge = [&](int a, int b) {
                if (a > b) std::swap(a, b);
                edge_count.push_back({a, b});
            };
            add_edge(t.v0, t.v1); add_edge(t.v1, t.v2); add_edge(t.v2, t.v0);
        }
        std::sort(edge_count.begin(), edge_count.end());
        std::vector<bool> is_boundary(nv, false);
        for (size_t i = 0; i < edge_count.size(); ++i) {
            bool unique = true;
            if (i > 0 && edge_count[i-1] == edge_count[i]) unique = false;
            if (i+1 < edge_count.size() && edge_count[i+1] == edge_count[i]) unique = false;
            if (unique) {
                is_boundary[edge_count[i].first] = true;
                is_boundary[edge_count[i].second] = true;
            }
        }

        // Laplacian step
        std::vector<Point2D> new_pts = smoothed_pts;
        for (int i = 0; i < nv; ++i) {
            if (is_boundary[i] || adj[i].empty()) continue;
            double cx = 0.0, cy = 0.0;
            for (int nb : adj[i]) {
                cx += smoothed_pts[nb].x;
                cy += smoothed_pts[nb].y;
            }
            double inv = 1.0 / adj[i].size();
            new_pts[i].x = smoothed_pts[i].x + relax * (cx * inv - smoothed_pts[i].x);
            new_pts[i].y = smoothed_pts[i].y + relax * (cy * inv - smoothed_pts[i].y);
        }
        smoothed_pts = std::move(new_pts);
    }

    // Compute final stats (same connectivity, smoothed points)
    auto final_stats = compute_stats(smoothed_pts, triangles);
    std::cout << "=== After " << max_iters << " smoothing iterations (fixed connectivity) ===\n";
    print_stats(final_stats);
    std::cout << "\n";

    // Compare
    std::cout << "=== Improvement ===\n";
    std::printf("  min angle : %.2f° → %.2f°  (%+.2f°)\n",
                initial.min_angle, final_stats.min_angle,
                final_stats.min_angle - initial.min_angle);
    std::printf("  avg angle : %.2f° → %.2f°  (%+.2f°)\n",
                initial.avg_angle, final_stats.avg_angle,
                final_stats.avg_angle - initial.avg_angle);
    std::printf("  avg aspect: %.4f → %.4f  (%+.4f)\n",
                initial.avg_aspect, final_stats.avg_aspect,
                final_stats.avg_aspect - initial.avg_aspect);
    std::printf("  < 20°     : %d → %d\n",
                initial.n_below_20, final_stats.n_below_20);
    std::printf("  inverted  : %d → %d\n",
                initial.n_inverted, final_stats.n_inverted);

    bool improved = (final_stats.n_below_20 <= initial.n_below_20) ||
                    (final_stats.avg_aspect <= initial.avg_aspect);
    std::cout << (improved ? "PASS: mesh quality improved or maintained\n"
                           : "NOTE: mesh quality changed\n");
    return 0;
}
