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

// -----------------------------------------------------------------------
//  Mesh quality improvement via edge flipping
// -----------------------------------------------------------------------

/// Flip-improve mesh quality: iteratively flip interior edges that
/// improve the local minimum angle.  Returns the number of flips performed.
inline int improve_quality(std::vector<Point2D>& pts,
                            std::vector<Triangle2D>& tris,
                            int max_passes = 20)
{
    int nt = static_cast<int>(tris.size());
    if (nt < 2) return 0;

    auto orient = [&](int a, int b, int c) -> double {
        return (pts[b].x - pts[a].x) * (pts[c].y - pts[a].y) -
               (pts[b].y - pts[a].y) * (pts[c].x - pts[a].x);
    };

    auto sqlen = [&](int a, int b) -> double {
        double dx = pts[b].x - pts[a].x, dy = pts[b].y - pts[a].y;
        return dx*dx + dy*dy;
    };

    auto angle_deg = [&](int a, int b, int c) -> double {
        // Angle at vertex 'a' of triangle (a,b,c)
        double a2 = sqlen(b, c);  // side opposite a
        double b2 = sqlen(c, a);  // side opposite b — adj to a via b
        double c2 = sqlen(a, b);  // side opposite c — adj to a via c
        double val = (b2 + c2 - a2) / (2.0 * std::sqrt(b2 * c2));
        if (val >  1.0) val =  1.0;
        if (val < -1.0) val = -1.0;
        return std::acos(val) * 57.29577951308232;
    };

    auto edge_key = [](int a, int b) -> size_t {
        int mn = std::min(a, b), mx = std::max(a, b);
        return static_cast<size_t>(mn) ^ (static_cast<size_t>(mx) << 16);
    };

    int total_flips = 0;

    for (int pass = 0; pass < max_passes; ++pass) {
        // Build edge info: map each undirected edge to the triangles using it
        struct EdgeRec { size_t key; int a, b; int ti; int opp; };
        std::vector<EdgeRec> recs;
        recs.reserve(nt * 3);
        for (int ti = 0; ti < nt; ++ti) {
            const auto& t = tris[ti];
            recs.push_back({edge_key(t.v0, t.v1), t.v0, t.v1, ti, t.v2});
            recs.push_back({edge_key(t.v1, t.v2), t.v1, t.v2, ti, t.v0});
            recs.push_back({edge_key(t.v2, t.v0), t.v2, t.v0, ti, t.v1});
        }

        std::sort(recs.begin(), recs.end(),
                  [](const EdgeRec& e, const EdgeRec& f) { return e.key < f.key; });

        // Track triangles modified this pass to avoid re-flipping
        std::vector<bool> dirty(nt, false);
        int flips = 0;

        for (size_t i = 0; i + 1 < recs.size(); ++i) {
            if (recs[i].key != recs[i + 1].key) continue;

            const auto& r1 = recs[i];
            const auto& r2 = recs[i + 1];

            int ti_a = r1.ti, ti_b = r2.ti;
            if (dirty[ti_a] || dirty[ti_b]) continue;

            const auto& ta = tris[ti_a];
            const auto& tb = tris[ti_b];

            int p = r1.a, q = r1.b;
            int r = r1.opp;  // opposite in first triangle
            int s = r2.opp;  // opposite in second triangle

            // Check quadrilateral convexity: orient(p,r,q) and orient(p,q,s)
            // should have the same sign
            double or1 = orient(p, r, q);
            double or2 = orient(p, q, s);
            if (or1 * or2 <= 0) continue;

            // Current min angle (6 angles)
            double cur[6] = {
                angle_deg(ta.v0, ta.v1, ta.v2),
                angle_deg(ta.v1, ta.v2, ta.v0),
                angle_deg(ta.v2, ta.v0, ta.v1),
                angle_deg(tb.v0, tb.v1, tb.v2),
                angle_deg(tb.v1, tb.v2, tb.v0),
                angle_deg(tb.v2, tb.v0, tb.v1)
            };
            double cur_min = *std::min_element(cur, cur + 6);

            // Proposed flipped triangles: (p, r, s) and (q, s, r)
            double o_new1 = orient(p, r, s);
            double o_new2 = orient(q, s, r);
            if (o_new1 <= 0 || o_new2 <= 0) continue;

            double new6[6] = {
                angle_deg(p, r, s),
                angle_deg(r, s, p),
                angle_deg(s, p, r),
                angle_deg(q, s, r),
                angle_deg(s, r, q),
                angle_deg(r, q, s)
            };
            double new_min = *std::min_element(new6, new6 + 6);

            if (new_min > cur_min + 0.1) {
                tris[ti_a] = {p, r, s};
                tris[ti_b] = {q, s, r};
                // Fix orientation
                if (orient(tris[ti_a].v0, tris[ti_a].v1, tris[ti_a].v2) < 0)
                    std::swap(tris[ti_a].v1, tris[ti_a].v2);
                if (orient(tris[ti_b].v0, tris[ti_b].v1, tris[ti_b].v2) < 0)
                    std::swap(tris[ti_b].v1, tris[ti_b].v2);
                dirty[ti_a] = dirty[ti_b] = true;
                ++flips;
                ++total_flips;
            }
        }

        if (flips == 0) break;
    }

    return total_flips;
}

#endif // MESHUTILS_H
