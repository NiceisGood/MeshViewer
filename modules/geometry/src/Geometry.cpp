#include "Geometry.h"
#include "MeshReader.h"  // for MeshData and read functions

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>

// =======================================================================
//  Construction
// =======================================================================

Geometry::Geometry(std::vector<float> verts, std::vector<unsigned int> idxs)
    : vertices_(std::move(verts))
    , indices_(std::move(idxs))
{
}

// =======================================================================
//  Bounding box
// =======================================================================

void Geometry::boundingBox(float& min_x, float& min_y, float& min_z,
                            float& max_x, float& max_y, float& max_z) const
{
    if (vertices_.empty()) {
        min_x = min_y = min_z = 0.0f;
        max_x = max_y = max_z = 0.0f;
        return;
    }
    min_x = max_x = vertices_[0];
    min_y = max_y = vertices_[1];
    min_z = max_z = vertices_[2];
    for (size_t i = 3; i < vertices_.size(); i += 3) {
        float x = vertices_[i];
        float y = vertices_[i + 1];
        float z = vertices_[i + 2];
        if (x < min_x) min_x = x;
        if (y < min_y) min_y = y;
        if (z < min_z) min_z = z;
        if (x > max_x) max_x = x;
        if (y > max_y) max_y = y;
        if (z > max_z) max_z = z;
    }
}

// =======================================================================
//  Info string
// =======================================================================

std::string Geometry::info() const
{
    float min_x, min_y, min_z, max_x, max_y, max_z;
    boundingBox(min_x, min_y, min_z, max_x, max_y, max_z);
    float dx = max_x - min_x;
    float dy = max_y - min_y;
    float dz = max_z - min_z;

    std::ostringstream oss;
    oss << "Vertices: " << numVertices()
        << "  Triangles: " << numTriangles();
    if (!normals_.empty())
        oss << "  Normals: yes";
    else
        oss << "  Normals: no";
    oss << "\n"
        << "BBox: [" << min_x << ", " << min_y << ", " << min_z << "]"
        << "  x "
        << "[" << max_x << ", " << max_y << ", " << max_z << "]"
        << "\n"
        << "Extent: " << dx << " x " << dy << " x " << dz;
    return oss.str();
}

// =======================================================================
//  Bridge to/from MeshData
// =======================================================================

MeshData Geometry::toMeshData() const
{
    MeshData md;
    md.vertices  = vertices_;
    md.indices   = indices_;
    md.normals   = normals_;
    // quad_indices are not stored in Geometry — leave empty
    return md;
}

void Geometry::fromMeshData(const MeshData& md)
{
    vertices_ = md.vertices;
    indices_  = md.indices;
    normals_  = md.normals;
}

// =======================================================================
//  I/O — Import (delegates to MeshReader)
// =======================================================================

bool Geometry::readSTL(const std::string& path)
{
    MeshData md;
    if (!read_stl_ascii(path, md))
        return false;
    fromMeshData(md);
    return true;
}

bool Geometry::readSTLBinary(const std::string& path)
{
    MeshData md;
    if (!read_stl_binary(path, md))
        return false;
    fromMeshData(md);
    return true;
}

bool Geometry::readOBJ(const std::string& path)
{
    MeshData md;
    if (!read_obj(path, md))
        return false;
    fromMeshData(md);
    return true;
}

bool Geometry::readFile(const std::string& path)
{
    // Try with MeshReader's auto-detect first
    MeshData md;
    if (read_mesh(path, md)) {
        fromMeshData(md);
        return true;
    }

    // Fallback: manual extension check
    auto pos = path.rfind('.');
    if (pos == std::string::npos) return false;
    std::string ext = path.substr(pos);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".stl") {
        if (readSTLBinary(path)) return true;
        return readSTL(path);
    } else if (ext == ".obj") {
        return readOBJ(path);
    }
    return false;
}

// =======================================================================
//  I/O — Export
// =======================================================================

