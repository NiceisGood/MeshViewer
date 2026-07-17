#ifndef OCTREE_H
#define OCTREE_H

#include <vector>
#include <cstdint>
#include <functional>
#include <map>
#include <tuple>

// ---- DLL export/import ---------------------------------------------------
#ifdef _WIN32
#  ifdef OCTREE_EXPORTS
#    define OCTREE_API __declspec(dllexport)
#  else
#    define OCTREE_API __declspec(dllimport)
#  endif
#else
#  define OCTREE_API
#endif

// -----------------------------------------------------------------------
// Octree — 3D mesh generation via recursive spatial subdivision.
//
// Builds an octree over a rectangular volume, enforces the 2:1
// neighbour constraint (no adjacent cells differ by more than one
// level), and extracts a conforming tetrahedral mesh using a 6-tet
// decomposition per leaf cube.
//
// Two usage modes:
//   Uniform — pass a criterion that always returns true up to the
//             desired depth (e.g. [](...){ return depth < 4; }).
//   Adaptive — pass a spatially-varying criterion (curvature, error
//              estimate, distance field, etc.).
//
// Output tetrahedra reference indices into the extracted points vector.
// -----------------------------------------------------------------------

struct OCTREE_API OctPoint3D {
    double x, y, z;
};

struct OCTREE_API OctTetrahedron {
    int v0, v1, v2, v3;
};

/// Subdivision criterion (3D).
/// Receives (cell_center_x, cell_center_y, cell_center_z, half_size, depth).
/// Return true to subdivide the cell further.
using OctSubdivideFunc = std::function<bool(double cx, double cy, double cz,
                                            double half_size, int depth)>;

class OCTREE_API Octree {
public:
    /// Construct an octree over the axis-aligned box
    /// [xmin, xmax] × [ymin, ymax] × [zmin, zmax].
    /// max_depth_limit is the hard limit on subdivision depth (default 8).
    Octree(double xmin, double ymin, double zmin,
           double xmax, double ymax, double zmax,
           int max_depth_limit = 8);

    /// Build the tree by recursively subdividing according to `criterion`.
    /// After building, the 2:1 neighbour constraint is enforced.
    void build(const OctSubdivideFunc& criterion);

    /// Extract a conforming tetrahedral mesh from the octree leaves.
    /// Each leaf cube is decomposed into 6 tetrahedra using a consistent
    /// diagonal pattern that ensures face compatibility between neighbours.
    std::vector<OctTetrahedron> tetrahedralize(
        std::vector<OctPoint3D>& points_out) const;

    // ---- Statistics -------------------------------------------------------
    int num_nodes()  const { return num_nodes_;  }
    int num_leaves() const { return num_leaves_; }
    int tree_depth() const { return max_depth_;  }

private:
    struct Node {
        double cx, cy, cz;   // centre
        double hs;           // half-size
        int    depth;
        int    children[8];  // -1 = leaf
    };

    // Child layout: 0=SWL, 1=SEL, 2=NWL, 3=NEL,
    //               4=SWU, 5=SEU, 6=NWU, 7=NEU
    //   S=South(y-), N=North(y+), W=West(x-), E=East(x+),
    //   L=Lower(z-), U=Upper(z+)

    int  add_node(double cx, double cy, double cz, double hs, int depth);
    void build_node(int idx, const OctSubdivideFunc& fn, int limit);
    void enforce_2to1();

    bool is_leaf(int idx) const {
        return nodes_[idx].children[0] == -1;
    }

    /// Find the leaf cell that contains point (x, y, z).
    /// Returns -1 if the point is outside the domain.
    int find_leaf(double x, double y, double z) const;

    /// Return the leaf cell face-adjacent to `node_idx` on the given face.
    /// face: 0=-X(west), 1=+X(east), 2=-Y(south), 3=+Y(north),
    ///       4=-Z(lower), 5=+Z(upper)
    int face_neighbour(int node_idx, int face) const;

    /// Add the 6 tetrahedra for a leaf cube, using the 6-tet
    /// decomposition along the main diagonal.
    /// Vertices are deduplicated via a (x,y,z) → index map.
    /// Returns the first vertex index (for diagnostics).
    int add_cube_tets(int node_idx,
                      std::map<std::tuple<double,double,double>, int>& vtx_map,
                      std::vector<OctPoint3D>& pts_out,
                      std::vector<OctTetrahedron>& tets_out) const;

    std::vector<Node> nodes_;
    int num_nodes_;
    int num_leaves_;
    int max_depth_;
    int max_depth_limit_;

    double xmin_, ymin_, zmin_, xmax_, ymax_, zmax_;
};

#endif // OCTREE_H
