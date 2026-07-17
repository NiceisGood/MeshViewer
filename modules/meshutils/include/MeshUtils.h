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
#include "Delaunay3D.h"  // Point3D, Tetrahedron

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
//  STL export — ASCII format (2D triangles)
// -----------------------------------------------------------------------

/// Write mesh as ASCII STL file (triangles projected onto z=0 plane).
/// Returns true on success.
inline bool write_stl_ascii(const std::vector<Point2D>& pts,
                             const std::vector<Triangle2D>& tris,
                             const std::string& path)
{
    FILE* f = std::fopen(path.c_str(), "w");
    if (!f) return false;

    std::fprintf(f, "solid mesh\n");
    for (const auto& t : tris) {
        const auto& a = pts[t.v0];
        const auto& b = pts[t.v1];
        const auto& c = pts[t.v2];

        // Compute face normal (pointing in +Z for CCW triangles)
        double nx = 0.0, ny = 0.0, nz = 1.0;

        std::fprintf(f, "  facet normal %.6f %.6f %.6f\n", nx, ny, nz);
        std::fprintf(f, "    outer loop\n");
        std::fprintf(f, "      vertex %.15f %.15f 0.0\n", a.x, a.y);
        std::fprintf(f, "      vertex %.15f %.15f 0.0\n", b.x, b.y);
        std::fprintf(f, "      vertex %.15f %.15f 0.0\n", c.x, c.y);
        std::fprintf(f, "    endloop\n");
        std::fprintf(f, "  endfacet\n");
    }
    std::fprintf(f, "endsolid mesh\n");
    std::fclose(f);
    return true;
}

// -----------------------------------------------------------------------
//  STL export — Binary format (2D triangles)
// -----------------------------------------------------------------------

/// Write mesh as binary STL file.
/// Returns true on success.
inline bool write_stl_binary(const std::vector<Point2D>& pts,
                              const std::vector<Triangle2D>& tris,
                              const std::string& path)
{
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;

    // 80-byte header
    char header[80] = {};
    std::snprintf(header, sizeof(header),
                  "Binary STL generated by Hermes delaunay library");
    std::fwrite(header, 1, 80, f);

    // 4-byte triangle count
    uint32_t n = static_cast<uint32_t>(tris.size());
    std::fwrite(&n, sizeof(n), 1, f);

    for (const auto& t : tris) {
        const auto& a = pts[t.v0];
        const auto& b = pts[t.v1];
        const auto& c = pts[t.v2];

        // Normal (z-up)
        float normal[3] = {0.0f, 0.0f, 1.0f};
        std::fwrite(normal, sizeof(float), 3, f);

        float va[3] = {static_cast<float>(a.x), static_cast<float>(a.y), 0.0f};
        float vb[3] = {static_cast<float>(b.x), static_cast<float>(b.y), 0.0f};
        float vc[3] = {static_cast<float>(c.x), static_cast<float>(c.y), 0.0f};
        std::fwrite(va, sizeof(float), 3, f);
        std::fwrite(vb, sizeof(float), 3, f);
        std::fwrite(vc, sizeof(float), 3, f);

        // 2-byte attribute byte count (unused)
        uint16_t attr = 0;
        std::fwrite(&attr, sizeof(attr), 1, f);
    }

    std::fclose(f);
    return true;
}

// -----------------------------------------------------------------------
//  VTK export — 2D triangles (VTK unstructured grid, legacy format)
// -----------------------------------------------------------------------

