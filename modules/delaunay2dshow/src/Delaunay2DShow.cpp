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
    // ── Canvas as central widget, controls float on top ──
    canvas_ = new DelaunayCanvas;
    setCentralWidget(canvas_);

    // Layout on canvas positions child widgets on top of the drawing area
    QVBoxLayout* canvas_layout = new QVBoxLayout(canvas_);
    canvas_layout->setContentsMargins(8, 8, 8, 8);

    // ── Row 1: overlay checkboxes ──
    QHBoxLayout* checks_row = new QHBoxLayout;
    checks_row->setSpacing(6);

    auto make_checkbox = [](const QString& text) {
        QCheckBox* cb = new QCheckBox(text);
        cb->setStyleSheet(QStringLiteral(
            "QCheckBox { background: rgba(255,255,255,200); border: 1px solid #bbb;"
            "  border-radius: 3px; padding: 3px 8px; }"));
        return cb;
    };

    triangles_check_ = make_checkbox(QStringLiteral("Triangles"));
    triangles_check_->setChecked(true);
    connect(triangles_check_, &QCheckBox::toggled,
            [this](bool checked) { canvas_->setShowTriangles(checked); });
    checks_row->addWidget(triangles_check_);

    circumcircle_check_ = make_checkbox(QStringLiteral("Circumcircles"));
    connect(circumcircle_check_, &QCheckBox::toggled,
            [this](bool checked) { canvas_->setShowCircumcircles(checked); });
    checks_row->addWidget(circumcircle_check_);

    voronoi_check_ = make_checkbox(QStringLiteral("Voronoi Diagram"));
    connect(voronoi_check_, &QCheckBox::toggled,
            [this](bool checked) { canvas_->setShowVoronoi(checked); });
    checks_row->addWidget(voronoi_check_);

    checks_row->addStretch(1);
    canvas_layout->addLayout(checks_row);

    // ── Row 2: toolbar — Clear + Add Point + X/Y inputs ──
    QHBoxLayout* toolbar_row = new QHBoxLayout;

    auto make_tool_btn = [](const QString& text, const QString& tip) {
        QPushButton* btn = new QPushButton(text);
        btn->setToolTip(tip);
        btn->setStyleSheet(QStringLiteral(
            "QPushButton { background: rgba(255,255,255,220); border: 1px solid #999;"
            "  border-radius: 4px; padding: 4px 12px; }"
            "QPushButton:hover { background: rgba(220,235,255,240); }"));
        return btn;
    };

    QPushButton* clear_btn = make_tool_btn(QStringLiteral("Clear"),
        QStringLiteral("Clear all points and reset"));
    connect(clear_btn, &QPushButton::clicked, [this]() {
        canvas_->clearPoints();
    });
    toolbar_row->addWidget(clear_btn);

    QPushButton* add_btn = make_tool_btn(QStringLiteral("Add Point"),
        QStringLiteral("Add point at the specified coordinates"));
    connect(add_btn, &QPushButton::clicked, this, &Delaunay2DShow::onAddPoint);
    toolbar_row->addWidget(add_btn);

    QPushButton* test_btn = make_tool_btn(QStringLiteral("Test"),
        QStringLiteral("Check all triangles for Delaunay property"));
    connect(test_btn, &QPushButton::clicked, [this]() {
        int nv = canvas_->checkDelaunayProperty();
        if (nv == 0) {
            statusBar()->showMessage(
                QStringLiteral("All triangles satisfy Delaunay condition. ✓"),
                5000);
        } else {
            statusBar()->showMessage(
                QStringLiteral("%1 triangle%2 violate Delaunay condition!")
                    .arg(nv).arg(nv == 1 ? "" : "s"),
                10000);
        }
    });
    toolbar_row->addWidget(test_btn);

    auto make_input = [](QLineEdit*& input, const QString& placeholder) {
        input = new QLineEdit;
        input->setPlaceholderText(placeholder);
        input->setValidator(new QDoubleValidator(input));
        input->setMaximumWidth(80);
        input->setStyleSheet(QStringLiteral(
            "QLineEdit { background: rgba(255,255,255,220); border: 1px solid #aaa;"
            "  border-radius: 3px; padding: 3px 6px; }"));
    };

    QLabel* x_label = new QLabel(QStringLiteral(" X:"));
    x_label->setStyleSheet(QStringLiteral("background: transparent; font-weight: bold;"));
    toolbar_row->addWidget(x_label);

    make_input(x_input_, QStringLiteral("0.0"));
    toolbar_row->addWidget(x_input_);

    QLabel* y_label = new QLabel(QStringLiteral(" Y:"));
    y_label->setStyleSheet(QStringLiteral("background: transparent; font-weight: bold;"));
    toolbar_row->addWidget(y_label);

    make_input(y_input_, QStringLiteral("0.0"));
    toolbar_row->addWidget(y_input_);

    toolbar_row->addStretch(1);
    canvas_layout->addLayout(toolbar_row);

    // ── Point coordinate list (below toolbar) ──
    point_log_ = new QTextEdit;
    point_log_->setReadOnly(true);
    point_log_->setFixedSize(280, 140);
    point_log_->setPlaceholderText(QStringLiteral("Point coordinates..."));
    point_log_->setStyleSheet(QStringLiteral(
        "QTextEdit { background: rgba(255,255,255,200); border: 1px solid #bbb;"
        "  border-radius: 4px; padding: 4px; font-size: 12px; }"));
    canvas_layout->addWidget(point_log_, 0, Qt::AlignLeft);

    canvas_layout->addStretch(1);  // drawing area

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
