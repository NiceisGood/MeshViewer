#include "Delaunay3D.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <utility>
#include <vector>
#include <cstdint>

// =======================================================================
//  Common helpers
// =======================================================================

void Delaunay3D::bounding_box(double& xmin, double& ymin, double& zmin,
                               double& xmax, double& ymax, double& zmax) const
{
    xmin = ymin = zmin = std::numeric_limits<double>::max();
    xmax = ymax = zmax = std::numeric_limits<double>::lowest();
    for (const auto& p : pts_) {
        if (p.x < xmin) xmin = p.x;
        if (p.x > xmax) xmax = p.x;
        if (p.y < ymin) ymin = p.y;
        if (p.y > ymax) ymax = p.y;
        if (p.z < zmin) zmin = p.z;
        if (p.z > zmax) zmax = p.z;
    }
}

double Delaunay3D::orient3d(const Point3D& a, const Point3D& b,
                             const Point3D& c, const Point3D& d)
{
    double abx = b.x - a.x, aby = b.y - a.y, abz = b.z - a.z;
    double acx = c.x - a.x, acy = c.y - a.y, acz = c.z - a.z;
    double adx = d.x - a.x, ady = d.y - a.y, adz = d.z - a.z;
    double cx = aby * acz - abz * acy;
    double cy = abz * acx - abx * acz;
    double cz = abx * acy - aby * acx;
    return cx * adx + cy * ady + cz * adz;
}

double Delaunay3D::circumsphere_test(
    const Point3D& a, const Point3D& b, const Point3D& c,
    const Point3D& d0, const Point3D& d)
{
    double bx = b.x - a.x, by = b.y - a.y, bz = b.z - a.z;
    double cx = c.x - a.x, cy = c.y - a.y, cz = c.z - a.z;
    double dx = d0.x - a.x, dy = d0.y - a.y, dz = d0.z - a.z;
    double ex = d.x - a.x,  ey = d.y - a.y,  ez = d.z - a.z;

    double b2 = bx*bx + by*by + bz*bz;
    double c2 = cx*cx + cy*cy + cz*cz;
    double d2 = dx*dx + dy*dy + dz*dz;
    double e2 = ex*ex + ey*ey + ez*ez;

    double m1 = cx * (dy * ez - dz * ey) -
                cy * (dx * ez - dz * ex) +
                cz * (dx * ey - dy * ex);
    double m2 = bx * (dy * ez - dz * ey) -
                by * (dx * ez - dz * ex) +
                bz * (dx * ey - dy * ex);
    double m3 = bx * (cy * ez - cz * ey) -
                by * (cx * ez - cz * ex) +
                bz * (cx * ey - cy * ex);
    double m4 = bx * (cy * dz - cz * dy) -
                by * (cx * dz - cz * dx) +
                bz * (cx * dy - cy * dx);

    return -b2 * m1 + c2 * m2 - d2 * m3 + e2 * m4;
}

Delaunay3D::SuperTetrahedron Delaunay3D::build_super_tetrahedron()
{
    double xmin, ymin, zmin, xmax, ymax, zmax;
    bounding_box(xmin, ymin, zmin, xmax, ymax, zmax);

    double dx = (xmax - xmin) * 0.5 + 1.0;
    double dy = (ymax - ymin) * 0.5 + 1.0;
    double dz = (zmax - zmin) * 0.5 + 1.0;
    double cx = (xmin + xmax) * 0.5;
    double cy = (ymin + ymax) * 0.5;
    double cz = (zmin + zmax) * 0.5;
    double r = std::max({dx, dy, dz}) * 20.0;

    Point3D p0{ cx - r,       cy - r,       cz - r };
    Point3D p1{ cx + r,       cy + r,       cz - r };
    Point3D p2{ cx - r,       cy + r,       cz + r };
    Point3D p3{ cx + r,       cy - r,       cz + r };

    int i0 = static_cast<int>(pts_.size());
    int i1 = i0 + 1;
    int i2 = i0 + 2;
    int i3 = i0 + 3;
    pts_.push_back(p0); pts_.push_back(p1);
    pts_.push_back(p2); pts_.push_back(p3);
    return {i0, i1, i2, i3};
}

