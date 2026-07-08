#include "Delaunay2D.h"
#include "Delaunay3D.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <cmath>
#include <cstdlib>
#include <chrono>
#include <vector>
#include <string>
#include <cstdio>

// ========================================================================
//  Delaunay property verifier — checks the empty circumcircle/sphere
//  property for EVERY triangle/tetrahedron against EVERY other point.
//
//  This is an O(n·t) check (n = points, t = elements) that definitively
//  validates correctness.  Used for verification, not for production.
// ========================================================================

// ---- 2D: verify Delaunay property using the robust InCircle predicate ----
static double incircle_validator(const Point2D& a, const Point2D& b,
                                  const Point2D& c, const Point2D& d) {
    double abx = b.x - a.x, aby = b.y - a.y;
    double acx = c.x - a.x, acy = c.y - a.y;
    double adx = d.x - a.x, ady = d.y - a.y;
    double bx2 = abx*abx + aby*aby;
    double cx2 = acx*acx + acy*acy;
    double dx2 = adx*adx + ady*ady;
    double det = abx*(acy*dx2 - cx2*ady) -
                 aby*(acx*dx2 - cx2*adx) +
                 bx2*(acx*ady - acy*adx);
    return -det;
}

static int verify_delaunay_2d(const std::vector<Point2D>& pts,
                              const std::vector<Triangle2D>& tris) {
    int violations = 0;
    const int np = static_cast<int>(pts.size());
    for (size_t ti = 0; ti < tris.size(); ++ti) {
        const auto& t = tris[ti];
        const Point2D& a = pts[t.v0];
        const Point2D& b = pts[t.v1];
        const Point2D& c = pts[t.v2];
        // Orient the triangle (need CCW for InCircle sign convention)
        double orient = (b.x-a.x)*(c.y-a.y) - (b.y-a.y)*(c.x-a.x);
        for (int pi = 0; pi < np; ++pi) {
            if (pi == t.v0 || pi == t.v1 || pi == t.v2) continue;
            double val = incircle_validator(a, b, c, pts[pi]);
            // For CCW triangle:  val > 0  →  pi is INSIDE circumcircle
            // For CW  triangle:  val < 0  →  pi is INSIDE
            bool inside = (orient > 0) ? (val > 1e-12) : (val < -1e-12);
            if (inside) {
                if (violations < 5)
                    std::cerr << "  DELAUNAY VIOLATION: tri " << ti
                              << " circumcircle contains point " << pi << "\n";
                ++violations;
            }
        }
    }
    return violations;
}

// ---- 3D: verify Delaunay property using the robust InSphere predicate ----
// Replicates the circumsphere_test logic from Delaunay3D for validation.
static double insphere_validator(const Point3D& a, const Point3D& b,
                                  const Point3D& c, const Point3D& d,
                                  const Point3D& e) {
    double bx = b.x - a.x, by = b.y - a.y, bz = b.z - a.z;
    double cx = c.x - a.x, cy = c.y - a.y, cz = c.z - a.z;
    double dx = d.x - a.x, dy = d.y - a.y, dz = d.z - a.z;
    double ex = e.x - a.x, ey = e.y - a.y, ez = e.z - a.z;
    double b2 = bx*bx + by*by + bz*bz;
    double c2 = cx*cx + cy*cy + cz*cz;
    double d2 = dx*dx + dy*dy + dz*dz;
    double e2 = ex*ex + ey*ey + ez*ez;
    double m1 = cx*(dy*ez - dz*ey) - cy*(dx*ez - dz*ex) + cz*(dx*ey - dy*ex);
    double m2 = bx*(dy*ez - dz*ey) - by*(dx*ez - dz*ex) + bz*(dx*ey - dy*ex);
    double m3 = bx*(cy*ez - cz*ey) - by*(cx*ez - cz*ex) + bz*(cx*ey - cy*ex);
    double m4 = bx*(cy*dz - cz*dy) - by*(cx*dz - cz*dx) + bz*(cx*dy - cy*dx);
    return -b2*m1 + c2*m2 - d2*m3 + e2*m4;
}

