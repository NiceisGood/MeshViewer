#include "Octree.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <set>
#include <utility>
#include <vector>

// =======================================================================
//  Construction
// =======================================================================

Octree::Octree(double xmin, double ymin, double zmin,
               double xmax, double ymax, double zmax,
               int max_depth_limit)
    : xmin_(xmin), ymin_(ymin), zmin_(zmin),
      xmax_(xmax), ymax_(ymax), zmax_(zmax),
      max_depth_limit_(max_depth_limit),
      num_nodes_(0), num_leaves_(0), max_depth_(0)
{
    double cx = (xmin + xmax) * 0.5;
    double cy = (ymin + ymax) * 0.5;
    double cz = (zmin + zmax) * 0.5;
    double hs = std::max({xmax - xmin, ymax - ymin, zmax - zmin}) * 0.5;
    add_node(cx, cy, cz, hs, 0);
}

// =======================================================================
//  Node management
// =======================================================================

int Octree::add_node(double cx, double cy, double cz, double hs, int depth)
{
    int idx = static_cast<int>(nodes_.size());
    Node n;
    n.cx = cx; n.cy = cy; n.cz = cz; n.hs = hs; n.depth = depth;
    std::memset(n.children, -1, sizeof(n.children));
    nodes_.push_back(n);
    ++num_nodes_;
    ++num_leaves_;
    if (depth > max_depth_) max_depth_ = depth;
    return idx;
}

// =======================================================================
//  Build — recursive subdivision with user criterion
// =======================================================================

void Octree::build(const OctSubdivideFunc& criterion)
{
    if (nodes_.empty()) return;
    build_node(0, criterion, max_depth_limit_);
    enforce_2to1();
}

void Octree::build_node(int idx, const OctSubdivideFunc& fn, int limit)
{
    const Node& n_ref = nodes_[idx];
    int depth = n_ref.depth;
    double cx = n_ref.cx, cy = n_ref.cy, cz = n_ref.cz, hs = n_ref.hs;

    // Stop if we've hit the depth limit or the criterion says no.
    if (depth >= limit) return;
    if (!fn(cx, cy, cz, hs, depth)) return;

    // Subdivide into 8 children.
    // Layout: 0=SWL, 1=SEL, 2=NWL, 3=NEL,
    //         4=SWU, 5=SEU, 6=NWU, 7=NEU
    double hs2 = hs * 0.5;
    int c0 = add_node(cx - hs2, cy - hs2, cz - hs2, hs2, depth + 1);  // SWL
    int c1 = add_node(cx + hs2, cy - hs2, cz - hs2, hs2, depth + 1);  // SEL
    int c2 = add_node(cx - hs2, cy + hs2, cz - hs2, hs2, depth + 1);  // NWL
    int c3 = add_node(cx + hs2, cy + hs2, cz - hs2, hs2, depth + 1);  // NEL
    int c4 = add_node(cx - hs2, cy - hs2, cz + hs2, hs2, depth + 1);  // SWU
    int c5 = add_node(cx + hs2, cy - hs2, cz + hs2, hs2, depth + 1);  // SEU
    int c6 = add_node(cx - hs2, cy + hs2, cz + hs2, hs2, depth + 1);  // NWU
    int c7 = add_node(cx + hs2, cy + hs2, cz + hs2, hs2, depth + 1);  // NEU

    int ch[8] = {c0, c1, c2, c3, c4, c5, c6, c7};
    std::memcpy(nodes_[idx].children, ch, sizeof(ch));
    --num_leaves_;  // was a leaf, now internal

    // Recurse.
    for (int i = 0; i < 8; ++i)
        build_node(ch[i], fn, limit);
}

// =======================================================================
//  Spatial lookup
// =======================================================================