/// Write mesh as VTK legacy unstructured grid file.
/// Returns true on success.
inline bool write_vtk_2d(const std::vector<Point2D>& pts,
                          const std::vector<Triangle2D>& tris,
                          const std::string& path)
{
    FILE* f = std::fopen(path.c_str(), "w");
    if (!f) return false;

    std::fprintf(f, "# vtk DataFile Version 3.0\n");
    std::fprintf(f, "2D mesh generated by Hermes delaunay library\n");
    std::fprintf(f, "ASCII\n");
    std::fprintf(f, "DATASET UNSTRUCTURED_GRID\n\n");

    std::fprintf(f, "POINTS %zu float\n", pts.size());
    for (const auto& p : pts)
        std::fprintf(f, "%.15f %.15f 0.0\n", p.x, p.y);

    std::fprintf(f, "\nCELLS %zu %zu\n", tris.size(), tris.size() * 4);
    for (const auto& t : tris)
        std::fprintf(f, "3 %d %d %d\n", t.v0, t.v1, t.v2);

    std::fprintf(f, "\nCELL_TYPES %zu\n", tris.size());
    for (size_t i = 0; i < tris.size(); ++i)
        std::fprintf(f, "%d\n", 5);  // VTK_TRIANGLE = 5

    std::fclose(f);
    return true;
}

// -----------------------------------------------------------------------
//  3D export helpers — surface extraction from tetrahedra
// -----------------------------------------------------------------------

namespace detail {

/// Extract surface triangles from a tetrahedral mesh.
/// Returns pairs of (face_triangle, original_tetra_index).
/// Each boundary face appears exactly once.
inline std::vector<Triangle2D> extract_surface(
    const std::vector<Tetrahedron>& tets)
{
    struct FaceKey {
        int v[3];
        int tet_idx;
        bool operator<(const FaceKey& o) const {
            for (int i = 0; i < 3; ++i)
                if (v[i] != o.v[i]) return v[i] < o.v[i];
            return false;
        }
    };

    auto make_key = [](int a, int b, int c, int ti) -> FaceKey {
        int v[3] = {a, b, c};
        std::sort(v, v + 3);
        return {v[0], v[1], v[2], ti};
    };

    std::vector<FaceKey> faces;
    for (size_t ti = 0; ti < tets.size(); ++ti) {
        const auto& t = tets[ti];
        // 4 faces of a tetrahedron
        faces.push_back(make_key(t.v0, t.v2, t.v1, static_cast<int>(ti)));
        faces.push_back(make_key(t.v0, t.v1, t.v3, static_cast<int>(ti)));
        faces.push_back(make_key(t.v1, t.v2, t.v3, static_cast<int>(ti)));
        faces.push_back(make_key(t.v0, t.v3, t.v2, static_cast<int>(ti)));
    }

    std::sort(faces.begin(), faces.end());

    std::vector<Triangle2D> surface;
    for (size_t i = 0; i < faces.size(); ++i) {
        // Check if this face appears exactly once (boundary face)
        bool unique = true;
        if (i > 0) {
            const auto& prev = faces[i - 1];
            if (prev.v[0] == faces[i].v[0] &&
                prev.v[1] == faces[i].v[1] &&
                prev.v[2] == faces[i].v[2]) {
                unique = false;
            }
        }
        if (i + 1 < faces.size()) {
            const auto& next = faces[i + 1];
            if (next.v[0] == faces[i].v[0] &&
                next.v[1] == faces[i].v[1] &&
                next.v[2] == faces[i].v[2]) {
                unique = false;
            }
        }
        if (unique) {
            surface.push_back({faces[i].v[0], faces[i].v[1], faces[i].v[2]});
        }
    }

    return surface;
}

} // namespace detail

// -----------------------------------------------------------------------
//  OBJ export — 3D surface (extracted from tetrahedra)
// -----------------------------------------------------------------------

/// Write 3D surface mesh as Wavefront OBJ file.
/// Extracts boundary surface from tetrahedral mesh.
/// Returns true on success.
inline bool write_obj_3d(const std::vector<Point3D>& pts,
                          const std::vector<Tetrahedron>& tets,
                          const std::string& path)
{
    auto surface = detail::extract_surface(tets);
    if (surface.empty()) return false;

    FILE* f = std::fopen(path.c_str(), "w");
    if (!f) return false;

    std::fprintf(f, "# 3D surface mesh generated by Hermes delaunay library\n");
    std::fprintf(f, "# %zu points, %zu surface triangles\n\n",
                 pts.size(), surface.size());

    for (const auto& p : pts)
        std::fprintf(f, "v  %.15f %.15f %.15f\n", p.x, p.y, p.z);

    std::fprintf(f, "\n");

    for (const auto& t : surface)
        std::fprintf(f, "f  %d %d %d\n",
                     t.v0 + 1, t.v1 + 1, t.v2 + 1);

    std::fclose(f);
    return true;
}

