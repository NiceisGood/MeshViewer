#include "MeshViewer.h"
#include "MeshRenderer.h"
#include "MeshReader.h"
#include "Geometry.h"
#include "Delaunay2D.h"
#include "Delaunay3D.h"
#include "MeshUtils.h"
#include "PointCloud.h"
#include "PointCloudUtils.h"
#include "PointCloudDialog.h"

#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QStatusBar>
#include <QCheckBox>
#include <QSlider>
#include <QComboBox>
#include <QApplication>
#include <QDockWidget>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>

#include <QInputDialog>
#include <cstdio>

// =======================================================================
//  Construction / Destruction
// =======================================================================

MeshViewer::MeshViewer(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Hermes Mesh Viewer"));
    resize(1024, 768);

    // Central widget: renderer
    renderer_ = new MeshRenderer(this);
    setCentralWidget(renderer_);

    connect(renderer_, &MeshRenderer::meshInfoChanged,
            this, &MeshViewer::onMeshInfoChanged);

    createMenus();
    createStatusBar();
    createSliceDock();
}

MeshViewer::~MeshViewer()
{
    delete geometry_;
    delete pc2d_;
    delete pc3d_;
    delete pc_dialog_;
}

// =======================================================================
//  Load a mesh file by path
// =======================================================================

bool MeshViewer::loadFile(const std::string& path)
{
    MeshData mesh;
    if (!read_mesh(path, mesh))
        return false;
    renderer_->loadMesh(mesh);
    statusBar()->showMessage(
        QStringLiteral("Loaded: %1").arg(QString::fromStdString(path)), 5000);
    return true;
}

// =======================================================================
//  Menus
// =======================================================================

