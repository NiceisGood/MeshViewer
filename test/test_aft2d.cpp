#include "AFT2D.h"
#include "Delaunay2D.h"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <cstdlib>
#include <chrono>
#include <vector>

// =======================================================================
//  Test harness for 2D Advancing Front mesh generation
// =======================================================================

static const double PI = 3.14159265358979323846;

/// Generate L-shaped domain boundary (6 points, CCW).
static std::vector<Point2D> lshape_boundary() {
    return {
        {0.0, 0.0}, {2.0, 0.0}, {2.0, 0.5},
        {0.5, 0.5}, {0.5, 2.0}, {0.0, 2.0}
    };
}

/// Generate a regular polygon boundary.
static std::vector<Point2D> regular_polygon(int n, double cx, double cy,
                                             double r) {
    std::vector<Point2D> pts(n);
    for (int i = 0; i < n; ++i) {
        double ang = 2.0 * PI * i / n;
        pts[i] = {cx + r * std::cos(ang), cy + r * std::sin(ang)};
    }
    return pts;  // CCW by construction
}

/// Generate a rectangular boundary.
static std::vector<Point2D> rect_boundary(double x0, double y0,
                                           double x1, double y1) {
    return {
        {x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}
    };
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
            double c_val = (a2 + b2 - opp2) / (2.0 * std::sqrt(a2 * b2));
            if (c_val > 1.0) c_val = 1.0; if (c_val < -1.0) c_val = -1.0;
            return std::acos(c_val) * 180.0 / PI;
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

    std::cout << "  pts    : " << pts.size() << "\n";
    std::cout << "  tris   : " << tris.size() << "\n";
    std::cout << "  area   : [" << min_area << ", " << max_area
              << "]  avg=" << (sum_area / tris.size()) << "\n";
    std::cout << "  min ang: " << min_ang << "°"
              << "  worst=" << max_ang << "°"
              << "  avg=" << (sum_ang / tris.size()) << "°\n";
    std::cout << "  < 20°  : " << bad << " / " << tris.size() << "\n";
}

int main() {
    std::cout << "=== 2D Advancing Front Mesh Generation Test ===\n\n";

    // ---- Test 1: L-shape domain ----
    {
        std::cout << "[Test 1] L-shape domain, target_area=0.04\n";
        auto boundary = lshape_boundary();
        AdvancingFront2D::Config cfg;
        cfg.target_area      = 0.04;
        cfg.min_angle_deg    = 20.0;
        cfg.max_iter         = 10000;

        AdvancingFront2D mesh(boundary, cfg);
        auto t0 = std::chrono::high_resolution_clock::now();
        mesh.generate();
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

        analyse(mesh.points(), mesh.triangles());
        std::cout << "  time   : " << std::fixed << std::setprecision(2) << ms << " ms\n\n";
    }

    // ---- Test 2: Square domain ----
    {
        std::cout << "[Test 2] Square domain, target_area=0.02\n";
        auto boundary = rect_boundary(0.0, 0.0, 2.0, 2.0);
        AdvancingFront2D::Config cfg;
        cfg.target_area      = 0.02;
        cfg.min_angle_deg    = 20.0;
        cfg.max_iter         = 10000;

        AdvancingFront2D mesh(boundary, cfg);
        auto t0 = std::chrono::high_resolution_clock::now();
        mesh.generate();
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

        analyse(mesh.points(), mesh.triangles());
        std::cout << "  time   : " << std::fixed << std::setprecision(2) << ms << " ms\n\n";
    }

    // ---- Test 3: Regular hexagon ----
    {
        std::cout << "[Test 3] Regular hexagon (r=1.0), target_area=0.03\n";
        auto boundary = regular_polygon(6, 0.0, 0.0, 1.0);
        AdvancingFront2D::Config cfg;
        cfg.target_area      = 0.03;
        cfg.min_angle_deg    = 20.0;
        cfg.max_iter         = 10000;

        AdvancingFront2D mesh(boundary, cfg);
        auto t0 = std::chrono::high_resolution_clock::now();
        mesh.generate();
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

        analyse(mesh.points(), mesh.triangles());
        std::cout << "  time   : " << std::fixed << std::setprecision(2) << ms << " ms\n\n";
    }

    // ---- Test 4: Coarse square (size_multiplier = 3) ----
    {
        std::cout << "[Test 4] Square domain, coarse (size_multiplier=3)\n";
        auto boundary = rect_boundary(0.0, 0.0, 2.0, 2.0);
        AdvancingFront2D::Config cfg;
        cfg.target_area      = 0.02;
        cfg.size_multiplier  = 3.0;
        cfg.min_angle_deg    = 15.0;
        cfg.max_iter         = 5000;

        AdvancingFront2D mesh(boundary, cfg);
        auto t0 = std::chrono::high_resolution_clock::now();
        mesh.generate();
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

        analyse(mesh.points(), mesh.triangles());
        std::cout << "  time   : " << std::fixed << std::setprecision(2) << ms << " ms\n\n";
    }

    std::cout << "=== Done ===\n";
    return 0;
}
