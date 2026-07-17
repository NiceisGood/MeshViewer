#ifndef MESHVIEWER_H
#define MESHVIEWER_H

#include <QMainWindow>
#include <QLabel>
#include <QCheckBox>

class MeshRenderer;

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
    void onMeshInfoChanged(const QString& info);

private:
    void createMenus();
    void createStatusBar();

    MeshRenderer* renderer_;
    QLabel* info_label_;
};

#endif // MESHVIEWER_H