void MeshViewer::createMenus()
{
    // ── File menu ──
    QMenu* file_menu = menuBar()->addMenu(QStringLiteral("&File"));

    QAction* open_act = file_menu->addAction(QStringLiteral("&Open Mesh..."));
    open_act->setShortcut(QKeySequence::Open);
    connect(open_act, &QAction::triggered, this, &MeshViewer::onOpenFile);

    QAction* import_act = file_menu->addAction(QStringLiteral("&Import Geometry..."));
    import_act->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_I));
    connect(import_act, &QAction::triggered, this, &MeshViewer::onImportFile);

    QAction* export_act = file_menu->addAction(QStringLiteral("&Export Geometry..."));
    export_act->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E));
    connect(export_act, &QAction::triggered, this, &MeshViewer::onExportFile);

    file_menu->addSeparator();

    QAction* quit_act = file_menu->addAction(QStringLiteral("&Quit"));
    quit_act->setShortcut(QKeySequence::Quit);
    connect(quit_act, &QAction::triggered, qApp, &QApplication::quit);

    // ── Delaunay menu ──
    QMenu* delaunay_menu = menuBar()->addMenu(QStringLiteral("&Delaunay"));

    // -- Delaunay 2D submenu --
    QMenu* d2d_menu = delaunay_menu->addMenu(QStringLiteral("Delaunay &2D"));

    QAction* d2d_tri_act = d2d_menu->addAction(QStringLiteral("2D Delaunay &Triangulation"));
    d2d_tri_act->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_2));
    connect(d2d_tri_act, &QAction::triggered, this, &MeshViewer::onDelaunay2D);

    QAction* d2d_opt_act = d2d_menu->addAction(QStringLiteral("2D Delaunay &Optimization"));
    d2d_opt_act->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_2));
    connect(d2d_opt_act, &QAction::triggered, this, &MeshViewer::onDelaunay2DOptimize);

    // -- Delaunay 3D submenu --
    QMenu* d3d_menu = delaunay_menu->addMenu(QStringLiteral("Delaunay &3D"));

    QAction* d3d_surf_act = d3d_menu->addAction(QStringLiteral("Surface Delaunay &Triangulation"));
    d3d_surf_act->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_3));
    connect(d3d_surf_act, &QAction::triggered, this, &MeshViewer::onDelaunay3DSurface);

    QAction* d3d_vol_act = d3d_menu->addAction(QStringLiteral("&Volume Delaunay Triangulation"));
    d3d_vol_act->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_3));
    connect(d3d_vol_act, &QAction::triggered, this, &MeshViewer::onDelaunay3DVolume);

    d3d_menu->addSeparator();

    QAction* d3d_surf_opt_act = d3d_menu->addAction(QStringLiteral("Surface Delaunay &Optimization"));
    d3d_surf_opt_act->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_4));
    connect(d3d_surf_opt_act, &QAction::triggered, this, &MeshViewer::onDelaunay3DSurfaceOptimize);

    QAction* d3d_vol_opt_act = d3d_menu->addAction(QStringLiteral("Volume Delaunay O&ptimization"));
    d3d_vol_opt_act->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_4));
    connect(d3d_vol_opt_act, &QAction::triggered, this, &MeshViewer::onDelaunay3DVolumeOptimize);

    // ── Point Cloud menu ──
    QMenu* pc_menu = menuBar()->addMenu(QStringLiteral("&Point Cloud"));

    QAction* pc_import_act = pc_menu->addAction(QStringLiteral("&Import Point Cloud..."));
    pc_import_act->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_U));
    connect(pc_import_act, &QAction::triggered, this, &MeshViewer::onPCImport);

    QAction* pc_export_act = pc_menu->addAction(QStringLiteral("&Export Point Cloud..."));
    pc_export_act->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_U));
    connect(pc_export_act, &QAction::triggered, this, &MeshViewer::onPCExport);

    pc_menu->addSeparator();

    QAction* pc_create2d_act = pc_menu->addAction(QStringLiteral("Create &2D Point Cloud..."));
    connect(pc_create2d_act, &QAction::triggered, this, &MeshViewer::onPCCreate2D);

    QAction* pc_create3d_act = pc_menu->addAction(QStringLiteral("Create &3D Point Cloud..."));
    connect(pc_create3d_act, &QAction::triggered, this, &MeshViewer::onPCCreate3D);

    // ── View menu ──
    QMenu* view_menu = menuBar()->addMenu(QStringLiteral("&View"));

    // Display mode submenu
    QMenu* display_menu = view_menu->addMenu(QStringLiteral("&Display Mode"));

    display_group_ = new QActionGroup(this);
    display_group_->setExclusive(true);

    QAction* solid_act = display_menu->addAction(QStringLiteral("&Solid"));
    solid_act->setCheckable(true);
    solid_act->setChecked(true);
    solid_act->setData(static_cast<int>(MeshRenderer::Solid));
    solid_act->setShortcut(QKeySequence(Qt::Key_1));
    display_group_->addAction(solid_act);

    QAction* wire_act = display_menu->addAction(QStringLiteral("&Wireframe"));
    wire_act->setCheckable(true);
    wire_act->setChecked(false);
    wire_act->setData(static_cast<int>(MeshRenderer::Wireframe));
    wire_act->setShortcut(QKeySequence(Qt::Key_2));
    display_group_->addAction(wire_act);

    QAction* both_act = display_menu->addAction(QStringLiteral("Wireframe && &Solid"));
    both_act->setCheckable(true);
    both_act->setChecked(false);
    both_act->setData(static_cast<int>(MeshRenderer::WireframeSolid));
    both_act->setShortcut(QKeySequence(Qt::Key_3));
    display_group_->addAction(both_act);

    display_menu->addSeparator();

    QAction* quad_wire_act = display_menu->addAction(QStringLiteral("Quad &Wireframe"));
    quad_wire_act->setCheckable(true);
    quad_wire_act->setChecked(false);
    quad_wire_act->setData(static_cast<int>(MeshRenderer::QuadWireframe));
    quad_wire_act->setShortcut(QKeySequence(Qt::Key_4));
    display_group_->addAction(quad_wire_act);

    QAction* quad_both_act = display_menu->addAction(QStringLiteral("Wireframe && &Solid + Quad"));
    quad_both_act->setCheckable(true);
    quad_both_act->setChecked(false);
    quad_both_act->setData(static_cast<int>(MeshRenderer::QuadWireframeSolid));
    quad_both_act->setShortcut(QKeySequence(Qt::Key_5));
    display_group_->addAction(quad_both_act);

    connect(display_group_, &QActionGroup::triggered,
            this, &MeshViewer::onDisplayModeChanged);

    view_menu->addSeparator();

    // Projection mode submenu
    QMenu* proj_menu = view_menu->addMenu(QStringLiteral("&Projection Mode"));

    projection_group_ = new QActionGroup(this);
    projection_group_->setExclusive(true);

    QAction* ortho_act = proj_menu->addAction(QStringLiteral("&Orthographic"));
    ortho_act->setCheckable(true);
    ortho_act->setChecked(true);
    ortho_act->setData(static_cast<int>(MeshRenderer::Orthographic));
    ortho_act->setShortcut(QKeySequence(Qt::Key_O));
    projection_group_->addAction(ortho_act);

    QAction* persp_act = proj_menu->addAction(QStringLiteral("&Perspective"));
    persp_act->setCheckable(true);
    persp_act->setChecked(false);
    persp_act->setData(static_cast<int>(MeshRenderer::Perspective));
    persp_act->setShortcut(QKeySequence(Qt::Key_P));
    projection_group_->addAction(persp_act);

    connect(projection_group_, &QActionGroup::triggered,
            [this](QAction* action) {
        if (!action) return;
        auto mode = static_cast<MeshRenderer::ProjectionMode>(action->data().toInt());
        renderer_->setProjectionMode(mode);
    });

    view_menu->addSeparator();

    // Slice plane toggle
    slice_act_ = view_menu->addAction(QStringLiteral("&Slice Plane..."));
    slice_act_->setCheckable(true);
    slice_act_->setChecked(false);
    slice_act_->setShortcut(QKeySequence(Qt::Key_S));
    connect(slice_act_, &QAction::triggered, [this](bool checked) {
        slice_dock_->setVisible(checked);
        if (!checked && slice_enable_check_->isChecked()) {
            slice_enable_check_->setChecked(false); // disable slice too
        }
    });

    view_menu->addSeparator();

    QAction* reset_act = view_menu->addAction(QStringLiteral("&Reset View"));
    reset_act->setShortcut(QKeySequence(Qt::Key_R));
    connect(reset_act, &QAction::triggered, [this]() {
        renderer_->resetView();
    });

    view_menu->addSeparator();

    QAction* clear_act = view_menu->addAction(QStringLiteral("Clea&r All"));
    clear_act->setShortcut(QKeySequence(Qt::Key_Delete));
    connect(clear_act, &QAction::triggered, this, &MeshViewer::onClearAll);
}

void MeshViewer::createStatusBar()
{
    info_label_ = new QLabel(QStringLiteral("No mesh loaded"));
    statusBar()->addPermanentWidget(info_label_);
}

// =======================================================================
//  Slots
// =======================================================================

void MeshViewer::onOpenFile()
{
    QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Open Mesh File"),
        QString(),
        QStringLiteral("Mesh Files (*.stl *.obj *.qmesh *.qmesh3d);;STL Files (*.stl);;OBJ Files (*.obj);;QMesh Files (*.qmesh *.qmesh3d);;All Files (*)")
    );

    if (path.isEmpty()) return;

    MeshData mesh;
    if (!read_mesh(path.toStdString(), mesh)) {
        QMessageBox::warning(this,
                             QStringLiteral("Error"),
                             QStringLiteral("Failed to load mesh file:\n%1").arg(path));
        return;
    }

    renderer_->loadMesh(mesh);
    statusBar()->showMessage(QStringLiteral("Loaded: %1").arg(path), 5000);
}

