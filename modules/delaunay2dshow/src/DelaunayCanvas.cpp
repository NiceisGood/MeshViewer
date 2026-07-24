#include "DelaunayCanvas.h"
#include "Delaunay2D.h"

#include <QPainter>
#include <QMouseEvent>
#include <QPen>
#include <QBrush>
#include <QtMath>

#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// =======================================================================
//  Construction / Destruction
// =======================================================================

DelaunayCanvas::DelaunayCanvas(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(500, 400);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    // Light gray background
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(230, 230, 230));
    setPalette(pal);
    setAutoFillBackground(true);
}

DelaunayCanvas::~DelaunayCanvas() = default;

// =======================================================================
//  Point management
// =======================================================================

void DelaunayCanvas::addPoint(const QPointF& pt)
{
    // Map canvas pixel to algorithm space (normalised ~[-1,1])
    double w = static_cast<double>(width());
    double h = static_cast<double>(height());
    double x = (pt.x() / w) * 2.0 - 1.0;
    double y = -((pt.y() / h) * 2.0 - 1.0);  // flip Y

    points_.push_back({x, y});
    retriangulate();
    emit pointsChanged(pointCount());
    update();
}

void DelaunayCanvas::clearPoints()
{
    points_.clear();
    triangles_.clear();
    delaunay_.reset();
    update();
    emit pointsChanged(0);
}

// =======================================================================
//  Triangulation
// =======================================================================

void DelaunayCanvas::retriangulate()
{
    triangles_.clear();
    if (points_.size() < 3) {
        delaunay_.reset();
        return;
    }

    try {
        auto pts_copy = points_;  // Delaunay2D takes ownership
        delaunay_ = std::make_unique<Delaunay2D>(
            std::move(pts_copy), Delaunay2D::Strategy::Optimized);
        triangles_ = delaunay_->triangulate();
    } catch (const std::exception&) {
        delaunay_.reset();
        triangles_.clear();
    }
}

// =======================================================================
//  Coordinate mapping
// =======================================================================

QPointF DelaunayCanvas::toScreen(const Point2D& pt) const
{
    double w = static_cast<double>(width());
    double h = static_cast<double>(height());
    double sx = (pt.x + 1.0) * 0.5 * w;
    double sy = (1.0 - pt.y) * 0.5 * h;  // flip Y
    return QPointF(static_cast<float>(sx), static_cast<float>(sy));
}

// =======================================================================
//  Paint
// =======================================================================

void DelaunayCanvas::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    drawTriangles(p);
    drawPoints(p);
    if (show_circumcircles_) drawCircumcircles(p);
    if (show_voronoi_)       drawVoronoi(p);
}

// =======================================================================
//  Drawing helpers
// =======================================================================

void DelaunayCanvas::drawPoints(QPainter& p)
{
    if (points_.empty()) return;

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(40, 80, 180));  // dark blue

    double r = 4.0;
    for (const auto& pt : points_) {
        QPointF sp = toScreen(pt);
        p.drawEllipse(sp, static_cast<float>(r), static_cast<float>(r));
    }
}

void DelaunayCanvas::drawTriangles(QPainter& p)
{
    // Draw edges for 1-2 points
    if (points_.size() == 2) {
        QPointF a = toScreen(points_[0]);
        QPointF b = toScreen(points_[1]);
        p.setPen(QPen(QColor(60, 60, 60), 2.0));
        p.drawLine(a, b);
        return;
    }

    if (triangles_.empty()) return;

    // Draw filled triangles with thin edges
    p.setPen(QPen(QColor(40, 40, 40), 1.5));
    p.setBrush(QColor(180, 210, 255, 120));  // light blue fill

    for (const auto& tri : triangles_) {
        QPointF a = toScreen(points_[tri.v0]);
        QPointF b = toScreen(points_[tri.v1]);
        QPointF c = toScreen(points_[tri.v2]);

        QPolygonF poly;
        poly << a << b << c;
        p.drawPolygon(poly);
    }
}

// =======================================================================
//  Circumcircle
// =======================================================================

QPointF DelaunayCanvas::circumcenter(const QPointF& a, const QPointF& b,
                                      const QPointF& c) const
{
    double d = 2.0 * (a.x() * (b.y() - c.y()) +
                      b.x() * (c.y() - a.y()) +
                      c.x() * (a.y() - b.y()));
    if (std::abs(d) < 1e-12) return (a + b + c) / 3.0;  // degenerate

    double ax2 = a.x() * a.x() + a.y() * a.y();
    double bx2 = b.x() * b.x() + b.y() * b.y();
    double cx2 = c.x() * c.x() + c.y() * c.y();

    double ux = (ax2 * (b.y() - c.y()) +
                 bx2 * (c.y() - a.y()) +
                 cx2 * (a.y() - b.y())) / d;
    double uy = (ax2 * (c.x() - b.x()) +
                 bx2 * (a.x() - c.x()) +
                 cx2 * (b.x() - a.x())) / d;
    return QPointF(static_cast<float>(ux), static_cast<float>(uy));
}

