#include "Quadtree.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <utility>
#include <vector>

// =======================================================================
//  Construction
// =======================================================================

Quadtree::Quadtree(double xmin, double ymin, double xmax, double ymax,
                   int max_depth_limit)
    : xmin_(xmin), ymin_(ymin), xmax_(xmax), ymax_(ymax),
      max_depth_limit_(max_depth_limit),
      num_nodes_(0), num_leaves_(0), max_depth_(0)
{
    double cx = (xmin + xmax) * 0.5;
    double cy = (ymin + ymax) * 0.5;
    double hs = std::max(xmax - xmin, ymax - ymin) * 0.5;
    add_node(cx, cy, hs, 0);
}

// =======================================================================
//  Node management
// =======================================================================

int Quadtree::add_node(double cx, double cy, double hs, int depth)
{
    int idx = static_cast<int>(nodes_.size());
    Node n;
    n.cx = cx; n.cy = cy; n.hs = hs; n.depth = depth;
    n.children[0] = n.children[1] = n.children[2] = n.children[3] = -1;
    nodes_.push_back(n);
    ++num_nodes_;
    ++num_leaves_;
    if (depth > max_depth_) max_depth_ = depth;
    return idx;
}

// =======================================================================
//  Build — recursive subdivision with user criterion
// =======================================================================

void Quadtree::build(const QSubdivideFunc& criterion)
{
    if (nodes_.empty()) return;
    build_node(0, criterion, max_depth_limit_);
    enforce_2to1();
}

void Quadtree::build_node(int idx, const QSubdivideFunc& fn, int limit)
{
    const Node& n_ref = nodes_[idx];
    int depth = n_ref.depth;
    double cx = n_ref.cx, cy = n_ref.cy, hs = n_ref.hs;

    // Stop if we've hit the depth limit or the criterion says no.
    if (depth >= limit) return;
    if (!fn(cx, cy, hs, depth)) return;

    // Subdivide into 4 children.
    // Layout: 0 = SW, 1 = SE, 2 = NW, 3 = NE.
    double hs2 = hs * 0.5;
    int c0 = add_node(cx - hs2, cy - hs2, hs2, depth + 1);  // SW
    int c1 = add_node(cx + hs2, cy - hs2, hs2, depth + 1);  // SE
    int c2 = add_node(cx - hs2, cy + hs2, hs2, depth + 1);  // NW
    int c3 = add_node(cx + hs2, cy + hs2, hs2, depth + 1);  // NE

    nodes_[idx].children[0] = c0;
    nodes_[idx].children[1] = c1;
    nodes_[idx].children[2] = c2;
    nodes_[idx].children[3] = c3;
    --num_leaves_;  // was a leaf, now internal

    // Recurse.
    build_node(c0, fn, limit);
    build_node(c1, fn, limit);
    build_node(c2, fn, limit);
    build_node(c3, fn, limit);
}

// =======================================================================
//  Spatial lookup
// =======================================================================

int Quadtree::find_leaf(double x, double y) const
{
    // Outside domain?
    if (x < xmin_ || x > xmax_ || y < ymin_ || y > ymax_)
        return -1;

    int idx = 0;
    while (!is_leaf(idx)) {
        const Node& n = nodes_[idx];
        int child;
        if (x <= n.cx) {
            child = (y <= n.cy) ? 0 : 2;  // SW or NW
        } else {
            child = (y <= n.cy) ? 1 : 3;  // SE or NE
        }
        idx = n.children[child];
    }
    return idx;
}

// =======================================================================
//  Edge neighbour — find the leaf cell just outside a given edge
// =======================================================================

int Quadtree::edge_neighbour(int node_idx, int edge) const
{
    const Node& n = nodes_[node_idx];

    // Shift a query point just outside the cell along the given edge.
    // edge: 0 = south, 1 = east, 2 = north, 3 = west
    const double eps = 1e-12;
    double qx, qy;
    switch (edge) {
    case 0: // south  — just below the south edge centre
        qx = n.cx;
        qy = n.cy - n.hs - eps;
        break;
    case 1: // east   — just right of the east edge centre
        qx = n.cx + n.hs + eps;
        qy = n.cy;
        break;
    case 2: // north  — just above the north edge centre
        qx = n.cx;
        qy = n.cy + n.hs + eps;
        break;
    case 3: // west   — just left of the west edge centre
        qx = n.cx - n.hs - eps;
        qy = n.cy;
        break;
    default:
        return -1;
    }
    return find_leaf(qx, qy);
}

// =======================================================================
//  2:1 neighbour constraint enforcement
//
//  Iteratively subdivide any leaf cell whose edge neighbour is more
//  than one level coarser (depth ≥ 2 levels shallower).  This prevents
//  cells from differing by more than one level across any edge.
// =======================================================================