bool Delaunay3D::uses_super_vertex(
    int tet_idx, const std::vector<Tetrahedron>& tets,
    const SuperTetrahedron& st) const
{
    const auto& t = tets[tet_idx];
    return (t.v0 == st.i0 || t.v0 == st.i1 || t.v0 == st.i2 || t.v0 == st.i3 ||
            t.v1 == st.i0 || t.v1 == st.i1 || t.v1 == st.i2 || t.v1 == st.i3 ||
            t.v2 == st.i0 || t.v2 == st.i1 || t.v2 == st.i2 || t.v2 == st.i3 ||
            t.v3 == st.i0 || t.v3 == st.i1 || t.v3 == st.i2 || t.v3 == st.i3);
}

// -----------------------------------------------------------------------
//  Face helpers (for boundary extraction)
// -----------------------------------------------------------------------
struct Face3D { int p0, p1, p2; };
struct FaceKey {
    int p0, p1, p2; // sorted
    bool operator==(const FaceKey& o) const noexcept {
        return p0 == o.p0 && p1 == o.p1 && p2 == o.p2;
    }
};
struct FaceKeyHash {
    std::size_t operator()(const FaceKey& f) const noexcept {
        return static_cast<std::size_t>(f.p0) ^
               (static_cast<std::size_t>(f.p1) << 10) ^
               (static_cast<std::size_t>(f.p2) << 20);
    }
};
static FaceKey make_face_key(int a, int b, int c) {
    if (a > b) std::swap(a, b);
    if (b > c) std::swap(b, c);
    if (a > b) std::swap(a, b);
    return {a, b, c};
}
static void tet_faces(const Tetrahedron& t, Face3D f[4]) {
    f[0] = {t.v0, t.v1, t.v2};
    f[1] = {t.v0, t.v1, t.v3};
    f[2] = {t.v0, t.v2, t.v3};
    f[3] = {t.v1, t.v2, t.v3};
}

// =======================================================================
//  Constructor
// =======================================================================
Delaunay3D::Delaunay3D(std::vector<Point3D> points, Strategy strategy)
    : pts_(std::move(points)), strat_(strategy)
{
    if (pts_.size() < 4)
        throw std::invalid_argument(
            "Delaunay3D: at least 4 non-coplanar points are required");
}

// =======================================================================
//  3D Hilbert curve sort (Morton → Hilbert, 10 bits/axis)
// =======================================================================

static inline uint32_t spread3d(uint32_t v) {
    v = (v | (v << 16)) & 0x030000FF;
    v = (v | (v << 8))  & 0x0300F00F;
    v = (v | (v << 4))  & 0x030C30C3;
    v = (v | (v << 2))  & 0x09249249;
    return v;
}

static uint32_t morton3d(uint16_t x, uint16_t y, uint16_t z) {
    return spread3d(x) | (spread3d(y) << 1) | (spread3d(z) << 2);
}

/// 3D Morton (Z-order) code with 10 bits/axis = 30-bit key.
/// This provides good spatial locality and is much simpler than
/// a full 3D Hilbert curve while achieving similar cache
/// performance for Delaunay point insertion.

uint64_t Delaunay3D::hilbert_index_3d(double x, double y, double z,
                                       int num_bits)
{
    if (x < 0) x = 0; if (x > 1) x = 1;
    if (y < 0) y = 0; if (y > 1) y = 1;
    if (z < 0) z = 0; if (z > 1) z = 1;
    uint16_t xi = static_cast<uint16_t>(x * ((1u << num_bits) - 1));
    uint16_t yi = static_cast<uint16_t>(y * ((1u << num_bits) - 1));
    uint16_t zi = static_cast<uint16_t>(z * ((1u << num_bits) - 1));
    // Return 3D Morton (Z-order) code — simpler and sufficient.
    return morton3d(xi, yi, zi);
}

