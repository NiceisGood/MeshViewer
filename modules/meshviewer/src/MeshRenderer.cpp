#include "MeshRenderer.h"

#include <QOpenGLShader>
#include <cmath>
#include <cstring>

// =======================================================================
//  Construction / Destruction
// =======================================================================

MeshRenderer::MeshRenderer(QWidget* parent)
    : QOpenGLWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(false);
}

MeshRenderer::~MeshRenderer()
{
    makeCurrent();
    vao_.destroy();
    wireframe_vao_.destroy();
    vbo_.destroy();
    ebo_.destroy();
    wireframe_ebo_.destroy();
    delete shader_;
    delete wire_shader_;
    doneCurrent();
}

// =======================================================================
//  Mesh loading
// =======================================================================

void MeshRenderer::loadMesh(const MeshData& mesh)
{
    mesh_ = mesh;
    if (initialized_) {
        buildBuffers();
        update();
    }
    emit meshInfoChanged(meshInfo());
}

void MeshRenderer::clearMesh()
{
    mesh_.clear();
    if (initialized_) {
        buildBuffers();
        update();
    }
    emit meshInfoChanged(meshInfo());
}

QString MeshRenderer::meshInfo() const
{
    if (mesh_.empty())
        return QStringLiteral("No mesh loaded");
    return QStringLiteral("%1 triangles, %2 vertices")
        .arg(mesh_.num_triangles())
        .arg(mesh_.num_vertices());
}

// =======================================================================
//  OpenGL initialization
// =======================================================================

void MeshRenderer::initializeGL()
{
    initializeOpenGLFunctions();
    glClearColor(0.15f, 0.15f, 0.18f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    setupShaders();

    vao_.create();
    wireframe_vao_.create();
    vbo_.create();
    ebo_.create();
    wireframe_ebo_.create();

    if (!mesh_.empty())
        buildBuffers();

    initialized_ = true;

    // Reset camera
    zoom_ = 1.0f;
    pan_x_ = pan_y_ = 0.0f;
    rotation_quat_[0] = 1.0f;
    rotation_quat_[1] = rotation_quat_[2] = rotation_quat_[3] = 0.0f;
}

void MeshRenderer::resizeGL(int w, int h)
{
    float aspect = (h > 0) ? static_cast<float>(w) / h : 1.0f;
    projection_.setToIdentity();
    projection_.perspective(45.0f, aspect, 0.01f, 1000.0f);
}

void MeshRenderer::setupShaders()
{
    // --- Solid shading shader ---
    {
        const char* vs_src = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uMVP;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";
        const char* fs_src = R"(
#version 330 core
out vec4 fragColor;
uniform vec3 uColor = vec3(0.7, 0.7, 0.8);
void main() {
    fragColor = vec4(uColor, 1.0);
}
)";

        QOpenGLShader* vs = new QOpenGLShader(QOpenGLShader::Vertex, this);
        vs->compileSourceCode(vs_src);
        QOpenGLShader* fs = new QOpenGLShader(QOpenGLShader::Fragment, this);
        fs->compileSourceCode(fs_src);

        shader_ = new QOpenGLShaderProgram(this);
        shader_->addShader(vs);
        shader_->addShader(fs);
        shader_->link();
    }

    // --- Wireframe shader ---
    {
        const char* vs_src = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uMVP;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";
        const char* fs_src = R"(
#version 330 core
out vec4 fragColor;
uniform vec3 uColor = vec3(0.2, 0.6, 1.0);
void main() {
    fragColor = vec4(uColor, 1.0);
}
)";

        QOpenGLShader* vs = new QOpenGLShader(QOpenGLShader::Vertex, this);
        vs->compileSourceCode(vs_src);
        QOpenGLShader* fs = new QOpenGLShader(QOpenGLShader::Fragment, this);
        fs->compileSourceCode(fs_src);

        wire_shader_ = new QOpenGLShaderProgram(this);
        wire_shader_->addShader(vs);
        wire_shader_->addShader(fs);
        wire_shader_->link();
    }
}

void MeshRenderer::buildBuffers()
{
    if (mesh_.empty()) return;

    // --- Solid mesh VAO ---
    vao_.bind();
    vbo_.bind();
    vbo_.allocate(mesh_.vertices.data(),
                  mesh_.vertices.size() * sizeof(float));

    ebo_.bind();
    ebo_.allocate(mesh_.indices.data(),
                  mesh_.indices.size() * sizeof(unsigned int));

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    vao_.release();

    // --- Wireframe VAO (line segments) ---
    // Build line list from triangle indices (3 edges per triangle)
    std::vector<unsigned int> line_indices;
    line_indices.reserve(mesh_.indices.size() * 2);
    for (size_t i = 0; i < mesh_.indices.size(); i += 3) {
        unsigned int i0 = mesh_.indices[i];
        unsigned int i1 = mesh_.indices[i + 1];
        unsigned int i2 = mesh_.indices[i + 2];
        line_indices.push_back(i0);
        line_indices.push_back(i1);
        line_indices.push_back(i1);
        line_indices.push_back(i2);
        line_indices.push_back(i2);
        line_indices.push_back(i0);
    }

    wireframe_vao_.bind();
    vbo_.bind();  // share vertex buffer
    wireframe_ebo_.bind();
    wireframe_ebo_.allocate(line_indices.data(),
                            line_indices.size() * sizeof(unsigned int));

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    wireframe_vao_.release();
}

// =======================================================================
//  Arcball camera helpers
// =======================================================================