void MeshViewer::onImportFile()
{
    QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Import Geometry File"),
        QString(),
        QStringLiteral("Geometry Files (*.stl *.obj);;STL Files (*.stl);;OBJ Files (*.obj);;All Files (*)")
    );

    if (path.isEmpty()) return;

    Geometry geom;
    if (!geom.readFile(path.toStdString())) {
        QMessageBox::warning(this,
                             QStringLiteral("Import Error"),
                             QStringLiteral("Failed to import geometry file:\n%1").arg(path));
        return;
    }

    loadGeometry(geom);
    statusBar()->showMessage(QStringLiteral("Imported: %1").arg(path), 5000);
}

void MeshViewer::onExportFile()
{
    if (!geometry_ || geometry_->empty()) {
        QMessageBox::information(this,
                                 QStringLiteral("Export"),
                                 QStringLiteral("No geometry loaded to export.\nLoad a file first via File → Import Geometry."));
        return;
    }

    QString path = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("Export Geometry As"),
        QString(),
        QStringLiteral("STL Binary (*.stl);;STL ASCII (*.stl);;Wavefront OBJ (*.obj);;All Files (*)")
    );

    if (path.isEmpty()) return;

    auto ext_pos = path.toStdString().rfind('.');
    if (ext_pos == std::string::npos) {
        // No extension — default to .stl
        path += QStringLiteral(".stl");
    }

    bool ok = false;
    QString suffix = QFileInfo(path).suffix().toLower();

    if (suffix == QStringLiteral("stl")) {
        // Check if the user selected ASCII — heuristically check the filter
        // (Qt doesn't expose which filter was chosen, so default to binary)
        ok = geometry_->writeSTLBinary(path.toStdString());
    } else if (suffix == QStringLiteral("obj")) {
        ok = geometry_->writeOBJ(path.toStdString());
    } else {
        ok = geometry_->writeFile(path.toStdString());
    }

    if (!ok) {
        QMessageBox::warning(this,
                             QStringLiteral("Export Error"),
                             QStringLiteral("Failed to export geometry to:\n%1").arg(path));
        return;
    }

    statusBar()->showMessage(QStringLiteral("Exported: %1").arg(path), 5000);
}

void MeshViewer::loadGeometry(const Geometry& geom)
{
    // Store geometry for potential export later
    delete geometry_;
    geometry_ = new Geometry(geom);

    // Convert to MeshData for rendering
    MeshData mesh = geom.toMeshData();
    renderer_->loadMesh(mesh);
}

void MeshViewer::onMeshInfoChanged(const QString& info)
{
    info_label_->setText(info);
}

void MeshViewer::onDisplayModeChanged(QAction* action)
{
    if (!action) return;
    auto mode = static_cast<MeshRenderer::DisplayMode>(action->data().toInt());
    renderer_->setDisplayMode(mode);
}

// =======================================================================
//  Slice plane dock
// =======================================================================

void MeshViewer::createSliceDock()
{
    slice_dock_ = new QDockWidget(QStringLiteral("Slice Plane"), this);
    slice_dock_->setObjectName(QStringLiteral("SliceDock"));
    slice_dock_->setFeatures(QDockWidget::DockWidgetClosable |
                             QDockWidget::DockWidgetMovable);
    slice_dock_->setVisible(false);  // hidden by default

    QWidget* container = new QWidget;

    QVBoxLayout* layout = new QVBoxLayout(container);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // ── Enable / disable ──
    slice_enable_check_ = new QCheckBox(QStringLiteral("Enable Slice"));
    slice_enable_check_->setChecked(false);
    connect(slice_enable_check_, &QCheckBox::toggled,
            this, &MeshViewer::onSliceToggled);
    layout->addWidget(slice_enable_check_);

    // ── Plane Settings group ──
    QGroupBox* plane_group = new QGroupBox(QStringLiteral("Plane Settings"));
    QVBoxLayout* plane_layout = new QVBoxLayout(plane_group);
    plane_layout->setContentsMargins(6, 10, 6, 6);
    plane_layout->setSpacing(6);

    // Position row
    QHBoxLayout* pos_row = new QHBoxLayout;
    QLabel* pos_label = new QLabel(QStringLiteral("Position:"));
    pos_label->setFixedWidth(60);
    pos_row->addWidget(pos_label);
    slice_pos_slider_ = new QSlider(Qt::Horizontal);
    slice_pos_slider_->setRange(-100, 100);
    slice_pos_slider_->setValue(0);
    slice_pos_slider_->setEnabled(false);
    connect(slice_pos_slider_, &QSlider::valueChanged,
            this, &MeshViewer::onSlicePosChanged);
    pos_row->addWidget(slice_pos_slider_);
    plane_layout->addLayout(pos_row);

    // Normal row
    QHBoxLayout* normal_row = new QHBoxLayout;
    QLabel* normal_label = new QLabel(QStringLiteral("Normal:"));
    normal_label->setFixedWidth(60);
    normal_row->addWidget(normal_label);
    slice_normal_combo_ = new QComboBox;
    slice_normal_combo_->addItem(QStringLiteral("+Z (front)"));
    slice_normal_combo_->addItem(QStringLiteral("-Z (back)"));
    slice_normal_combo_->addItem(QStringLiteral("+Y (top)"));
    slice_normal_combo_->addItem(QStringLiteral("-Y (bottom)"));
    slice_normal_combo_->addItem(QStringLiteral("+X (right)"));
    slice_normal_combo_->addItem(QStringLiteral("-X (left)"));
    slice_normal_combo_->setEnabled(false);
    slice_normal_combo_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(slice_normal_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MeshViewer::onSliceNormalChanged);
    normal_row->addWidget(slice_normal_combo_);
    plane_layout->addLayout(normal_row);

    plane_group->setLayout(plane_layout);
    layout->addWidget(plane_group);

    // ── Display group ──
    QGroupBox* display_group = new QGroupBox(QStringLiteral("Display"));
    QVBoxLayout* display_layout = new QVBoxLayout(display_group);
    display_layout->setContentsMargins(6, 10, 6, 6);

    QHBoxLayout* mode_row = new QHBoxLayout;
    QLabel* mode_label = new QLabel(QStringLiteral("Mode:"));
    mode_label->setFixedWidth(60);
    mode_row->addWidget(mode_label);
    slice_display_combo_ = new QComboBox;
    slice_display_combo_->addItem(QStringLiteral("Full Mesh + Contour"));
    slice_display_combo_->addItem(QStringLiteral("Half Mesh + Contour"));
    slice_display_combo_->addItem(QStringLiteral("Contour Only"));
    slice_display_combo_->setCurrentIndex(0);
    slice_display_combo_->setEnabled(false);
    slice_display_combo_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(slice_display_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MeshViewer::onSliceDisplayModeChanged);
    mode_row->addWidget(slice_display_combo_);
    display_layout->addLayout(mode_row);

    display_group->setLayout(display_layout);
    layout->addWidget(display_group);

    layout->addStretch(1);

    container->setLayout(layout);
    slice_dock_->setWidget(container);
    addDockWidget(Qt::RightDockWidgetArea, slice_dock_);

    // Close dock → auto-disable slice
    connect(slice_dock_, &QDockWidget::visibilityChanged, [this](bool visible) {
        slice_act_->setChecked(visible);
        if (!visible && slice_enable_check_->isChecked()) {
            slice_enable_check_->setChecked(false);
        }
    });

    // Default: +Z normal
    renderer_->setSliceNormal(QVector3D(0.0f, 0.0f, 1.0f));
    renderer_->setSlicePosition(0.0f);
}

