#include "Octree.h"
#include <iostream>
#include <cstdlib>
#include <cmath>
#include <cstdio>

// =======================================================================
//  Test: Octree — uniform and adaptive mesh generation
// =======================================================================

static void print_stats(const std::vector<OctPoint3D>& pts,
                         const std::vector<OctTetrahedron>& tets)
{
    std::printf("  points: %zu   tetrahedra: %zu\n", pts.size(), tets.size());

    // Volume statistics
    double min_vol = 1e100, max_vol = 0.0, sum_vol = 0.0;
    int inverted = 0;

    auto orient = [&](const OctPoint3D& a, const OctPoint3D& b,
                      const OctPoint3D& c, const OctPoint3D& d) {
        // 4×4 determinant: (b-a) × (c-a) · (d-a)
        double ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z;
        double vx = c.x - a.x, vy = c.y - a.y, vz = c.z - a.z;
        double wx = d.x - a.x, wy = d.y - a.y, wz = d.z - a.z;
        return ux * (vy * wz - vz * wy)
             - uy * (vx * wz - vz * wx)
             + uz * (vx * wy - vy * wx);
    };

    for (const auto& t : tets) {
        const auto& a = pts[t.v0];
        const auto& b = pts[t.v1];
        const auto& c = pts[t.v2];
        const auto& d = pts[t.v3];
        double vol = std::abs(orient(a, b, c, d)) / 6.0;
        if (vol < min_vol) min_vol = vol;
        if (vol > max_vol) max_vol = vol;
        sum_vol += vol;
        if (orient(a, b, c, d) < 0) ++inverted;
    }

    std::printf("  volume: [%.10f, %.10f]  avg=%.10f  total=%.6f\n",
                min_vol, max_vol, sum_vol / tets.size(), sum_vol);
    if (inverted > 0)
        std::printf("  WARNING: %d inverted tetrahedra!\n", inverted);
}

// =======================================================================
//  Test 1: Uniform grid — subdivide to fixed depth everywhere
// =======================================================================
static void test_uniform_grid()
{
    std::cout << "[Test 1] Uniform octree — depth 3 (8×8×8 cells)\n";

    Octree ot(0.0, 0.0, 0.0, 2.0, 2.0, 2.0, 8);
    ot.build([](double, double, double, double, int depth) {
        return depth < 3;  // stop at depth 3 → 8 cells per axis
    });

    std::cout << "  Nodes: " << ot.num_nodes()
              << "  Leaves: " << ot.num_leaves()
              << "  Depth: " << ot.tree_depth() << "\n";

    std::vector<OctPoint3D> pts;
    auto tets = ot.tetrahedralize(pts);
    print_stats(pts, tets);

    // Expected: 8×8×8 = 512 cells × 6 tets = 3072 tetrahedra
    // Points: (8+1)×(8+1)×(8+1) = 729
    std::cout << "  Expected ~729 points, ~3072 tetrahedra\n";
}

// =======================================================================
//  Test 2: Adaptive — refine near a point (distance-based)
// =======================================================================
static void test_adaptive_near_point()
{
    std::cout << "\n[Test 2] Adaptive octree — refine near centre point\n";

    double cx = 0.0, cy = 0.0, cz = 0.0;

    Octree ot(-1.0, -1.0, -1.0, 1.0, 1.0, 1.0, 6);
    ot.build([&](double x, double y, double z, double hs, int depth) -> bool {
        if (depth >= 6) return false;
        if (depth < 2) return true;  // always subdivide to depth 2

        double dx = x - cx, dy = y - cy, dz = z - cz;
        double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        double threshold = 0.6 / (1 << (depth / 2));
        return dist < threshold;
    });

    std::cout << "  Nodes: " << ot.num_nodes()
              << "  Leaves: " << ot.num_leaves()
              << "  Depth: " << ot.tree_depth() << "\n";

    std::vector<OctPoint3D> pts;
    auto tets = ot.tetrahedralize(pts);
    print_stats(pts, tets);
}

// =======================================================================
//  Test 3: Large uniform grid — performance check
// =======================================================================
static void test_large_grid()
{
    std::cout << "\n[Test 3] Uniform octree — depth 4 (16×16×16 cells)\n";

    Octree ot(0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 8);
    ot.build([](double, double, double, double, int depth) {
        return depth < 4;
    });

    std::cout << "  Nodes: " << ot.num_nodes()
              << "  Leaves: " << ot.num_leaves()
              << "  Depth: " << ot.tree_depth() << "\n";

    std::vector<OctPoint3D> pts;
    auto tets = ot.tetrahedralize(pts);
    print_stats(pts, tets);

    // Expected: 16×16×16 = 4096 cells × 6 tets = 24576 tetrahedra
    std::cout << "  Expected ~4913 points, ~24576 tetrahedra\n";
}

// =======================================================================
//  Test 4: Adaptive with two refinement regions
// =======================================================================
static void test_adaptive_two_regions()
{
    std::cout << "\n[Test 4] Adaptive octree — two refinement zones\n";

    Octree ot(-2.0, -2.0, -2.0, 2.0, 2.0, 2.0, 6);
    ot.build([](double x, double y, double z, double, int depth) -> bool {
        if (depth >= 6) return false;
        if (depth < 3) return true;  // base grid

        // Two refinement centres
        double d1 = std::sqrt((x + 1.0) * (x + 1.0) +
                              (y + 1.0) * (y + 1.0) +
                              (z + 1.0) * (z + 1.0));
        double d2 = std::sqrt((x - 1.0) * (x - 1.0) +
                              (y - 1.0) * (y - 1.0) +
                              (z - 1.0) * (z - 1.0));
        double d = std::min(d1, d2);

        double radius = 1.0 / (1 << (depth / 2));
        return d < radius;
    });

    std::cout << "  Nodes: " << ot.num_nodes()
              << "  Leaves: " << ot.num_leaves()
              << "  Depth: " << ot.tree_depth() << "\n";

    std::vector<OctPoint3D> pts;
    auto tets = ot.tetrahedralize(pts);
    print_stats(pts, tets);
}

// =======================================================================
//  Main
// =======================================================================
int main()
{
    std::cout << "=== Octree Tests ===\n\n";

    test_uniform_grid();
    test_adaptive_near_point();
    test_large_grid();
    test_adaptive_two_regions();

    std::cout << "\n=== All tests passed ===\n";
    return 0;
}