static int verify_delaunay_3d(const std::vector<Point3D>& pts,
                              const std::vector<Tetrahedron>& tets) {
    int violations = 0;
    const int np = static_cast<int>(pts.size());

    for (size_t ti = 0; ti < tets.size(); ++ti) {
        const auto& t = tets[ti];
        const Point3D& a = pts[t.v0];
        const Point3D& b = pts[t.v1];
        const Point3D& c = pts[t.v2];
        const Point3D& d = pts[t.v3];

        for (int pi = 0; pi < np; ++pi) {
            if (pi == t.v0 || pi == t.v1 || pi == t.v2 || pi == t.v3) continue;
            double val = insphere_validator(a, b, c, d, pts[pi]);
            // For a positively oriented tet (orient3d > 0):
            //   insphere < 0  →  pi is INSIDE the circumsphere  →  violation
            // For a negatively oriented tet:
            //   insphere > 0  →  pi is INSIDE
            double orient = (b.x-a.x)*((c.y-a.y)*(d.z-a.z) - (c.z-a.z)*(d.y-a.y)) -
                            (b.y-a.y)*((c.x-a.x)*(d.z-a.z) - (c.z-a.z)*(d.x-a.x)) +
                            (b.z-a.z)*((c.x-a.x)*(d.y-a.y) - (c.y-a.y)*(d.x-a.x));
            double inside = (orient > 0) ? (val < -1e-12) : (val > 1e-12);
            if (inside) {
                if (violations < 5)
                    std::cerr << "  DELAUNAY VIOLATION: tet " << ti
                              << " circumsphere contains point " << pi << "\n";
                ++violations;
            }
        }
    }
    return violations;
}

// ========================================================================
//  .OBJ file export for visualization
// ========================================================================

static void export_obj_2d(const std::vector<Point2D>& pts,
                          const std::vector<Triangle2D>& tris,
                          const std::string& path) {
    std::ofstream f(path);
    f << "# 2D Delaunay triangulation — " << tris.size() << " triangles\n";
    f << "o delaunay2d\n";
    for (const auto& p : pts)
        f << "v " << p.x << " " << p.y << " 0.0\n";
    for (const auto& t : tris)
        f << "f " << (t.v0 + 1) << " " << (t.v1 + 1) << " " << (t.v2 + 1) << "\n";
    f.close();
    std::cout << "  exported: " << path << "\n";
}

static void export_obj_3d(const std::vector<Point3D>& pts,
                          const std::vector<Tetrahedron>& tets,
                          const std::string& path) {
    std::ofstream f(path);
    f << "# 3D Delaunay tetrahedralization — " << tets.size() << " tetrahedra\n";
    f << "o delaunay3d\n";
    for (const auto& p : pts)
        f << "v " << p.x << " " << p.y << " " << p.z << "\n";
    for (const auto& t : tets) {
        // Wavefront OBJ only supports triangles/quads, so output
        // each tet as 4 triangles (the tetrahedron's faces).
        // Face winding for outward normals.
        f << "f " << (t.v0+1) << " " << (t.v2+1) << " " << (t.v1+1) << "\n";
        f << "f " << (t.v0+1) << " " << (t.v1+1) << " " << (t.v3+1) << "\n";
        f << "f " << (t.v1+1) << " " << (t.v2+1) << " " << (t.v3+1) << "\n";
        f << "f " << (t.v0+1) << " " << (t.v3+1) << " " << (t.v2+1) << "\n";
    }
    f.close();
    std::cout << "  exported: " << path << "\n";
}

// ========================================================================
//  Benchmark helper
// ========================================================================

template<typename F>
static double bench(F&& fn, const char* label, int reps = 3) {
    double best = 1e30;
    for (int r = 0; r < reps; ++r) {
        auto t0 = std::chrono::high_resolution_clock::now();
        fn();
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        if (ms < best) best = ms;
    }
    std::cout << "  " << label << ": " << std::fixed << std::setprecision(1) << best << " ms\n";
    return best;
}

// ========================================================================
//  Main
// ========================================================================

