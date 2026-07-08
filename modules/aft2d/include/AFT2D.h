#ifndef AFT2D_H
#define AFT2D_H

#include <vector>
#include <cstdint>
#include <stdexcept>

// ---- DLL export/import ---------------------------------------------------
#ifdef _WIN32
#  ifdef AFT2D_EXPORTS
#    define AFT2D_API __declspec(dllexport)
#  else
#    define AFT2D_API __declspec(dllimport)
#  endif
#else
#  define AFT2D_API
#endif

struct Point2D;
struct Triangle2D;

// -----------------------------------------------------------------------
// AdvancingFront2D — 2D advancing-front mesh generator.
//
// Starts from a closed boundary polygon (CCW) and progressively
// advances a front of edges into the interior, filling the domain
// with triangles of approximately the target size.
//
// Quality criteria:
//   target_area      — desired triangle area  (governs element size)
//   min_angle_deg    — lower bound on minimum angle  (0 = unlimited)
//   max_iter         — safety iteration limit
//   size_multiplier  — global scaling of element size  (1.0 = target)
// -----------------------------------------------------------------------
class AFT2D_API AdvancingFront2D {
public:
    struct Config {
        double target_area      = 0.01;   // desired element area
        double min_angle_deg    = 20.0;   // degrees, 0 = unlimited
        int    max_iter         = 50000;  // safety limit
        double size_multiplier  = 1.0;    // global size scale
    };

    /// Construct with boundary polygon vertices (CCW order, at least 3).
    AdvancingFront2D(std::vector<Point2D> boundary, Config cfg);

    /// Construct with default config.
    explicit AdvancingFront2D(std::vector<Point2D> boundary);

    /// Run the advancing-front mesh generation.
    void generate();

    /// Access results after generation.
    const std::vector<Point2D>&   points()    const { return pts_; }
    const std::vector<Triangle2D>& triangles() const { return tris_; }

    /// Laplacian smoothing (iterative) — moves interior vertices to the
    /// average of their neighbours.  Improves element quality.
    void smooth(int iterations = 8);

private:
    // Front edge in a circular doubly-linked list.
    struct FrontEdge {
        int i, j;       // point indices in pts_
        int next, prev; // linked-list indices into edges_
    };

    std::vector<Point2D>    pts_;
    std::vector<Triangle2D> tris_;
    Config cfg_;

    std::vector<FrontEdge> edges_;  // edge pool
    int front_head_;                // head of circular linked list (-1 = empty)
    int n_boundary_;                // number of original boundary points

    // --- geometry helpers ---
    static double orient2d(const Point2D& a, const Point2D& b,
                           const Point2D& c);
    static double edge_len2(const Point2D& a, const Point2D& b);
    static double triangle_area2(const Point2D& a, const Point2D& b,
                                 const Point2D& c);

    /// Minimum angle of triangle (a,b,c) in radians.
    static double min_angle_rad(const Point2D& a, const Point2D& b,
                                const Point2D& c);

    /// Compute ideal third point for edge (a,b) with target area `area`.
    /// Returns the point in `c`.  Returns true if edge length > 0.
    static bool ideal_point(const Point2D& a, const Point2D& b,
                            double area, Point2D& c);

    /// Check if segment (p,q) intersects any front edge (excluding shared vertices).
    /// Returns the intersected edge index, or -1.
    int intersect_front(const Point2D& p, const Point2D& q) const;

    /// Check if segment (a,b) intersects segment (c,d), excluding endpoints.
    static bool segments_intersect(const Point2D& a, const Point2D& b,
                                   const Point2D& c, const Point2D& d);

    /// Find the nearest front vertex to point p, within tolerance dist2.
    /// Returns the vertex index or -1.
    int nearest_front_vertex(const Point2D& p, double max_dist2) const;

    /// Find a front edge with endpoints (a,b) or (b,a); returns index or -1.
    int find_front_edge(int a, int b) const;

    /// Add a new point and return its index.
    int add_point(const Point2D& p);

    /// Remove edge `ei` from the front list; returns its successor.
    int remove_front_edge(int ei);

    /// Insert edge (a,b) into the front list after edge `after_ei`.
    /// Returns the new edge index.
    int insert_front_edge(int after_ei, int a, int b);

    /// Build the initial front from the boundary polygon.
    void init_front();

    /// Subdivide boundary edges that are much longer than target size.
    void refine_boundary(double target_area);

    /// Ear-clip triangulation of the remaining front polygon.
    /// Used as deadlock recovery when AFT cannot advance further.
    void earclip_front();
};

#endif // AFT2D_H
