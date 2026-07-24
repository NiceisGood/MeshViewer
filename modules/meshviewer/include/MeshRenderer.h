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

    /// Slice display mode enumeration.
    enum SliceDisplayMode {
        FullMesh = 0,       ///< Full mesh + contour (default).
        HalfMesh,           ///< Keep one half of mesh + contour.
        ContourOnly         ///< Hide mesh, show only slice contour.
    };
    Q_ENUM(SliceDisplayMode)

    explicit MeshRenderer(QWidget* parent = nullptr);
    ~MeshRenderer();

    /// Load a mesh for display.
    void loadMesh(const MeshData& mesh);

    /// Load a 3D point cloud for display (interleaved x,y,z floats).
    /// Clears any previously loaded point cloud.
    void loadPointCloud(const std::vector<float>& points);

    /// Clear the current point cloud.
    void clearPointCloud();

    /// Check if a point cloud is loaded.
    bool hasPointCloud() const { return point_count_ > 0; }

    /// Get point cloud info string.
    QString pointCloudInfo() const;

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

    // ---- Slice plane ----------------------------------------------------
    /// Enable/disable slice contour rendering.
    void setSliceEnabled(bool enabled) { slice_enabled_ = enabled; update(); }
    bool sliceEnabled() const { return slice_enabled_; }

    /// Set slice plane position (distance along normal in model space).
    void setSlicePosition(float pos);
    float slicePosition() const { return slice_pos_; }

    /// Set slice plane normal direction (unit vector in model space).
    void setSliceNormal(const QVector3D& n);
    QVector3D sliceNormal() const { return slice_normal_; }

    /// Get the model-space bounding diagonal (for UI range mapping).
    float modelDiagonal() const { return model_diag_; }

    /// Set slice display mode (ContourOnly / HalfMesh / FullMesh).
    void setSliceDisplayMode(SliceDisplayMode mode) { slice_display_mode_ = mode; update(); }
    SliceDisplayMode sliceDisplayMode() const { return slice_display_mode_; }

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

    // Slice contour computation
    void computeSliceContour();
    void buildSliceBuffers();

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
    float model_diag_ = 1.0f;  // bounding box diagonal for near/far plane
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

    // Slice plane
    bool slice_enabled_ = false;
    float slice_pos_ = 0.0f;
    QVector3D slice_normal_{0.0f, 0.0f, 1.0f};
    SliceDisplayMode slice_display_mode_ = FullMesh;
    bool slice_dirty_ = true;
    std::vector<float> slice_contour_verts_;  // computed contour line vertices
    GLuint slice_vao_ = 0;
    GLuint slice_vbo_ = 0;

    // Point cloud rendering
    GLuint point_vao_ = 0;
    GLuint point_vbo_ = 0;
    int point_count_ = 0;
    float point_cloud_diag_ = 1.0f;  // bounding diagonal for camera framing
    QVector3D point_cloud_center_{0.0f, 0.0f, 0.0f};
};

#endif // MESHRENDERER_H
