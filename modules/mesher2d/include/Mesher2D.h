#ifndef MESHER2D_H
#define MESHER2D_H

#include <vector>
#include <cstdint>
#include <stdexcept>

// ---- DLL export/import ---------------------------------------------------
#ifdef _WIN32
#  ifdef MESHER2D_EXPORTS
#    define MESHER2D_API __declspec(dllexport)
#  else
#    define MESHER2D_API __declspec(dllimport)
#  endif
#else
#  define MESHER2D_API
#endif

struct Point2D;
struct Triangle2D;

// -----------------------------------------------------------------------
// Mesher2D — Delaunay refinement 2D mesh generator.
//
// Starts from a Delaunay triangulation of the input points, then
// iteratively inserts circumcenters of triangles that violate quality
// criteria until the mesh converges or max iterations are reached.
//
// Quality criteria:
//   max_area      — upper bound on triangle area  (0 = unlimited)
//   min_angle_deg — lower bound on minimum angle  (0 = unlimited)
// -----------------------------------------------------------------------

class MESHER2D_API Mesher2D {
public:
    struct Config {
        double max_area      = 0.0;    // 0 = unlimited
        double min_angle_deg = 20.0;   // degrees, 0 = unlimited
        int    max_iter      = 2000;
    };

    /// Construct with initial point set and quality config.
    /// Use default config if cfg is omitted.
    Mesher2D(std::vector<Point2D> points, Config cfg);

    /// Construct with default quality config.
    explicit Mesher2D(std::vector<Point2D> points);

    /// Run the refinement loop.  May be called multiple times with
    /// different config between calls (e.g. progressive refinement).
    void refine();

    /// Access results after refinement.
    const std::vector<Point2D>&   points()    const { return pts_; }
    const std::vector<Triangle2D>& triangles() const { return tris_; }

private:
    std::vector<Point2D>   pts_;
    std::vector<Triangle2D> tris_;
    Config cfg_;

    /// Compute circumcenter of triangle (a,b,c); returns {cx, cy, r2}.
    static void circumcenter(const Point2D& a, const Point2D& b,
                             const Point2D& c,
                             double& cx, double& cy, double& r2);

    /// Minimum angle of triangle (a,b,c) in radians.
    static double min_angle_rad(const Point2D& a, const Point2D& b,
                                const Point2D& c);
};

#endif // MESHER2D_H
