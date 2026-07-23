#ifndef GEOMETRY_H
#define GEOMETRY_H

#include <string>
#include <vector>
#include <cstdint>

struct MeshData;  // forward declaration for bridge method

// -----------------------------------------------------------------------
// Geometry — discrete triangle mesh data model with STL/OBJ I/O.
//
// Stores vertices, triangle indices, and optional per-face normals.
// Provides import/export for common discrete geometry file formats (STL, OBJ).
// Acts as the input data source for other computational modules (Delaunay,
// octree, mesh quality analysis, etc.).
// -----------------------------------------------------------------------
class Geometry {
public:
    // ---- Construction ---------------------------------------------------
    Geometry() = default;
    Geometry(const Geometry&) = default;
    Geometry& operator=(const Geometry&) = default;
    Geometry(Geometry&&) noexcept = default;
    Geometry& operator=(Geometry&&) noexcept = default;

    /// Construct from vertex and index arrays (deep copy).
    Geometry(std::vector<float> verts, std::vector<unsigned int> idxs);

    // ---- Data access ----------------------------------------------------
    const std::vector<float>& vertices() const { return vertices_; }
    const std::vector<unsigned int>& indices() const { return indices_; }
    const std::vector<float>& normals() const { return normals_; }

    std::vector<float>& vertices() { return vertices_; }
    std::vector<unsigned int>& indices() { return indices_; }
    std::vector<float>& normals() { return normals_; }

    int numVertices() const { return static_cast<int>(vertices_.size() / 3); }
    int numTriangles() const { return static_cast<int>(indices_.size() / 3); }
    bool empty() const { return vertices_.empty() || indices_.empty(); }

    // ---- Bounding box ---------------------------------------------------
    /// Compute axis-aligned bounding box. Returns {min_x, min_y, min_z, max_x, max_y, max_z}.
    void boundingBox(float& min_x, float& min_y, float& min_z,
                     float& max_x, float& max_y, float& max_z) const;

    // ---- Info string ----------------------------------------------------
    /// Return a human-readable summary string (vertex count, triangle count, bounding box).
    std::string info() const;

    // ---- Clear ----------------------------------------------------------
    void clear() {
        vertices_.clear();
        indices_.clear();
        normals_.clear();
    }

    // ---- Bridge to MeshData (for rendering) -----------------------------
    /// Convert to MeshData for use with MeshRenderer.
    /// The returned MeshData is a snapshot; subsequent Geometry changes
    /// do not affect it.
    MeshData toMeshData() const;

    /// Load from a MeshData (copies data into this Geometry).
    void fromMeshData(const MeshData& md);

    // ---- I/O — Import ---------------------------------------------------
    /// Read an ASCII STL file. Returns true on success.
    bool readSTL(const std::string& path);

    /// Read a binary STL file. Returns true on success.
    bool readSTLBinary(const std::string& path);

    /// Read a Wavefront OBJ file (triangles only). Returns true on success.
    bool readOBJ(const std::string& path);

    /// Auto-detect format by file extension and read. Returns true on success.
    /// Supported: .stl (ASCII/binary), .obj.
    bool readFile(const std::string& path);

    // ---- I/O — Export ---------------------------------------------------
    /// Write as ASCII STL file. Returns true on success.
    bool writeSTL(const std::string& path) const;

    /// Write as binary STL file. Returns true on success.
    bool writeSTLBinary(const std::string& path) const;

    /// Write as Wavefront OBJ file. Returns true on success.
    bool writeOBJ(const std::string& path) const;

    /// Auto-detect format by file extension and write. Returns true on success.
    /// Supported: .stl (binary), .obj.
    bool writeFile(const std::string& path) const;

private:
    std::vector<float> vertices_;          // interleaved x,y,z per vertex
    std::vector<unsigned int> indices_;    // triangle indices (3 per tri)
    std::vector<float> normals_;           // optional per-face normals (3 per tri)
};

#endif // GEOMETRY_H
