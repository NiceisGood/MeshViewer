#ifndef MESHRENDERER_H
#define MESHRENDERER_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QMatrix4x4>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QVector3D>

#include "MeshReader.h"

#include <vector>

// -----------------------------------------------------------------------
// MeshRenderer — QOpenGLWidget for 3D mesh display with arcball camera.
// -----------------------------------------------------------------------

class MeshRenderer : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core
{
    Q_OBJECT

public:
    /// Display mode enumeration.
    enum DisplayMode {
        Solid = 0,          ///< Solid fill only (default).
        Wireframe,          ///< Wireframe only (triangulated).
        WireframeSolid,     ///< Solid fill + triangulated wireframe overlay.
        QuadWireframe,      ///< Octree cell quad wireframe only.
        QuadWireframeSolid  ///< Solid fill + octree cell quad wireframe overlay.
    };
    Q_ENUM(DisplayMode)

    /// Projection mode enumeration.
    enum ProjectionMode {
        Perspective = 0,    ///< Perspective projection.
        Orthographic        ///< Orthographic projection.
    };
    Q_ENUM(ProjectionMode)

    explicit MeshRenderer(QWidget* parent = nullptr);
    ~MeshRenderer();

    /// Load a mesh for display.
    void loadMesh(const MeshData& mesh);

    /// Clear the current mesh.
    void clearMesh();

    /// Reset camera to default view (preserves mesh).
    void resetView();

    /// Set display mode.
    void setDisplayMode(DisplayMode mode) { display_mode_ = mode; update(); }
    DisplayMode displayMode() const { return display_mode_; }

    /// Set projection mode.
    void setProjectionMode(ProjectionMode mode);
    ProjectionMode projectionMode() const { return projection_mode_; }

    /// Get the current mesh info string.
    QString meshInfo() const;

signals:
    void meshInfoChanged(const QString& info);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void setupShaders();
    void buildBuffers();

    // Arcball helpers
    QVector3D arcballVector(const QPointF& p) const;
    QVector3D computeCenter() const;

    // Mesh data
    MeshData mesh_;

    // OpenGL resources — raw GLuint for core profile
    GLuint vao_ = 0;
    GLuint wireframe_vao_ = 0;
    GLuint quad_vao_ = 0;       // quad wireframe VAO
    GLuint vbo_ = 0;
    GLuint ebo_ = 0;
    GLuint wireframe_ebo_ = 0;
    GLuint quad_ebo_ = 0;       // quad wireframe index buffer
    QOpenGLShaderProgram* shader_ = nullptr;
    QOpenGLShaderProgram* wire_shader_ = nullptr;

    // Camera state
    QMatrix4x4 projection_;
    QMatrix4x4 view_;
    QMatrix4x4 model_;
    QVector3D center_{0.0f, 0.0f, 0.0f};
    float rotation_quat_[4] = {1.0f, 0.0f, 0.0f, 0.0f};  // w, x, y, z
    float pan_x_ = 0.0f, pan_y_ = 0.0f;
    float zoom_ = 1.0f;
    int quad_line_count_ = 0;  // number of quad wireframe line indices

    // Interaction state
    bool dragging_ = false;
    QPointF last_mouse_;
    QPointF press_pos_;      // arcball press position (used as va reference)
    int last_button_ = Qt::NoButton;
    float prev_quat_[4] = {1.0f, 0.0f, 0.0f, 0.0f};

    // Display settings
    DisplayMode display_mode_ = Solid;
    ProjectionMode projection_mode_ = Orthographic;
    bool initialized_ = false;
};

#endif // MESHRENDERER_H