// =======================================================================
//  Default strategy — plain Bowyer-Watson
// =======================================================================
std::vector<Tetrahedron> Delaunay3D::tetrahedralize_default()
{
    SuperTetrahedron st = build_super_tetrahedron();
    std::vector<Tetrahedron> tets;
    tets.push_back({st.i0, st.i1, st.i2, st.i3});
    if (orient3d(pts_[st.i0], pts_[st.i1], pts_[st.i2], pts_[st.i3]) < 0.0)
        std::swap(tets.back().v2, tets.back().v3);

    int nin = static_cast<int>(pts_.size()) - 4;

    for (int pi = 0; pi < nin; ++pi) {
        const Point3D& pt = pts_[pi];

        std::vector<int> bad;
        for (int ti = 0; ti < static_cast<int>(tets.size()); ++ti) {
            const auto& t = tets[ti];
            if (circumsphere_test(pts_[t.v0], pts_[t.v1], pts_[t.v2], pts_[t.v3], pt) < -1e-12)
                bad.push_back(ti);
        }

        std::unordered_map<FaceKey, Face3D, FaceKeyHash> face_map;
        for (int bi : bad) {
            Face3D faces[4]; tet_faces(tets[bi], faces);
            for (auto& f : faces) {
                FaceKey fk = make_face_key(f.p0, f.p1, f.p2);
                auto it = face_map.find(fk);
                if (it != face_map.end()) face_map.erase(it);
                else                      face_map[fk] = f;
            }
        }

        std::sort(bad.begin(), bad.end());
        for (auto it = bad.rbegin(); it != bad.rend(); ++it)
            tets.erase(tets.begin() + *it);

        for (const auto& [key, face] : face_map) {
            int ti = static_cast<int>(tets.size());
            tets.push_back({face.p0, face.p1, face.p2, pi});
            auto& t = tets[ti];
            double vol = orient3d(pts_[t.v0], pts_[t.v1], pts_[t.v2], pts_[t.v3]);
            if (vol < 0.0) std::swap(t.v2, t.v3);
        }
    }

    std::vector<Tetrahedron> result;
    result.reserve(tets.size());
    for (int ti = 0; ti < static_cast<int>(tets.size()); ++ti)
        if (!uses_super_vertex(ti, tets, st))
            result.push_back(tets[ti]);

    for (auto& t : result) {
        double vol = orient3d(pts_[t.v0], pts_[t.v1], pts_[t.v2], pts_[t.v3]);
        if (vol < 0.0) std::swap(t.v2, t.v3);
    }
    return result;
}

