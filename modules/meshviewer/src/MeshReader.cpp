#include "MeshReader.h"

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <sstream>
#include <string>
#include <unordered_map>

// =======================================================================
//  STL — ASCII
// =======================================================================

bool read_stl_ascii(const std::string& path, MeshData& out)
{
    out.clear();
    FILE* f = std::fopen(path.c_str(), "r");
    if (!f) return false;

    char line[1024];
    bool in_loop = false;
    int loop_vertex_count = 0;

    while (std::fgets(line, sizeof(line), f)) {
        // Trim leading whitespace
        char* s = line;
        while (*s == ' ' || *s == '\t') ++s;

        if (std::strncmp(s, "facet normal", 12) == 0) {
            float nx, ny, nz;
            if (std::sscanf(s, "facet normal %f %f %f", &nx, &ny, &nz) == 3) {
                out.normals.push_back(nx);
                out.normals.push_back(ny);
                out.normals.push_back(nz);
            }
        } else if (std::strncmp(s, "outer loop", 10) == 0) {
            in_loop = true;
            loop_vertex_count = 0;
        } else if (std::strncmp(s, "vertex", 6) == 0 && in_loop) {
            float x, y, z;
            if (std::sscanf(s, "vertex %f %f %f", &x, &y, &z) == 3) {
                out.vertices.push_back(x);
                out.vertices.push_back(y);
                out.vertices.push_back(z);
                ++loop_vertex_count;
            }
        } else if (std::strncmp(s, "endloop", 7) == 0) {
            in_loop = false;
            // Record triangle indices (vertices were added in order)
            if (loop_vertex_count == 3) {
                int base = out.num_vertices() - 3;
                out.indices.push_back(static_cast<unsigned int>(base));
                out.indices.push_back(static_cast<unsigned int>(base + 1));
                out.indices.push_back(static_cast<unsigned int>(base + 2));
            }
        }
    }

    std::fclose(f);
    return !out.empty();
}

// =======================================================================
//  STL — Binary
// =======================================================================

bool read_stl_binary(const std::string& path, MeshData& out)
{
    out.clear();
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;

    // 80-byte header
    char header[80];
    if (std::fread(header, 1, 80, f) != 80) {
        std::fclose(f);
        return false;
    }

    // 4-byte triangle count
    uint32_t num_tris;
    if (std::fread(&num_tris, sizeof(num_tris), 1, f) != 1) {
        std::fclose(f);
        return false;
    }

    // Sanity check: a reasonable STL file won't have more than ~100M triangles
    if (num_tris > 100000000) {
        std::fclose(f);
        return false;
    }

    out.vertices.reserve(num_tris * 3 * 3);
    out.indices.reserve(num_tris * 3);

    for (uint32_t i = 0; i < num_tris; ++i) {
        float normal[3];
        float verts[3][3];
        uint16_t attr;

        if (std::fread(normal, sizeof(float), 3, f) != 3) break;
        if (std::fread(verts[0], sizeof(float), 3, f) != 3) break;
        if (std::fread(verts[1], sizeof(float), 3, f) != 3) break;
        if (std::fread(verts[2], sizeof(float), 3, f) != 3) break;
        if (std::fread(&attr, sizeof(attr), 1, f) != 1) break;

        out.normals.push_back(normal[0]);
        out.normals.push_back(normal[1]);
        out.normals.push_back(normal[2]);

        int base = out.num_vertices();
        out.vertices.push_back(verts[0][0]);
        out.vertices.push_back(verts[0][1]);
        out.vertices.push_back(verts[0][2]);
        out.vertices.push_back(verts[1][0]);
        out.vertices.push_back(verts[1][1]);
        out.vertices.push_back(verts[1][2]);
        out.vertices.push_back(verts[2][0]);
        out.vertices.push_back(verts[2][1]);
        out.vertices.push_back(verts[2][2]);

        out.indices.push_back(static_cast<unsigned int>(base));
        out.indices.push_back(static_cast<unsigned int>(base + 1));
        out.indices.push_back(static_cast<unsigned int>(base + 2));
    }

    std::fclose(f);
    return !out.empty();
}

// =======================================================================
//  OBJ — Wavefront
// =======================================================================

