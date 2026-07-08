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

    // ---- Test 2: AFT statistics ----
    {
        std::cout << "\n[Test 2] AdvancingFront2D — square 2×2, target=0.02\n";
        auto boundary = rect_boundary(0.0, 0.0, 2.0, 2.0);
        AdvancingFront2D::Config cfg;
        cfg.target_area   = 0.02;
        cfg.min_angle_deg = 20.0;
        cfg.max_iter      = 50000;

        AdvancingFront2D mesh(boundary, cfg);
        mesh.generate();

        auto stats = compute_stats(mesh.points(), mesh.triangles());
        print_stats(stats);

        // Export coloured OBJ
        std::string obj_path = "test_aft2d_square.obj";
        if (write_obj_coloured(mesh.points(), mesh.triangles(), obj_path))
            std::cout << "  → Exported " << obj_path << "\n";
    }

    // ---- Test 3: OBJ export — L-shape AFT ----
    {
        std::cout << "\n[Test 3] AFT L-shape OBJ export\n";
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

        auto stats = compute_stats(mesh.points(), mesh.triangles());
        print_stats(stats);

        std::string obj_path = "test_aft2d_lshape.obj";
        if (write_obj(mesh.points(), mesh.triangles(), obj_path))
            std::cout << "  → Exported " << obj_path << "\n";
    }

    std::cout << "\n=== Done ===\n";
    return 0;
}
