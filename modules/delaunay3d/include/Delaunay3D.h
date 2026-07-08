#ifndef DELAUNAY3D_H
#define DELAUNAY3D_H

#include <vector>
#include <cstdint>
#include <stdexcept>

// ---- DLL export/import ---------------------------------------------------
#ifdef _WIN32
#  ifdef DELAUNAY3D_EXPORTS
#    define DELAUNAY3D_API __declspec(dllexport)
#  else
#    define DELAUNAY3D_API __declspec(dllimport)
#  endif
#else
#  define DELAUNAY3D_API
#endif

// -----------------------------------------------------------------------
// Delaunay3D — incremental Bowyer-Watson Delaunay tetrahedralization in 3D.
//
//   Strategy::Default    — plain Bowyer-Watson (no sorting).
//   Strategy::Optimized  — Morton sort + breadcrumb scan + pre-allocation.
//
// Output tetrahedra reference indices into the original input points.
// -----------------------------------------------------------------------

struct DELAUNAY3D_API Point3D {
    double x, y, z;
};

struct DELAUNAY3D_API Tetrahedron {
    int v0, v1, v2, v3;
};

class DELAUNAY3D_API Delaunay3D {
public:
    enum class Strategy : uint8_t {
        Default,
        Optimized
    };

    explicit Delaunay3D(std::vector<Point3D> points,
                        Strategy strategy = Strategy::Optimized);

    std::vector<Tetrahedron> tetrahedralize();

    const std::vector<Point3D>& points() const { return pts_; }

private:
    std::vector<Point3D> pts_;
    Strategy strat_;

    struct SuperTetrahedron { int i0, i1, i2, i3; };
    SuperTetrahedron build_super_tetrahedron();

    void bounding_box(double& xmin, double& ymin, double& zmin,
                      double& xmax, double& ymax, double& zmax) const;

    static double orient3d(const Point3D& a, const Point3D& b,
                           const Point3D& c, const Point3D& d);
    static double circumsphere_test(const Point3D& a, const Point3D& b,
                                    const Point3D& c, const Point3D& d0,
                                    const Point3D& d);
    bool uses_super_vertex(int ti, const std::vector<Tetrahedron>& tets,
                           const SuperTetrahedron& st) const;

    static uint64_t hilbert_index_3d(double x, double y, double z,
                                     int nb = 10);

    std::vector<Tetrahedron> tetrahedralize_default();
    std::vector<Tetrahedron> tetrahedralize_optimized();
};

#endif // DELAUNAY3D_H
