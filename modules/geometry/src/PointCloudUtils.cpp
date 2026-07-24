#include "PointCloudUtils.h"

#include <random>
#include <chrono>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

PointCloud2D PointCloudUtils::generateRandom2D(
    int count,
    float xMin, float xMax,
    float yMin, float yMax,
    unsigned int seed)
{
    if (count <= 0)
        return PointCloud2D{};

    // Determine seed
    unsigned int actual_seed = seed;
    if (actual_seed == 0) {
        actual_seed = static_cast<unsigned int>(
            std::chrono::steady_clock::now().time_since_epoch().count());
    }

    std::mt19937 rng(actual_seed);
    std::uniform_real_distribution<float> dist_x(xMin, xMax);
    std::uniform_real_distribution<float> dist_y(yMin, yMax);

    std::vector<float> pts;
    pts.reserve(count * 2);
    for (int i = 0; i < count; ++i) {
        pts.push_back(dist_x(rng));
        pts.push_back(dist_y(rng));
    }

    return PointCloud2D(std::move(pts));
}

PointCloud3D PointCloudUtils::generateRandom3D(
    int count,
    float xMin, float xMax,
    float yMin, float yMax,
    float zMin, float zMax,
    unsigned int seed)
{
    if (count <= 0)
        return PointCloud3D{};

    unsigned int actual_seed = seed;
    if (actual_seed == 0) {
        actual_seed = static_cast<unsigned int>(
            std::chrono::steady_clock::now().time_since_epoch().count());
    }

    std::mt19937 rng(actual_seed);
    std::uniform_real_distribution<float> dist_x(xMin, xMax);
    std::uniform_real_distribution<float> dist_y(yMin, yMax);
    std::uniform_real_distribution<float> dist_z(zMin, zMax);

    std::vector<float> pts;
    pts.reserve(count * 3);
    for (int i = 0; i < count; ++i) {
        pts.push_back(dist_x(rng));
        pts.push_back(dist_y(rng));
        pts.push_back(dist_z(rng));
    }

    return PointCloud3D(std::move(pts));
}

// =======================================================================
//  Circular disc (2D) — uniform distribution using polar coordinates
//  r = R * sqrt(u),  theta = 2*PI * v
// =======================================================================

PointCloud2D PointCloudUtils::generateRandomInCircle(
    int count,
    float cx, float cy,
    float radius,
    unsigned int seed)
{
    if (count <= 0 || radius <= 0.0f)
        return PointCloud2D{};

    unsigned int actual_seed = seed;
    if (actual_seed == 0) {
        actual_seed = static_cast<unsigned int>(
            std::chrono::steady_clock::now().time_since_epoch().count());
    }

    std::mt19937 rng(actual_seed);
    std::uniform_real_distribution<float> dist01(0.0f, 1.0f);

    std::vector<float> pts;
    pts.reserve(count * 2);
    for (int i = 0; i < count; ++i) {
        float u = dist01(rng);          // [0,1) for radius
        float v = dist01(rng);          // [0,1) for angle
        float r = radius * std::sqrt(u);
        float theta = 2.0f * static_cast<float>(M_PI) * v;
        pts.push_back(cx + r * std::cos(theta));
        pts.push_back(cy + r * std::sin(theta));
    }

    return PointCloud2D(std::move(pts));
}

// =======================================================================
//  Sphere (3D) — uniform distribution using spherical coordinates
//  r = R * cbrt(u),  theta = 2*PI * v,  phi = acos(2*w - 1)
// =======================================================================

PointCloud3D PointCloudUtils::generateRandomInSphere(
    int count,
    float cx, float cy, float cz,
    float radius,
    unsigned int seed)
{
    if (count <= 0 || radius <= 0.0f)
        return PointCloud3D{};

    unsigned int actual_seed = seed;
    if (actual_seed == 0) {
        actual_seed = static_cast<unsigned int>(
            std::chrono::steady_clock::now().time_since_epoch().count());
    }

    std::mt19937 rng(actual_seed);
    std::uniform_real_distribution<float> dist01(0.0f, 1.0f);

    std::vector<float> pts;
    pts.reserve(count * 3);
    for (int i = 0; i < count; ++i) {
        float u = dist01(rng);          // [0,1) for radius
        float v = dist01(rng);          // [0,1) for theta
        float w = dist01(rng);          // [0,1) for phi

        float r = radius * std::cbrt(u);
        float theta = 2.0f * static_cast<float>(M_PI) * v;
        float phi = std::acos(2.0f * w - 1.0f);  // uniform on sphere surface

        float sin_phi = std::sin(phi);
        pts.push_back(cx + r * sin_phi * std::cos(theta));
        pts.push_back(cy + r * sin_phi * std::sin(theta));
        pts.push_back(cz + r * std::cos(phi));
    }

    return PointCloud3D(std::move(pts));
}
