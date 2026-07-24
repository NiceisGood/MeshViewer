#include "Delaunay2D.h"
#include "PointCloud.h"
#include "PointCloudUtils.h"
#include <cstdio>
#include <cmath>
#include <unordered_set>
#include <algorithm>

// Quick validation: count overlapping/intersecting triangles by checking
// edge sharing — in a valid Delaunay triangulation every edge should
// appear in either 1 (boundary) or 2 (interior) triangles.
int count_bad_edges(const std::vector<Point2D>& pts,
                    const std::vector<Triangle2D>& tris) {
    struct Edge { int a, b; };
    auto make_edge = [](int a, int b) {
        return Edge{std::min(a, b), std::max(a, b)};
    };
    std::vector<Edge> all_edges;
    for (const auto& t : tris) {
        all_edges.push_back(make_edge(t.v0, t.v1));
        all_edges.push_back(make_edge(t.v1, t.v2));
        all_edges.push_back(make_edge(t.v2, t.v0));
    }
    std::sort(all_edges.begin(), all_edges.end(),
        [](const Edge& a, const Edge& b) {
            if (a.a != b.a) return a.a < b.a;
            return a.b < b.b;
        });
    int bad = 0;
    for (size_t i = 0; i < all_edges.size();) {
        size_t j = i;
        while (j < all_edges.size() &&
               all_edges[j].a == all_edges[i].a &&
               all_edges[j].b == all_edges[i].b)
            ++j;
        int count = static_cast<int>(j - i);
        if (count > 2) bad += (count - 2);
        i = j;
    }
    return bad;
}

int main() {
    // Test with several random point clouds
    int total_bad = 0;
    int total_tris = 0;
    const int runs = 5;

    for (int run = 0; run < runs; ++run) {
        auto pc = PointCloudUtils::generateRandom2D(200, -1.0f, 1.0f, -1.0f, 1.0f, run + 1);

        std::vector<Point2D> pts;
        pts.reserve(pc.numPoints());
        for (int i = 0; i < pc.numPoints(); ++i) {
            float x, y;
            pc.getPoint(i, x, y);
            pts.push_back({static_cast<double>(x), static_cast<double>(y)});
        }

        Delaunay2D delaunay(std::move(pts), Delaunay2D::Strategy::Optimized);
        auto tris = delaunay.triangulate();

        total_tris += static_cast<int>(tris.size());
        int bad = count_bad_edges(delaunay.points(), tris);
        total_bad += bad;

        printf("Run %d: %d pts → %d tris, bad edges: %d\n",
               run + 1, pc.numPoints(), (int)tris.size(), bad);
    }

    printf("\nTotal: %d tris across %d runs, total bad edges: %d\n",
           total_tris, runs, total_bad);

    if (total_bad == 0)
        printf("=== ALL PASSED: No overlapping edges ===\n");
    else
        printf("=== FAILED: Found %d overlapping edges ===\n", total_bad);

    return (total_bad == 0) ? 0 : 1;
}
