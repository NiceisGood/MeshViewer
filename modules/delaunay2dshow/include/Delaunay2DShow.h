#ifndef DELAUNAY2DSHOW_H
#define DELAUNAY2DSHOW_H

#include <QMainWindow>
#include <QLineEdit>
#include <QTextEdit>
#include <QCheckBox>
#include <QLabel>

class DelaunayCanvas;

// -----------------------------------------------------------------------
// Delaunay2DShow — standalone main window for interactive 2D Delaunay
// triangulation demonstration.
//
// Layout:
//   ┌──────────────────────────────────────────────┐
//   │ [Add Point]  X: [____]  Y: [____]            │  ← toolbar
//   ├─────────────────────┬────────────────────────┤
//   │                     │  ☐ Circumcircles        │
//   │                     │  ☐ Voronoi Diagram      │  ← controls
//   │     Canvas          │                         │
//   │  (DelaunayCanvas)   │        Status           │
//   │                     │                         │
//   ├─────────────────────┴────────────────────────┤
//   │ Point list: (1) 0.12, 0.34                   │  ← text log
//   │              (2) ...                         │
//   └──────────────────────────────────────────────┘
// -----------------------------------------------------------------------
class Delaunay2DShow : public QMainWindow
{
    Q_OBJECT

public:
    explicit Delaunay2DShow(QWidget* parent = nullptr);
    ~Delaunay2DShow();

private slots:
    void onAddPoint();
    void onPointsChanged(int count);
    void onMouseMoved(const QPointF& pos);

private:
    void buildUI();

    DelaunayCanvas* canvas_ = nullptr;

    // Toolbar
    QLineEdit* x_input_ = nullptr;
    QLineEdit* y_input_ = nullptr;

    // Point log
    QTextEdit* point_log_ = nullptr;

    // Overlay checkboxes
    QCheckBox* triangles_check_ = nullptr;
    QCheckBox* circumcircle_check_ = nullptr;
    QCheckBox* voronoi_check_ = nullptr;

    // Status
    QLabel* status_label_ = nullptr;
};

#endif // DELAUNAY2DSHOW_H