void MeshViewer::onSliceToggled(bool enabled)
{
    slice_pos_slider_->setEnabled(enabled);
    slice_normal_combo_->setEnabled(enabled);
    slice_display_combo_->setEnabled(enabled);
    renderer_->setSliceEnabled(enabled);
}

void MeshViewer::onSlicePosChanged(int value)
{
    // Map slider [-100, 100] to model-space [-diag, diag]
    float diag = renderer_->modelDiagonal();
    float pos = static_cast<float>(value) / 100.0f * diag;
    renderer_->setSlicePosition(pos);
}

void MeshViewer::onSliceNormalChanged(int index)
{
    QVector3D normal;
    switch (index) {
    case 0: normal = QVector3D(0.0f, 0.0f,  1.0f); break;  // +Z
    case 1: normal = QVector3D(0.0f, 0.0f, -1.0f); break;  // -Z
    case 2: normal = QVector3D(0.0f,  1.0f, 0.0f); break;  // +Y
    case 3: normal = QVector3D(0.0f, -1.0f, 0.0f); break;  // -Y
    case 4: normal = QVector3D( 1.0f, 0.0f, 0.0f); break;  // +X
    case 5: normal = QVector3D(-1.0f, 0.0f, 0.0f); break;  // -X
    default: normal = QVector3D(0.0f, 0.0f, 1.0f); break;
    }
    renderer_->setSliceNormal(normal);
}

void MeshViewer::onSliceDisplayModeChanged(int index)
{
    auto mode = static_cast<MeshRenderer::SliceDisplayMode>(index);
    renderer_->setSliceDisplayMode(mode);
}

// =======================================================================
//  Point Cloud slots
// =======================================================================

void MeshViewer::onPCImport()
{
    QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Import Point Cloud"),
        QString(),
        QStringLiteral("Point Cloud Files (*.p2d *.p3d);;2D Point Cloud (*.p2d);;3D Point Cloud (*.p3d);;All Files (*)")
    );

    if (path.isEmpty()) return;

    QString suffix = QFileInfo(path).suffix().toLower();

    // Clean up previous point cloud
    delete pc2d_; pc2d_ = nullptr;
    delete pc3d_; pc3d_ = nullptr;

    if (suffix == QStringLiteral("p2d")) {
        // 2D point cloud
        pc2d_ = new PointCloud2D;
        if (!pc2d_->readFile(path.toStdString())) {
            delete pc2d_; pc2d_ = nullptr;
            QMessageBox::warning(this,
                                 QStringLiteral("Import Error"),
                                 QStringLiteral("Failed to import 2D point cloud:\n%1").arg(path));
            return;
        }
        // Convert to 3D for rendering (z = 0)
        std::vector<float> pts3d;
        pts3d.reserve(pc2d_->numPoints() * 3);
        for (int i = 0; i < pc2d_->numPoints(); ++i) {
            float x, y;
            pc2d_->getPoint(i, x, y);
            pts3d.push_back(x);
            pts3d.push_back(y);
            pts3d.push_back(0.0f);
        }
        pc3d_ = new PointCloud3D(std::move(pts3d));
    } else if (suffix == QStringLiteral("p3d")) {
        // 3D point cloud
        pc3d_ = new PointCloud3D;
        if (!pc3d_->readFile(path.toStdString())) {
            delete pc3d_; pc3d_ = nullptr;
            QMessageBox::warning(this,
                                 QStringLiteral("Import Error"),
                                 QStringLiteral("Failed to import 3D point cloud:\n%1").arg(path));
            return;
        }
    } else {
        QMessageBox::warning(this,
                             QStringLiteral("Import Error"),
                             QStringLiteral("Unrecognized file extension: .%1\nSupported: .p2d, .p3d").arg(suffix));
        return;
    }

    // Render the 3D point cloud
    renderer_->loadPointCloud(pc3d_->points());

    statusBar()->showMessage(
        QStringLiteral("Imported point cloud: %1 (%2 points)")
            .arg(QFileInfo(path).fileName())
            .arg(pc3d_->numPoints()),
        10000);
}

