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

    QAction* wire_act = view_menu->addAction(QStringLiteral("&Wireframe"));
    wire_act->setCheckable(true);
    wire_act->setChecked(false);
    wire_act->setShortcut(QKeySequence(Qt::Key_W));
    connect(wire_act, &QAction::toggled, renderer_, &MeshRenderer::setWireframe);

    view_menu->addSeparator();

    QAction* reset_act = view_menu->addAction(QStringLiteral("&Reset View"));
    reset_act->setShortcut(QKeySequence(Qt::Key_R));
    connect(reset_act, &QAction::triggered, [this]() {
        renderer_->clearMesh();
        renderer_->loadMesh(MeshData()); // triggers reset
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
        QStringLiteral("Mesh Files (*.stl *.obj);;STL Files (*.stl);;OBJ Files (*.obj);;All Files (*)")
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