int Octree::find_leaf(double x, double y, double z) const
{
    // Outside domain?
    if (x < xmin_ || x > xmax_ ||
        y < ymin_ || y > ymax_ ||
        z < zmin_ || z > zmax_)
        return -1;

    int idx = 0;
    while (!is_leaf(idx)) {
        const Node& n = nodes_[idx];
        int child;
        int cx_bit = (x <= n.cx) ? 0 : 1;  // 0=West, 1=East
        int cy_bit = (y <= n.cy) ? 0 : 2;  // 0=South, 2=North
        int cz_bit = (z <= n.cz) ? 0 : 4;  // 0=Lower, 4=Upper
        child = cx_bit | cy_bit | cz_bit;  // produces index 0-7
        idx = n.children[child];
    }
    return idx;
}

// =======================================================================
//  Face neighbour — find the leaf cell adjacent on a given face
// =======================================================================

int Octree::face_neighbour(int node_idx, int face) const
{
    const Node& n = nodes_[node_idx];
    const double eps = 1e-12;

    double qx = n.cx, qy = n.cy, qz = n.cz;
    switch (face) {
    case 0: qx = n.cx - n.hs - eps; break;  // -X (west)
    case 1: qx = n.cx + n.hs + eps; break;  // +X (east)
    case 2: qy = n.cy - n.hs - eps; break;  // -Y (south)
    case 3: qy = n.cy + n.hs + eps; break;  // +Y (north)
    case 4: qz = n.cz - n.hs - eps; break;  // -Z (lower)
    case 5: qz = n.cz + n.hs + eps; break;  // +Z (upper)
    default: return -1;
    }
    return find_leaf(qx, qy, qz);
}

// =======================================================================
//  2:1 neighbour constraint enforcement
//
//  Iteratively subdivide any leaf cell whose face neighbour is more
//  than one level coarser (depth ≥ 2 levels shallower).
// =======================================================================

void Octree::enforce_2to1()
{
    bool changed = true;
    int max_iter = 200;

    while (changed && max_iter-- > 0) {
        changed = false;

        // Snapshot current leaf indices.
        std::vector<int> leaves;
        for (int i = 0; i < static_cast<int>(nodes_.size()); ++i)
            if (is_leaf(i)) leaves.push_back(i);

        for (int li : leaves) {
            if (!is_leaf(li)) continue;

            const Node& n_ref = nodes_[li];
            int n_depth = n_ref.depth;

            // Check all 6 faces.
            for (int f = 0; f < 6; ++f) {
                int nidx = face_neighbour(li, f);
                if (nidx == -1) continue;

                const Node& nb_ref = nodes_[nidx];
                if (!is_leaf(nidx)) continue;

                // If neighbour is 2+ levels coarser, subdivide it.
                if (nb_ref.depth <= n_depth - 2) {
                    double nb_cx = nb_ref.cx, nb_cy = nb_ref.cy, nb_cz = nb_ref.cz;
                    double nb_hs = nb_ref.hs;
                    int nb_depth = nb_ref.depth;

                    double hs2 = nb_hs * 0.5;
                    int c0 = add_node(nb_cx - hs2, nb_cy - hs2, nb_cz - hs2, hs2, nb_depth + 1);
                    int c1 = add_node(nb_cx + hs2, nb_cy - hs2, nb_cz - hs2, hs2, nb_depth + 1);
                    int c2 = add_node(nb_cx - hs2, nb_cy + hs2, nb_cz - hs2, hs2, nb_depth + 1);
                    int c3 = add_node(nb_cx + hs2, nb_cy + hs2, nb_cz - hs2, hs2, nb_depth + 1);
                    int c4 = add_node(nb_cx - hs2, nb_cy - hs2, nb_cz + hs2, hs2, nb_depth + 1);
                    int c5 = add_node(nb_cx + hs2, nb_cy - hs2, nb_cz + hs2, hs2, nb_depth + 1);
                    int c6 = add_node(nb_cx - hs2, nb_cy + hs2, nb_cz + hs2, hs2, nb_depth + 1);
                    int c7 = add_node(nb_cx + hs2, nb_cy + hs2, nb_cz + hs2, hs2, nb_depth + 1);

                    int ch[8] = {c0, c1, c2, c3, c4, c5, c6, c7};
                    std::memcpy(nodes_[nidx].children, ch, sizeof(ch));
                    --num_leaves_;
                    changed = true;
                }
            }
        }
    }
}

