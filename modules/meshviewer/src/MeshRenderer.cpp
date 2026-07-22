#include "MeshRenderer.h"

#include <QOpenGLShader>
#include <algorithm>
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
    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (wireframe_vao_) glDeleteVertexArrays(1, &wireframe_vao_);
    if (quad_vao_) glDeleteVertexArrays(1, &quad_vao_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (ebo_) glDeleteBuffers(1, &ebo_);
    if (wireframe_ebo_) glDeleteBuffers(1, &wireframe_ebo_);
    if (quad_ebo_) glDeleteBuffers(1, &quad_ebo_);
    if (slice_vao_) glDeleteVertexArrays(1, &slice_vao_);
    if (slice_vbo_) glDeleteBuffers(1, &slice_vbo_);
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
    center_ = computeCenter();
    // Compute bounding box diagonal for near/far plane adjustment
    {
        float minX, maxX, minY, maxY, minZ, maxZ;
        if (!mesh_.vertices.empty()) {
            minX = maxX = mesh_.vertices[0];
            minY = maxY = mesh_.vertices[1];
            minZ = maxZ = mesh_.vertices[2];
            for (size_t i = 3; i < mesh_.vertices.size(); i += 3) {
                minX = std::min(minX, mesh_.vertices[i]);
                maxX = std::max(maxX, mesh_.vertices[i]);
                minY = std::min(minY, mesh_.vertices[i + 1]);
                maxY = std::max(maxY, mesh_.vertices[i + 1]);
                minZ = std::min(minZ, mesh_.vertices[i + 2]);
                maxZ = std::max(maxZ, mesh_.vertices[i + 2]);
            }
        } else {
            minX = maxX = minY = maxY = minZ = maxZ = 0.0f;
        }
        float dx = maxX - minX;
        float dy = maxY - minY;
        float dz = maxZ - minZ;
        model_diag_ = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (model_diag_ < 1e-6f) model_diag_ = 1.0f;
    }
    resetView();
    slice_dirty_ = true;
    if (initialized_) {
        makeCurrent();
        buildBuffers();
        doneCurrent();
        update();
    }
    emit meshInfoChanged(meshInfo());
}