bool read_obj(const std::string& path, MeshData& out)
{
    out.clear();
    FILE* f = std::fopen(path.c_str(), "r");
    if (!f) return false;

    // Temporary storage for OBJ's 1-based indexing
    struct ObjVertex { float x, y, z; };
    std::vector<ObjVertex> obj_verts;
    obj_verts.push_back({0,0,0}); // dummy at index 0

    char line[1024];
    while (std::fgets(line, sizeof(line), f)) {
        char* s = line;
        while (*s == ' ' || *s == '\t') ++s;

        if (s[0] == 'v' && (s[1] == ' ' || s[1] == '\t')) {
            float x, y, z;
            if (std::sscanf(s + 1, "%f %f %f", &x, &y, &z) >= 3) {
                obj_verts.push_back({x, y, z});
            }
        } else if (s[0] == 'f' && (s[1] == ' ' || s[1] == '\t')) {
            // Parse face line: could be "f v1 v2 v3" or "f v1/t1 v2/t2 v3/t3"
            // or "f v1//n1 v2//n2 v3//n3"
            int vi[4] = {0, 0, 0, 0};
            int tokens = 0;
            char* p = s + 1;
            while (*p && tokens < 4) {
                while (*p == ' ' || *p == '\t') ++p;
                if (*p == '\0' || *p == '\n') break;
                int v = 0;
                if (std::sscanf(p, "%d", &v) == 1) {
                    vi[tokens++] = v;
                }
                // Skip past this token (handle v/t/n format)
                while (*p && *p != ' ' && *p != '\t') ++p;
            }

            if (tokens >= 3) {
                // Triangulate: for quads, split into (0,1,2) and (0,2,3)
                // OBJ is 1-based, convert to 0-based
                int v0 = vi[0] - 1;
                int v1 = vi[1] - 1;
                int v2 = vi[2] - 1;
                // Add vertices if not already in the output
                auto add_vert = [&](int vi) {
                    if (vi >= 0 && vi < static_cast<int>(obj_verts.size())) {
                        out.vertices.push_back(obj_verts[vi].x);
                        out.vertices.push_back(obj_verts[vi].y);
                        out.vertices.push_back(obj_verts[vi].z);
                    }
                    return static_cast<unsigned int>(out.num_vertices() - 1);
                };
                out.indices.push_back(add_vert(v0));
                out.indices.push_back(add_vert(v1));
                out.indices.push_back(add_vert(v2));

                if (tokens >= 4) {
                    int v3 = vi[3] - 1;
                    // Need to re-add v0 for the second triangle
                    out.indices.push_back(add_vert(v0));
                    out.indices.push_back(add_vert(v2));
                    out.indices.push_back(add_vert(v3));
                }
            }
        }
    }

    std::fclose(f);
    return !out.empty();
}

// =======================================================================
//  Auto-detect
// =======================================================================

bool read_mesh(const std::string& path, MeshData& out)
{
    // Find extension
    auto pos = path.rfind('.');
    if (pos == std::string::npos) return false;

    std::string ext = path.substr(pos);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".stl") {
        // Try binary first (more common), fall back to ASCII
        if (read_stl_binary(path, out)) return true;
        return read_stl_ascii(path, out);
    } else if (ext == ".obj") {
        return read_obj(path, out);
    } else if (ext == ".qmesh") {
        return read_qmesh(path, out);
    } else if (ext == ".qmesh3d") {
        return read_qmesh_3d(path, out);
    }

    return false;
}

// =======================================================================
//  QMesh3D — custom binary 3D tetrahedral mesh format
// =======================================================================