// -----------------------------------------------------------------------
//  STL export — 3D surface (extracted from tetrahedra)
// -----------------------------------------------------------------------

/// Write 3D surface as ASCII STL file.
/// Returns true on success.
inline bool write_stl_ascii_3d(const std::vector<Point3D>& pts,
                                const std::vector<Tetrahedron>& tets,
                                const std::string& path)
{
    auto surface = detail::extract_surface(tets);
    if (surface.empty()) return false;

    FILE* f = std::fopen(path.c_str(), "w");
    if (!f) return false;

    // Compute face normals
    auto cross = [](const Point3D& a, const Point3D& b, const Point3D& c) {
        double ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z;
        double vx = c.x - a.x, vy = c.y - a.y, vz = c.z - a.z;
        double nx = uy * vz - uz * vy;
        double ny = uz * vx - ux * vz;
        double nz = ux * vy - uy * vx;
        double len = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (len > 0) { nx /= len; ny /= len; nz /= len; }
        return std::make_tuple(nx, ny, nz);
    };

    std::fprintf(f, "solid mesh_3d\n");
    for (const auto& t : surface) {
        const auto& a = pts[t.v0];
        const auto& b = pts[t.v1];
        const auto& c = pts[t.v2];
        auto [nx, ny, nz] = cross(a, b, c);
        std::fprintf(f, "  facet normal %.6f %.6f %.6f\n", nx, ny, nz);
        std::fprintf(f, "    outer loop\n");
        std::fprintf(f, "      vertex %.15f %.15f %.15f\n", a.x, a.y, a.z);
        std::fprintf(f, "      vertex %.15f %.15f %.15f\n", b.x, b.y, b.z);
        std::fprintf(f, "      vertex %.15f %.15f %.15f\n", c.x, c.y, c.z);
        std::fprintf(f, "    endloop\n");
        std::fprintf(f, "  endfacet\n");
    }
    std::fprintf(f, "endsolid mesh_3d\n");
    std::fclose(f);
    return true;
}

/// Write 3D surface as binary STL file.
/// Returns true on success.
inline bool write_stl_binary_3d(const std::vector<Point3D>& pts,
                                 const std::vector<Tetrahedron>& tets,
                                 const std::string& path)
{
    auto surface = detail::extract_surface(tets);
    if (surface.empty()) return false;

    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;

    char header[80] = {};
    std::snprintf(header, sizeof(header),
                  "Binary STL 3D surface from Hermes delaunay library");
    std::fwrite(header, 1, 80, f);

    uint32_t n = static_cast<uint32_t>(surface.size());
    std::fwrite(&n, sizeof(n), 1, f);

    auto cross = [](const Point3D& a, const Point3D& b, const Point3D& c,
                     float* n_out) {
        double ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z;
        double vx = c.x - a.x, vy = c.y - a.y, vz = c.z - a.z;
        double nx = uy * vz - uz * vy;
        double ny = uz * vx - ux * vz;
        double nz = ux * vy - uy * vx;
        double len = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (len > 0) { nx /= len; ny /= len; nz /= len; }
        n_out[0] = static_cast<float>(nx);
        n_out[1] = static_cast<float>(ny);
        n_out[2] = static_cast<float>(nz);
    };

    for (const auto& t : surface) {
        float normal[3];
        cross(pts[t.v0], pts[t.v1], pts[t.v2], normal);
        std::fwrite(normal, sizeof(float), 3, f);

        float va[3] = {static_cast<float>(pts[t.v0].x),
                       static_cast<float>(pts[t.v0].y),
                       static_cast<float>(pts[t.v0].z)};
        float vb[3] = {static_cast<float>(pts[t.v1].x),
                       static_cast<float>(pts[t.v1].y),
                       static_cast<float>(pts[t.v1].z)};
        float vc[3] = {static_cast<float>(pts[t.v2].x),
                       static_cast<float>(pts[t.v2].y),
                       static_cast<float>(pts[t.v2].z)};
        std::fwrite(va, sizeof(float), 3, f);
        std::fwrite(vb, sizeof(float), 3, f);
        std::fwrite(vc, sizeof(float), 3, f);

        uint16_t attr = 0;
        std::fwrite(&attr, sizeof(attr), 1, f);
    }

    std::fclose(f);
    return true;
}

