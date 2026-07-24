#include "Delaunay2DShow.h"
#include "DelaunayCanvas.h"

#include <QMenuBar>
#include <QToolBar>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QTextEdit>
#include <QCheckBox>
#include <QLabel>
#include <QSplitter>
#include <QStatusBar>
#include <QDoubleValidator>
#include <QMessageBox>
#include <QCloseEvent>
#include <QApplication>

#include <cstdio>

// =======================================================================
//  Construction / Destruction
// =======================================================================

Delaunay2DShow::Delaunay2DShow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Delaunay Triangulation"));
    resize(900, 650);

    buildUI();

    // Status bar
    status_label_ = new QLabel(QStringLiteral("Click canvas or enter coordinates to add points"));
    statusBar()->addPermanentWidget(status_label_);
}

Delaunay2DShow::~Delaunay2DShow() = default;

// =======================================================================
//  UI construction
// =======================================================================

void Delaunay2DShow::buildUI()
{
    // ── Central widget: horizontal split ──
    QSplitter* splitter = new QSplitter(Qt::Horizontal, this);

    // ── Left side: canvas ──
    QWidget* left_panel = new QWidget;
    QVBoxLayout* left_layout = new QVBoxLayout(left_panel);
    left_layout->setContentsMargins(4, 4, 4, 4);

    // Toolbar row: Add Point button + X/Y inputs
    QHBoxLayout* toolbar = new QHBoxLayout;

    QPushButton* add_btn = new QPushButton(QStringLiteral("Add Point"));
    add_btn->setToolTip(QStringLiteral("Add point at the specified coordinates"));
    connect(add_btn, &QPushButton::clicked, this, &Delaunay2DShow::onAddPoint);
    toolbar->addWidget(add_btn);

    QLabel* x_label = new QLabel(QStringLiteral("X:"));
    toolbar->addWidget(x_label);

    x_input_ = new QLineEdit;
    x_input_->setPlaceholderText(QStringLiteral("0.0"));
    x_input_->setValidator(new QDoubleValidator(x_input_));
    x_input_->setMaximumWidth(80);
    toolbar->addWidget(x_input_);

    QLabel* y_label = new QLabel(QStringLiteral("Y:"));
    toolbar->addWidget(y_label);

    y_input_ = new QLineEdit;
    y_input_->setPlaceholderText(QStringLiteral("0.0"));
    y_input_->setValidator(new QDoubleValidator(y_input_));
    y_input_->setMaximumWidth(80);
    toolbar->addWidget(y_input_);

    toolbar->addStretch(1);
    left_layout->addLayout(toolbar);

    // Canvas
    canvas_ = new DelaunayCanvas;
    left_layout->addWidget(canvas_, 1);

    // Point log
    point_log_ = new QTextEdit;
    point_log_->setReadOnly(true);
    point_log_->setMaximumHeight(120);
    point_log_->setPlaceholderText(QStringLiteral("Point coordinates will be listed here..."));
    left_layout->addWidget(point_log_);

    // ── Right side: overlay controls ──
    QWidget* right_panel = new QWidget;
    QVBoxLayout* right_layout = new QVBoxLayout(right_panel);
    right_layout->setContentsMargins(8, 8, 8, 8);
    right_layout->setAlignment(Qt::AlignTop);

    circumcircle_check_ = new QCheckBox(QStringLiteral("Circumcircles"));
    connect(circumcircle_check_, &QCheckBox::toggled,
            [this](bool checked) { canvas_->setShowCircumcircles(checked); });
    right_layout->addWidget(circumcircle_check_);

    voronoi_check_ = new QCheckBox(QStringLiteral("Voronoi Diagram"));
    connect(voronoi_check_, &QCheckBox::toggled,
            [this](bool checked) { canvas_->setShowVoronoi(checked); });
    right_layout->addWidget(voronoi_check_);

    right_layout->addStretch(1);
    right_panel->setFixedWidth(160);

    splitter->addWidget(left_panel);
    splitter->addWidget(right_panel);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 0);

    setCentralWidget(splitter);

    // ── Connections ──
    connect(canvas_, &DelaunayCanvas::pointsChanged,
            this, &Delaunay2DShow::onPointsChanged);
    connect(canvas_, &DelaunayCanvas::mouseMoved,
            this, &Delaunay2DShow::onMouseMoved);
}

// =======================================================================
//  Slots
// =======================================================================

void Delaunay2DShow::onAddPoint()
{
    bool x_ok = false, y_ok = false;
    double x = x_input_->text().toDouble(&x_ok);
    double y = y_input_->text().toDouble(&y_ok);

    if (!x_ok || !y_ok) {
        statusBar()->showMessage(
            QStringLiteral("Enter valid numeric X and Y coordinates"), 3000);
        return;
    }

    // Map algorithm-space coordinates [-1,1] to canvas pixel coordinates
    double w = static_cast<double>(canvas_->width());
    double h = static_cast<double>(canvas_->height());
    double sx = (x + 1.0) * 0.5 * w;
    double sy = (1.0 - y) * 0.5 * h;  // flip Y

    canvas_->addPoint(QPointF(static_cast<float>(sx), static_cast<float>(sy)));
}

void Delaunay2DShow::onPointsChanged(int count)
{
    // Update point log
    point_log_->clear();
    for (int i = 0; i < count; ++i) {
        const auto& pt = canvas_->points()[i];
        point_log_->append(
            QStringLiteral("(%1)  %2, %3")
                .arg(i + 1)
                .arg(pt.x, 0, 'f', 4)
                .arg(pt.y, 0, 'f', 4));
    }

    // Update status
    if (count == 0) {
        status_label_->setText(QStringLiteral("No points. Click canvas to add."));
    } else {
        int tris = 0;
        // Count triangles from the canvas (HACK: only accessible via internal;
        // for display we just recompute count from the known relationship)
        if (count >= 3) {
            tris = 2 * count - 2 - 5;  // approximate: 2n - 2 - hull
            if (tris < 0) tris = 0;
        }
        status_label_->setText(
            QStringLiteral("%1 point%2 | ~%3 triangle%4")
                .arg(count)
                .arg(count == 1 ? QStringLiteral("") : QStringLiteral("s"))
                .arg(tris)
                .arg(tris == 1 ? QStringLiteral("") : QStringLiteral("s")));
    }
}

void Delaunay2DShow::onMouseMoved(const QPointF& pos)
{
    // Map to algorithm space for display
    double w = static_cast<double>(canvas_->width());
    double h = static_cast<double>(canvas_->height());
    if (w < 1.0 || h < 1.0) return;
    double ax = (pos.x() / w) * 2.0 - 1.0;
    double ay = -((pos.y() / h) * 2.0 - 1.0);

    status_label_->setText(
        QStringLiteral("(%.4f, %.4f) | %1 point%2")
            .arg(ax, 0, 'f', 4)
            .arg(ay, 0, 'f', 4)
            .arg(canvas_->pointCount())
            .arg(canvas_->pointCount() == 1 ? QStringLiteral("") : QStringLiteral("s")));
}