// =======================================================================
//  Optimized strategy — Hilbert sort + breadcrumb + pre-allocation
// =======================================================================
std::vector<Tetrahedron> Delaunay3D::tetrahedralize_optimized()
{
    // ---- 0. Hilbert (Morton Z-order) sort ----
    double xmin0, ymin0, zmin0, xmax0, ymax0, zmax0;
    bounding_box(xmin0, ymin0, zmin0, xmax0, ymax0, zmax0);
    double sx0 = (xmax0 > xmin0) ? (1.0 / (xmax0 - xmin0)) : 1.0;
    double sy0 = (ymax0 > ymin0) ? (1.0 / (ymax0 - ymin0)) : 1.0;
    double sz0 = (zmax0 > zmin0) ? (1.0 / (zmax0 - zmin0)) : 1.0;
    int n_orig = static_cast<int>(pts_.size());
    std::vector<std::pair<uint64_t, int>> ord(n_orig);
    for (int i = 0; i < n_orig; ++i)
        ord[i] = {hilbert_index_3d((pts_[i].x - xmin0) * sx0,
                                    (pts_[i].y - ymin0) * sy0,
                                    (pts_[i].z - zmin0) * sz0, 10), i};
    std::sort(ord.begin(), ord.end());
    std::vector<Point3D> sorted(n_orig);
    std::vector<int> perm(n_orig);
    for (int i = 0; i < n_orig; ++i) { sorted[i] = pts_[ord[i].second]; perm[i] = ord[i].second; }
    pts_ = std::move(sorted);

    // ---- 1. Super tetrahedron ----
    SuperTetrahedron st = build_super_tetrahedron();
    int nin = static_cast<int>(pts_.size()) - 4;
    int est = std::max(4, 6 * nin + 10);

    // ---- 2. Pre-allocate + initial tet ----
    std::vector<Tetrahedron> tets;
    tets.reserve(est);
    tets.push_back({st.i0, st.i1, st.i2, st.i3});
    if (orient3d(pts_[st.i0], pts_[st.i1], pts_[st.i2], pts_[st.i3]) < 0.0)
        std::swap(tets.back().v2, tets.back().v3);
    int hot_start = 0;

    // ---- 3. Insert with breadcrumb ----
    for (int pi = 0; pi < nin; ++pi) {
        const Point3D& pt = pts_[pi];

        std::vector<int> bad;
        int nt = static_cast<int>(tets.size());

        for (int ti = hot_start; ti < nt; ++ti) {
            const auto& t = tets[ti];
            if (circumsphere_test(pts_[t.v0], pts_[t.v1], pts_[t.v2], pts_[t.v3], pt) < -1e-12)
                bad.push_back(ti);
        }
        for (int ti = 0; ti < hot_start; ++ti) {
            const auto& t = tets[ti];
            if (circumsphere_test(pts_[t.v0], pts_[t.v1], pts_[t.v2], pts_[t.v3], pt) < -1e-12)
                bad.push_back(ti);
        }

        if (!bad.empty()) hot_start = bad[0];

        // Boundary faces (winding-preserving map).
        std::unordered_map<FaceKey, Face3D, FaceKeyHash> face_map;
        for (int bi : bad) {
            Face3D faces[4]; tet_faces(tets[bi], faces);
            for (auto& f : faces) {
                FaceKey fk = make_face_key(f.p0, f.p1, f.p2);
                auto it = face_map.find(fk);
                if (it != face_map.end()) face_map.erase(it);
                else                      face_map[fk] = f;
            }
        }

        // Remove bad tets.
        std::sort(bad.begin(), bad.end());
        int removed_before_hot = 0;
        for (auto it = bad.rbegin(); it != bad.rend(); ++it) {
            if (*it < hot_start) ++removed_before_hot;
            tets.erase(tets.begin() + *it);
        }
        hot_start -= removed_before_hot;
        if (hot_start < 0) hot_start = 0;

        // Stitch new point to boundary faces.
        for (const auto& [key, face] : face_map) {
            int ti = static_cast<int>(tets.size());
            tets.push_back({face.p0, face.p1, face.p2, pi});
            auto& t = tets[ti];
            double vol = orient3d(pts_[t.v0], pts_[t.v1], pts_[t.v2], pts_[t.v3]);
            if (vol < 0.0) std::swap(t.v2, t.v3);
        }
    }

    // ---- 4. Remove super-vertex tets ----
    std::vector<Tetrahedron> result;
    result.reserve(tets.size());
    for (int ti = 0; ti < static_cast<int>(tets.size()); ++ti)
        if (!uses_super_vertex(ti, tets, st))
            result.push_back(tets[ti]);

    // ---- 5. Orientation + remap ----
    for (auto& t : result) {
        double vol = orient3d(pts_[t.v0], pts_[t.v1], pts_[t.v2], pts_[t.v3]);
        if (vol < 0.0) std::swap(t.v2, t.v3);
        t.v0 = perm[t.v0];
        t.v1 = perm[t.v1];
        t.v2 = perm[t.v2];
        t.v3 = perm[t.v3];
    }

    // ---- 6. Restore pts_ to original order ----
    // After Morton sort, pts_[0..n_orig-1] are in sorted order but
    // tetrahedron indices have been remapped to original input index
    // space.  Restore pts_ so pts_[i] = original point i.
    {
        std::vector<Point3D> orig_order(n_orig);
        for (int i = 0; i < n_orig; ++i)
            orig_order[perm[i]] = pts_[i];
        for (int i = 0; i < n_orig; ++i)
            pts_[i] = orig_order[i];
        // Super-tetrahedron vertices at pts_[n_orig..n_orig+3] stay unchanged.
    }
    return result;
}

// =======================================================================
//  Dispatch
// =======================================================================
std::vector<Tetrahedron> Delaunay3D::tetrahedralize()
{
    switch (strat_) {
    case Strategy::Default:   return tetrahedralize_default();
    case Strategy::Optimized: return tetrahedralize_optimized();
    }
    return tetrahedralize_default();
}
