#include "MeshViewer.h"
#include "MeshRenderer.h"
#include "MeshReader.h"

#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QStatusBar>
#include <QCheckBox>
#include <QApplication>

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
}

MeshViewer::~MeshViewer() = default;

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

    file_menu->addSeparator();

    QAction* quit_act = file_menu->addAction(QStringLiteral("&Quit"));
    quit_act->setShortcut(QKeySequence::Quit);
    connect(quit_act, &QAction::triggered, qApp, &QApplication::quit);

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

    QAction* reset_act = view_menu->addAction(QStringLiteral("&Reset View"));
    reset_act->setShortcut(QKeySequence(Qt::Key_R));
    connect(reset_act, &QAction::triggered, [this]() {
        renderer_->resetView();
    });
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