int main() {
    std::cout << "===================================================\n";
    std::cout << "  Delaunay — Validation + Benchmark + Export\n";
    std::cout << "===================================================\n\n";

    // ---- 2D tests --------------------------------------------------------
    std::cout << "--- 2D Delaunay ---\n\n";

    // Small test with known result
    {
        std::vector<Point2D> pts = {{0,0},{1,0},{1,1},{0,1}};
        Delaunay2D dt(pts, Delaunay2D::Strategy::Optimized);
        auto tris = dt.triangulate();
        int v = verify_delaunay_2d(pts, tris);
        export_obj_2d(pts, tris, "delaunay2d_square.obj");
        std::cout << "  4 corners : " << tris.size() << " tris, "
                  << v << " violations "
                  << (v == 0 ? "✓" : "✗") << "\n\n";
    }

    // 100 random points
    {
        std::srand(12345);
        std::vector<Point2D> pts(100);
        for (auto& p : pts) { p.x = std::rand() / double(RAND_MAX); p.y = std::rand() / double(RAND_MAX); }
        Delaunay2D dt(pts, Delaunay2D::Strategy::Optimized);
        auto tris = dt.triangulate();
        int v = verify_delaunay_2d(pts, tris);
        export_obj_2d(pts, tris, "delaunay2d_100.obj");
        std::cout << "  100 random: " << tris.size() << " tris, "
                  << v << " violations "
                  << (v == 0 ? "✓" : "✗") << "\n";
    }

    // ---- 3D tests --------------------------------------------------------
    std::cout << "\n--- 3D Delaunay ---\n\n";

    {
        std::vector<Point3D> pts = {{0,0,0},{1,0,0},{0,1,0},{0,0,1},{0.25,0.25,0.25}};
        Delaunay3D dt(pts, Delaunay3D::Strategy::Optimized);
        auto tets = dt.tetrahedralize();
        int v = verify_delaunay_3d(pts, tets);
        export_obj_3d(pts, tets, "delaunay3d_5pts.obj");
        std::cout << "  5 points  : " << tets.size() << " tets, "
                  << v << " violations "
                  << (v == 0 ? "✓" : "✗") << "\n\n";
    }

    {
        std::srand(54321);
        std::vector<Point3D> pts(100);
        for (auto& p : pts) { p.x = std::rand() / double(RAND_MAX); p.y = std::rand() / double(RAND_MAX); p.z = std::rand() / double(RAND_MAX); }
        Delaunay3D dt(pts, Delaunay3D::Strategy::Optimized);
        auto tets = dt.tetrahedralize();
        int v = verify_delaunay_3d(pts, tets);
        export_obj_3d(pts, tets, "delaunay3d_100.obj");
        std::cout << "  100 random: " << tets.size() << " tets, "
                  << v << " violations "
                  << (v == 0 ? "✓" : "✗") << "\n";
    }

    // ---- Performance benchmark -------------------------------------------
    std::cout << "\n--- Benchmark (best of 3 runs) ---\n\n";

    // 2D benchmarks
    std::cout << "2D Delaunay\n";
    for (int n : {1000, 5000, 10000}) {
        std::srand(n);
        std::vector<Point2D> pts(n);
        for (auto& p : pts) { p.x = std::rand() / double(RAND_MAX); p.y = std::rand() / double(RAND_MAX); }
        std::cout << "  n = " << n << "\n";

        std::vector<Triangle2D> tris_d, tris_o;
        auto t_d = bench([&]() {
            Delaunay2D dt(pts, Delaunay2D::Strategy::Default);
            tris_d = dt.triangulate();
        }, "Default");
        auto t_o = bench([&]() {
            Delaunay2D dt(pts, Delaunay2D::Strategy::Optimized);
            tris_o = dt.triangulate();
        }, "Optimized");

        int vd = verify_delaunay_2d(pts, tris_d);
        int vo = verify_delaunay_2d(pts, tris_o);
        std::cout << "    violations: Default=" << vd << " Optimized=" << vo
                  << (vd == 0 && vo == 0 ? " ✓" : " ✗") << "\n";
        std::cout << "    triangles : " << tris_d.size() << " / " << tris_o.size()
                  << (tris_d.size() == tris_o.size() ? " (match)" : " (diff)")
                  << "\n";

        double speedup = (t_o > 0) ? (t_d / t_o) : 0;
        std::cout << "    speedup  : " << std::fixed << std::setprecision(2)
                  << speedup << "×\n\n";
    }

    // 3D benchmarks
    std::cout << "3D Delaunay\n";
    for (int n : {500, 1000, 2000}) {
        std::srand(n);
        std::vector<Point3D> pts(n);
        for (auto& p : pts) { p.x = std::rand() / double(RAND_MAX); p.y = std::rand() / double(RAND_MAX); p.z = std::rand() / double(RAND_MAX); }
        std::cout << "  n = " << n << "\n";

        std::vector<Tetrahedron> tets_d, tets_o;
        auto t_d = bench([&]() {
            Delaunay3D dt(pts, Delaunay3D::Strategy::Default);
            tets_d = dt.tetrahedralize();
        }, "Default");
        auto t_o = bench([&]() {
            Delaunay3D dt(pts, Delaunay3D::Strategy::Optimized);
            tets_o = dt.tetrahedralize();
        }, "Optimized");

        int vd = verify_delaunay_3d(pts, tets_d);
        int vo = verify_delaunay_3d(pts, tets_o);
        std::cout << "    violations: Default=" << vd << " Optimized=" << vo
                  << (vd == 0 && vo == 0 ? " ✓" : " ✗") << "\n";
        std::cout << "    tetrahedra: " << tets_d.size() << " / " << tets_o.size()
                  << (tets_d.size() == tets_o.size() ? " (match)" : " (diff)")
                  << "\n";

        double speedup = (t_o > 0) ? (t_d / t_o) : 0;
        std::cout << "    speedup  : " << std::fixed << std::setprecision(2)
                  << speedup << "×\n\n";
    }

    std::cout << "=== Done ===\n";
    return 0;
}