// =======================================================================
//  Tetrahedralization — 6-tet decomposition per leaf cube
//
//  Each leaf cube is decomposed into 6 tetrahedra using a consistent
//  main-diagonal pattern.  The diagonal goes from the min corner (SWL)
//  to the max corner (NEU), ensuring face compatibility between
//  adjacent cells.
//
//  For leaf cubes with smaller (finer) neighbours on any face, a
//  virtual 8-sub-cube subdivision is used instead, ensuring conforming
//  faces at level transitions.
//
//  Cube vertices (local):
//    v0 = (cx-hs, cy-hs, cz-hs)  SWL
//    v1 = (cx+hs, cy-hs, cz-hs)  SEL
//    v2 = (cx-hs, cy+hs, cz-hs)  NWL
//    v3 = (cx+hs, cy+hs, cz-hs)  NEL
//    v4 = (cx-hs, cy-hs, cz+hs)  SWU
//    v5 = (cx+hs, cy-hs, cz+hs)  SEU
//    v6 = (cx-hs, cy+hs, cz+hs)  NWU
//    v7 = (cx+hs, cy+hs, cz+hs)  NEU
//
//  6 tetrahedra along diagonal (v0, v7):
//    (v0, v1, v3, v7)  — bottom-front-right
//    (v0, v3, v2, v7)  — bottom-back-left
//    (v0, v7, v5, v1)  — right-front
//    (v0, v5, v7, v4)  — top-front
//    (v0, v4, v7, v6)  — top-back-left
//    (v0, v2, v6, v7)  — left-back
//
//  Each tetrahedron has positive orientation and volume = hs³/6.
//  Total volume per cube = 6 × hs³/6 = hs³ = (2*hs)³/8 = cell volume.
// =======================================================================

void Octree::add_cube_tets_at(double cx, double cy, double cz, double hs,
                               std::map<std::tuple<double,double,double>, int>& vtx_map,
                               std::vector<OctPoint3D>& pts_out,
                               std::vector<OctTetrahedron>& tets_out) const
{
    double x0 = cx - hs, x1 = cx + hs;
    double y0 = cy - hs, y1 = cy + hs;
    double z0 = cz - hs, z1 = cz + hs;

    auto get_vtx = [&](double x, double y, double z) -> int {
        auto key = std::make_tuple(x, y, z);
        auto it = vtx_map.find(key);
        if (it != vtx_map.end()) return it->second;
        int idx = static_cast<int>(pts_out.size());
        pts_out.push_back({x, y, z});
        vtx_map[key] = idx;
        return idx;
    };

    // 8 corner vertices (deduplicated)
    int v0 = get_vtx(x0, y0, z0);  // SWL
    int v1 = get_vtx(x1, y0, z0);  // SEL
    int v2 = get_vtx(x0, y1, z0);  // NWL
    int v3 = get_vtx(x1, y1, z0);  // NEL
    int v4 = get_vtx(x0, y0, z1);  // SWU
    int v5 = get_vtx(x1, y0, z1);  // SEU
    int v6 = get_vtx(x0, y1, z1);  // NWU
    int v7 = get_vtx(x1, y1, z1);  // NEU

    // 6 tetrahedra along diagonal (v0, v7) — all positively oriented
    tets_out.push_back({v0, v1, v3, v7});  // bottom-front-right
    tets_out.push_back({v0, v3, v2, v7});  // bottom-back-left
    tets_out.push_back({v0, v7, v5, v1});  // right-front
    tets_out.push_back({v0, v5, v7, v4});  // top-front
    tets_out.push_back({v0, v4, v7, v6});  // top-back-left
    tets_out.push_back({v0, v2, v6, v7});  // left-back
}