void MeshViewer::onPCExport()
{
    // Determine what to export
    bool has_pc = (pc2d_ && !pc2d_->empty()) || (pc3d_ && !pc3d_->empty());
    if (!has_pc) {
        QMessageBox::information(this,
                                 QStringLiteral("Export Point Cloud"),
                                 QStringLiteral("No point cloud to export.\nImport or create a point cloud first."));
        return;
    }

    QString filter;
    if (pc2d_ && !pc2d_->empty()) {
        filter = QStringLiteral("2D Point Cloud (*.p2d);;3D Point Cloud (*.p3d)");
    } else {
        filter = QStringLiteral("3D Point Cloud (*.p3d)");
    }

    QString path = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("Export Point Cloud As"),
        QString(),
        filter
    );

    if (path.isEmpty()) return;

    QString suffix = QFileInfo(path).suffix().toLower();

    bool ok = false;
    if (suffix == QStringLiteral("p2d") && pc2d_ && !pc2d_->empty()) {
        ok = pc2d_->writeFile(path.toStdString());
    } else if (suffix == QStringLiteral("p3d") && pc3d_ && !pc3d_->empty()) {
        ok = pc3d_->writeFile(path.toStdString());
    } else {
        QMessageBox::warning(this,
                             QStringLiteral("Export Error"),
                             QStringLiteral("Cannot export to this format.\n2D point cloud requires .p2d, 3D requires .p3d."));
        return;
    }

    if (!ok) {
        QMessageBox::warning(this,
                             QStringLiteral("Export Error"),
                             QStringLiteral("Failed to export point cloud to:\n%1").arg(path));
        return;
    }

    statusBar()->showMessage(QStringLiteral("Exported point cloud: %1").arg(path), 5000);
}