void Quadtree::enforce_2to1()
{
    bool changed = true;
    int max_iter = 100;

    while (changed && max_iter-- > 0) {
        changed = false;

        // Snapshot current leaf indices (list may grow as we subdivide).
        std::vector<int> leaves;
        for (int i = 0; i < static_cast<int>(nodes_.size()); ++i)
            if (is_leaf(i)) leaves.push_back(i);

        for (int li : leaves) {
            if (!is_leaf(li)) continue;  // may have been subdivided

            // Copy values before any add_node() calls (which may reallocate nodes_).
            const Node& n_ref = nodes_[li];
            int n_depth = n_ref.depth;

            // Check all 4 edges.
            for (int e = 0; e < 4; ++e) {
                int nidx = edge_neighbour(li, e);
                if (nidx == -1) continue;  // outside domain

                const Node& nb_ref = nodes_[nidx];
                if (!is_leaf(nidx)) continue;

                // If neighbour is 2+ levels coarser, subdivide it.
                if (nb_ref.depth <= n_depth - 2) {
                    // Copy all needed values before any add_node call.
                    double nb_cx = nb_ref.cx, nb_cy = nb_ref.cy;
                    double nb_hs = nb_ref.hs;
                    int nb_depth = nb_ref.depth;

                    double hs2 = nb_hs * 0.5;
                    int c0 = add_node(nb_cx - hs2, nb_cy - hs2, hs2, nb_depth + 1);
                    int c1 = add_node(nb_cx + hs2, nb_cy - hs2, hs2, nb_depth + 1);
                    int c2 = add_node(nb_cx - hs2, nb_cy + hs2, hs2, nb_depth + 1);
                    int c3 = add_node(nb_cx + hs2, nb_cy + hs2, hs2, nb_depth + 1);
                    nodes_[nidx].children[0] = c0;
                    nodes_[nidx].children[1] = c1;
                    nodes_[nidx].children[2] = c2;
                    nodes_[nidx].children[3] = c3;
                    --num_leaves_;
                    changed = true;
                }
            }
        }
    }
}

// =======================================================================
//  Triangulate — extract a conforming triangle mesh from the quadtree
//
//  For each leaf cell we detect T‑junctions on its 4 edges (caused by
//  finer neighbours at depth+1).  When a T‑junction is present, a
//  mid‑edge vertex is inserted.  The cell is then triangulated using a
//  centre fan: the cell centre connects to each edge segment, producing
//  4-8 triangles per cell depending on the T‑junction pattern.
//
//  Vertices are deduplicated via an (x,y) → index map so the mesh is
//  watertight across cell boundaries.
// =======================================================================

std::vector<QTriangle2D> Quadtree::triangulate(
    std::vector<QPoint2D>& points_out) const
{
    points_out.clear();

    // Vertex deduplication map.
    struct PointKey {
        double x, y;
        bool operator<(const PointKey& o) const {
            if (x != o.x) return x < o.x;
            return y < o.y;
        }
    };
    std::map<PointKey, int> vertex_map;

    auto get_vertex = [&](double x, double y) -> int {
        PointKey k{x, y};
        auto it = vertex_map.find(k);
        if (it != vertex_map.end()) return it->second;
        int idx = static_cast<int>(points_out.size());
        points_out.push_back({x, y});
        vertex_map[k] = idx;
        return idx;
    };

    std::vector<QTriangle2D> triangles;

    // Collect leaf nodes.
    std::vector<int> leaves;
    for (int i = 0; i < static_cast<int>(nodes_.size()); ++i)
        if (is_leaf(i)) leaves.push_back(i);

    for (int li : leaves) {
        const Node& n = nodes_[li];
        double h = n.hs;
        double x0 = n.cx - h, x1 = n.cx + h;
        double y0 = n.cy - h, y1 = n.cy + h;

        // Corner vertices.
        int v_sw = get_vertex(x0, y0);
        int v_se = get_vertex(x1, y0);
        int v_ne = get_vertex(x1, y1);
        int v_nw = get_vertex(x0, y1);

        // Detect T‑junctions on each edge.
        // A T‑junction exists when the edge neighbour is exactly one
        // level finer (depth == our depth + 1).
        bool t_junction[4] = {false, false, false, false};
        int nb[4];
        nb[0] = edge_neighbour(li, 0);  // south
        nb[1] = edge_neighbour(li, 1);  // east
        nb[2] = edge_neighbour(li, 2);  // north
        nb[3] = edge_neighbour(li, 3);  // west

        for (int e = 0; e < 4; ++e) {
            if (nb[e] != -1 && is_leaf(nb[e]) && nodes_[nb[e]].depth == n.depth + 1)
                t_junction[e] = true;
        }

        int t_count = 0;
        for (int e = 0; e < 4; ++e)
            if (t_junction[e]) ++t_count;

        if (t_count == 0) {
            // No T‑junctions — simple 2-triangle split.
            triangles.push_back({v_sw, v_se, v_nw});
            triangles.push_back({v_se, v_ne, v_nw});
        } else {
            // Centre vertex for fan triangulation.
            int v_c = get_vertex(n.cx, n.cy);

            // Mid-edge vertices where T‑junctions exist.
            int v_ms = t_junction[0] ? get_vertex(n.cx, y0) : -1;  // south
            int v_me = t_junction[1] ? get_vertex(x1, n.cy) : -1;  // east
            int v_mn = t_junction[2] ? get_vertex(n.cx, y1) : -1;  // north
            int v_mw = t_junction[3] ? get_vertex(x0, n.cy) : -1;  // west

            // South edge: SW → (MS) → SE → centre
            if (t_junction[0]) {
                triangles.push_back({v_sw, v_ms, v_c});
                triangles.push_back({v_ms, v_se, v_c});
            } else {
                triangles.push_back({v_sw, v_se, v_c});
            }

            // East edge: SE → (ME) → NE → centre
            if (t_junction[1]) {
                triangles.push_back({v_se, v_me, v_c});
                triangles.push_back({v_me, v_ne, v_c});
            } else {
                triangles.push_back({v_se, v_ne, v_c});
            }

            // North edge: NE → (MN) → NW → centre
            if (t_junction[2]) {
                triangles.push_back({v_ne, v_mn, v_c});
                triangles.push_back({v_mn, v_nw, v_c});
            } else {
                triangles.push_back({v_ne, v_nw, v_c});
            }

            // West edge: NW → (MW) → SW → centre
            if (t_junction[3]) {
                triangles.push_back({v_nw, v_mw, v_c});
                triangles.push_back({v_mw, v_sw, v_c});
            } else {
                triangles.push_back({v_nw, v_sw, v_c});
            }
        }
    }

    return triangles;
}
