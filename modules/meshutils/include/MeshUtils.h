#ifndef MESHUTILS_H
#define MESHUTILS_H

#ifdef _MSC_VER
#pragma warning(disable : 4996)  // fopen_s deprecation
#endif

#include <vector>
#include <string>
#include <cmath>
#include <cstdio>
#include <algorithm>
#include <limits>

#include "Delaunay2D.h"  // Point2D, Triangle2D

// -----------------------------------------------------------------------
// MeshStats — comprehensive mesh quality metrics.
// -----------------------------------------------------------------------
struct MeshStats {
    int    num_points    = 0;
    int    num_triangles = 0;

    // Angle statistics (degrees)
    double min_angle     = 180.0;
    double max_angle     = 0.0;
    double avg_angle     = 0.0;

    // Area statistics
    double min_area      = 1e100;
    double max_area      = 0.0;
    double avg_area      = 0.0;
    double total_area    = 0.0;

    // Aspect ratio (circumradius / (2*inradius)) — 1.0 = equilateral
    double min_aspect    = 1e100;
    double max_aspect    = 0.0;
    double avg_aspect    = 0.0;

    // Counts
    int    n_below_20    = 0;   // triangles with min angle < 20°
    int    n_below_30    = 0;   // triangles with min angle < 30°
    int    n_inverted    = 0;   // negatively-oriented triangles
};

/// Compute comprehensive mesh statistics.
inline MeshStats compute_stats(const std::vector<Point2D>& pts,
                                const std::vector<Triangle2D>& tris)
{
    MeshStats s;
    s.num_points    = static_cast<int>(pts.size());
    s.num_triangles = static_cast<int>(tris.size());
    if (tris.empty()) return s;

    static const double PI = 3.14159265358979323846;
    static const double RAD2DEG = 180.0 / PI;

    double sum_ang = 0, sum_area = 0, sum_aspect = 0;
    int n_angles = 0;

    auto sqlen = [](const Point2D& p, const Point2D& q) {
        double dx = p.x - q.x, dy = p.y - q.y;
        return dx*dx + dy*dy;
    };

    auto angle = [](double opp2, double a2, double b2) {
        double c = (a2 + b2 - opp2) / (2.0 * std::sqrt(a2 * b2));
        if (c >  1.0) c =  1.0;
        if (c < -1.0) c = -1.0;
        return std::acos(c);
    };

    for (const auto& t : tris) {
        const Point2D& a = pts[t.v0];
        const Point2D& b = pts[t.v1];
        const Point2D& c = pts[t.v2];

        // Orientation
        double orient = (b.x - a.x) * (c.y - a.y) -
                        (b.y - a.y) * (c.x - a.x);
        if (orient < 0) ++s.n_inverted;

        double area = std::abs(orient) * 0.5;
        if (area < s.min_area) s.min_area = area;
        if (area > s.max_area) s.max_area = area;
        sum_area += area;

        // Edge lengths squared
        double a2 = sqlen(b, c);  // opposite a
        double b2 = sqlen(c, a);  // opposite b
        double c2 = sqlen(a, b);  // opposite c

        // Min/max/avg angle
        double ang_a = angle(a2, b2, c2) * RAD2DEG;
        double ang_b = angle(b2, a2, c2) * RAD2DEG;
        double ang_c = angle(c2, a2, b2) * RAD2DEG;

        double min_a = std::min({ang_a, ang_b, ang_c});
        double max_a = std::max({ang_a, ang_b, ang_c});

        if (min_a < s.min_angle) s.min_angle = min_a;
        if (max_a > s.max_angle) s.max_angle = max_a;

        sum_ang   += (ang_a + ang_b + ang_c);
        n_angles  += 3;

        if (min_a < 20.0) ++s.n_below_20;
        if (min_a < 30.0) ++s.n_below_30;

        // Aspect ratio: for a triangle, a common formula is
        //   (a*b*c) / (8*(s-a)*(s-b)*(s-c))  = R / (2r)
        // where R = circumradius, r = inradius.
        // Equivalently: (longest edge) / (2 * inradius * sqrt(3))
        // We use the simpler: max_edge_len / min_height
        // where min_height = 2*area / max_edge_len
        double max_e2 = std::max({a2, b2, c2});
        double max_e  = std::sqrt(max_e2);
        double min_h  = (area > 0) ? (2.0 * area / max_e) : 1e-20;
        double ar     = max_e / min_h;  // ≥ 1, 1 = equilateral
        if (ar < s.min_aspect) s.min_aspect = ar;
        if (ar > s.max_aspect) s.max_aspect = ar;
        sum_aspect += ar;
    }

    s.avg_angle  = sum_ang  / n_angles;
    s.avg_area   = sum_area / tris.size();
    s.total_area = sum_area;
    s.avg_aspect = sum_aspect / tris.size();

    return s;
}

