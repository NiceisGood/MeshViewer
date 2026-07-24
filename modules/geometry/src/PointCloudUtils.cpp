#include "PointCloudUtils.h"

#include <random>
#include <chrono>
#include <cmath>
#include <cstdlib>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// =======================================================================
//  Internal helpers
// =======================================================================

/// Seed an RNG.  seed==0 → time-based seed.
static std::mt19937 make_rng(unsigned int seed) {
    if (seed == 0) {
        seed = static_cast<unsigned int>(
            std::chrono::steady_clock::now().time_since_epoch().count());
    }
    return std::mt19937(seed);
}

// =======================================================================
//  generateRandom2D (Bounding Box)
// =======================================================================

PointCloud2D PointCloudUtils::generateRandom2D(
    int count,
    float xMin, float xMax,
    float yMin, float yMax,
    bool onlyOnBoundary,
    unsigned int seed)
{
    if (count <= 0)
        return PointCloud2D{};

    std::mt19937 rng = make_rng(seed);
    std::uniform_real_distribution<float> dist01(0.0f, 1.0f);

    std::vector<float> pts;
    pts.reserve(count * 2);

    if (onlyOnBoundary) {
        // Distribute points uniformly along the perimeter
        float dx = xMax - xMin;
        float dy = yMax - yMin;
        float perim = 2.0f * (dx + dy);

        for (int i = 0; i < count; ++i) {
            float t = dist01(rng) * perim;
            if (t < dx) {
                // bottom edge (left→right)
                pts.push_back(xMin + t);
                pts.push_back(yMin);
            } else if (t < dx + dy) {
                // right edge (bottom→top)
                pts.push_back(xMax);
                pts.push_back(yMin + (t - dx));
            } else if (t < dx + dx + dy) {
                // top edge (right→left)
                pts.push_back(xMax - (t - dx - dy));
                pts.push_back(yMax);
            } else {
                // left edge (top→bottom)
                pts.push_back(xMin);
                pts.push_back(yMax - (t - dx - dx - dy));
            }
        }
    } else {
        // Uniform inside rectangle
        std::uniform_real_distribution<float> dist_x(xMin, xMax);
        std::uniform_real_distribution<float> dist_y(yMin, yMax);
        for (int i = 0; i < count; ++i) {
            pts.push_back(dist_x(rng));
            pts.push_back(dist_y(rng));
        }
    }

    return PointCloud2D(std::move(pts));
}

// =======================================================================
//  generateRandom3D (Bounding Box)
// =======================================================================

PointCloud3D PointCloudUtils::generateRandom3D(
    int count,
    float xMin, float xMax,
    float yMin, float yMax,
    float zMin, float zMax,
    bool onlyOnBoundary,
    unsigned int seed)
{
    if (count <= 0)
        return PointCloud3D{};

    std::mt19937 rng = make_rng(seed);
    std::uniform_real_distribution<float> dist01(0.0f, 1.0f);

    std::vector<float> pts;
    pts.reserve(count * 3);

    if (onlyOnBoundary) {
        // Distribute points uniformly on the box surface (6 faces)
        float dx = xMax - xMin;
        float dy = yMax - yMin;
        float dz = zMax - zMin;
        float face_xy = dx * dy;
        float face_xz = dx * dz;
        float face_yz = dy * dz;
        float total_area = 2.0f * (face_xy + face_xz + face_yz);

        for (int i = 0; i < count; ++i) {
            float u = dist01(rng);
            float v = dist01(rng);
            float t = dist01(rng) * total_area;

            float x, y, z;
            if (t < face_xy) {
                // -Z face (bottom)
                x = xMin + u * dx;
                y = yMin + v * dy;
                z = zMin;
            } else if (t < 2.0f * face_xy) {
                // +Z face (top)
                x = xMin + u * dx;
                y = yMin + v * dy;
                z = zMax;
            } else if (t < 2.0f * face_xy + face_xz) {
                // -Y face
                x = xMin + u * dx;
                y = yMin;
                z = zMin + v * dz;
            } else if (t < 2.0f * face_xy + 2.0f * face_xz) {
                // +Y face
                x = xMin + u * dx;
                y = yMax;
                z = zMin + v * dz;
            } else if (t < 2.0f * face_xy + 2.0f * face_xz + face_yz) {
                // -X face
                x = xMin;
                y = yMin + u * dy;
                z = zMin + v * dz;
            } else {
                // +X face
                x = xMax;
                y = yMin + u * dy;
                z = zMin + v * dz;
            }

            pts.push_back(x);
            pts.push_back(y);
            pts.push_back(z);
        }
    } else {
        // Uniform inside box
        std::uniform_real_distribution<float> dist_x(xMin, xMax);
        std::uniform_real_distribution<float> dist_y(yMin, yMax);
        std::uniform_real_distribution<float> dist_z(zMin, zMax);
        for (int i = 0; i < count; ++i) {
            pts.push_back(dist_x(rng));
            pts.push_back(dist_y(rng));
            pts.push_back(dist_z(rng));
        }
    }

    return PointCloud3D(std::move(pts));
}

