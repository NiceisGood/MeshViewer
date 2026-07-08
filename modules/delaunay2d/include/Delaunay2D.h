#ifndef DELAUNAY2D_H
#define DELAUNAY2D_H

#include <vector>
#include <cstdint>
#include <stdexcept>

// ---- DLL export/import ---------------------------------------------------
#ifdef _WIN32
#  ifdef DELAUNAY2D_EXPORTS
#    define DELAUNAY2D_API __declspec(dllexport)
#  else
#    define DELAUNAY2D_API __declspec(dllimport)
#  endif
#else
#  define DELAUNAY2D_API
#endif

// -----------------------------------------------------------------------
// Delaunay2D — incremental Bowyer-Watson Delaunay triangulation in 2D.
//
//   Strategy::Default    — plain Bowyer-Watson (O(n²) scan).
//   Strategy::Optimized  — Hilbert sort + breadcrumb scan + pre-allocation.
//
// Output triangles reference indices into the original input points.
// -----------------------------------------------------------------------

struct DELAUNAY2D_API Point2D {
    double x, y;
};

struct DELAUNAY2D_API Triangle2D {
    int v0, v1, v2;
};

class DELAUNAY2D_API Delaunay2D {
public:
    enum class Strategy : uint8_t {
        Default,
        Optimized
    };

    explicit Delaunay2D(std::vector<Point2D> points,
                        Strategy strategy = Strategy::Optimized);

    std::vector<Triangle2D> triangulate();

    const std::vector<Point2D>& points() const { return pts_; }

private:
    std::vector<Point2D> pts_;
    Strategy strat_;

    struct SuperTriangle { int i0, i1, i2; };
    SuperTriangle build_super_triangle();

    void bounding_box(double& xmin, double& ymin,
                      double& xmax, double& ymax) const;

    static double orient2d(const Point2D& a, const Point2D& b,
                           const Point2D& c);
    static double circumcircle_test(const Point2D& a, const Point2D& b,
                                    const Point2D& c, const Point2D& d);
    bool uses_super_vertex(int ti, const std::vector<int>& v0,
                           const std::vector<int>& v1,
                           const std::vector<int>& v2,
                           const SuperTriangle& st) const;

    static uint64_t hilbert_index(double x, double y, int nb = 16);

    std::vector<Triangle2D> triangulate_default();
    std::vector<Triangle2D> triangulate_optimized();
};

#endif // DELAUNAY2D_H
