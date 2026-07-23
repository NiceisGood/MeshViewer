#ifndef MESHVIEWER_H
#define MESHVIEWER_H

#include <QMainWindow>
#include <QLabel>
#include <QActionGroup>
#include <QDockWidget>
#include <QCheckBox>
#include <QSlider>
#include <QComboBox>
#include <QAction>
#include <QString>

class MeshRenderer;
class Geometry;

// -----------------------------------------------------------------------
// MeshViewer — main window with menu bar, status bar, and OpenGL view.
// -----------------------------------------------------------------------

class MeshViewer : public QMainWindow
{
    Q_OBJECT

public:
    explicit MeshViewer(QWidget* parent = nullptr);
    ~MeshViewer();

    /// Load a mesh file by path (supports .stl, .obj, .qmesh, .qmesh3d).
    /// Returns true on success.
    bool loadFile(const std::string& path);

private slots:
    void onOpenFile();
    void onImportFile();
    void onExportFile();
    void onMeshInfoChanged(const QString& info);
    void onDisplayModeChanged(QAction* action);
    void onSliceToggled(bool enabled);
    void onSlicePosChanged(int value);
    void onSliceNormalChanged(int index);
    void onSliceDisplayModeChanged(int index);

    // ── Delaunay slots ──
    void onDelaunay2D();
    void onDelaunay2DOptimize();
    void onDelaunay3DSurface();
    void onDelaunay3DVolume();
    void onDelaunay3DSurfaceOptimize();
    void onDelaunay3DVolumeOptimize();

private:
    void createMenus();
    void createStatusBar();
    void createSliceDock();

    /// Load a mesh from a Geometry object for display.
    void loadGeometry(const Geometry& geom);

    MeshRenderer* renderer_;
    Geometry* geometry_ = nullptr;  // current geometry data (owned)
    QLabel* info_label_;
    QActionGroup* display_group_ = nullptr;
    QActionGroup* projection_group_ = nullptr;
    QAction* slice_act_ = nullptr;       // View → Slice toggle action
    QDockWidget* slice_dock_ = nullptr;
    QCheckBox* slice_enable_check_ = nullptr;
    QSlider* slice_pos_slider_ = nullptr;
    QComboBox* slice_normal_combo_ = nullptr;
    QComboBox* slice_display_combo_ = nullptr;
};

#endif // MESHVIEWER_H
