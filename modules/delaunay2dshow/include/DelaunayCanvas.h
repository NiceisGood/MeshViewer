#ifndef DELAUNAYCANVAS_H
#define DELAUNAYCANVAS_H

#include <QWidget>
#include <QPointF>
#include <QVector2D>
#include <vector>
#include <memory>

#include "Delaunay2D.h"

// -----------------------------------------------------------------------
// DelaunayCanvas — interactive 2D drawing canvas for Delaunay
// triangulation demo.
//
// Manages a list of input points and a cached Delaunay triangulation.
// On every point addition the triangulation is re-run and the display
// updated.  Supports optional overlay of circumcircles and Voronoi
// diagram.
// -----------------------------------------------------------------------
class DelaunayCanvas : public QWidget
{
    Q_OBJECT

public:
    explicit DelaunayCanvas(QWidget* parent = nullptr);
    ~DelaunayCanvas();

    /// Add a point in canvas coordinates (pixels).
    void addPoint(const QPointF& pt);

    /// Remove all points and reset triangulation.
    void clearPoints();

    /// Number of points currently held.
    int pointCount() const { return static_cast<int>(points_.size()); }

    /// Access raw 2D point data.
    const std::vector<Point2D>& points() const { return points_; }

    /// Enable/disable circumcircle overlay.
    void setShowCircumcircles(bool show) { show_circumcircles_ = show; update(); }
    bool showCircumcircles() const { return show_circumcircles_; }

    /// Enable/disable Voronoi diagram overlay.
    void setShowVoronoi(bool show) { show_voronoi_ = show; update(); }
    bool showVoronoi() const { return show_voronoi_; }

    /// Enable/disable triangle mesh display.
    void setShowTriangles(bool show) { show_triangles_ = show; update(); }
    bool showTriangles() const { return show_triangles_; }

    /// Re-run triangulation.  Called automatically after addPoint/clearPoints.
    void retriangulate();

    /// Check all triangles for Delaunay property. Returns violation count.
    /// Stores violating indices for highlight rendering.
    int checkDelaunayProperty();

signals:
    /// Emitted after a point is added and the triangulation updated.
    void pointsChanged(int count);
    /// Emitted when mouse moves over the canvas (for status bar).
    void mouseMoved(const QPointF& pos);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    // ---- Drawing helpers ----
    void drawPoints(QPainter& p);
    void drawTriangles(QPainter& p);
    void drawCircumcircles(QPainter& p);
    void drawVoronoi(QPainter& p);

    /// Map a Point2D (algorithm space) to canvas pixel coordinates.
    QPointF toScreen(const Point2D& pt) const;

    // ---- Data ----
    std::vector<Point2D> points_;           // input points (algorithm space)
    std::vector<Triangle2D> triangles_;     // cached triangulation result

    // Delaunay2D instance (re-created on each triangulation)
    std::unique_ptr<Delaunay2D> delaunay_;

    // Overlay flags
    bool show_circumcircles_ = false;
    bool show_voronoi_ = false;
    bool show_triangles_ = true;

    // Delaunay property check results
    std::vector<int> bad_tri_indices_;  // indices of triangles that violate Delaunay
};

#endif // DELAUNAYCANVAS_H