int Octree::add_cube_tets(int node_idx,
                           std::map<std::tuple<double,double,double>, int>& vtx_map,
                           std::vector<OctPoint3D>& pts_out,
                           std::vector<OctTetrahedron>& tets_out) const
{
    const Node& n = nodes_[node_idx];
    add_cube_tets_at(n.cx, n.cy, n.cz, n.hs, vtx_map, pts_out, tets_out);
    return 0;  // first vertex index not needed externally
}

void Octree::add_cube_tets_subdivided(
    int node_idx,
    std::map<std::tuple<double,double,double>, int>& vtx_map,
    std::vector<OctPoint3D>& pts_out,
    std::vector<OctTetrahedron>& tets_out) const
{
    const Node& n = nodes_[node_idx];
    double hs2 = n.hs * 0.5;
    double cx = n.cx, cy = n.cy, cz = n.cz;

    // 8 virtual sub-cubes (same layout as the octree's own children)
    //  0=SWL, 1=SEL, 2=NWL, 3=NEL,
    //  4=SWU, 5=SEU, 6=NWU, 7=NEU
    add_cube_tets_at(cx - hs2, cy - hs2, cz - hs2, hs2, vtx_map, pts_out, tets_out);  // SWL
    add_cube_tets_at(cx + hs2, cy - hs2, cz - hs2, hs2, vtx_map, pts_out, tets_out);  // SEL
    add_cube_tets_at(cx - hs2, cy + hs2, cz - hs2, hs2, vtx_map, pts_out, tets_out);  // NWL
    add_cube_tets_at(cx + hs2, cy + hs2, cz - hs2, hs2, vtx_map, pts_out, tets_out);  // NEL
    add_cube_tets_at(cx - hs2, cy - hs2, cz + hs2, hs2, vtx_map, pts_out, tets_out);  // SWU
    add_cube_tets_at(cx + hs2, cy - hs2, cz + hs2, hs2, vtx_map, pts_out, tets_out);  // SEU
    add_cube_tets_at(cx - hs2, cy + hs2, cz + hs2, hs2, vtx_map, pts_out, tets_out);  // NWU
    add_cube_tets_at(cx + hs2, cy + hs2, cz + hs2, hs2, vtx_map, pts_out, tets_out);  // NEU
}

std::vector<OctTetrahedron> Octree::tetrahedralize(
    std::vector<OctPoint3D>& points_out) const
{
    points_out.clear();

    // Vertex deduplication map: (x,y,z) → index
    std::map<std::tuple<double,double,double>, int> vtx_map;

    // Collect leaf nodes.
    std::vector<int> leaves;
    for (int i = 0; i < static_cast<int>(nodes_.size()); ++i)
        if (is_leaf(i)) leaves.push_back(i);

    // Reserve approximate capacity.
    points_out.reserve(leaves.size() * 8);
    std::vector<OctTetrahedron> tets;
    tets.reserve(leaves.size() * 6);

    for (int li : leaves) {
        const Node& n = nodes_[li];
        int n_depth = n.depth;

        // Check if any face neighbour is finer (depth > current depth).
        // If so, use virtual 8-sub-cube subdivision to ensure face-conforming
        // triangulation at level transitions.
        bool has_finer_neighbour = false;
        for (int f = 0; f < 6; ++f) {
            int nidx = face_neighbour(li, f);
            if (nidx != -1 && is_leaf(nidx) && nodes_[nidx].depth > n_depth) {
                has_finer_neighbour = true;
                break;
            }
        }

        if (has_finer_neighbour)
            add_cube_tets_subdivided(li, vtx_map, points_out, tets);
        else
            add_cube_tets(li, vtx_map, points_out, tets);
    }

    return tets;
}
