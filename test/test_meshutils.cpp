#include "MeshUtils.h"
#include "Delaunay2D.h"
#include "AFT2D.h"
#include <iostream>
#include <cstdlib>
#include <cmath>

// =======================================================================
//  Test: Mesh utility functions — statistics and OBJ export
// =======================================================================

static const double PI = 3.14159265358979323846;

static std::vector<Point2D> random_points(int n, unsigned seed = 42) {
    std::srand(seed);
    std::vector<Point2D> pts(n);
    for (auto& p : pts) { p.x = std::rand() / double(RAND_MAX); p.y = std::rand() / double(RAND_MAX); }
    return pts;
}

static std::vector<Point2D> rect_boundary(double x0, double y0,
                                           double x1, double y1) {
    return {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
}

int main() {
    std::cout << "=== Mesh Utility Tests ===\n\n";

    // ---- Test 1: Delaunay statistics ----
    {
        std::cout << "[Test 1] Delaunay2D — 200 random points\n";
        auto pts = random_points(200, 42);
        Delaunay2D dt(pts, Delaunay2D::Strategy::Optimized);
        auto tris = dt.triangulate();
        auto stats = compute_stats(dt.points(), tris);
        print_stats(stats);
    }

    // ---- Test 2: AFT square with quality improvement ----
    {
        std::cout << "\n[Test 2] AdvancingFront2D — square 2×2, target=0.02\n";
        auto boundary = rect_boundary(0.0, 0.0, 2.0, 2.0);
        AdvancingFront2D::Config cfg;
        cfg.target_area   = 0.02;
        cfg.min_angle_deg = 20.0;
        cfg.max_iter      = 50000;

        AdvancingFront2D mesh(boundary, cfg);
        mesh.generate();

        auto pts = mesh.points();
        auto tris = mesh.triangles();

        std::cout << "  Before improvement:\n";
        auto stats_before = compute_stats(pts, tris);
        print_stats(stats_before);

        int nflip = improve_quality(pts, tris);
        std::cout << "  Flips: " << nflip << "\n";

        std::cout << "  After improvement:\n";
        auto stats_after = compute_stats(pts, tris);
        print_stats(stats_after);

        // Export coloured OBJ (after improvement)
        std::string obj_path = "data/test_aft2d_square.obj";
        if (write_obj_coloured(pts, tris, obj_path))
            std::cout << "  → Exported " << obj_path << "\n";
    }

    // ---- Test 3: AFT L-shape with quality improvement ----
    {
        std::cout << "\n[Test 3] AFT L-shape, target=0.04\n";
        std::vector<Point2D> boundary = {
            {0.0, 0.0}, {2.0, 0.0}, {2.0, 0.5},
            {0.5, 0.5}, {0.5, 2.0}, {0.0, 2.0}
        };
        AdvancingFront2D::Config cfg;
        cfg.target_area   = 0.04;
        cfg.min_angle_deg = 20.0;
        cfg.max_iter      = 50000;

        AdvancingFront2D mesh(boundary, cfg);
        mesh.generate();

        auto pts = mesh.points();
        auto tris = mesh.triangles();

        std::cout << "  Before:\n";
        print_stats(compute_stats(pts, tris));

        int nflip = improve_quality(pts, tris);
        std::cout << "  Flips: " << nflip << "\n";

        std::cout << "  After:\n";
        print_stats(compute_stats(pts, tris));

        std::string obj_path = "data/test_aft2d_lshape.obj";
        if (write_obj(pts, tris, obj_path))
            std::cout << "  → Exported " << obj_path << "\n";
    }

    std::cout << "\n=== Done ===\n";
    return 0;
}