// =======================================================================
//  generateRandomInCircle (disc) — uniform area distribution
//  r = R * sqrt(u),  theta = 2*PI * v
// =======================================================================

PointCloud2D PointCloudUtils::generateRandomInCircle(
    int count,
    float cx, float cy,
    float radius,
    bool onlyOnBoundary,
    unsigned int seed)
{
    if (count <= 0 || radius <= 0.0f)
        return PointCloud2D{};

    std::mt19937 rng = make_rng(seed);
    std::uniform_real_distribution<float> dist01(0.0f, 1.0f);

    std::vector<float> pts;
    pts.reserve(count * 2);

    for (int i = 0; i < count; ++i) {
        float u = dist01(rng);
        float v = dist01(rng);
        float r = onlyOnBoundary ? radius : (radius * std::sqrt(u));
        float theta = 2.0f * static_cast<float>(M_PI) * v;
        pts.push_back(cx + r * std::cos(theta));
        pts.push_back(cy + r * std::sin(theta));
    }

    return PointCloud2D(std::move(pts));
}

// =======================================================================
//  generateRandomInCircle (annular region) — uniform area distribution
//  r = sqrt(inner^2 + u * (outer^2 - inner^2)),  theta = 2*PI * v
// =======================================================================

PointCloud2D PointCloudUtils::generateRandomInCircle(
    int count,
    float cx, float cy,
    float radiusOuter,
    float radiusInner,
    bool onlyOnBoundary,
    unsigned int seed)
{
    if (count <= 0 || radiusOuter <= 0.0f || radiusInner <= 0.0f)
        return PointCloud2D{};
    if (radiusOuter <= radiusInner)
        return PointCloud2D{};  // invalid: outer must be larger

    std::mt19937 rng = make_rng(seed);
    std::uniform_real_distribution<float> dist01(0.0f, 1.0f);

    std::vector<float> pts;
    pts.reserve(count * 2);

    float ro2 = radiusOuter * radiusOuter;
    float ri2 = radiusInner * radiusInner;

    for (int i = 0; i < count; ++i) {
        float u = dist01(rng);
        float v = dist01(rng);

        float r;
        if (onlyOnBoundary) {
            // Boundary = outer circumference
            r = radiusOuter;
        } else {
            // Uniform in annular area
            r = std::sqrt(ri2 + u * (ro2 - ri2));
        }

        float theta = 2.0f * static_cast<float>(M_PI) * v;
        pts.push_back(cx + r * std::cos(theta));
        pts.push_back(cy + r * std::sin(theta));
    }

    return PointCloud2D(std::move(pts));
}

// =======================================================================
//  generateRandomInSphere — uniform volume distribution
//  r = R * cbrt(u),  theta = 2*PI * v,  phi = acos(2*w - 1)
// =======================================================================

PointCloud3D PointCloudUtils::generateRandomInSphere(
    int count,
    float cx, float cy, float cz,
    float radius,
    bool onlyOnBoundary,
    unsigned int seed)
{
    if (count <= 0 || radius <= 0.0f)
        return PointCloud3D{};

    std::mt19937 rng = make_rng(seed);
    std::uniform_real_distribution<float> dist01(0.0f, 1.0f);

    std::vector<float> pts;
    pts.reserve(count * 3);

    for (int i = 0; i < count; ++i) {
        float u = dist01(rng);
        float v = dist01(rng);
        float w = dist01(rng);

        float r = onlyOnBoundary ? radius : (radius * std::cbrt(u));
        float theta = 2.0f * static_cast<float>(M_PI) * v;
        float phi = std::acos(2.0f * w - 1.0f);

        float sin_phi = std::sin(phi);
        pts.push_back(cx + r * sin_phi * std::cos(theta));
        pts.push_back(cy + r * sin_phi * std::sin(theta));
        pts.push_back(cz + r * std::cos(phi));
    }

    return PointCloud3D(std::move(pts));
}