void MeshViewer::onPCCreate2D()
{
    // If dialog already open, just bring it to front
    if (pc_dialog_) {
        pc_dialog_->raise();
        pc_dialog_->activateWindow();
        return;
    }

    PointCloudCreateDialog* dlg = new PointCloudCreateDialog(
        PointCloudCreateDialog::Create2D, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    connect(dlg, &PointCloudCreateDialog::applyRequested2D,
            this, &MeshViewer::onPCApply2D);
    connect(dlg, &QDialog::accepted, this, [this, dlg]() {
        onPCApply2D(dlg->count(), dlg->methodIndex(), dlg->seed(), dlg->onlyBoundary(),
                    dlg->xMin(), dlg->xMax(), dlg->yMin(), dlg->yMax(),
                    dlg->cx(), dlg->cy(),
                    dlg->radius(), dlg->radiusInner());
    });
    connect(dlg, &QObject::destroyed, this, [this]() {
        pc_dialog_ = nullptr;
    });

    pc_dialog_ = dlg;
    dlg->show();
}

void MeshViewer::onPCCreate3D()
{
    // If dialog already open, just bring it to front
    if (pc_dialog_) {
        pc_dialog_->raise();
        pc_dialog_->activateWindow();
        return;
    }

    PointCloudCreateDialog* dlg = new PointCloudCreateDialog(
        PointCloudCreateDialog::Create3D, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    connect(dlg, &PointCloudCreateDialog::applyRequested3D,
            this, &MeshViewer::onPCApply3D);
    connect(dlg, &QDialog::accepted, this, [this, dlg]() {
        onPCApply3D(dlg->count(), dlg->methodIndex(), dlg->seed(), dlg->onlyBoundary(),
                    dlg->xMin(), dlg->xMax(), dlg->yMin(), dlg->yMax(),
                    dlg->zMin(), dlg->zMax(),
                    dlg->cx(), dlg->cy(), dlg->cz(),
                    dlg->radius(), dlg->radiusInner());
    });
    connect(dlg, &QObject::destroyed, this, [this]() {
        pc_dialog_ = nullptr;
    });

    pc_dialog_ = dlg;
    dlg->show();
}

// =======================================================================
//  Point Cloud Apply — live preview from dialog
// =======================================================================

void MeshViewer::onPCApply2D(int count, int method, int seed, bool onlyBoundary,
                              float xMin, float xMax,
                              float yMin, float yMax,
                              float cx, float cy,
                              float radius, float radiusInner)
{
    unsigned int s = static_cast<unsigned int>(seed);

    // Generate 2D point cloud
    PointCloud2D* new_pc2d;
    if (method == 0) {
        new_pc2d = new PointCloud2D(PointCloudUtils::generateRandom2D(
            count, xMin, xMax, yMin, yMax, onlyBoundary, s));
    } else {
        // Circle: use annular overload if inner radius > 0
        if (radiusInner > 0.0f) {
            new_pc2d = new PointCloud2D(PointCloudUtils::generateRandomInCircle(
                count, cx, cy, radius, radiusInner, onlyBoundary, s));
        } else {
            new_pc2d = new PointCloud2D(PointCloudUtils::generateRandomInCircle(
                count, cx, cy, radius, onlyBoundary, s));
        }
    }

    // Replace old 2D PC
    delete pc2d_;
    pc2d_ = new_pc2d;

    // Convert to 3D for rendering (z = 0)
    std::vector<float> pts3d;
    pts3d.reserve(pc2d_->numPoints() * 3);
    for (int i = 0; i < pc2d_->numPoints(); ++i) {
        float x, y;
        pc2d_->getPoint(i, x, y);
        pts3d.push_back(x);
        pts3d.push_back(y);
        pts3d.push_back(0.0f);
    }

    delete pc3d_;
    pc3d_ = new PointCloud3D(std::move(pts3d));

    renderer_->loadPointCloud(pc3d_->points());

    QString method_name;
    if (method == 0) {
        method_name = QStringLiteral("Bounding Box");
        if (onlyBoundary)
            method_name += QStringLiteral(" (boundary)");
    } else if (radiusInner > 0.0f) {
        method_name = QStringLiteral("Annulus");
        if (onlyBoundary)
            method_name += QStringLiteral(" (boundary)");
    } else {
        method_name = QStringLiteral("Circle");
        if (onlyBoundary)
            method_name += QStringLiteral(" (boundary)");
    }

    statusBar()->showMessage(
        QStringLiteral("2D point cloud: %1 points (%2)%3")
            .arg(pc3d_->numPoints())
            .arg(method_name)
            .arg(seed != 0
                ? QStringLiteral(" [seed=%1]").arg(seed)
                : QStringLiteral("")),
        10000);
}

void MeshViewer::onPCApply3D(int count, int method, int seed, bool onlyBoundary,
                              float xMin, float xMax,
                              float yMin, float yMax,
                              float zMin, float zMax,
                              float cx, float cy, float cz,
                              float radius, float radiusInner)
{
    unsigned int s = static_cast<unsigned int>(seed);

    // Replace old PCs
    delete pc2d_;
    pc2d_ = nullptr;
    delete pc3d_;

    // Generate 3D point cloud
    if (method == 0) {
        pc3d_ = new PointCloud3D(PointCloudUtils::generateRandom3D(
            count, xMin, xMax, yMin, yMax, zMin, zMax, onlyBoundary, s));
    } else {
        if (radiusInner > 0.0f) {
            // Spherical shell not directly supported — fallback to sphere
            pc3d_ = new PointCloud3D(PointCloudUtils::generateRandomInSphere(
                count, cx, cy, cz, radius, onlyBoundary, s));
        } else {
            pc3d_ = new PointCloud3D(PointCloudUtils::generateRandomInSphere(
                count, cx, cy, cz, radius, onlyBoundary, s));
        }
    }

    renderer_->loadPointCloud(pc3d_->points());

    QString method_name;
    if (method == 0) {
        method_name = QStringLiteral("Bounding Box");
        if (onlyBoundary)
            method_name += QStringLiteral(" (boundary)");
    } else {
        method_name = QStringLiteral("Sphere");
        if (onlyBoundary)
            method_name += QStringLiteral(" (surface)");
    }

    statusBar()->showMessage(
        QStringLiteral("3D point cloud: %1 points (%2)%3")
            .arg(pc3d_->numPoints())
            .arg(method_name)
            .arg(seed != 0
                ? QStringLiteral(" [seed=%1]").arg(seed)
                : QStringLiteral("")),
        10000);
}

// =======================================================================
//  Clear all data
// =======================================================================

void MeshViewer::onClearAll()
{
    // Clear geometry
    delete geometry_;
    geometry_ = nullptr;

    // Clear point clouds
    delete pc2d_;
    pc2d_ = nullptr;
    delete pc3d_;
    pc3d_ = nullptr;

    // Clear renderer
    renderer_->clearMesh();
    renderer_->clearPointCloud();

    // Disable slice if active
    if (slice_enable_check_->isChecked()) {
        slice_enable_check_->setChecked(false);
    }

    // Reset info label
    info_label_->setText(QStringLiteral("No data loaded"));

    // Reset camera
    renderer_->resetView();

    statusBar()->showMessage(QStringLiteral("Cleared all data"), 3000);
}

// =======================================================================
//  Delaunay menu slots (placeholders — functionality to be added later)
// =======================================================================

void MeshViewer::onDelaunay2D()
{
    // Determine point source: point cloud first, then geometry vertices
    std::vector<Point2D> points;

    if (pc2d_ && !pc2d_->empty()) {
        // Use 2D point cloud directly
        points.reserve(pc2d_->numPoints());
        for (int i = 0; i < pc2d_->numPoints(); ++i) {
            float x, y;
            pc2d_->getPoint(i, x, y);
            points.push_back({static_cast<double>(x),
                              static_cast<double>(y)});
        }
    } else if (pc3d_ && !pc3d_->empty()) {
        // Extract XY from 3D point cloud
        points.reserve(pc3d_->numPoints());
        for (int i = 0; i < pc3d_->numPoints(); ++i) {
            float x, y, z;
            pc3d_->getPoint(i, x, y, z);
            (void)z;  // discard Z
            points.push_back({static_cast<double>(x),
                              static_cast<double>(y)});
        }
    } else if (geometry_ && !geometry_->empty()) {
        // Fallback: extract XY from geometry vertices
        const auto& verts = geometry_->vertices();
        points.reserve(geometry_->numVertices());
        for (int i = 0; i < geometry_->numVertices(); ++i) {
            points.push_back({static_cast<double>(verts[i * 3]),
                              static_cast<double>(verts[i * 3 + 1])});
        }
    } else {
        statusBar()->showMessage(QStringLiteral("No data — import/load a mesh or create/import a point cloud first"), 5000);
        return;
    }

    if (static_cast<int>(points.size()) < 3) {
        statusBar()->showMessage(QStringLiteral("Need at least 3 points for 2D Delaunay triangulation"), 5000);
        return;
    }

    int orig_count = static_cast<int>(points.size());  // save before move

    // Run Delaunay2D triangulation
    Delaunay2D delaunay(std::move(points), Delaunay2D::Strategy::Optimized);
    auto triangles = delaunay.triangulate();

    if (triangles.empty()) {
        statusBar()->showMessage(QStringLiteral("2D Delaunay triangulation produced no triangles"), 5000);
        return;
    }

    // Convert result to MeshData for rendering
    // NOTE: only copy original input points (skip super-triangle vertices at delaunay.points()[orig_count..])
    MeshData mesh;
    const auto& all_pts = delaunay.points();
    mesh.vertices.reserve(orig_count * 3);
    for (int i = 0; i < orig_count; ++i) {
        mesh.vertices.push_back(static_cast<float>(all_pts[i].x));
        mesh.vertices.push_back(static_cast<float>(all_pts[i].y));
        mesh.vertices.push_back(0.0f);
    }
    mesh.indices.reserve(triangles.size() * 3);
    for (const auto& t : triangles) {
        mesh.indices.push_back(static_cast<unsigned int>(t.v0));
        mesh.indices.push_back(static_cast<unsigned int>(t.v1));
        mesh.indices.push_back(static_cast<unsigned int>(t.v2));
    }

    renderer_->loadMesh(mesh);

    statusBar()->showMessage(
        QStringLiteral("2D Delaunay: %1 points → %2 triangles")
            .arg(delaunay.points().size())
            .arg(triangles.size()),
        10000);
}

void MeshViewer::onDelaunay2DOptimize()
{
    // Determine point source: point cloud first, then geometry vertices
    std::vector<Point2D> points;

    if (pc2d_ && !pc2d_->empty()) {
        points.reserve(pc2d_->numPoints());
        for (int i = 0; i < pc2d_->numPoints(); ++i) {
            float x, y;
            pc2d_->getPoint(i, x, y);
            points.push_back({static_cast<double>(x),
                              static_cast<double>(y)});
        }
    } else if (pc3d_ && !pc3d_->empty()) {
        points.reserve(pc3d_->numPoints());
        for (int i = 0; i < pc3d_->numPoints(); ++i) {
            float x, y, z;
            pc3d_->getPoint(i, x, y, z);
            (void)z;
            points.push_back({static_cast<double>(x),
                              static_cast<double>(y)});
        }
    } else if (geometry_ && !geometry_->empty()) {
        const auto& verts = geometry_->vertices();
        points.reserve(geometry_->numVertices());
        for (int i = 0; i < geometry_->numVertices(); ++i) {
            points.push_back({static_cast<double>(verts[i * 3]),
                              static_cast<double>(verts[i * 3 + 1])});
        }
    } else {
        statusBar()->showMessage(QStringLiteral("No data — import/load a mesh or create/import a point cloud first"), 5000);
        return;
    }

    if (static_cast<int>(points.size()) < 3) {
        statusBar()->showMessage(QStringLiteral("Need at least 3 points for 2D Delaunay optimization"), 5000);
        return;
    }

    int orig_count = static_cast<int>(points.size());  // save before move

    // Initial Delaunay triangulation to establish connectivity
    Delaunay2D delaunay(points, Delaunay2D::Strategy::Optimized);
    auto triangles = delaunay.triangulate();
    if (triangles.empty()) {
        statusBar()->showMessage(QStringLiteral("Triangulation produced no triangles"), 5000);
        return;
    }

    // Use the original input points (not delaunay.points() which includes super-triangle vertices)
    // The triangle indices only reference original point indices (0..n-1)
    const auto& orig_pts = delaunay.points();

    // Compute initial quality stats on original points
    auto initial_stats = compute_stats(orig_pts, triangles);

    // Laplacian smoothing: move interior vertices toward centroid of neighbors
    // Keep connectivity fixed — no re-triangulation
    auto smoothed_pts = points;  // mutable copy of original points only (no super-vertices)

    const int max_iters = 20;
    const double relax = 0.3;

    for (int iter = 0; iter < max_iters; ++iter) {
        // Build adjacency
        int nv = static_cast<int>(smoothed_pts.size());
        std::vector<std::vector<int>> adj(nv);
        for (const auto& t : triangles) {
            adj[t.v0].push_back(t.v1); adj[t.v0].push_back(t.v2);
            adj[t.v1].push_back(t.v0); adj[t.v1].push_back(t.v2);
            adj[t.v2].push_back(t.v0); adj[t.v2].push_back(t.v1);
        }

        // Boundary detection: edges appearing only once
        std::vector<std::pair<int,int>> edge_count;
        for (const auto& t : triangles) {
            auto add_edge = [&](int a, int b) {
                if (a > b) std::swap(a, b);
                edge_count.push_back({a, b});
            };
            add_edge(t.v0, t.v1); add_edge(t.v1, t.v2); add_edge(t.v2, t.v0);
        }
        std::sort(edge_count.begin(), edge_count.end());
        std::vector<bool> is_boundary(nv, false);
        for (size_t i = 0; i < edge_count.size(); ++i) {
            bool unique = true;
            if (i > 0 && edge_count[i-1] == edge_count[i]) unique = false;
            if (i+1 < edge_count.size() && edge_count[i+1] == edge_count[i]) unique = false;
            if (unique) {
                is_boundary[edge_count[i].first] = true;
                is_boundary[edge_count[i].second] = true;
            }
        }

        // Laplacian step: move interior vertices toward centroid of neighbors
        std::vector<Point2D> new_pts = smoothed_pts;
        for (int i = 0; i < nv; ++i) {
            if (is_boundary[i] || adj[i].empty()) continue;
            double cx = 0.0, cy = 0.0;
            for (int nb : adj[i]) {
                cx += smoothed_pts[nb].x;
                cy += smoothed_pts[nb].y;
            }
            double inv = 1.0 / adj[i].size();
            new_pts[i].x = smoothed_pts[i].x + relax * (cx * inv - smoothed_pts[i].x);
            new_pts[i].y = smoothed_pts[i].y + relax * (cy * inv - smoothed_pts[i].y);
        }
        smoothed_pts = std::move(new_pts);
    }

    // Compute final quality stats
    auto final_stats = compute_stats(smoothed_pts, triangles);

    // Display result: convert smoothed points + same triangles to MeshData
    MeshData mesh;
    read_2d_mesh(smoothed_pts, triangles, mesh);
    renderer_->loadMesh(mesh);

    // Show quality comparison
    statusBar()->showMessage(
        QStringLiteral("2D Opt: min∠ %.1f°→%.1f°  avg∠ %.1f°→%.1f°  avgAR %.2f→%.2f  %1 tris")
            .arg(initial_stats.min_angle).arg(final_stats.min_angle)
            .arg(initial_stats.avg_angle).arg(final_stats.avg_angle)
            .arg(initial_stats.avg_aspect).arg(final_stats.avg_aspect)
            .arg(triangles.size()),
        15000);
}

void MeshViewer::onDelaunay3DSurface()
{
    // Determine point source: 3D point cloud first, then geometry vertices
    std::vector<Point3D> points;

    if (pc3d_ && !pc3d_->empty()) {
        points.reserve(pc3d_->numPoints());
        for (int i = 0; i < pc3d_->numPoints(); ++i) {
            float x, y, z;
            pc3d_->getPoint(i, x, y, z);
            points.push_back({static_cast<double>(x),
                              static_cast<double>(y),
                              static_cast<double>(z)});
        }
    } else if (geometry_ && !geometry_->empty()) {
        const auto& verts = geometry_->vertices();
        points.reserve(geometry_->numVertices());
        for (int i = 0; i < geometry_->numVertices(); ++i) {
            points.push_back({static_cast<double>(verts[i * 3]),
                              static_cast<double>(verts[i * 3 + 1]),
                              static_cast<double>(verts[i * 3 + 2])});
        }
    } else {
        statusBar()->showMessage(QStringLiteral("No data — create/import a 3D point cloud or load a mesh first"), 5000);
        return;
    }

    if (static_cast<int>(points.size()) < 4) {
        statusBar()->showMessage(QStringLiteral("Need at least 4 points for 3D Delaunay tetrahedralization"), 5000);
        return;
    }

    // Run Delaunay3D tetrahedralization
    Delaunay3D delaunay(std::move(points), Delaunay3D::Strategy::Optimized);
    auto tets = delaunay.tetrahedralize();

    if (tets.empty()) {
        statusBar()->showMessage(QStringLiteral("3D Delaunay tetrahedralization produced no tetrahedra"), 5000);
        return;
    }

    // Convert to MeshData (extract surface triangles)
    MeshData mesh;
    read_3d_mesh(delaunay.points(), tets, mesh);

    if (mesh.indices.empty()) {
        statusBar()->showMessage(QStringLiteral("3D Delaunay: no surface triangles extracted"), 5000);
        return;
    }

    renderer_->loadMesh(mesh);

    statusBar()->showMessage(
        QStringLiteral("3D Surface Delaunay: %1 points → %2 tets → %3 surface tris")
            .arg(delaunay.points().size())
            .arg(tets.size())
            .arg(mesh.num_triangles()),
        10000);
}

void MeshViewer::onDelaunay3DVolume()
{
    // Determine point source
    std::vector<Point3D> points;

    if (pc3d_ && !pc3d_->empty()) {
        points.reserve(pc3d_->numPoints());
        for (int i = 0; i < pc3d_->numPoints(); ++i) {
            float x, y, z;
            pc3d_->getPoint(i, x, y, z);
            points.push_back({static_cast<double>(x),
                              static_cast<double>(y),
                              static_cast<double>(z)});
        }
    } else if (geometry_ && !geometry_->empty()) {
        const auto& verts = geometry_->vertices();
        points.reserve(geometry_->numVertices());
        for (int i = 0; i < geometry_->numVertices(); ++i) {
            points.push_back({static_cast<double>(verts[i * 3]),
                              static_cast<double>(verts[i * 3 + 1]),
                              static_cast<double>(verts[i * 3 + 2])});
        }
    } else {
        statusBar()->showMessage(QStringLiteral("No data — create/import a 3D point cloud or load a mesh first"), 5000);
        return;
    }

    if (static_cast<int>(points.size()) < 4) {
        statusBar()->showMessage(QStringLiteral("Need at least 4 points for 3D Delaunay tetrahedralization"), 5000);
        return;
    }

    // Run Delaunay3D tetrahedralization
    Delaunay3D delaunay(std::move(points), Delaunay3D::Strategy::Optimized);
    auto tets = delaunay.tetrahedralize();

    if (tets.empty()) {
        statusBar()->showMessage(QStringLiteral("3D Delaunay tetrahedralization produced no tetrahedra"), 5000);
        return;
    }

    // Volume mode: render ALL tetrahedron faces (including interior)
    MeshData mesh;
    {
        const auto& pts = delaunay.points();
        mesh.vertices.reserve(pts.size() * 3);
        for (const auto& p : pts) {
            mesh.vertices.push_back(static_cast<float>(p.x));
            mesh.vertices.push_back(static_cast<float>(p.y));
            mesh.vertices.push_back(static_cast<float>(p.z));
        }
        mesh.indices.reserve(tets.size() * 12);  // 4 faces × 3 indices
        for (const auto& tet : tets) {
            // All 4 faces (interior faces will be depth-tested)
            auto add_tri = [&](int a, int b, int c) {
                mesh.indices.push_back(static_cast<unsigned int>(a));
                mesh.indices.push_back(static_cast<unsigned int>(b));
                mesh.indices.push_back(static_cast<unsigned int>(c));
            };
            add_tri(tet.v0, tet.v1, tet.v2);
            add_tri(tet.v0, tet.v1, tet.v3);
            add_tri(tet.v0, tet.v2, tet.v3);
            add_tri(tet.v1, tet.v2, tet.v3);
        }
    }

    renderer_->loadMesh(mesh);

    statusBar()->showMessage(
        QStringLiteral("3D Volume Delaunay: %1 points → %2 tets → %3 faces")
            .arg(delaunay.points().size())
            .arg(tets.size())
            .arg(mesh.num_triangles()),
        10000);
}

void MeshViewer::onDelaunay3DSurfaceOptimize()
{
    // Same as surface, uses Delaunay3D::Strategy::Optimized (already default)
    onDelaunay3DSurface();
    statusBar()->showMessage(
        QStringLiteral("3D Surface Delaunay (optimized) completed"),
        5000);
}

void MeshViewer::onDelaunay3DVolumeOptimize()
{
    // Same as volume, uses Delaunay3D::Strategy::Optimized (already default)
    onDelaunay3DVolume();
    statusBar()->showMessage(
        QStringLiteral("3D Volume Delaunay (optimized) completed"),
        5000);
}
