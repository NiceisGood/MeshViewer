#include "Delaunay2D.h"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <cstdlib>
#include <chrono>

// =======================================================================
//  Test harness for 2D Delaunay triangulation
// =======================================================================

static std::vector<Point2D> random_points(int n, unsigned seed = 42) {
    std::srand(seed);
    std::vector<Point2D> pts;
    pts.reserve(n);
    for (int i = 0; i < n; ++i) {
        double x = static_cast<double>(std::rand()) / RAND_MAX;
        double y = static_cast<double>(std::rand()) / RAND_MAX;
        pts.push_back({x, y});
    }
    return pts;
}

static std::vector<Point2D> grid_points(int nx, int ny) {
    std::vector<Point2D> pts;
    pts.reserve(nx * ny);
    for (int i = 0; i < nx; ++i)
        for (int j = 0; j < ny; ++j)
            pts.push_back({static_cast<double>(i) / (nx - 1),
                           static_cast<double>(j) / (ny - 1)});
    return pts;
}

static int validate(const std::vector<Point2D>& pts,
                    const std::vector<Triangle2D>& tris) {
    int errors = 0;
    int np = static_cast<int>(pts.size());
    std::cout << "  triangles : " << tris.size() << "\n";

    for (size_t i = 0; i < tris.size(); ++i) {
        const auto& t = tris[i];
        if (t.v0 >= np || t.v1 >= np || t.v2 >= np ||
            t.v0 < 0 || t.v1 < 0 || t.v2 < 0) {
            std::cerr << "  ERROR: triangle " << i
                      << " out-of-range (" << t.v0 << "," << t.v1 << "," << t.v2
                      << "), max=" << (np - 1) << "\n";
            ++errors;
            continue;
        }
        double ax = pts[t.v1].x - pts[t.v0].x;
        double ay = pts[t.v1].y - pts[t.v0].y;
        double bx = pts[t.v2].x - pts[t.v0].x;
        double by = pts[t.v2].y - pts[t.v0].y;
        if (std::abs(ax * by - ay * bx) < 1e-15)
            std::cerr << "  WARNING: triangle " << i << " near-zero area\n";
    }
    return errors;
}

static void run_test(const char* name,
                     const std::vector<Point2D>& pts,
                     Delaunay2D::Strategy strat,
                     const char* strat_name,
                     int expected_lo, int expected_hi) {
    std::cout << "[" << name << "] " << strat_name << "\n";
    Delaunay2D dt(pts, strat);
    auto t0 = std::chrono::high_resolution_clock::now();
    auto tris = dt.triangulate();
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    int err = validate(pts, tris);
    bool ok = (err == 0 && tris.size() >= static_cast<size_t>(expected_lo) &&
               tris.size() <= static_cast<size_t>(expected_hi));
    std::cout << "  time  : " << std::fixed << std::setprecision(2) << ms << " ms\n";
    std::cout << "  errors: " << err << "\n";
    std::cout << "  " << (ok ? "PASS" : "FAIL") << "\n\n";
}

int main() {
    std::cout << "=== 2D Delaunay Triangulation — Strategy Comparison ===\n\n";

    // ---------- Test 1: 4 corners ----------
    {
        std::vector<Point2D> pts = {
            {0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}
        };
        run_test("4 corners", pts, Delaunay2D::Strategy::Default,
                 "Default", 2, 2);
        run_test("4 corners", pts, Delaunay2D::Strategy::Optimized,
                 "Optimized", 2, 2);
    }

    // ---------- Test 2: 50 random ----------
    {
        auto pts = random_points(50, 12345);
        run_test("50 random", pts, Delaunay2D::Strategy::Default,
                 "Default", 80, 100);
        run_test("50 random", pts, Delaunay2D::Strategy::Optimized,
                 "Optimized", 80, 100);
    }

    // ---------- Test 3: 6x6 grid ----------
    {
        auto pts = grid_points(6, 6);
        run_test("6x6 grid", pts, Delaunay2D::Strategy::Default,
                 "Default", 40, 70);
        run_test("6x6 grid", pts, Delaunay2D::Strategy::Optimized,
                 "Optimized", 40, 70);
    }

    // ---------- Test 4: 1000 random (stress) ----------
    {
        auto pts = random_points(1000, 999);
        run_test("1000 random", pts, Delaunay2D::Strategy::Default,
                 "Default", 1900, 2100);
        run_test("1000 random", pts, Delaunay2D::Strategy::Optimized,
                 "Optimized", 1900, 2100);
    }

    // ---------- Test 5: 10 000 random (big) ----------
    {
        auto pts = random_points(10000, 42);
        run_test("10k random", pts, Delaunay2D::Strategy::Default,
                 "Default", 19500, 21000);
        run_test("10k random", pts, Delaunay2D::Strategy::Optimized,
                 "Optimized", 19500, 21000);
    }

    std::cout << "=== All tests completed ===\n";
    return 0;
}