double DelaunayCanvas::circumradius(const QPointF& a, const QPointF& b,
                                     const QPointF& c) const
{
    QPointF cc = circumcenter(a, b, c);
    double dx = a.x() - cc.x();
    double dy = a.y() - cc.y();
    return std::sqrt(dx * dx + dy * dy);
}

void DelaunayCanvas::drawCircumcircles(QPainter& p)
{
    if (points_.empty()) return;

    p.setPen(QPen(QColor(220, 60, 40), 1.2));  // red outline

    if (points_.size() == 1) {
        // Single point: circle from point straight down to canvas bottom
        QPointF sp = toScreen(points_[0]);
        double r = static_cast<double>(height()) - sp.y();
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(sp, static_cast<float>(r), static_cast<float>(r));
        return;
    }

    if (points_.size() == 2) {
        // Two points: diameter circle
        QPointF a = toScreen(points_[0]);
        QPointF b = toScreen(points_[1]);
        QPointF center = (a + b) / 2.0;
        double dx = a.x() - b.x();
        double dy = a.y() - b.y();
        double r = std::sqrt(dx * dx + dy * dy) / 2.0;
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(center, static_cast<float>(r), static_cast<float>(r));
        return;
    }

    // 3+ points: circumcircle of each triangle
    p.setBrush(QColor(220, 60, 40, 30));  // very faint red fill
    for (const auto& tri : triangles_) {
        QPointF a = toScreen(points_[tri.v0]);
        QPointF b = toScreen(points_[tri.v1]);
        QPointF c = toScreen(points_[tri.v2]);
        QPointF cc = circumcenter(a, b, c);
        double r = circumradius(a, b, c);
        p.drawEllipse(cc, static_cast<float>(r), static_cast<float>(r));
    }
}

// =======================================================================
//  Voronoi diagram (from Delaunay triangulation)
// =======================================================================