// -----------------------------------------------------------------------
//  VTK export — 3D tetrahedra
// -----------------------------------------------------------------------

/// Write 3D tetrahedral mesh as VTK legacy unstructured grid file.
/// Returns true on success.
inline bool write_vtk_3d(const std::vector<Point3D>& pts,
                          const std::vector<Tetrahedron>& tets,
                          const std::string& path)
{
    FILE* f = std::fopen(path.c_str(), "w");
    if (!f) return false;

    std::fprintf(f, "# vtk DataFile Version 3.0\n");
    std::fprintf(f, "3D tetrahedral mesh generated by Hermes delaunay library\n");
    std::fprintf(f, "ASCII\n");
    std::fprintf(f, "DATASET UNSTRUCTURED_GRID\n\n");

    std::fprintf(f, "POINTS %zu float\n", pts.size());
    for (const auto& p : pts)
        std::fprintf(f, "%.15f %.15f %.15f\n", p.x, p.y, p.z);

    std::fprintf(f, "\nCELLS %zu %zu\n", tets.size(), tets.size() * 5);
    for (const auto& t : tets)
        std::fprintf(f, "4 %d %d %d %d\n", t.v0, t.v1, t.v2, t.v3);

    std::fprintf(f, "\nCELL_TYPES %zu\n", tets.size());
    for (size_t i = 0; i < tets.size(); ++i)
        std::fprintf(f, "%d\n", 10);  // VTK_TETRA = 10

    std::fclose(f);
    return true;
}

// -----------------------------------------------------------------------
//  QMesh — custom binary format for 2D triangle meshes (.qmesh)
//
//  Format:
//    Magic:   "QMESH2D\0"  (8 bytes)
//    Version: uint32       (1)
//    Domain:  4 × double   (xmin, ymin, xmax, ymax)
//    #Points: uint32
//    Points:  N × 2 × double (x, y)
//    #Tris:   uint32
//    Tris:    M × 3 × int32  (v0, v1, v2)
// -----------------------------------------------------------------------

/// Write a 2D triangle mesh to custom QMesh binary format.
/// Works with any point type that has .x/.y and any triangle type
/// that has .v0/.v1/.v2 (e.g. QPoint2D/QTriangle2D, Point2D/Triangle2D).
/// Returns true on success.
template<typename PointT, typename TriT>
inline bool write_qmesh(const std::vector<PointT>& pts,
                         const std::vector<TriT>& tris,
                         const std::string& path)
{
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;

    // Magic
    const char magic[] = "QMESH2D";
    std::fwrite(magic, 1, 8, f);

    // Version
    uint32_t version = 1;
    std::fwrite(&version, sizeof(version), 1, f);

    // Domain bounds (computed from points)
    double xmin = 0.0, ymin = 0.0, xmax = 0.0, ymax = 0.0;
    if (!pts.empty()) {
        xmin = xmax = pts[0].x;
        ymin = ymax = pts[0].y;
        for (size_t i = 1; i < pts.size(); ++i) {
            if (pts[i].x < xmin) xmin = pts[i].x;
            if (pts[i].x > xmax) xmax = pts[i].x;
            if (pts[i].y < ymin) ymin = pts[i].y;
            if (pts[i].y > ymax) ymax = pts[i].y;
        }
    }
    std::fwrite(&xmin, sizeof(double), 1, f);
    std::fwrite(&ymin, sizeof(double), 1, f);
    std::fwrite(&xmax, sizeof(double), 1, f);
    std::fwrite(&ymax, sizeof(double), 1, f);

    // Points
    uint32_t np = static_cast<uint32_t>(pts.size());
    std::fwrite(&np, sizeof(np), 1, f);
    for (const auto& p : pts) {
        double xy[2] = {p.x, p.y};
        std::fwrite(xy, sizeof(double), 2, f);
    }

    // Triangles
    uint32_t nt = static_cast<uint32_t>(tris.size());
    std::fwrite(&nt, sizeof(nt), 1, f);
    for (const auto& t : tris) {
        int32_t tri[3] = {t.v0, t.v1, t.v2};
        std::fwrite(tri, sizeof(int32_t), 3, f);
    }

    std::fclose(f);
    return true;
}