bool read_qmesh_3d(const std::string& path, MeshData& out)
{
    out.clear();
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;

    // Read magic
    char magic[8];
    if (std::fread(magic, 1, 8, f) != 8) {
        std::fclose(f);
        return false;
    }
    if (std::strncmp(magic, "QMESH3D", 7) != 0) {
        std::fclose(f);
        return false;
    }

    // Read version
    uint32_t version;
    if (std::fread(&version, sizeof(version), 1, f) != 1) {
        std::fclose(f);
        return false;
    }
    if (version != 1) {
        std::fclose(f);
        return false;
    }

    // Skip domain bounds (6 doubles)
    double bounds[6];
    if (std::fread(bounds, sizeof(double), 6, f) != 6) {
        std::fclose(f);
        return false;
    }

    // Read points
    uint32_t num_pts;
    if (std::fread(&num_pts, sizeof(num_pts), 1, f) != 1) {
        std::fclose(f);
        return false;
    }
    if (num_pts > 100000000) {
        std::fclose(f);
        return false;
    }

    out.vertices.reserve(num_pts * 3);
    for (uint32_t i = 0; i < num_pts; ++i) {
        double xyz[3];
        if (std::fread(xyz, sizeof(double), 3, f) != 3) {
            std::fclose(f);
            return false;
        }
        out.vertices.push_back(static_cast<float>(xyz[0]));
        out.vertices.push_back(static_cast<float>(xyz[1]));
        out.vertices.push_back(static_cast<float>(xyz[2]));
    }

    // Read tetrahedra
    uint32_t num_tets;
    if (std::fread(&num_tets, sizeof(num_tets), 1, f) != 1) {
        std::fclose(f);
        return false;
    }
    if (num_tets > 100000000) {
        std::fclose(f);
        return false;
    }

    // Extract surface: collect all tetra faces and keep boundary-only faces
    struct TetFace {
        int v[3];      // sorted indices (for deduplication)
        int orig[3];   // original vertex order (for consistent winding)
    };
    auto make_face = [](int a, int b, int c) -> TetFace {
        int orig[3] = {a, b, c};
        int v[3] = {a, b, c};
        std::sort(v, v + 3);
        return {{v[0], v[1], v[2]}, {orig[0], orig[1], orig[2]}};
    };

    std::vector<TetFace> faces;
    faces.reserve(num_tets * 4);

    for (uint32_t i = 0; i < num_tets; ++i) {
        int32_t tet[4];
        if (std::fread(tet, sizeof(int32_t), 4, f) != 4) {
            std::fclose(f);
            return false;
        }
        // 4 faces of a tetrahedron, stored with original winding
        // for consistent outward-facing surface normals.
        faces.push_back(make_face(tet[0], tet[2], tet[1]));  // opposite v3
        faces.push_back(make_face(tet[0], tet[1], tet[3]));  // opposite v2
        faces.push_back(make_face(tet[1], tet[2], tet[3]));  // opposite v0
        faces.push_back(make_face(tet[0], tet[3], tet[2]));  // opposite v1
    }

    std::fclose(f);

    // Find boundary faces (those that appear exactly once)
    std::sort(faces.begin(), faces.end(), [](const TetFace& a, const TetFace& b) {
        if (a.v[0] != b.v[0]) return a.v[0] < b.v[0];
        if (a.v[1] != b.v[1]) return a.v[1] < b.v[1];
        return a.v[2] < b.v[2];
    });

    out.indices.clear();
    out.indices.reserve(faces.size());

    for (size_t i = 0; i < faces.size(); ++i) {
        bool unique = true;
        if (i > 0) {
            const auto& prev = faces[i - 1];
            if (prev.v[0] == faces[i].v[0] &&
                prev.v[1] == faces[i].v[1] &&
                prev.v[2] == faces[i].v[2])
                unique = false;
        }
        if (i + 1 < faces.size()) {
            const auto& next = faces[i + 1];
            if (next.v[0] == faces[i].v[0] &&
                next.v[1] == faces[i].v[1] &&
                next.v[2] == faces[i].v[2])
                unique = false;
        }
        if (unique) {
            // Use the ORIGINAL vertex order (not sorted) to preserve
            // consistent winding direction from the tetrahedron.
            out.indices.push_back(static_cast<unsigned int>(faces[i].orig[0]));
            out.indices.push_back(static_cast<unsigned int>(faces[i].orig[1]));
            out.indices.push_back(static_cast<unsigned int>(faces[i].orig[2]));
        }
    }

    return !out.empty();
}

// =======================================================================
//  QMesh — custom binary 2D mesh format
// =======================================================================

bool read_qmesh(const std::string& path, MeshData& out)
{
    out.clear();
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;

    // Read magic
    char magic[8];
    if (std::fread(magic, 1, 8, f) != 8) {
        std::fclose(f);
        return false;
    }
    if (std::strncmp(magic, "QMESH2D", 7) != 0) {
        std::fclose(f);
        return false;
    }

    // Read version
    uint32_t version;
    if (std::fread(&version, sizeof(version), 1, f) != 1) {
        std::fclose(f);
        return false;
    }
    if (version != 1) {
        std::fclose(f);
        return false;
    }

    // Skip domain bounds (4 doubles)
    double bounds[4];
    if (std::fread(bounds, sizeof(double), 4, f) != 4) {
        std::fclose(f);
        return false;
    }

    // Read points
    uint32_t num_pts;
    if (std::fread(&num_pts, sizeof(num_pts), 1, f) != 1) {
        std::fclose(f);
        return false;
    }
    if (num_pts > 100000000) {  // sanity check
        std::fclose(f);
        return false;
    }

    out.vertices.reserve(num_pts * 3);
    for (uint32_t i = 0; i < num_pts; ++i) {
        double xy[2];
        if (std::fread(xy, sizeof(double), 2, f) != 2) {
            std::fclose(f);
            return false;
        }
        out.vertices.push_back(static_cast<float>(xy[0]));
        out.vertices.push_back(static_cast<float>(xy[1]));
        out.vertices.push_back(0.0f);  // z = 0 for 2D mesh
    }

    // Read triangles
    uint32_t num_tris;
    if (std::fread(&num_tris, sizeof(num_tris), 1, f) != 1) {
        std::fclose(f);
        return false;
    }
    if (num_tris > 100000000) {
        std::fclose(f);
        return false;
    }

    out.indices.reserve(num_tris * 3);
    for (uint32_t i = 0; i < num_tris; ++i) {
        int32_t tri[3];
        if (std::fread(tri, sizeof(int32_t), 3, f) != 3) {
            std::fclose(f);
            return false;
        }
        out.indices.push_back(static_cast<unsigned int>(tri[0]));
        out.indices.push_back(static_cast<unsigned int>(tri[1]));
        out.indices.push_back(static_cast<unsigned int>(tri[2]));
    }

    std::fclose(f);
    return !out.empty();
}
