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
        // Perpendicular bisector of the segment
        QPointF a = toScreen(points_[0]);
        QPointF b = toScreen(points_[1]);
        QPointF mid = (a + b) / 2.0;
        double dx = b.x() - a.x();
        double dy = b.y() - a.y();
        // Extend perpendicular bisector to canvas edges
        double len = std::max(width(), height()) * 2.0;
        if (std::abs(dx) < 1e-6f && std::abs(dy) < 1e-6f) return;
        double nx = -dy;
        double ny = dx;
        double nl = std::sqrt(nx * nx + ny * ny);
        nx = nx / nl * len;
        ny = ny / nl * len;
        p.drawLine(QPointF(mid.x() - static_cast<float>(nx),
                            mid.y() - static_cast<float>(ny)),
                   QPointF(mid.x() + static_cast<float>(nx),
                            mid.y() + static_cast<float>(ny)));
        return;
    }

    // 3+ points: connect circumcenters of adjacent triangles
    if (triangles_.empty()) return;

    // Build adjacency: for each edge (sorted vertex pair), store which
    // triangles share it.
    struct EdgeKey { int a, b; };
    auto make_key = [](int a, int b) {
        return EdgeKey{std::min(a, b), std::max(a, b)};
    };
    auto edge_less = [](const EdgeKey& x, const EdgeKey& y) {
        if (x.a != y.a) return x.a < y.a;
        return x.b < y.b;
    };

    // Collect all triangle circumcenters in screen space
    int nt = static_cast<int>(triangles_.size());
    std::vector<QPointF> circs(nt);
    for (int i = 0; i < nt; ++i) {
        const auto& tri = triangles_[i];
        QPointF a = toScreen(points_[tri.v0]);
        QPointF b = toScreen(points_[tri.v1]);
        QPointF c = toScreen(points_[tri.v2]);
        circs[i] = circumcenter(a, b, c);
    }

    // Build edge → triangle index map
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

    // For each edge:
    //   count == 2 → interior edge, draw segment between the two circumcenters
    //   count == 1 → boundary edge, draw ray from circumcenter to canvas boundary
    for (size_t i = 0; i < edge_tri.size();) {
        size_t j = i;
        while (j < edge_tri.size() &&
               edge_tri[j].first.a == edge_tri[i].first.a &&
               edge_tri[j].first.b == edge_tri[i].first.b)
            ++j;
        size_t count = j - i;

        if (count == 2) {
            // Interior edge: connect the two adjacent circumcenters
            int t1 = edge_tri[i].second;
            int t2 = edge_tri[i + 1].second;
            p.drawLine(circs[t1], circs[t2]);
        } else if (count == 1) {
            // Boundary edge: extend ray from circumcenter outward to canvas boundary
            int a = edge_tri[i].first.a;
            int b = edge_tri[i].first.b;
            int tri_idx = edge_tri[i].second;
            const auto& tri = triangles_[tri_idx];

            // Find the third vertex (the one NOT in this edge)
            int c = -1;
            if (tri.v0 != a && tri.v0 != b) c = tri.v0;
            else if (tri.v1 != a && tri.v1 != b) c = tri.v1;
            else c = tri.v2;

            QPointF pa = toScreen(points_[a]);
            QPointF pb = toScreen(points_[b]);
            QPointF pc = toScreen(points_[c]);
            QPointF O = circs[tri_idx];               // circumcenter on the bisector
            QPointF M = (pa + pb) / 2.0f;              // edge midpoint

            // Edge direction and outward perpendicular
            double dx = static_cast<double>(pb.x() - pa.x());
            double dy = static_cast<double>(pb.y() - pa.y());
            double len = std::sqrt(dx * dx + dy * dy);
            if (len < 1.0) continue;

            // Two perpendicular directions
            double nx = -dy / len;
            double ny =  dx / len;

            // Pick the direction that points AWAY from third vertex C
            // (the inward direction has a component toward C)
            double mx = static_cast<double>(pc.x() - M.x());
            double my = static_cast<double>(pc.y() - M.y());
            if (nx * mx + ny * my > 0.0) { nx = -nx; ny = -ny; }
            // Now (nx, ny) points outward from the convex hull

            // Extend from O in the outward direction to the canvas boundary
            double ox = static_cast<double>(O.x());
            double oy = static_cast<double>(O.y());
            double extent = std::max(width(), height()) * 2.0;
            double ex = ox + nx * extent;
            double ey = oy + ny * extent;

            // Clip to canvas rectangle [0, w] x [0, h]
            double w = static_cast<double>(width());
            double h = static_cast<double>(height());
            // Parametric line O + t * dir, t in [0, extent]
            // Find t where x = 0, x = w, y = 0, or y = h
            double t_max = extent;
            auto clip_axis = [&](double origin, double dir, double bound) {
                if (std::abs(dir) < 1e-12) return;
                double t = (bound - origin) / dir;
                if (t > 0 && t < t_max) t_max = t;
            };
            clip_axis(ox, nx, 0.0);
            clip_axis(ox, nx, w);
            clip_axis(oy, ny, 0.0);
            clip_axis(oy, ny, h);

            if (t_max > 0.0) {
                QPointF end(static_cast<float>(ox + nx * t_max),
                            static_cast<float>(oy + ny * t_max));
                p.drawLine(O, end);
            }
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