void DelaunayCanvas::drawVoronoi(QPainter& p)
{
    if (points_.size() < 2) return;

    p.setPen(QPen(QColor(0, 150, 0), 1.0, Qt::DashLine));  // green dashed

    if (points_.size() == 2) {
        // Perpendicular bisector — drawn in screen space, which is correct
        // because the bisector of two screen-space points is visually correct.
        QPointF a = toScreen(points_[0]);
        QPointF b = toScreen(points_[1]);
        QPointF mid = (a + b) / 2.0;
        double dx = b.x() - a.x();
        double dy = b.y() - a.y();
        double len = std::max(width(), height()) * 2.0;
        if (std::abs(dx) < 1e-6f && std::abs(dy) < 1e-6f) return;
        double nx = -dy, ny = dx;
        double nl = std::sqrt(nx * nx + ny * ny);
        nx = nx / nl * len;
        ny = ny / nl * len;
        p.drawLine(QPointF(mid.x() - static_cast<float>(nx),
                            mid.y() - static_cast<float>(ny)),
                   QPointF(mid.x() + static_cast<float>(nx),
                            mid.y() + static_cast<float>(ny)));
        return;
    }

    // 3+ points: Voronoi computed in algorithm space, then mapped to screen.
    if (triangles_.empty()) return;

    // ── Algorithm‑space helpers ──
    auto circumcenter_algo = [](const Point2D& a, const Point2D& b,
                                 const Point2D& c) -> QPointF {
        double ax = a.x, ay = a.y;
        double bx = b.x, by = b.y;
        double cx = c.x, cy = c.y;
        double d = 2.0 * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));
        if (std::abs(d) < 1e-12)
            return QPointF(static_cast<float>((ax + bx + cx) / 3.0),
                           static_cast<float>((ay + by + cy) / 3.0));
        double a2 = ax * ax + ay * ay;
        double b2 = bx * bx + by * by;
        double c2 = cx * cx + cy * cy;
        double ux = (a2 * (by - cy) + b2 * (cy - ay) + c2 * (ay - by)) / d;
        double uy = (a2 * (cx - bx) + b2 * (ax - cx) + c2 * (bx - ax)) / d;
        return QPointF(static_cast<float>(ux), static_cast<float>(uy));
    };

    auto algo_to_screen = [&](const QPointF& p_algo) -> QPointF {
        double w = static_cast<double>(width());
        double h = static_cast<double>(height());
        double sx = (p_algo.x() + 1.0) * 0.5 * w;
        double sy = (1.0 - p_algo.y()) * 0.5 * h;
        return QPointF(static_cast<float>(sx), static_cast<float>(sy));
    };

    // Clip a ray from origin O in direction (dx, dy) to the algorithm‑space
    // square [-1,1]×[-1,1].  Returns the intersection point, or O if none.
    auto clip_ray = [&](const QPointF& O, double dx, double dy) -> QPointF {
        const double ext = 2.0;  // well beyond [-1,1]
        double ox = static_cast<double>(O.x());
        double oy = static_cast<double>(O.y());
        // Extend ray far enough
        double t_max = ext / (std::abs(dx) + std::abs(dy) + 1e-12);
        auto clip_axis = [&](double o, double d, double bound) {
            if (std::abs(d) < 1e-12) return;
            double t = (bound - o) / d;
            if (t > 0 && t < t_max) t_max = t;
        };
        clip_axis(ox, dx, -1.0);
        clip_axis(ox, dx,  1.0);
        clip_axis(oy, dy, -1.0);
        clip_axis(oy, dy,  1.0);
        return QPointF(static_cast<float>(ox + dx * t_max),
                       static_cast<float>(oy + dy * t_max));
    };

    // ── Build edge‑adjacency map (indices are algorithm‑space point indices) ──
    struct EdgeKey { int a, b; };
    auto make_key = [](int a, int b) {
        return EdgeKey{std::min(a, b), std::max(a, b)};
    };
    auto edge_less = [](const EdgeKey& x, const EdgeKey& y) {
        if (x.a != y.a) return x.a < y.a;
        return x.b < y.b;
    };

    // Circumcenters in algorithm space
    int nt = static_cast<int>(triangles_.size());
    std::vector<QPointF> circs_algo(nt);
    for (int i = 0; i < nt; ++i) {
        const auto& tri = triangles_[i];
        circs_algo[i] = circumcenter_algo(
            points_[tri.v0], points_[tri.v1], points_[tri.v2]);
    }

    // Edge → triangle map
    std::vector<std::pair<EdgeKey, int>> edge_tri;
    edge_tri.reserve(nt * 3);
    for (int i = 0; i < nt; ++i) {
        const auto& tri = triangles_[i];
        edge_tri.push_back({make_key(tri.v0, tri.v1), i});
        edge_tri.push_back({make_key(tri.v1, tri.v2), i});
        edge_tri.push_back({make_key(tri.v2, tri.v0), i});
    }
    std::sort(edge_tri.begin(), edge_tri.end(),
              [&](const auto& x, const auto& y) {
                  return edge_less(x.first, y.first);
              });

    // ── Draw Voronoi edges ──
    for (size_t i = 0; i < edge_tri.size();) {
        size_t j = i;
        while (j < edge_tri.size() &&
               edge_tri[j].first.a == edge_tri[i].first.a &&
               edge_tri[j].first.b == edge_tri[i].first.b)
            ++j;
        size_t count = j - i;

        if (count == 2) {
            // Interior edge: connect the two algorithm‑space circumcenters
            int t1 = edge_tri[i].second;
            int t2 = edge_tri[i + 1].second;
            p.drawLine(algo_to_screen(circs_algo[t1]),
                       algo_to_screen(circs_algo[t2]));
        } else if (count == 1) {
            // Boundary edge on convex hull: ray from circumcenter outward.
            int a_idx = edge_tri[i].first.a;
            int b_idx = edge_tri[i].first.b;
            int tri_idx = edge_tri[i].second;
            const auto& tri = triangles_[tri_idx];

            // Third vertex not in this edge
            int c_idx = -1;
            if (tri.v0 != a_idx && tri.v0 != b_idx) c_idx = tri.v0;
            else if (tri.v1 != a_idx && tri.v1 != b_idx) c_idx = tri.v1;
            else c_idx = tri.v2;

            const Point2D& A = points_[a_idx];
            const Point2D& B = points_[b_idx];
            const Point2D& C = points_[c_idx];
            QPointF O = circs_algo[tri_idx];

            // Edge vector AB in algorithm space
            double dx = B.x - A.x;
            double dy = B.y - A.y;
            double len = std::sqrt(dx * dx + dy * dy);
            if (len < 1e-12) { i = j; continue; }

            // Two perpendicular directions to AB
            double nx_left  = -dy / len;  // rotate_left(AB)
            double ny_left  =  dx / len;
            double nx_right =  dy / len;  // rotate_right(AB) = −rotate_left
            double ny_right = -dx / len;

            // Determine triangle orientation (CCW or CW) via 2D cross product.
            // cross = (B-A) × (C-A)
            double cross = dx * (C.y - A.y) - dy * (C.x - A.x);

            // If cross > 0: A→B→C is CCW → interior is LEFT of A→B
            //                → rotate_left points inward,  outward = rotate_right
            // If cross < 0: A→B→C is CW  → interior is RIGHT of A→B
            //                → rotate_right points inward, outward = rotate_left
            double nx_out, ny_out;
            if (cross > 0.0) {
                nx_out = nx_right; ny_out = ny_right;  // CCW → outward = right
            } else {
                nx_out = nx_left;  ny_out = ny_left;   // CW  → outward = left
            }

            // Outward ray from O to algorithm-space boundary
            QPointF end = clip_ray(O, nx_out, ny_out);
            p.drawLine(algo_to_screen(O), algo_to_screen(end));
        }
        i = j;
    }
}

// =======================================================================
//  Mouse events
// =======================================================================

void DelaunayCanvas::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        addPoint(event->pos());
    }
}

void DelaunayCanvas::mouseMoveEvent(QMouseEvent* event)
{
    emit mouseMoved(event->pos());
}