// -----------------------------------------------------------------------
//  QMesh3D — custom binary format for 3D tetrahedral meshes (.qmesh3d)
//
//  Format:
//    Magic:   "QMESH3D\0"  (8 bytes)
//    Version: uint32       (1)
//    Domain:  6 × double   (xmin, ymin, zmin, xmax, ymax, zmax)
//    #Points: uint32
//    Points:  N × 3 × double (x, y, z)
//    #Tets:   uint32
//    Tets:    M × 4 × int32  (v0, v1, v2, v3)
// -----------------------------------------------------------------------

/// Write a 3D tetrahedral mesh to custom QMesh3D binary format.
/// Works with any point type that has .x/.y/.z and any tetrahedron type
/// that has .v0/.v1/.v2/.v3 (e.g. OctPoint3D/OctTetrahedron, Point3D/Tetrahedron).
/// Returns true on success.
template<typename PointT, typename TetT>
inline bool write_qmesh_3d(const std::vector<PointT>& pts,
                            const std::vector<TetT>& tets,
                            const std::string& path)
{
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;

    // Magic
    const char magic[] = "QMESH3D";
    std::fwrite(magic, 1, 8, f);

    // Version
    uint32_t version = 1;
    std::fwrite(&version, sizeof(version), 1, f);

    // Domain bounds
    double xmin = 0.0, ymin = 0.0, zmin = 0.0;
    double xmax = 0.0, ymax = 0.0, zmax = 0.0;
    if (!pts.empty()) {
        xmin = xmax = pts[0].x;
        ymin = ymax = pts[0].y;
        zmin = zmax = pts[0].z;
        for (size_t i = 1; i < pts.size(); ++i) {
            if (pts[i].x < xmin) xmin = pts[i].x;
            if (pts[i].x > xmax) xmax = pts[i].x;
            if (pts[i].y < ymin) ymin = pts[i].y;
            if (pts[i].y > ymax) ymax = pts[i].y;
            if (pts[i].z < zmin) zmin = pts[i].z;
            if (pts[i].z > zmax) zmax = pts[i].z;
        }
    }
    std::fwrite(&xmin, sizeof(double), 1, f);
    std::fwrite(&ymin, sizeof(double), 1, f);
    std::fwrite(&zmin, sizeof(double), 1, f);
    std::fwrite(&xmax, sizeof(double), 1, f);
    std::fwrite(&ymax, sizeof(double), 1, f);
    std::fwrite(&zmax, sizeof(double), 1, f);

    // Points
    uint32_t np = static_cast<uint32_t>(pts.size());
    std::fwrite(&np, sizeof(np), 1, f);
    for (const auto& p : pts) {
        double xyz[3] = {p.x, p.y, p.z};
        std::fwrite(xyz, sizeof(double), 3, f);
    }

    // Tetrahedra
    uint32_t nt = static_cast<uint32_t>(tets.size());
    std::fwrite(&nt, sizeof(nt), 1, f);
    for (const auto& t : tets) {
        int32_t tet[4] = {t.v0, t.v1, t.v2, t.v3};
        std::fwrite(tet, sizeof(int32_t), 4, f);
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
