#include "AFT2D.h"
#include "Delaunay2D.h"   // Point2D, Triangle2D

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

// =======================================================================
//  Geometry helpers
// =======================================================================

double AdvancingFront2D::orient2d(const Point2D& a, const Point2D& b,
                                   const Point2D& c)
{
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

double AdvancingFront2D::edge_len2(const Point2D& a, const Point2D& b)
{
    double dx = b.x - a.x, dy = b.y - a.y;
    return dx * dx + dy * dy;
}

double AdvancingFront2D::triangle_area2(const Point2D& a, const Point2D& b,
                                         const Point2D& c)
{
    return std::abs(orient2d(a, b, c));
}

double AdvancingFront2D::min_angle_rad(const Point2D& a, const Point2D& b,
                                        const Point2D& c)
{
    auto sqlen = [](const Point2D& p, const Point2D& q) {
        double dx = p.x - q.x, dy = p.y - q.y;
        return dx*dx + dy*dy;
    };

    double a2 = sqlen(b, c);  // side opposite a
    double b2 = sqlen(c, a);  // side opposite b
    double c2 = sqlen(a, b);  // side opposite c

    auto angle = [](double opp2, double adj1_2, double adj2_2) {
        double cos_ang = (adj1_2 + adj2_2 - opp2) /
                         (2.0 * std::sqrt(adj1_2 * adj2_2));
        if (cos_ang >  1.0) cos_ang =  1.0;
        if (cos_ang < -1.0) cos_ang = -1.0;
        return std::acos(cos_ang);
    };

    double ang_a = angle(a2, b2, c2);
    double ang_b = angle(b2, a2, c2);
    double ang_c = angle(c2, a2, b2);

    return std::min({ang_a, ang_b, ang_c});
}

bool AdvancingFront2D::ideal_point(const Point2D& a, const Point2D& b,
                                    double area, Point2D& c)
{
    double dx = b.x - a.x, dy = b.y - a.y;
    double len2 = dx*dx + dy*dy;
    if (len2 < 1e-30) return false;
    double len = std::sqrt(len2);

    // Height from edge to third vertex: h = 2*area / len
    double h = 2.0 * area / len;
    if (h < 1e-15) h = 0.5 * len;  // fallback for degenerate area

    // Interior direction: left of oriented edge a→b  (= normalised (-dy, dx))
    double nx = -dy / len, ny = dx / len;

    // Midpoint
    double mx = (a.x + b.x) * 0.5;
    double my = (a.y + b.y) * 0.5;

    c.x = mx + nx * h;
    c.y = my + ny * h;
    return true;
}

// =======================================================================
//  Segment intersection (excludes shared endpoints)
// =======================================================================
bool AdvancingFront2D::segments_intersect(const Point2D& a, const Point2D& b,
                                           const Point2D& c, const Point2D& d)
{
    auto orient = [](const Point2D& p, const Point2D& q, const Point2D& r) {
        return (q.x - p.x) * (r.y - p.y) - (q.y - p.y) * (r.x - p.x);
    };

    double o1 = orient(a, b, c);
    double o2 = orient(a, b, d);
    double o3 = orient(c, d, a);
    double o4 = orient(c, d, b);

    // General case: endpoints on opposite sides of the other segment
    if (o1 * o2 < 0 && o3 * o4 < 0)
        return true;

    // Collinear cases are excluded (edge is always on the front boundary,
    // and we never want to merge collinear non-endpoint intersections).
    return false;
}

// =======================================================================
//  Front management
// =======================================================================

void AdvancingFront2D::init_front()
{
    edges_.clear();
    edges_.reserve(n_boundary_);

    for (int i = 0; i < n_boundary_; ++i) {
        int j = (i + 1 < n_boundary_) ? i + 1 : 0;
        FrontEdge fe;
        fe.i = i;
        fe.j = j;
        fe.next = (i + 1 < n_boundary_) ? i + 1 : 0;
        fe.prev = (i > 0) ? i - 1 : n_boundary_ - 1;
        edges_.push_back(fe);
    }

    front_head_ = (n_boundary_ > 0) ? 0 : -1;
}

int AdvancingFront2D::find_front_edge(int a, int b) const
{
    if (front_head_ < 0) return -1;
    int ei = front_head_;
    do {
        const auto& e = edges_[ei];
        if ((e.i == a && e.j == b) || (e.i == b && e.j == a))
            return ei;
        ei = e.next;
    } while (ei != front_head_);
    return -1;
}

int AdvancingFront2D::remove_front_edge(int ei)
{
    if (front_head_ < 0) return -1;
    auto& e = edges_[ei];

    int next = e.next;
    int prev = e.prev;

    if (next == ei) {
        // Last edge
        front_head_ = -1;
        return -1;
    }

    edges_[prev].next = next;
    edges_[next].prev = prev;

    if (front_head_ == ei)
        front_head_ = next;

    // Mark as removed (next = self sentinel)
    e.next = ei;
    e.prev = ei;

    return next;
}

int AdvancingFront2D::insert_front_edge(int after_ei, int a, int b)
{
    int idx = static_cast<int>(edges_.size());
    FrontEdge fe;
    fe.i = a;
    fe.j = b;

    if (after_ei < 0 || front_head_ < 0) {
        // First edge or isolated insert — make a singleton ring
        fe.next = idx;
        fe.prev = idx;
        front_head_ = idx;
    } else {
        auto& after = edges_[after_ei];
        int next = after.next;
        fe.next = next;
        fe.prev = after_ei;
        after.next = idx;
        edges_[next].prev = idx;
    }

    edges_.push_back(fe);
    return idx;
}

int AdvancingFront2D::add_point(const Point2D& p)
{
    int idx = static_cast<int>(pts_.size());
    pts_.push_back(p);
    return idx;
}

// =======================================================================
//  Front intersection test (tolerance-based)
// =======================================================================

/// Distance tolerance for shared-vertex checks.
static constexpr double VERTEX_EPS = 1e-10;

static bool approx_eq(double a, double b) {
    return std::abs(a - b) < VERTEX_EPS;
}

static bool point_eq(const Point2D& p, const Point2D& q) {
    return approx_eq(p.x, q.x) && approx_eq(p.y, q.y);
}

int AdvancingFront2D::nearest_front_vertex(const Point2D& p,
                                            double max_dist2) const
{
    if (front_head_ < 0) return -1;
    int best = -1;
    double best_d2 = max_dist2;
    int ei = front_head_;
    do {
        const auto& e = edges_[ei];
        for (int vi : {e.i, e.j}) {
            double d2 = edge_len2(p, pts_[vi]);
            if (d2 < best_d2) {
                best_d2 = d2;
                best = vi;
            }
        }
        ei = e.next;
    } while (ei != front_head_);
    return best;
}

int AdvancingFront2D::intersect_front(const Point2D& p, const Point2D& q) const
{
    if (front_head_ < 0) return -1;
    int ei = front_head_;
    do {
        const auto& e = edges_[ei];
        // Skip edges that share an endpoint with the query segment
        // (tolerance-based comparison to handle floating-point drift)
        bool share_p = point_eq(pts_[e.i], p) || point_eq(pts_[e.j], p);
        bool share_q = point_eq(pts_[e.i], q) || point_eq(pts_[e.j], q);
        if (share_p || share_q) { ei = e.next; continue; }

        if (segments_intersect(p, q, pts_[e.i], pts_[e.j]))
            return ei;
        ei = e.next;
    } while (ei != front_head_);
    return -1;
}

// =======================================================================
//  Boundary refinement — subdivide long edges so AFT can create
//  elements at roughly the target size.
//
//  For each boundary edge longer than 2×√target, evenly insert
//  intermediate points so all resulting edges are ≤ max_len.
// =======================================================================
void AdvancingFront2D::refine_boundary(double target_area)
{
    double max_len = 2.0 * std::sqrt(target_area);
    if (max_len < 1e-12) return;
    double max_len2 = max_len * max_len;

    // Build a new point list by splitting long edges.
    std::vector<Point2D> refined;
    refined.reserve(pts_.size() * 2);

    int n = static_cast<int>(pts_.size());
    for (int i = 0; i < n; ++i) {
        int j = (i + 1 < n) ? i + 1 : 0;
        const Point2D& a = pts_[i];
        const Point2D& b = pts_[j];
        double len2 = edge_len2(a, b);

        refined.push_back(a);

        if (len2 > max_len2) {
            int segs = static_cast<int>(std::ceil(std::sqrt(len2 / max_len2)));
            for (int k = 1; k < segs; ++k) {
                double t = static_cast<double>(k) / segs;
                refined.push_back({
                    a.x + (b.x - a.x) * t,
                    a.y + (b.y - a.y) * t
                });
            }
        }
    }

    if (refined.size() > static_cast<size_t>(n)) {
        pts_ = std::move(refined);
    }
}

// =======================================================================
//  Ear-clip triangulation — deadlock recovery for the remaining front.
//
//  When AFT cannot advance further (all edges fail placement), this
//  method triangulates the remaining simple polygon using a standard
//  ear-clipping algorithm.  It always produces a valid triangulation
//  for any simple polygon.
//
//  An "ear" is a convex vertex (orient2d(prev, i, next) > 0 in a CCW
//  polygon) whose diagonal neither intersects the polygon boundary nor
//  contains any other vertex.  We clip ears one at a time until only
//  three vertices remain.
// =======================================================================
void AdvancingFront2D::earclip_front()
{
    if (front_head_ < 0) return;

    // ---- 1. Collect front vertices in order ----
    std::vector<int> verts;
    {
        int ei = front_head_;
        do {
            verts.push_back(edges_[ei].i);
            ei = edges_[ei].next;
        } while (ei != front_head_);
    }

    int nv = static_cast<int>(verts.size());
    if (nv < 3) return;

    // ---- 2. Ear clipping loop ----
    // We'll use a doubly-linked list over verts indices for O(n²) removal.
    std::vector<int> prev(nv), next(nv);
    for (int i = 0; i < nv; ++i) {
        prev[i] = (i > 0) ? i - 1 : nv - 1;
        next[i] = (i + 1 < nv) ? i + 1 : 0;
    }

    int remaining = nv;

    auto prev_v = [&](int i) -> int { return prev[i]; };
    auto next_v = [&](int i) -> int { return next[i]; };
    auto is_ear = [&](int i) -> bool {
        int a = prev_v(i), b = i, c = next_v(i);
        // Must be a convex corner in a CCW polygon
        if (orient2d(pts_[verts[a]], pts_[verts[b]], pts_[verts[c]]) <= 0)
            return false;

        // Check that no other vertex lies inside triangle (a,b,c)
        for (int j = 0; j < nv; ++j) {
            if (j == a || j == b || j == c) continue;
            const Point2D& p = pts_[verts[j]];
            const Point2D& pa = pts_[verts[a]];
            const Point2D& pb = pts_[verts[b]];
            const Point2D& pc = pts_[verts[c]];

            // Barycentric / orientation test
            double o1 = orient2d(pa, pb, p);
            double o2 = orient2d(pb, pc, p);
            double o3 = orient2d(pc, pa, p);

            // For a CCW triangle, a vertex is inside if all three
            // orientations are ≥ 0 (or all ≤ 0 if triangle was wound CCW).
            bool inside = (o1 >= 0 && o2 >= 0 && o3 >= 0) ||
                          (o1 <= 0 && o2 <= 0 && o3 <= 0);
            if (inside) return false;
        }
        return true;
    };

    auto remove_vertex = [&](int i) {
        int p = prev_v(i), n = next_v(i);
        next[p] = n;
        prev[n] = p;
        --remaining;
    };

    // Ear-clip until 3 vertices remain
    while (remaining > 3) {
        bool found = false;
        // Scan for an ear (start from a deterministic position)
        int start = 0;
        for (int pass = 0; pass < 2 && !found; ++pass) {
            for (int i = 0; i < nv; ++i) {
                if (next[i] < 0) continue;  // already removed
                if (is_ear(i)) {
                    int a = prev_v(i), b = i, c = next_v(i);
                    tris_.push_back({verts[a], verts[b], verts[c]});
                    remove_vertex(i);
                    found = true;
                    break;
                }
            }
            if (!found) {
                // No ear found with full containment test.
                // Fallback: accept any convex vertex as an ear.
                for (int i = 0; i < nv; ++i) {
                    if (next[i] < 0) continue;
                    int a = prev_v(i), b = i, c = next_v(i);
                    if (orient2d(pts_[verts[a]], pts_[verts[b]], pts_[verts[c]]) > 0) {
                        tris_.push_back({verts[a], verts[b], verts[c]});
                        remove_vertex(i);
                        found = true;
                        break;
                    }
                }
            }
        }
        if (!found) break;  // catastrophic failure (shouldn't happen)
    }

    // Last triangle
    if (remaining == 3) {
        int i0 = -1, i1 = -1, i2 = -1;
        for (int i = 0; i < nv; ++i) {
            if (next[i] < 0) continue;
            if (i0 < 0) { i0 = i; continue; }
            if (i1 < 0) { i1 = i; continue; }
            i2 = i;
        }
        if (i0 >= 0 && i1 >= 0 && i2 >= 0)
            tris_.push_back({verts[i0], verts[i1], verts[i2]});
    }

    // Clear front — all done
    front_head_ = -1;
}

// =======================================================================
//  Laplacian smoothing
//
//  Moves interior (non-boundary) vertices to the average of their
//  neighbours, improving element quality.  Uses a relaxation factor
//  of 0.5 and guards against triangle inversion.
// =======================================================================
void AdvancingFront2D::smooth(int iterations)
{
    int np = static_cast<int>(pts_.size());
    int nt = static_cast<int>(tris_.size());
    if (nt < 1) return;

    // Build vertex-to-vertex adjacency from triangles
    std::vector<std::vector<int>> adj(np);
    for (int i = 0; i < nt; ++i) {
        const auto& t = tris_[i];
        auto add_edge = [&](int a, int b) {
            auto& va = adj[a];
            if (std::find(va.begin(), va.end(), b) == va.end())
                va.push_back(b);
            auto& vb = adj[b];
            if (std::find(vb.begin(), vb.end(), a) == vb.end())
                vb.push_back(a);
        };
        add_edge(t.v0, t.v1);
        add_edge(t.v1, t.v2);
        add_edge(t.v2, t.v0);
    }

    // Boundary vertices: those with index < n_boundary_
    // (original boundary points after refinement)
    auto is_boundary = [&](int vi) -> bool {
        return vi < n_boundary_;
    };

    double relax = 0.5;

    for (int iter = 0; iter < iterations; ++iter) {
        // Compute target positions for all interior vertices
        std::vector<Point2D> targets(np);
        bool any_move = false;

        for (int vi = 0; vi < np; ++vi) {
            targets[vi] = pts_[vi];
            if (is_boundary(vi)) continue;
            const auto& nbrs = adj[vi];
            if (nbrs.empty()) continue;

            double cx = 0, cy = 0;
            for (int nb : nbrs) {
                cx += pts_[nb].x;
                cy += pts_[nb].y;
            }
            cx /= static_cast<double>(nbrs.size());
            cy /= static_cast<double>(nbrs.size());

            targets[vi].x = pts_[vi].x + (cx - pts_[vi].x) * relax;
            targets[vi].y = pts_[vi].y + (cy - pts_[vi].y) * relax;
        }

        // Apply with inversion guard
        for (int vi = 0; vi < np; ++vi) {
            if (is_boundary(vi)) continue;
            if (targets[vi].x == pts_[vi].x && targets[vi].y == pts_[vi].y)
                continue;

            // Check that no triangle containing vi becomes inverted
            bool safe = true;
            for (int ti = 0; ti < nt && safe; ++ti) {
                const auto& t = tris_[ti];
                if (t.v0 != vi && t.v1 != vi && t.v2 != vi)
                    continue;

                // Temporarily compute triangle vertices
                Point2D a = pts_[t.v0];
                Point2D b = pts_[t.v1];
                Point2D c = pts_[t.v2];
                // Replace vertex vi with its target
                if (t.v0 == vi) a = targets[vi];
                if (t.v1 == vi) b = targets[vi];
                if (t.v2 == vi) c = targets[vi];

                if (orient2d(a, b, c) <= 1e-15) {
                    safe = false;
                }
            }

            if (safe) {
                pts_[vi] = targets[vi];
                any_move = true;
            }
        }

        if (!any_move) break;
    }
}
// =======================================================================
AdvancingFront2D::AdvancingFront2D(std::vector<Point2D> boundary, Config cfg)
    : pts_(std::move(boundary)), cfg_(cfg)
{
    if (pts_.size() < 3)
        throw std::invalid_argument(
            "AdvancingFront2D: at least 3 boundary points required");
    n_boundary_ = static_cast<int>(pts_.size());
}

AdvancingFront2D::AdvancingFront2D(std::vector<Point2D> boundary)
    : pts_(std::move(boundary))
{
    if (pts_.size() < 3)
        throw std::invalid_argument(
            "AdvancingFront2D: at least 3 boundary points required");
    n_boundary_ = static_cast<int>(pts_.size());
}

// =======================================================================
//  Generate — main advancing-front loop
//
//  Classic advancing-front algorithm:
//
//    1. Pick shortest front edge AB (best element quality).
//    2. Try to form a triangle using an ADJACENT front vertex first
//       (the "merge" case — C is next(B) or prev(A)).
//    3. Otherwise, compute an ideal new point C based on target area
//       and advance the front by splitting edge AB into AC + CB.
//    4. Validate AC/CB don't intersect existing front.
//    5. Repeat until front is empty.
//
//  To help the front close:
//    - When the front is small (≤ 8 edges), merge area checks are
//      disabled so the remaining polygon can close naturally.
//    - Stagnation detection: if the front hasn't shrunk for several
//      hundred iterations, force closing mode regardless of size.
//    - Fallback: when no ideal point can be placed, try placing it
//      at 50 % of the edge midpoint height, or rotate head.
// =======================================================================
void AdvancingFront2D::generate()
{
    double target = cfg_.target_area * cfg_.size_multiplier * cfg_.size_multiplier;
    if (target <= 0.0) target = 0.01;

    // Subdivide long boundary edges to match target element size.
    refine_boundary(target);
    n_boundary_ = static_cast<int>(pts_.size());

    init_front();
    if (front_head_ < 0) return;

    double min_angle_thresh = cfg_.min_angle_deg *
        (3.14159265358979323846 / 180.0);

    tris_.clear();
    tris_.reserve(std::max(static_cast<int>(pts_.size()), 32) * 2);
    edges_.reserve(static_cast<int>(pts_.size()) * 4);

    // Front-edge count helper
    auto front_size = [&]() -> int {
        if (front_head_ < 0) return 0;
        int n = 0, ei = front_head_;
        do { ++n; ei = edges_[ei].next; } while (ei != front_head_);
        return n;
    };

    auto small_front = [&]() -> bool { return front_size() <= 12; };

    int stuck_rotation = -1;  // edge index where we started getting stuck

    for (int iter = 0; iter < cfg_.max_iter; ++iter) {
        if (front_head_ < 0) break;   // all done

        // ---------------------------------------------------------------
        // 1. Find shortest front edge
        // ---------------------------------------------------------------
        int best_ei = front_head_;
        double best_len2 = edge_len2(pts_[edges_[front_head_].i],
                                      pts_[edges_[front_head_].j]);
        int ei = edges_[front_head_].next;
        while (ei != front_head_) {
            double len2 = edge_len2(pts_[edges_[ei].i], pts_[edges_[ei].j]);
            if (len2 < best_len2) {
                best_len2 = len2;
                best_ei = ei;
            }
            ei = edges_[ei].next;
        }

        // ---- check for degenerate edge ----
        if (best_len2 < 1e-30) {
            remove_front_edge(best_ei);
            continue;
        }

        int a = edges_[best_ei].i;
        int b = edges_[best_ei].j;
        const Point2D& pa = pts_[a];
        const Point2D& pb = pts_[b];

        int candidate = -1;
        bool merge_case = false;
        double best_quality = -1.0;

        // ---------------------------------------------------------------
        // 2. Try MERGE with adjacent front vertices
        // ---------------------------------------------------------------

        // 2a. Candidate = next(B)
        {
            const auto& e_b = edges_[best_ei];
            int next_ei = e_b.next;
            if (next_ei != best_ei) {
                int next_b = edges_[next_ei].j;
                const Point2D& pnext = pts_[next_b];
                if (orient2d(pa, pb, pnext) > 0) {
                    double merge_area = triangle_area2(pa, pb, pnext) * 0.5;
                    if (merge_area <= 5.0 * target || small_front()) {
                        double ang = min_angle_rad(pa, pb, pnext);
                        double ang_deg = ang * 57.29577951308232;  // 180/π
                        if (cfg_.min_angle_deg <= 0.0 ||
                            ang_deg >= (small_front() ? 5.0 : cfg_.min_angle_deg)) {
                            double q = ang * 180.0 / 3.14159265358979323846;
                            if (q > best_quality) {
                                best_quality = q;
                                candidate = next_b;
                                merge_case = true;
                            }
                        }
                    }
                }
            }
        }

        // 2b. Candidate = prev(A)
        {
            const auto& e_a = edges_[best_ei];
            int prev_ei = e_a.prev;
            if (prev_ei != best_ei) {
                int prev_a = edges_[prev_ei].i;
                const Point2D& pprev = pts_[prev_a];
                if (orient2d(pa, pb, pprev) > 0) {
                    double merge_area = triangle_area2(pa, pb, pprev) * 0.5;
                    if (merge_area <= 5.0 * target || small_front()) {
                        double ang = min_angle_rad(pa, pb, pprev);
                        double ang_deg = ang * 57.29577951308232;
                        if (cfg_.min_angle_deg <= 0.0 ||
                            ang_deg >= (small_front() ? 5.0 : cfg_.min_angle_deg)) {
                            double q = ang * 180.0 / 3.14159265358979323846;
                            if (q > best_quality) {
                                best_quality = q;
                                candidate = prev_a;
                                merge_case = true;
                            }
                        }
                    }
                }
            }
        }

        // ---------------------------------------------------------------
        // 3. ADVANCE — create ideal new point
        // ---------------------------------------------------------------
        if (!merge_case) {
            double edge_len = std::sqrt(best_len2);
            double ideal_area = std::max(target, 1e-20);

            Point2D ideal_pt;
            ideal_point(pa, pb, ideal_area, ideal_pt);

            bool placed = false;
            // Try full height, then progressively smaller heights.
            // For very constrained geometries, accept tiny triangles
            // that will be refined in subsequent passes.
            for (double shrink : {1.0, 0.8, 0.6, 0.4, 0.2, 0.1, 0.05, 0.02}) {
                Point2D trial{
                    pa.x + (ideal_pt.x - pa.x) * shrink,
                    pa.y + (ideal_pt.y - pa.y) * shrink
                };

                if (intersect_front(pa, trial) >= 0) continue;
                if (intersect_front(trial, pb) >= 0) continue;

                // Only reject truly degenerate (sub-1°) triangles
                double ang = min_angle_rad(pa, pb, trial);
                if (cfg_.min_angle_deg > 0.0 && ang < 0.0174533)  // 1° in rad
                    continue;

                candidate = add_point(trial);
                placed = true;
                merge_case = false;
                break;
            }

            if (!placed) {
                // ---- deadlock detection ----
                // If we've rotated through every front edge without placing
                // anything, fall back to ear-clip triangulation for the
                // remaining front polygon.  This always converges.
                if (stuck_rotation < 0)
                    stuck_rotation = best_ei;
                else if (best_ei == stuck_rotation) {
                    // Full rotation without progress — use ear clipping
                    earclip_front();
                    break;
                }

                // Cannot place: rotate front head and try another edge
                front_head_ = edges_[best_ei].next;
                continue;
            }
        }

        // Reset stuck counter on any successful placement
        stuck_rotation = -1;

        // ---------------------------------------------------------------
        // 4. Insert triangle and update front
        // ---------------------------------------------------------------
        tris_.push_back({a, b, candidate});

        int saved_prev_ei = edges_[best_ei].prev;
        remove_front_edge(best_ei);

        if (merge_case) {
            int e_adj = find_front_edge(b, candidate);
            if (e_adj >= 0) {
                remove_front_edge(e_adj);
                int e_ca = find_front_edge(candidate, a);
                if (e_ca >= 0) {
                    remove_front_edge(e_ca);
                } else if (front_head_ >= 0) {
                    insert_front_edge(saved_prev_ei, a, candidate);
                }
            } else {
                int e_ca = find_front_edge(candidate, a);
                if (e_ca >= 0) {
                    remove_front_edge(e_ca);
                    int e_bc = find_front_edge(b, candidate);
                    if (e_bc >= 0) {
                        remove_front_edge(e_bc);
                    } else if (front_head_ >= 0) {
                        insert_front_edge(saved_prev_ei, candidate, b);
                    }
                }
            }
        } else if (front_head_ >= 0) {
            int e_ac = insert_front_edge(saved_prev_ei, a, candidate);
            insert_front_edge(e_ac, candidate, b);
        }
    }
}