/// Print mesh statistics to stdout.
inline void print_stats(const MeshStats& s)
{
    std::printf("Mesh stats:\n");
    std::printf("  pts  : %d   tris : %d\n", s.num_points, s.num_triangles);
    std::printf("  area : [%.4f, %.4f]  avg=%.4f  total=%.4f\n",
                s.min_area, s.max_area, s.avg_area, s.total_area);
    std::printf("  angle: [%.2f°, %.2f°]  avg=%.2f°\n",
                s.min_angle, s.max_angle, s.avg_angle);
    std::printf("  aspect: [%.2f, %.2f]  avg=%.2f   (1 = equilateral)\n",
                s.min_aspect, s.max_aspect, s.avg_aspect);
    std::printf("  < 20°: %d   < 30°: %d   inverted: %d\n",
                s.n_below_20, s.n_below_30, s.n_inverted);
}

// -----------------------------------------------------------------------
//  OBJ export — write mesh to Wavefront OBJ format.
// -----------------------------------------------------------------------

/// Write mesh as a Wavefront OBJ file (triangles only, no normals/UV).
/// Returns true on success.
inline bool write_obj(const std::vector<Point2D>& pts,
                       const std::vector<Triangle2D>& tris,
                       const std::string& path)
{
    FILE* f = std::fopen(path.c_str(), "w");
    if (!f) return false;

    std::fprintf(f, "# Mesh generated by Hermes delaunay library\n");
    std::fprintf(f, "# %zu points, %zu triangles\n\n", pts.size(), tris.size());

    for (const auto& p : pts)
        std::fprintf(f, "v  %.15f %.15f 0.0\n", p.x, p.y);

    std::fprintf(f, "\n");

    for (const auto& t : tris)
        std::fprintf(f, "f  %d %d %d\n",
                     t.v0 + 1, t.v1 + 1, t.v2 + 1);  // OBJ is 1-indexed

    std::fclose(f);
    return true;
}

/// Write mesh as a Wavefront OBJ file, with per-triangle colour based on
/// quality (green = good, yellow = marginal, red = poor).
/// Returns true on success.
inline bool write_obj_coloured(const std::vector<Point2D>& pts,
                                const std::vector<Triangle2D>& tris,
                                const std::string& path)
{
    FILE* f = std::fopen(path.c_str(), "w");
    if (!f) return false;

    std::fprintf(f, "# Mesh generated by Hermes delaunay library (coloured)\n");
    std::fprintf(f, "# %zu points, %zu triangles\n\n", pts.size(), tris.size());

    for (const auto& p : pts)
        std::fprintf(f, "v  %.15f %.15f 0.0\n", p.x, p.y);

    std::fprintf(f, "\n");

    static const double PI = 3.14159265358979323846;
    auto angle_fn = [](double opp2, double a2, double b2) {
        double c = (a2 + b2 - opp2) / (2.0 * std::sqrt(a2 * b2));
        if (c >  1.0) c =  1.0;
        if (c < -1.0) c = -1.0;
        return std::acos(c) * 180.0 / PI;
    };
    auto sqlen = [](const Point2D& a, const Point2D& b) {
        double dx = a.x - b.x, dy = a.y - b.y;
        return dx*dx + dy*dy;
    };

    for (const auto& t : tris) {
        double a2 = sqlen(pts[t.v1], pts[t.v2]);
        double b2 = sqlen(pts[t.v2], pts[t.v0]);
        double c2 = sqlen(pts[t.v0], pts[t.v1]);
        double min_a = std::min({angle_fn(a2, b2, c2),
                                 angle_fn(b2, a2, c2),
                                 angle_fn(c2, a2, b2)});

        // Colour: green ≥ 30°, yellow 20-30°, red < 20°
        const char* colour;
        if (min_a >= 30.0)
            colour = "1.0 0.8 0.2";   // gold
        else if (min_a >= 20.0)
            colour = "1.0 0.6 0.0";   // orange
        else
            colour = "1.0 0.2 0.1";   // red

        std::fprintf(f, "f  %d %d %d  %s\n",
                     t.v0 + 1, t.v1 + 1, t.v2 + 1, colour);
    }

    std::fclose(f);
    return true;
}

#endif // MESHUTILS_H
