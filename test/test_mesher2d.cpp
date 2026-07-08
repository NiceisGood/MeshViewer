#include "Mesher2D.h"
#include "Delaunay2D.h"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <cstdlib>
#include <chrono>

// ===================================================================
//  Test harness for 2D mesh generation
// ===================================================================

static const double PI = 3.14159265358979323846;

/// Generate L-shaped domain points (6 corners).
static std::vector<Point2D> lshape_points() {
    return {
        {0.0, 0.0}, {2.0, 0.0}, {2.0, 0.5},
        {0.5, 0.5}, {0.5, 2.0}, {0.0, 2.0}
    };
}

/// Generate random points in unit square.
static std::vector<Point2D> random_points(int n, unsigned seed = 42) {
    std::srand(seed);
    std::vector<Point2D> pts(n);
    for (auto& p : pts) { p.x = std::rand() / double(RAND_MAX); p.y = std::rand() / double(RAND_MAX); }
    return pts;
}

/// Compute triangle quality metrics.
static void analyse(const std::vector<Point2D>& pts,
                    const std::vector<Triangle2D>& tris) {
    if (tris.empty()) { std::cout << "  (empty)\n"; return; }
    double min_ang = 1e10, max_ang = 0, sum_ang = 0;
    double min_area = 1e10, max_area = 0, sum_area = 0;
    int bad = 0;

    for (const auto& t : tris) {
        const auto& a = pts[t.v0], b = pts[t.v1], c = pts[t.v2];
        double area2 = std::abs((b.x-a.x)*(c.y-a.y) - (b.y-a.y)*(c.x-a.x));
        double area = area2 * 0.5;

        auto sqlen = [&](const Point2D& p, const Point2D& q) {
            double dx = p.x-q.x, dy = p.y-q.y; return dx*dx + dy*dy;
        };
        auto angle = [&](double opp2, double a2, double b2) {
            double c = (a2 + b2 - opp2) / (2.0 * std::sqrt(a2 * b2));
            if (c > 1.0) c = 1.0; if (c < -1.0) c = -1.0;
            return std::acos(c) * 180.0 / PI;
        };
        double a2 = sqlen(b,c), b2 = sqlen(c,a), c2 = sqlen(a,b);
        double ang1 = angle(a2, b2, c2);
        double ang2 = angle(b2, a2, c2);
        double ang3 = angle(c2, a2, b2);
        double min_a = std::min({ang1, ang2, ang3});
        double max_a = std::max({ang1, ang2, ang3});

        if (area < min_area) min_area = area;
        if (area > max_area) max_area = area;
        if (min_a < min_ang) min_ang = min_a;
        if (max_a > max_ang) max_ang = max_a;
        sum_area += area;
        sum_ang  += min_a;
        if (min_a < 20.0) ++bad;
    }

    std::cout << "  tris   : " << tris.size() << "\n";
    std::cout << "  area   : [" << min_area << ", " << max_area
              << "]  avg=" << (sum_area / tris.size()) << "\n";
    std::cout << "  min ang: " << min_ang << "°"
              << "  worst=" << max_ang << "°"
              << "  avg=" << (sum_ang / tris.size()) << "°\n";
    std::cout << "  < 20°  : " << bad << " / " << tris.size() << "\n";
}

int main() {
    std::cout << "=== 2D Mesh Generation Test ===\n\n";

    // ---- Test 1: L-shape, min angle 25° ----
    {
        std::cout << "[Test 1] L-shape domain, min_angle=25°\n";
        auto pts = lshape_points();
        Mesher2D::Config cfg;
        cfg.min_angle_deg = 25.0;
        cfg.max_area      = 0.0;   // no area limit
        cfg.max_iter      = 500;

        Mesher2D mesh(pts, cfg);
        auto t0 = std::chrono::high_resolution_clock::now();
        mesh.refine();
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

        std::cout << "  points : " << mesh.points().size() << "\n";
        analyse(mesh.points(), mesh.triangles());
        std::cout << "  time   : " << std::fixed << std::setprecision(2) << ms << " ms\n\n";
    }

    // ---- Test 2: 50 random points, min angle 20° ----
    {
        std::cout << "[Test 2] 50 random pts, min_angle=20°, max_area=0.02\n";
        auto pts = random_points(50, 12345);
        Mesher2D::Config cfg;
        cfg.min_angle_deg = 20.0;
        cfg.max_area      = 0.02;
        cfg.max_iter      = 1000;

        Mesher2D mesh(pts, cfg);
        auto t0 = std::chrono::high_resolution_clock::now();
        mesh.refine();
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

        std::cout << "  points : " << mesh.points().size() << "\n";
        analyse(mesh.points(), mesh.triangles());
        std::cout << "  time   : " << std::fixed << std::setprecision(2) << ms << " ms\n\n";
    }

    // ---- Test 3: 200 random points, strict quality ----
    {
        std::cout << "[Test 3] 200 random pts, min_angle=30°, max_area=0.01\n";
        auto pts = random_points(200, 999);
        Mesher2D::Config cfg;
        cfg.min_angle_deg = 30.0;
        cfg.max_area      = 0.01;
        cfg.max_iter      = 2000;

        Mesher2D mesh(pts, cfg);
        auto t0 = std::chrono::high_resolution_clock::now();
        mesh.refine();
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

        std::cout << "  points : " << mesh.points().size() << "\n";
        analyse(mesh.points(), mesh.triangles());
        std::cout << "  time   : " << std::fixed << std::setprecision(2) << ms << " ms\n\n";
    }

    std::cout << "=== Done ===\n";
    return 0;
}
