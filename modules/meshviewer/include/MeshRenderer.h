#ifndef MESHRENDERER_H
#define MESHRENDERER_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>
#include <QOpenGLShaderProgram>
#include <QMatrix4x4>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QVector3D>

#include "MeshReader.h"

// -----------------------------------------------------------------------
// MeshRenderer — QOpenGLWidget for 3D mesh display with arcball camera.
// -----------------------------------------------------------------------

class MeshRenderer : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core
{
    Q_OBJECT

public:
    explicit MeshRenderer(QWidget* parent = nullptr);
    ~MeshRenderer();

    /// Load a mesh for display.
    void loadMesh(const MeshData& mesh);

    /// Clear the current mesh.
    void clearMesh();

    /// Toggle wireframe overlay.
    void setWireframe(bool on) { wireframe_ = on; update(); }
    bool wireframe() const { return wireframe_; }

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

    // Mesh data
    MeshData mesh_;

    // OpenGL resources
    QOpenGLVertexArrayObject vao_;
    QOpenGLVertexArrayObject wireframe_vao_;
    QOpenGLBuffer vbo_;
    QOpenGLBuffer ebo_;
    QOpenGLBuffer wireframe_ebo_;
    QOpenGLShaderProgram* shader_ = nullptr;
    QOpenGLShaderProgram* wire_shader_ = nullptr;

    // Camera state
    QMatrix4x4 projection_;
    QMatrix4x4 view_;
    QMatrix4x4 model_;
    float rotation_quat_[4] = {1.0f, 0.0f, 0.0f, 0.0f};  // w, x, y, z
    float pan_x_ = 0.0f, pan_y_ = 0.0f;
    float zoom_ = 1.0f;

    // Interaction state
    bool dragging_ = false;
    QPointF last_mouse_;
    int last_button_ = Qt::NoButton;
    float prev_quat_[4] = {1.0f, 0.0f, 0.0f, 0.0f};

    // Display settings
    bool wireframe_ = false;
    bool initialized_ = false;
};

#endif // MESHRENDERER_H