bool Geometry::writeSTL(const std::string& path) const
{
    if (empty()) return false;

    FILE* f = std::fopen(path.c_str(), "w");
    if (!f) return false;

    std::fprintf(f, "solid geometry\n");
    for (int i = 0; i < numTriangles(); ++i) {
        unsigned int i0 = indices_[i * 3];
        unsigned int i1 = indices_[i * 3 + 1];
        unsigned int i2 = indices_[i * 3 + 2];

        float ax = vertices_[i0 * 3], ay = vertices_[i0 * 3 + 1], az = vertices_[i0 * 3 + 2];
        float bx = vertices_[i1 * 3], by = vertices_[i1 * 3 + 1], bz = vertices_[i1 * 3 + 2];
        float cx = vertices_[i2 * 3], cy = vertices_[i2 * 3 + 1], cz = vertices_[i2 * 3 + 2];

        // Compute face normal
        float ux = bx - ax, uy = by - ay, uz = bz - az;
        float vx = cx - ax, vy = cy - ay, vz = cz - az;
        float nx = uy * vz - uz * vy;
        float ny = uz * vx - ux * vz;
        float nz = ux * vy - uy * vx;
        float len = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (len > 0.0f) { nx /= len; ny /= len; nz /= len; }

        std::fprintf(f, "  facet normal %.6f %.6f %.6f\n", nx, ny, nz);
        std::fprintf(f, "    outer loop\n");
        std::fprintf(f, "      vertex %.15f %.15f %.15f\n", ax, ay, az);
        std::fprintf(f, "      vertex %.15f %.15f %.15f\n", bx, by, bz);
        std::fprintf(f, "      vertex %.15f %.15f %.15f\n", cx, cy, cz);
        std::fprintf(f, "    endloop\n");
        std::fprintf(f, "  endfacet\n");
    }
    std::fprintf(f, "endsolid geometry\n");
    std::fclose(f);
    return true;
}

bool Geometry::writeSTLBinary(const std::string& path) const
{
    if (empty()) return false;

    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;

    // 80-byte header
    char header[80] = {};
    std::snprintf(header, sizeof(header),
                  "Binary STL generated by Hermes geometry library");
    std::fwrite(header, 1, 80, f);

    // 4-byte triangle count
    uint32_t n = static_cast<uint32_t>(numTriangles());
    std::fwrite(&n, sizeof(n), 1, f);

    for (uint32_t i = 0; i < n; ++i) {
        unsigned int i0 = indices_[i * 3];
        unsigned int i1 = indices_[i * 3 + 1];
        unsigned int i2 = indices_[i * 3 + 2];

        float ax = vertices_[i0 * 3], ay = vertices_[i0 * 3 + 1], az = vertices_[i0 * 3 + 2];
        float bx = vertices_[i1 * 3], by = vertices_[i1 * 3 + 1], bz = vertices_[i1 * 3 + 2];
        float cx = vertices_[i2 * 3], cy = vertices_[i2 * 3 + 1], cz = vertices_[i2 * 3 + 2];

        // Compute face normal
        float ux = bx - ax, uy = by - ay, uz = bz - az;
        float vx = cx - ax, vy = cy - ay, vz = cz - az;
        float nx = uy * vz - uz * vy;
        float ny = uz * vx - ux * vz;
        float nz = ux * vy - uy * vx;
        float len = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (len > 0.0f) { nx /= len; ny /= len; nz /= len; }

        float normal[3] = {nx, ny, nz};
        float va[3] = {ax, ay, az};
        float vb[3] = {bx, by, bz};
        float vc[3] = {cx, cy, cz};

        std::fwrite(normal, sizeof(float), 3, f);
        std::fwrite(va, sizeof(float), 3, f);
        std::fwrite(vb, sizeof(float), 3, f);
        std::fwrite(vc, sizeof(float), 3, f);

        uint16_t attr = 0;
        std::fwrite(&attr, sizeof(attr), 1, f);
    }

    std::fclose(f);
    return true;
}

bool Geometry::writeOBJ(const std::string& path) const
{
    if (empty()) return false;

    FILE* f = std::fopen(path.c_str(), "w");
    if (!f) return false;

    std::fprintf(f, "# Geometry generated by Hermes delaunay library\n");
    std::fprintf(f, "# %d vertices, %d triangles\n\n",
                 numVertices(), numTriangles());

    for (int i = 0; i < numVertices(); ++i)
        std::fprintf(f, "v  %.15f %.15f %.15f\n",
                     vertices_[i * 3], vertices_[i * 3 + 1], vertices_[i * 3 + 2]);

    std::fprintf(f, "\n");

    for (int i = 0; i < numTriangles(); ++i)
        std::fprintf(f, "f  %u %u %u\n",
                     indices_[i * 3] + 1,
                     indices_[i * 3 + 1] + 1,
                     indices_[i * 3 + 2] + 1);

    std::fclose(f);
    return true;
}

bool Geometry::writeFile(const std::string& path) const
{
    auto pos = path.rfind('.');
    if (pos == std::string::npos) return false;

    std::string ext = path.substr(pos);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".stl") {
        // Default to binary STL for export (smaller file)
        return writeSTLBinary(path);
    } else if (ext == ".obj") {
        return writeOBJ(path);
    }
    return false;
}
