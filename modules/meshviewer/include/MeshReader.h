#ifndef MESHREADER_H
#define MESHREADER_H

#include <string>
#include <vector>
#include <cstdint>
#include <map>
#include <algorithm>

// -----------------------------------------------------------------------
// MeshReader — read STL, OBJ, and custom mesh formats into a common
//              triangle-mesh representation.
// -----------------------------------------------------------------------

struct MeshData {
    std::vector<float> vertices;  // interleaved x,y,z per vertex
    std::vector<unsigned int> indices;  // triangle vertex indices (3 per tri)

    // Optional quad faces (4 indices per quad) for octree cell face wireframe
    std::vector<unsigned int> quad_indices;

    // Optional per-face normals (3 floats per triangle, for STL)
    std::vector<float> normals;

    void clear() {
        vertices.clear();
        indices.clear();
        quad_indices.clear();
        normals.clear();
    }

    bool empty() const { return vertices.empty() || indices.empty(); }
    int num_triangles() const { return static_cast<int>(indices.size() / 3); }
    int num_vertices() const { return static_cast<int>(vertices.size() / 3); }
    int num_quads() const { return static_cast<int>(quad_indices.size() / 4); }
};

// -----------------------------------------------------------------------
//  File readers
// -----------------------------------------------------------------------

/// Read an ASCII STL file. Returns true on success.
bool read_stl_ascii(const std::string& path, MeshData& out);

/// Read a binary STL file. Returns true on success.
bool read_stl_binary(const std::string& path, MeshData& out);

/// Read a Wavefront OBJ file (triangles only, no UV/normal).
/// Supports 'v' and 'f' lines. 'f' lines can be "v1 v2 v3" or "v1/t1 v2/t2 v3/t3".
/// Returns true on success.
bool read_obj(const std::string& path, MeshData& out);

/// Auto-detect format by extension and read the file.
/// Supported: .stl (ASCII/binary), .obj, .qmesh, .qmesh3d
/// Returns true on success.
bool read_mesh(const std::string& path, MeshData& out);

/// Read a QMesh binary file (.qmesh) — custom 2D mesh format.
/// The mesh is projected onto the z=0 plane for 3D display.
/// Returns true on success.
bool read_qmesh(const std::string& path, MeshData& out);

/// Read a QMesh3D binary file (.qmesh3d) — custom 3D tetrahedral format.
/// The surface is extracted for display (tetrahedra → boundary triangles).
/// Returns true on success.
bool read_qmesh_3d(const std::string& path, MeshData& out);

/// Read a 2D mesh from our library format (Point2D + Triangle2D vectors).
/// Z is set to 0.
template<typename Point2D, typename Triangle2D>
void read_2d_mesh(const std::vector<Point2D>& pts,
                  const std::vector<Triangle2D>& tris,
                  MeshData& out)
{
    out.clear();
    out.vertices.reserve(pts.size() * 3);
    for (const auto& p : pts) {
        out.vertices.push_back(static_cast<float>(p.x));
        out.vertices.push_back(static_cast<float>(p.y));
        out.vertices.push_back(0.0f);
    }
    out.indices.reserve(tris.size() * 3);
    for (const auto& t : tris) {
        out.indices.push_back(static_cast<unsigned int>(t.v0));
        out.indices.push_back(static_cast<unsigned int>(t.v1));
        out.indices.push_back(static_cast<unsigned int>(t.v2));
    }
}

/// Read a 3D mesh from our library format (Point3D + Tetrahedron vectors).
/// Extracts the surface and returns it as MeshData.
template<typename Point3D, typename Tetrahedron>
void read_3d_mesh(const std::vector<Point3D>& pts,
                  const std::vector<Tetrahedron>& tets,
                  MeshData& out)
{
    out.clear();
    if (pts.empty() || tets.empty()) return;

    // Copy all points (3D)
    out.vertices.reserve(pts.size() * 3);
    for (const auto& p : pts) {
        out.vertices.push_back(static_cast<float>(p.x));
        out.vertices.push_back(static_cast<float>(p.y));
        out.vertices.push_back(static_cast<float>(p.z));
    }

    // Count face occurrences using a sorted-key map
    // Each face is a sorted triple (min, mid, max) of vertex indices
    struct FaceKey {
        int a, b, c;  // sorted: a < b < c
        bool operator<(const FaceKey& o) const {
            if (a != o.a) return a < o.a;
            if (b != o.b) return b < o.b;
            return c < o.c;
        }
    };
    std::map<FaceKey, int> face_count;

    // Helper to add a face
    auto add_face = [&](int i0, int i1, int i2) {
        int v[3] = {i0, i1, i2};
        std::sort(v, v + 3);
        face_count[{v[0], v[1], v[2]}]++;
    };

    for (const auto& tet : tets) {
        int v0 = tet.v0, v1 = tet.v1, v2 = tet.v2, v3 = tet.v3;
        // 4 faces of a tetrahedron
        add_face(v0, v1, v2);
        add_face(v0, v1, v3);
        add_face(v0, v2, v3);
        add_face(v1, v2, v3);
    }

    // Boundary faces are those that appear exactly once
    // (interior faces appear twice — shared by 2 tetrahedra)
    std::vector<unsigned int> tri_indices;
    for (const auto& kv : face_count) {
        if (kv.second == 1) {
            tri_indices.push_back(static_cast<unsigned int>(kv.first.a));
            tri_indices.push_back(static_cast<unsigned int>(kv.first.b));
            tri_indices.push_back(static_cast<unsigned int>(kv.first.c));
        }
    }

    out.indices = std::move(tri_indices);
}

/// Forward declaration for octree bridge.
class Octree;

/// Fill MeshData from an Octree: extracts the surface as triangles (for solid
/// rendering) and the octree cell faces as quads (for quad wireframe display).
/// The quad data populates MeshData::quad_indices so QuadWireframe /
/// QuadWireframeSolid display modes work correctly.
void fill_meshdata_from_octree(const Octree& oct, MeshData& out);

#endif // MESHREADER_H
