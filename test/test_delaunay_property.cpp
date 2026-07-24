#include "Delaunay2D.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <cstdint>

// -----------------------------------------------------------------------
//  Circumcircle test (same formula as Delaunay2D::circumcircle_test)
// -----------------------------------------------------------------------
static double incircle(const Point2D& a, const Point2D& b,
                       const Point2D& c, const Point2D& d)
{
    double abx = b.x - a.x, aby = b.y - a.y;
    double acx = c.x - a.x, acy = c.y - a.y;
    double adx = d.x - a.x, ady = d.y - a.y;
    double bx2 = abx * abx + aby * aby;
    double cx2 = acx * acx + acy * acy;
    double dx2 = adx * adx + ady * ady;
    double det = abx * (acy * dx2 - cx2 * ady) -
                 aby * (acx * dx2 - cx2 * adx) +
                 bx2 * (acx * ady - acy * adx);
    return det;
}

// -----------------------------------------------------------------------
//  Verify the Delaunay property: for each triangle, no other point lies
//  inside its circumcircle (det > 0 means inside for CCW triangle).
// -----------------------------------------------------------------------
static int count_violations(const std::vector<Point2D>& pts,
                            const std::vector<Triangle2D>& tris)
{
    int violations = 0;
    int n = static_cast<int>(pts.size());
    int nt = static_cast<int>(tris.size());

    for (int ti = 0; ti < nt; ++ti) {
        const auto& t = tris[ti];
        int i0 = t.v0, i1 = t.v1, i2 = t.v2;

        // Ensure CCW winding for correct sign convention
        const Point2D& p0 = pts[i0];
        const Point2D& p1 = pts[i1];
        const Point2D& p2 = pts[i2];
        double orient = (p1.x - p0.x) * (p2.y - p0.y) - (p1.y - p0.y) * (p2.x - p0.x);
        if (orient < 0.0)
            std::swap(i1, i2);

        const Point2D& pa = pts[i0];
        const Point2D& pb = pts[i1];
        const Point2D& pc = pts[i2];

        for (int pi = 0; pi < n; ++pi) {
            if (pi == t.v0 || pi == t.v1 || pi == t.v2)
                continue;
            // incircle returns raw determinant (not negated).
            // For CCW (a,b,c): det < 0  → pi is INSIDE circumcircle
            //                   det > 0  → pi is OUTSIDE
            double det = incircle(pa, pb, pc, pts[pi]);
            if (det < -1e-10) {
                ++violations;
                if (violations <= 5) {
                    printf("  VIOLATION tri[%d] (%d,%d,%d): pt %d inside "
                           "(det=%.2e)\n",
                           ti, t.v0, t.v1, t.v2, pi, det);
                }
            }
        }
    }
    return violations;
}

int main()
{
    printf("=== Delaunay2D property verification ===\n\n");

    const int runs = 5;
    int total_violations = 0;
    int total_tris = 0;

    for (int run = 0; run < runs; ++run) {
        // Generate random points
        int n = 100 + run * 50;
        std::vector<Point2D> pts;
        pts.reserve(n);

        // Use a fixed seed for reproducibility
        std::srand(42 + run * 13);
        for (int i = 0; i < n; ++i) {
            double x = (std::rand() / (double)RAND_MAX) * 2.0 - 1.0;
            double y = (std::rand() / (double)RAND_MAX) * 2.0 - 1.0;
            pts.push_back({x, y});
        }

        // Test Default strategy
        {
            Delaunay2D delaunay(pts, Delaunay2D::Strategy::Default);
            auto tris = delaunay.triangulate();
            int v = count_violations(delaunay.points(), tris);
            total_violations += v;
            total_tris += static_cast<int>(tris.size());
            printf("Default [%d pts]: %d tris, %d violations", n, (int)tris.size(), v);
            if (v == 0) printf("  OK\n");
            else        printf("  FAIL\n");
        }

        // Test Optimized strategy
        {
            Delaunay2D delaunay(pts, Delaunay2D::Strategy::Optimized);
            auto tris = delaunay.triangulate();
            int v = count_violations(delaunay.points(), tris);
            printf("Optimized [%d pts]: %d tris, %d violations", n, (int)tris.size(), v);
            if (v == 0) printf("  OK\n");
            else        printf("  FAIL\n");
        }
    }

    printf("\n=== Summary ===\n");
    printf("Total triangles: %d\n", total_tris);
    printf("Total violations: %d\n", total_violations);
    printf("Result: %s\n", total_violations == 0 ? "ALL PASS" : "FAILED");
    return (total_violations == 0) ? 0 : 1;
}
