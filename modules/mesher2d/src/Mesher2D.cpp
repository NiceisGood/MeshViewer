#include "Mesher2D.h"
#include "Delaunay2D.h"   // Delaunay2D, Point2D, Triangle2D

#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <vector>

// =======================================================================
//  Internal helpers
// =======================================================================

void Mesher2D::circumcenter(const Point2D& a, const Point2D& b,
                             const Point2D& c,
                             double& cx, double& cy, double& r2)
{
    double d = 2.0 * (a.x * (b.y - c.y) + b.x * (c.y - a.y) + c.x * (a.y - b.y));
    if (std::abs(d) < 1e-20) { d = (d < 0) ? -1e-20 : 1e-20; }
    double ax2 = a.x*a.x + a.y*a.y;
    double bx2 = b.x*b.x + b.y*b.y;
    double cx2 = c.x*c.x + c.y*c.y;
    cx = (ax2 * (b.y - c.y) + bx2 * (c.y - a.y) + cx2 * (a.y - b.y)) / d;
    cy = (ax2 * (c.x - b.x) + bx2 * (a.x - c.x) + cx2 * (b.x - a.x)) / d;
    double dx = a.x - cx, dy = a.y - cy;
    r2 = dx*dx + dy*dy;
}

double Mesher2D::min_angle_rad(const Point2D& a, const Point2D& b,
                                const Point2D& c)
{
    auto sqlen = [](const Point2D& p, const Point2D& q) {
        double dx = p.x - q.x, dy = p.y - q.y;
        return dx*dx + dy*dy;
    };
    double a2 = sqlen(b, c);  // side opposite a
    double b2 = sqlen(c, a);  // side opposite b
    double c2 = sqlen(a, b);  // side opposite c

    // Law of cosines, clamp to valid range for acos
    auto angle = [](double opp2, double adj1_2, double adj2_2) {
        double cos_ang = (adj1_2 + adj2_2 - opp2) /
                         (2.0 * std::sqrt(adj1_2 * adj2_2));
        if (cos_ang >  1.0) cos_ang =  1.0;
        if (cos_ang < -1.0) cos_ang = -1.0;
        return std::acos(cos_ang);
    };

    double ang_a = angle(a2, b2, c2);
    double ang_b = angle(b2, a2, c2);
    double ang_c = angle(c2, a2, b2);

    return std::min({ang_a, ang_b, ang_c});
}

// =======================================================================
//  Constructors
// =======================================================================
Mesher2D::Mesher2D(std::vector<Point2D> points, Config cfg)
    : pts_(std::move(points)), cfg_(cfg)
{
    if (pts_.size() < 3)
        throw std::invalid_argument("Mesher2D: at least 3 points required");
}

Mesher2D::Mesher2D(std::vector<Point2D> points)
    : pts_(std::move(points))
{
    if (pts_.size() < 3)
        throw std::invalid_argument("Mesher2D: at least 3 points required");
}

// =======================================================================
//  Refinement loop
// =======================================================================
void Mesher2D::refine()
{
    double min_angle_thresh = cfg_.min_angle_deg * (3.14159265358979323846 / 180.0);

    // Bounding-box-aware quantisation for midpoint dedup.
    double bx0 = pts_[0].x, by0 = pts_[0].y, bx1 = bx0, by1 = by0;
    for (const auto& p : pts_) {
        if (p.x < bx0) bx0 = p.x; if (p.x > bx1) bx1 = p.x;
        if (p.y < by0) by0 = p.y; if (p.y > by1) by1 = p.y;
    }
    double range = std::max(bx1 - bx0, by1 - by0);
    double quant = (range > 1e-12) ? range / 1000.0 : 1e-6;
    double inv_quant = 1.0 / quant;

    std::unordered_set<uint64_t> global_seen;
    auto cc_key = [&](double x, double y) -> uint64_t {
        int64_t ix = static_cast<int64_t>((x - bx0) * inv_quant);
        int64_t iy = static_cast<int64_t>((y - by0) * inv_quant);
        return static_cast<uint64_t>(ix) ^
               (static_cast<uint64_t>(iy) << 32);
    };

    int stalled = 0;        // consecutive non-improving iterations
    int prev_nbad = -1;

    for (int iter = 0; iter < cfg_.max_iter; ++iter) {
        // ---- 1. Delaunay triangulate current points ----
        Delaunay2D dt(pts_, Delaunay2D::Strategy::Optimized);
        tris_ = dt.triangulate();

        // ---- 2. Identify bad triangles ----
        struct BadTri { double cx, cy; };
        std::vector<BadTri> bad;
        bad.reserve(tris_.size() / 4);

        for (const auto& t : tris_) {
            const Point2D& a = pts_[t.v0];
            const Point2D& b = pts_[t.v1];
            const Point2D& c = pts_[t.v2];

            bool is_bad = false;

            if (cfg_.max_area > 0.0) {
                double area2 = std::abs(
                    (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x));
                if (area2 * 0.5 > cfg_.max_area)
                    is_bad = true;
            }

            if (!is_bad && cfg_.min_angle_deg > 0.0) {
                double ang = min_angle_rad(a, b, c);
                if (ang < min_angle_thresh)
                    is_bad = true;
            }

            if (is_bad) {
                // Midpoint of the longest edge (more stable than circumcenter).
                double a2 = (b.x-a.x)*(b.x-a.x)+(b.y-a.y)*(b.y-a.y);
                double b2 = (c.x-b.x)*(c.x-b.x)+(c.y-b.y)*(c.y-b.y);
                double c2 = (a.x-c.x)*(a.x-c.x)+(a.y-c.y)*(a.y-c.y);

                double mx, my;
                if (a2 >= b2 && a2 >= c2) {       // longest edge a-b
                    mx = (a.x + b.x) * 0.5; my = (a.y + b.y) * 0.5;
                } else if (b2 >= a2 && b2 >= c2) { // longest edge b-c
                    mx = (b.x + c.x) * 0.5; my = (b.y + c.y) * 0.5;
                } else {                           // longest edge c-a
                    mx = (c.x + a.x) * 0.5; my = (c.y + a.y) * 0.5;
                }

                uint64_t key = cc_key(mx, my);
                if (global_seen.insert(key).second)
                    bad.push_back({mx, my});
            }
        }

        // ---- 3. Converged? ----
        int nbad = static_cast<int>(bad.size());
        if (nbad == 0)
            break;

        // ---- 4. Stagnation guard ----
        if (prev_nbad >= 0 && nbad >= prev_nbad) {
            if (++stalled >= 3) break;
        } else {
            stalled = 0;
        }
        prev_nbad = nbad;

        // ---- 5. Insert midpoints ----
        for (const auto& b : bad)
            pts_.push_back({b.cx, b.cy});
    }

    // Final triangulation after loop ends.
    Delaunay2D dt(pts_, Delaunay2D::Strategy::Optimized);
    tris_ = dt.triangulate();
}