void MeshRenderer::clearMesh()
{
    mesh_.clear();
    if (initialized_) {
        makeCurrent();
        buildBuffers();
        doneCurrent();
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
    glDepthFunc(GL_LEQUAL);
    // Note: glCullFace intentionally disabled — the 6-tet decomposition
    // produces tetrahedron faces with mixed winding on each cube face.
    // One triangle per face is CW (culled) and the other is CCW (visible),
    // causing holes when zoomed out.  Depth test handles occlusion correctly
    // for closed 2-manifold surfaces without culling.
    glDisable(GL_CULL_FACE);

    setupShaders();

    glGenVertexArrays(1, &vao_);
    glGenVertexArrays(1, &wireframe_vao_);
    glGenVertexArrays(1, &quad_vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ebo_);
    glGenBuffers(1, &wireframe_ebo_);
    glGenBuffers(1, &quad_ebo_);
    glGenVertexArrays(1, &slice_vao_);
    glGenBuffers(1, &slice_vbo_);

    if (!mesh_.empty())
        buildBuffers();

    initialized_ = true;

    // Reset camera
    zoom_ = 1.0f;
    pan_x_ = pan_y_ = 0.0f;
    center_ = QVector3D(0.0f, 0.0f, 0.0f);
    rotation_quat_[0] = 1.0f;
    rotation_quat_[1] = rotation_quat_[2] = rotation_quat_[3] = 0.0f;
}

void MeshRenderer::resizeGL(int w, int h)
{
    float aspect = (h > 0) ? static_cast<float>(w) / h : 1.0f;
    // Dynamic near/far based on model size to avoid depth precision issues
    // when zoomed out.  near = 1% of diagonal, far = max(100*diag, 2*camera_dist).
    float near_plane = (3.0f * zoom_ - model_diag_ * 0.5f) * 0.5f;
    if (near_plane < 0.001f) near_plane = 0.001f;
    float far_plane = (3.0f * zoom_ + model_diag_ * 0.5f) * 2.0f;
    projection_.setToIdentity();
    if (projection_mode_ == Perspective) {
        projection_.perspective(45.0f, aspect, near_plane, far_plane);
    } else {
        // Orthographic: scale bounds by zoom_ so zoom wheel works
        float size = 1.5f / zoom_;
        projection_.ortho(-size * aspect, size * aspect,
                          -size, size,
                          near_plane, far_plane);
    }
}

void MeshRenderer::setupShaders()
{
    // --- Solid shading shader ---
    {
        const char* vs_src = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uMVP;
uniform vec4 uClipPlane;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
    gl_ClipDistance[0] = dot(uClipPlane, vec4(aPos, 1.0));
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
uniform vec4 uClipPlane;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
    gl_ClipDistance[0] = dot(uClipPlane, vec4(aPos, 1.0));
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
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 mesh_.vertices.size() * sizeof(float),
                 mesh_.vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 mesh_.indices.size() * sizeof(unsigned int),
                 mesh_.indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

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

    glBindVertexArray(wireframe_vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);  // share vertex buffer
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, wireframe_ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 line_indices.size() * sizeof(unsigned int),
                 line_indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    // --- Quad wireframe VAO (line segments from quad faces) ---
    quad_line_count_ = 0;
    if (!mesh_.quad_indices.empty()) {
        // Each quad → 4 line segments (8 indices)
        std::vector<unsigned int> quad_lines;
        quad_lines.reserve(mesh_.quad_indices.size() / 4 * 8);
        for (size_t i = 0; i < mesh_.quad_indices.size(); i += 4) {
            unsigned int a = mesh_.quad_indices[i];
            unsigned int b = mesh_.quad_indices[i + 1];
            unsigned int c = mesh_.quad_indices[i + 2];
            unsigned int d = mesh_.quad_indices[i + 3];
            quad_lines.push_back(a);
            quad_lines.push_back(b);
            quad_lines.push_back(b);
            quad_lines.push_back(c);
            quad_lines.push_back(c);
            quad_lines.push_back(d);
            quad_lines.push_back(d);
            quad_lines.push_back(a);
        }
        quad_line_count_ = static_cast<int>(quad_lines.size());

        glBindVertexArray(quad_vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);  // share vertex buffer
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quad_ebo_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     quad_lines.size() * sizeof(unsigned int),
                     quad_lines.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);
    }
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

QVector3D MeshRenderer::computeCenter() const
{
    if (mesh_.vertices.empty())
        return QVector3D(0.0f, 0.0f, 0.0f);

    float minX = mesh_.vertices[0], maxX = mesh_.vertices[0];
    float minY = mesh_.vertices[1], maxY = mesh_.vertices[1];
    float minZ = mesh_.vertices[2], maxZ = mesh_.vertices[2];

    for (size_t i = 3; i < mesh_.vertices.size(); i += 3) {
        minX = std::min(minX, mesh_.vertices[i]);
        maxX = std::max(maxX, mesh_.vertices[i]);
        minY = std::min(minY, mesh_.vertices[i + 1]);
        maxY = std::max(maxY, mesh_.vertices[i + 1]);
        minZ = std::min(minZ, mesh_.vertices[i + 2]);
        maxZ = std::max(maxZ, mesh_.vertices[i + 2]);
    }

    return QVector3D((minX + maxX) * 0.5f,
                     (minY + maxY) * 0.5f,
                     (minZ + maxZ) * 0.5f);
}

void MeshRenderer::resetView()
{
    zoom_ = 1.0f;
    pan_x_ = pan_y_ = 0.0f;
    rotation_quat_[0] = 1.0f;
    rotation_quat_[1] = rotation_quat_[2] = rotation_quat_[3] = 0.0f;
    resizeGL(width(), height());
    update();
}

void MeshRenderer::setProjectionMode(ProjectionMode mode)
{
    projection_mode_ = mode;
    // Rebuild projection matrix with the new mode
    int w = width();
    int h = height();
    if (w > 0 && h > 0)
        resizeGL(w, h);
    update();
}

// =======================================================================
//  Slice plane
// =======================================================================

void MeshRenderer::setSlicePosition(float pos)
{
    slice_pos_ = pos;
    slice_dirty_ = true;
    update();
}

void MeshRenderer::setSliceNormal(const QVector3D& n)
{
    slice_normal_ = n;
    slice_dirty_ = true;
    update();
}

void MeshRenderer::computeSliceContour()
{
    slice_contour_verts_.clear();

    if (mesh_.indices.empty()) return;

    const float* verts = mesh_.vertices.data();
    const float nx = slice_normal_.x();
    const float ny = slice_normal_.y();
    const float nz = slice_normal_.z();
    const float d  = slice_pos_;  // plane: dot(n, v) == d

    // Temporary buffer for line segments (6 floats per segment = 2 vertices)
    std::vector<float> segments;
    segments.reserve(mesh_.indices.size() / 3 * 6);  // upper bound

    for (size_t i = 0; i < mesh_.indices.size(); i += 3) {
        unsigned int ia = mesh_.indices[i];
        unsigned int ib = mesh_.indices[i + 1];
        unsigned int ic = mesh_.indices[i + 2];

        // Vertex positions
        float ax = verts[ia * 3],     ay = verts[ia * 3 + 1], az = verts[ia * 3 + 2];
        float bx = verts[ib * 3],     by = verts[ib * 3 + 1], bz = verts[ib * 3 + 2];
        float cx = verts[ic * 3],     cy = verts[ic * 3 + 1], cz = verts[ic * 3 + 2];

        // Signed distances to plane (positive = above/near normal direction)
        float da = nx * ax + ny * ay + nz * az - d;
        float db = nx * bx + ny * by + nz * bz - d;
        float dc = nx * cx + ny * cy + nz * cz - d;

        // Quick skip if all same sign (no intersection)
        bool a_pos = (da >= 0.0f);
        bool b_pos = (db >= 0.0f);
        bool c_pos = (dc >= 0.0f);
        int pos_count = (a_pos ? 1 : 0) + (b_pos ? 1 : 0) + (c_pos ? 1 : 0);
        if (pos_count == 0 || pos_count == 3)
            continue;

        // Lambda: intersect edge v0->v1, store the intersection point
        auto intersect_edge = [&](float v0x, float v0y, float v0z, float d0,
                                   float v1x, float v1y, float v1z, float d1) {
            // Edge crosses plane when d0 and d1 have opposite signs
            float t = d0 / (d0 - d1);  // lerp factor
            segments.push_back(v0x + t * (v1x - v0x));
            segments.push_back(v0y + t * (v1y - v0y));
            segments.push_back(v0z + t * (v1z - v0z));
        };

        // Collect crossing edges — always exactly 2 intersection points
        // (one edge has the solo vertex on one side)
        if (pos_count == 1) {
            // One vertex on positive side, two on negative → 2 intersecions
            if (a_pos) {
                intersect_edge(ax, ay, az, da, bx, by, bz, db);
                intersect_edge(ax, ay, az, da, cx, cy, cz, dc);
            } else if (b_pos) {
                intersect_edge(bx, by, bz, db, ax, ay, az, da);
                intersect_edge(bx, by, bz, db, cx, cy, cz, dc);
            } else {
                intersect_edge(cx, cy, cz, dc, ax, ay, az, da);
                intersect_edge(cx, cy, cz, dc, bx, by, bz, db);
            }
        } else {
            // Two vertices on positive side, one on negative → 2 intersections
            // (still 2 intersection points, on the 2 edges from the solo vertex)
            if (!a_pos) {
                intersect_edge(ax, ay, az, da, bx, by, bz, db);
                intersect_edge(ax, ay, az, da, cx, cy, cz, dc);
            } else if (!b_pos) {
                intersect_edge(bx, by, bz, db, ax, ay, az, da);
                intersect_edge(bx, by, bz, db, cx, cy, cz, dc);
            } else {
                intersect_edge(cx, cy, cz, dc, ax, ay, az, da);
                intersect_edge(cx, cy, cz, dc, bx, by, bz, db);
            }
        }
    }

    slice_contour_verts_.swap(segments);
}

void MeshRenderer::buildSliceBuffers()
{
    if (slice_contour_verts_.empty()) {
        glBindVertexArray(slice_vao_);
        glBindBuffer(GL_ARRAY_BUFFER, slice_vbo_);
        glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
        glBindVertexArray(0);
        return;
    }

    glBindVertexArray(slice_vao_);
    glBindBuffer(GL_ARRAY_BUFFER, slice_vbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 slice_contour_verts_.size() * sizeof(float),
                 slice_contour_verts_.data(),
                 GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

// =======================================================================
//  Mouse / wheel events
// =======================================================================

void MeshRenderer::mousePressEvent(QMouseEvent* event)
{
    last_mouse_ = event->pos();
    press_pos_ = event->pos();  // save for arcball reference
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
        // Arcball rotation — always compute delta from press position
        QVector3D va = arcballVector(press_pos_);
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
        // Pan — scale by camera distance so pan speed is consistent at all zoom levels
        float dist = 3.0f * zoom_;
        float scale = dist * 0.0033f;
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
    // Rebuild projection — near/far depends on zoom_ in both modes
    resizeGL(width(), height());
    update();
}

// =======================================================================
//  Rendering
// =======================================================================

void MeshRenderer::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (mesh_.empty()) return;

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

    // Model matrix: rotate around mesh center
    // model = translate(center) * rotate * translate(-center)
    QMatrix4x4 t_center, t_neg;
    t_center.translate(center_);
    t_neg.translate(-center_);
    model_ = t_center * rot * t_neg;

    // View matrix: look at mesh center + camera distance (zoom) + pan
    view_.setToIdentity();
    view_.translate(pan_x_, pan_y_, 0.0f);
    view_.translate(-center_);
    view_.translate(0.0f, 0.0f, -3.0f * zoom_);

    QMatrix4x4 mvp = projection_ * view_ * model_;

    // ---- Slice plane: clip distance setup (for HalfMesh mode) ----
    if (slice_enabled_ && slice_display_mode_ == HalfMesh) {
        glEnable(GL_CLIP_DISTANCE0);
        QVector4D clip_plane(slice_normal_.x(), slice_normal_.y(),
                             slice_normal_.z(), -slice_pos_);
        shader_->bind();
        shader_->setUniformValue("uClipPlane", clip_plane);
        shader_->release();
        wire_shader_->bind();
        wire_shader_->setUniformValue("uClipPlane", clip_plane);
        wire_shader_->release();
    } else {
        glDisable(GL_CLIP_DISTANCE0);
    }

    // --- Solid fill (skip for ContourOnly) ---
    const bool draw_solid = (display_mode_ == Solid || display_mode_ == WireframeSolid || display_mode_ == QuadWireframeSolid)
                            && slice_display_mode_ != ContourOnly;
    const bool draw_wire = (display_mode_ == Wireframe || display_mode_ == WireframeSolid);
    const bool draw_quad_wire = (display_mode_ == QuadWireframe || display_mode_ == QuadWireframeSolid) && quad_line_count_ > 0;

    // --- Solid fill ---
    if (draw_solid) {
        shader_->bind();
        shader_->setUniformValue("uMVP", mvp);
        shader_->setUniformValue("uColor", QVector3D(0.65f, 0.65f, 0.75f));

        glBindVertexArray(vao_);
        glDrawElements(GL_TRIANGLES,
                       static_cast<int>(mesh_.indices.size()),
                       GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
        shader_->release();
    }

    // --- Wireframe overlay ---
    if (draw_wire) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glEnable(GL_POLYGON_OFFSET_LINE);
        glPolygonOffset(-1.0f, -1.0f);
        glLineWidth(1.5f);

        wire_shader_->bind();
        wire_shader_->setUniformValue("uMVP", mvp);
        wire_shader_->setUniformValue("uColor", QVector3D(0.2f, 0.6f, 1.0f));

        glBindVertexArray(wireframe_vao_);
        glDrawElements(GL_LINES,
                       static_cast<int>(mesh_.indices.size() * 2),
                       GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
        wire_shader_->release();

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDisable(GL_POLYGON_OFFSET_LINE);
    }

    // --- Quad wireframe (octree cell faces) ---
    if (draw_quad_wire) {
        glLineWidth(1.5f);

        wire_shader_->bind();
        wire_shader_->setUniformValue("uMVP", mvp);
        wire_shader_->setUniformValue("uColor", QVector3D(0.2f, 0.8f, 0.4f));  // green

        glBindVertexArray(quad_vao_);
        glDrawElements(GL_LINES,
                       quad_line_count_,
                       GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
        wire_shader_->release();
    }

    // ---- Slice contour overlay (always on top when slice is enabled) ----
    if (slice_enabled_) {
        // Recompute contour if parameters changed
        if (slice_dirty_) {
            computeSliceContour();
            buildSliceBuffers();
            slice_dirty_ = false;
        }

        if (!slice_contour_verts_.empty()) {
            wire_shader_->bind();
            wire_shader_->setUniformValue("uMVP", mvp);
            wire_shader_->setUniformValue("uColor", QVector3D(1.0f, 0.3f, 0.1f));  // bright orange-red

            glLineWidth(3.0f);
            glBindVertexArray(slice_vao_);
            glDrawArrays(GL_LINES, 0,
                         static_cast<int>(slice_contour_verts_.size() / 3));
            glBindVertexArray(0);
            wire_shader_->release();
        }
    }
}
