#include "Delaunay3D.h"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <cstdlib>
#include <chrono>

// =======================================================================
//  Test harness for 3D Delaunay tetrahedralization
// =======================================================================

static std::vector<Point3D> random_points_3d(int n, unsigned seed = 42) {
    std::srand(seed);
    std::vector<Point3D> pts;
    pts.reserve(n);
    for (int i = 0; i < n; ++i)
        pts.push_back({static_cast<double>(std::rand()) / RAND_MAX,
                       static_cast<double>(std::rand()) / RAND_MAX,
                       static_cast<double>(std::rand()) / RAND_MAX});
    return pts;
}

static std::vector<Point3D> grid_points_3d(int nx, int ny, int nz) {
    std::vector<Point3D> pts;
    pts.reserve(nx * ny * nz);
    for (int i = 0; i < nx; ++i)
        for (int j = 0; j < ny; ++j)
            for (int k = 0; k < nz; ++k)
                pts.push_back({static_cast<double>(i) / (nx - 1),
                               static_cast<double>(j) / (ny - 1),
                               static_cast<double>(k) / (nz - 1)});
    return pts;
}

static int validate_3d(const std::vector<Point3D>& pts,
                       const std::vector<Tetrahedron>& tets) {
    int errors = 0;
    int np = static_cast<int>(pts.size());
    std::cout << "  tetrahedra : " << tets.size() << "\n";

    for (size_t i = 0; i < tets.size(); ++i) {
        const auto& t = tets[i];
        if (t.v0 >= np || t.v1 >= np || t.v2 >= np || t.v3 >= np ||
            t.v0 < 0 || t.v1 < 0 || t.v2 < 0 || t.v3 < 0) {
            std::cerr << "  ERROR: tet " << i << " out-of-range\n";
            ++errors;
            continue;
        }
        double vol = std::abs(
            (pts[t.v1].x - pts[t.v0].x) *
                ((pts[t.v2].y - pts[t.v0].y) * (pts[t.v3].z - pts[t.v0].z) -
                 (pts[t.v2].z - pts[t.v0].z) * (pts[t.v3].y - pts[t.v0].y)) +
            (pts[t.v1].y - pts[t.v0].y) *
                ((pts[t.v2].z - pts[t.v0].z) * (pts[t.v3].x - pts[t.v0].x) -
                 (pts[t.v2].x - pts[t.v0].x) * (pts[t.v3].z - pts[t.v0].z)) +
            (pts[t.v1].z - pts[t.v0].z) *
                ((pts[t.v2].x - pts[t.v0].x) * (pts[t.v3].y - pts[t.v0].y) -
                 (pts[t.v2].y - pts[t.v0].y) * (pts[t.v3].x - pts[t.v0].x))
        ) / 6.0;
        if (vol < 1e-15)
            std::cerr << "  WARNING: tet " << i << " near-zero volume\n";
    }
    return errors;
}

static void run_test(const char* name,
                     const std::vector<Point3D>& pts,
                     Delaunay3D::Strategy strat,
                     const char* strat_name,
                     int expected_lo, int expected_hi) {
    std::cout << "[" << name << "] " << strat_name << "\n";
    Delaunay3D dt(pts, strat);
    auto t0 = std::chrono::high_resolution_clock::now();
    auto tets = dt.tetrahedralize();
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    int err = validate_3d(pts, tets);
    bool ok = (err == 0 && tets.size() >= static_cast<size_t>(expected_lo) &&
               tets.size() <= static_cast<size_t>(expected_hi));
    std::cout << "  time  : " << std::fixed << std::setprecision(2) << ms << " ms\n";
    std::cout << "  errors: " << err << "\n";
    std::cout << "  " << (ok ? "PASS" : "FAIL") << "\n\n";
}

int main() {
    std::cout << "=== 3D Delaunay Tetrahedralization — Strategy Comparison ===\n\n";

    // Test 1: minimal
    {
        std::vector<Point3D> pts = {
            {0,0,0},{1,0,0},{0,1,0},{0,0,1},{0.25,0.25,0.25}
        };
        run_test("5 pts", pts, Delaunay3D::Strategy::Default, "Default", 2, 6);
        run_test("5 pts", pts, Delaunay3D::Strategy::Optimized, "Optimized", 2, 6);
    }

    // Test 2: 50 random
    {
        auto pts = random_points_3d(50, 54321);
        run_test("50 random", pts, Delaunay3D::Strategy::Default, "Default", 150, 400);
        run_test("50 random", pts, Delaunay3D::Strategy::Optimized, "Optimized", 150, 400);
    }

    // Test 3: 4x4x4 grid
    {
        auto pts = grid_points_3d(4, 4, 4);
        run_test("4x4x4 grid", pts, Delaunay3D::Strategy::Default, "Default", 150, 400);
        run_test("4x4x4 grid", pts, Delaunay3D::Strategy::Optimized, "Optimized", 150, 400);
    }

    // Test 4: 500 random
    {
        auto pts = random_points_3d(500, 777);
        run_test("500 random", pts, Delaunay3D::Strategy::Default, "Default", 2000, 4000);
        run_test("500 random", pts, Delaunay3D::Strategy::Optimized, "Optimized", 2000, 4000);
    }

    // Test 5: 2000 random (big)
    {
        auto pts = random_points_3d(2000, 42);
        run_test("2000 random", pts, Delaunay3D::Strategy::Default, "Default", 8000, 16000);
        run_test("2000 random", pts, Delaunay3D::Strategy::Optimized, "Optimized", 8000, 16000);
    }

    std::cout << "=== All tests completed ===\n";
    return 0;
}
