#ifndef QUADTREE_H
#define QUADTREE_H

#include <vector>
#include <cstdint>
#include <functional>

// ---- DLL export/import ---------------------------------------------------
#ifdef _WIN32
#  ifdef QUADTREE_EXPORTS
#    define QUADTREE_API __declspec(dllexport)
#  else
#    define QUADTREE_API __declspec(dllimport)
#  endif
#else
#  define QUADTREE_API
#endif

// -----------------------------------------------------------------------
// Quadtree — planar mesh generation via recursive spatial subdivision.
//
// Builds a quadtree over a rectangular domain, enforces the 2:1
// neighbour constraint (no adjacent cells differ by more than one
// level), and extracts a conforming triangle mesh that properly
// handles T‑junctions at transition edges.
//
// Two usage modes:
//   Uniform — pass a criterion that always returns true up to the
//             desired depth (e.g. [](...){ return depth < 4; }).
//   Adaptive — pass a spatially-varying criterion (curvature, error
//              estimate, distance field, etc.).
//
// Output triangles reference indices into the extracted points vector.
// -----------------------------------------------------------------------

struct QUADTREE_API QPoint2D {
    double x, y;
};

struct QUADTREE_API QTriangle2D {
    int v0, v1, v2;
};

/// Subdivision criterion.
/// Receives (cell_center_x, cell_center_y, half_size, depth).
/// Return true to subdivide the cell further.
using QSubdivideFunc = std::function<bool(double cx, double cy,
                                           double half_size, int depth)>;

class QUADTREE_API Quadtree {
public:
    /// Construct a quadtree over the axis-aligned rectangle
    /// [xmin, xmax] × [ymin, ymax].
    /// max_depth_limit is the hard limit on subdivision depth (default 10).
    Quadtree(double xmin, double ymin, double xmax, double ymax,
             int max_depth_limit = 10);

    /// Build the tree by recursively subdividing according to `criterion`.
    /// After building, the 2:1 neighbour constraint is enforced.
    void build(const QSubdivideFunc& criterion);

    /// Extract a conforming triangle mesh from the quadtree leaves.
    /// T‑junctions at transition edges are resolved by adding mid-edge
    /// vertices and triangulating with a centre fan.
    std::vector<QTriangle2D> triangulate(
        std::vector<QPoint2D>& points_out) const;

    // ---- Statistics -------------------------------------------------------
    int num_nodes()  const { return num_nodes_;  }
    int num_leaves() const { return num_leaves_; }
    int tree_depth() const { return max_depth_;  }

private:
    struct Node {
        double cx, cy;   // centre
        double hs;       // half-size (cells span [cx-hs, cx+hs] × [cy-hs, cy+hs])
        int    depth;
        int    children[4];  // -1 = leaf
    };

    int  add_node(double cx, double cy, double hs, int depth);
    void build_node(int idx, const QSubdivideFunc& fn, int limit);
    void enforce_2to1();

    bool is_leaf(int idx) const {
        return nodes_[idx].children[0] == -1;
    }

    /// Find the leaf cell that contains point (x, y).  Returns -1 if
    /// the point is outside the domain.
    int find_leaf(double x, double y) const;

    /// Return the leaf cell adjacent to the edge of `node_idx` at the
    /// given outward-shifted position.  Used for T‑junction detection.
    int edge_neighbour(int node_idx, int edge) const;

    std::vector<Node> nodes_;
    int num_nodes_;
    int num_leaves_;
    int max_depth_;
    int max_depth_limit_;

    double xmin_, ymin_, xmax_, ymax_;
};

#endif // QUADTREE_H
