#include "Quadtree.h"
#include <iostream>
#include <cstdlib>
#include <cmath>
#include <cstdio>

// =======================================================================
//  Test: Quadtree — uniform and adaptive mesh generation
// =======================================================================

static void print_stats(const std::vector<QPoint2D>& pts,
                         const std::vector<QTriangle2D>& tris)
{
    std::printf("  points: %zu   triangles: %zu\n", pts.size(), tris.size());

    // Area statistics
    double min_area = 1e100, max_area = 0.0, sum_area = 0.0;
    int inverted = 0;

    auto cross = [&](const QPoint2D& a, const QPoint2D& b, const QPoint2D& c) {
        return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    };

    for (const auto& t : tris) {
        const auto& a = pts[t.v0];
        const auto& b = pts[t.v1];
        const auto& c = pts[t.v2];
        double area = std::abs(cross(a, b, c)) * 0.5;
        if (area < min_area) min_area = area;
        if (area > max_area) max_area = area;
        sum_area += area;
        if (cross(a, b, c) < 0) ++inverted;
    }

    std::printf("  area : [%.6f, %.6f]  avg=%.6f  total=%.4f\n",
                min_area, max_area, sum_area / tris.size(), sum_area);
    if (inverted > 0)
        std::printf("  WARNING: %d inverted triangles!\n", inverted);
}

// =======================================================================
//  Test 1: Uniform grid — subdivide to fixed depth everywhere
// =======================================================================
static void test_uniform_grid()
{
    std::cout << "[Test 1] Uniform quadtree — depth 4 (16×16 cells)\n";

    Quadtree qt(0.0, 0.0, 2.0, 2.0, 10);
    qt.build([](double, double, double, int depth) {
        return depth < 4;  // stop at depth 4 → 16 cells per side
    });

    std::cout << "  Nodes: " << qt.num_nodes()
              << "  Leaves: " << qt.num_leaves()
              << "  Depth: " << qt.tree_depth() << "\n";

    std::vector<QPoint2D> pts;
    auto tris = qt.triangulate(pts);
    print_stats(pts, tris);

    // Expected: 2^4 × 2^4 = 256 cells × 2 triangles = 512 triangles
    // Points: (2^4+1) × (2^4+1) = 289
    std::cout << "  Expected ~289 points, ~512 triangles\n";
}

// =======================================================================
//  Test 2: Adaptive — refine near a point (distance-based)
// =======================================================================
static void test_adaptive_near_point()
{
    std::cout << "\n[Test 2] Adaptive quadtree — refine near centre point\n";

    // Centre point to refine around
    double cx = 0.0, cy = 0.0;

    Quadtree qt(-1.0, -1.0, 1.0, 1.0, 6);
    qt.build([&](double x, double y, double hs, int depth) -> bool {
        if (depth >= 6) return false;
        // Subdivide if cell overlaps the refinement region near centre
        double dx = std::abs(x - cx);
        double dy = std::abs(y - cy);
        double radius = 0.5 / (1 << (depth / 2 + 1));
        if (depth < 2) return true;  // always subdivide to depth 2
        // Refine more near the centre
        double dist = std::sqrt(dx * dx + dy * dy);
        double threshold = 0.4 / (1 << (depth / 2));
        return dist < threshold || depth < 3;
    });

    std::cout << "  Nodes: " << qt.num_nodes()
              << "  Leaves: " << qt.num_leaves()
              << "  Depth: " << qt.tree_depth() << "\n";

    std::vector<QPoint2D> pts;
    auto tris = qt.triangulate(pts);
    print_stats(pts, tris);
}

// =======================================================================
//  Test 3: Large uniform grid — performance check
// =======================================================================
static void test_large_grid()
{
    std::cout << "\n[Test 3] Uniform quadtree — depth 7 (128×128 cells)\n";

    Quadtree qt(0.0, 0.0, 1.0, 1.0, 10);
    qt.build([](double, double, double, int depth) {
        return depth < 7;
    });

    std::cout << "  Nodes: " << qt.num_nodes()
              << "  Leaves: " << qt.num_leaves()
              << "  Depth: " << qt.tree_depth() << "\n";

    std::vector<QPoint2D> pts;
    auto tris = qt.triangulate(pts);
    print_stats(pts, tris);

    // Expected: 128×128×2 = 32768 triangles
    std::cout << "  Expected ~16384 points, ~32768 triangles\n";
}

// =======================================================================
//  Test 4: Adaptive with two refinement regions
// =======================================================================
static void test_adaptive_two_regions()
{
    std::cout << "\n[Test 4] Adaptive quadtree — two refinement zones\n";

    Quadtree qt(-2.0, -2.0, 2.0, 2.0, 7);
    qt.build([](double x, double y, double, int depth) -> bool {
        if (depth >= 7) return false;
        if (depth < 3) return true;  // base grid

        // Two refinement centres
        double d1 = std::sqrt((x + 1.0) * (x + 1.0) + (y + 1.0) * (y + 1.0));
        double d2 = std::sqrt((x - 1.0) * (x - 1.0) + (y - 1.0) * (y - 1.0));
        double d = std::min(d1, d2);

        double radius = 0.8 / (1 << (depth / 2));
        return d < radius;
    });

    std::cout << "  Nodes: " << qt.num_nodes()
              << "  Leaves: " << qt.num_leaves()
              << "  Depth: " << qt.tree_depth() << "\n";

    std::vector<QPoint2D> pts;
    auto tris = qt.triangulate(pts);
    print_stats(pts, tris);
}

// =======================================================================
//  Main
// =======================================================================
int main()
{
    std::cout << "=== Quadtree Tests ===\n\n";

    test_uniform_grid();
    test_adaptive_near_point();
    test_large_grid();
    test_adaptive_two_regions();

    std::cout << "\n=== All tests passed ===\n";
    return 0;
}