QVector3D MeshRenderer::arcballVector(const QPointF& p) const
{
    float w = static_cast<float>(width());
    float h = static_cast<float>(height());
    float x = (2.0f * static_cast<float>(p.x()) - w) / w;
    float y = -(2.0f * static_cast<float>(p.y()) - h) / h;
    float len2 = x * x + y * y;
    float z = (len2 < 1.0f) ? std::sqrt(1.0f - len2) : 0.0f;
    return QVector3D(x, y, z).normalized();
}

// =======================================================================
//  Mouse / wheel events
// =======================================================================

void MeshRenderer::mousePressEvent(QMouseEvent* event)
{
    last_mouse_ = event->pos();
    last_button_ = event->button();
    dragging_ = true;

    if (event->button() == Qt::LeftButton) {
        std::memcpy(prev_quat_, rotation_quat_, sizeof(prev_quat_));
    }
}

void MeshRenderer::mouseMoveEvent(QMouseEvent* event)
{
    if (!dragging_) return;

    QPointF current = event->pos();
    float dx = static_cast<float>(current.x() - last_mouse_.x());
    float dy = static_cast<float>(current.y() - last_mouse_.y());

    if (last_button_ == Qt::LeftButton) {
        // Arcball rotation
        QVector3D va = arcballVector(last_mouse_);
        QVector3D vb = arcballVector(current);
        float dot = QVector3D::dotProduct(va, vb);
        float angle = std::acos(std::min(1.0f, std::max(-1.0f, dot)));
        if (angle > 0.001f) {
            QVector3D axis = QVector3D::crossProduct(va, vb).normalized();
            float s = std::sin(angle * 0.5f);
            float qw = std::cos(angle * 0.5f);
            float qx = axis.x() * s;
            float qy = axis.y() * s;
            float qz = axis.z() * s;
            // q * prev_quat
            float rw = qw * prev_quat_[0] - qx * prev_quat_[1] - qy * prev_quat_[2] - qz * prev_quat_[3];
            float rx = qw * prev_quat_[1] + qx * prev_quat_[0] + qy * prev_quat_[3] - qz * prev_quat_[2];
            float ry = qw * prev_quat_[2] - qx * prev_quat_[3] + qy * prev_quat_[0] + qz * prev_quat_[1];
            float rz = qw * prev_quat_[3] + qx * prev_quat_[2] - qy * prev_quat_[1] + qz * prev_quat_[0];
            rotation_quat_[0] = rw; rotation_quat_[1] = rx;
            rotation_quat_[2] = ry; rotation_quat_[3] = rz;
        }
    } else if (last_button_ == Qt::RightButton) {
        // Pan
        float scale = 0.01f * zoom_;
        pan_x_ += dx * scale;
        pan_y_ -= dy * scale;
    }

    last_mouse_ = current;
    update();
}

void MeshRenderer::mouseReleaseEvent(QMouseEvent*)
{
    dragging_ = false;
}

void MeshRenderer::wheelEvent(QWheelEvent* event)
{
    float delta = static_cast<float>(event->angleDelta().y()) / 120.0f;
    zoom_ *= std::pow(0.85f, delta);
    zoom_ = std::max(0.01f, std::min(100.0f, zoom_));
    update();
}

// =======================================================================
//  Rendering
// =======================================================================

void MeshRenderer::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (mesh_.empty()) return;

    // Build view matrix
    view_.setToIdentity();
    // Apply zoom (translate back)
    view_.translate(0.0f, 0.0f, -3.0f * zoom_);
    // Apply pan
    view_.translate(pan_x_, pan_y_, 0.0f);

    // Build rotation matrix from quaternion
    QMatrix4x4 rot;
    float qw = rotation_quat_[0], qx = rotation_quat_[1];
    float qy = rotation_quat_[2], qz = rotation_quat_[3];
    rot(0,0) = 1 - 2*(qy*qy + qz*qz);
    rot(0,1) = 2*(qx*qy + qz*qw);
    rot(0,2) = 2*(qx*qz - qy*qw);
    rot(1,0) = 2*(qx*qy - qz*qw);
    rot(1,1) = 1 - 2*(qx*qx + qz*qz);
    rot(1,2) = 2*(qy*qz + qx*qw);
    rot(2,0) = 2*(qx*qz + qy*qw);
    rot(2,1) = 2*(qy*qz - qx*qw);
    rot(2,2) = 1 - 2*(qx*qx + qy*qy);

    model_.setToIdentity();
    model_ = rot;

    QMatrix4x4 mvp = projection_ * view_ * model_;

    // --- Solid fill ---
    shader_->bind();
    shader_->setUniformValue("uMVP", mvp);
    shader_->setUniformValue("uColor", QVector3D(0.65f, 0.65f, 0.75f));

    vao_.bind();
    glDrawElements(GL_TRIANGLES,
                   static_cast<int>(mesh_.indices.size()),
                   GL_UNSIGNED_INT, nullptr);
    vao_.release();
    shader_->release();

    // --- Wireframe overlay ---
    if (wireframe_) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glEnable(GL_POLYGON_OFFSET_LINE);
        glPolygonOffset(-1.0f, -1.0f);
        glLineWidth(1.5f);

        wire_shader_->bind();
        wire_shader_->setUniformValue("uMVP", mvp);
        wire_shader_->setUniformValue("uColor", QVector3D(0.2f, 0.6f, 1.0f));

        wireframe_vao_.bind();
        glDrawElements(GL_LINES,
                       static_cast<int>(mesh_.indices.size() * 2),
                       GL_UNSIGNED_INT, nullptr);
        wireframe_vao_.release();
        wire_shader_->release();

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDisable(GL_POLYGON_OFFSET_LINE);
    }
}
