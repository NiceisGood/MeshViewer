#include "PointCloud.h"

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <sstream>
#include <algorithm>
#include <cctype>

// =======================================================================
//  PointCloud2D
// =======================================================================

PointCloud2D::PointCloud2D(std::vector<float> points)
    : points_(std::move(points))
{
    // Allow caller to pass any vector — we assume even size
}

void PointCloud2D::getPoint(int i, float& x, float& y) const
{
    x = points_[i * 2];
    y = points_[i * 2 + 1];
}

void PointCloud2D::setPoint(int i, float x, float y)
{
    points_[i * 2]     = x;
    points_[i * 2 + 1] = y;
}

void PointCloud2D::addPoint(float x, float y)
{
    points_.push_back(x);
    points_.push_back(y);
}

void PointCloud2D::boundingBox(float& min_x, float& min_y,
                                float& max_x, float& max_y) const
{
    if (points_.empty()) {
        min_x = min_y = max_x = max_y = 0.0f;
        return;
    }
    min_x = max_x = points_[0];
    min_y = max_y = points_[1];
    for (size_t i = 2; i < points_.size(); i += 2) {
        float x = points_[i];
        float y = points_[i + 1];
        if (x < min_x) min_x = x;
        if (y < min_y) min_y = y;
        if (x > max_x) max_x = x;
        if (y > max_y) max_y = y;
    }
}

std::string PointCloud2D::info() const
{
    float min_x, min_y, max_x, max_y;
    boundingBox(min_x, min_y, max_x, max_y);
    float dx = max_x - min_x;
    float dy = max_y - min_y;

    std::ostringstream oss;
    oss << "PointCloud2D: " << numPoints() << " points"
        << "\nBBox: [" << min_x << ", " << min_y << "]"
        << " x [" << max_x << ", " << max_y << "]"
        << "\nExtent: " << dx << " x " << dy;
    return oss.str();
}

bool PointCloud2D::readFile(const std::string& path)
{
    FILE* f = std::fopen(path.c_str(), "r");
    if (!f) return false;

    points_.clear();

    char buf[512];
    // Read header
    if (!std::fgets(buf, sizeof(buf), f)) {
        std::fclose(f);
        return false;
    }
    // Trim trailing whitespace/newline
    size_t len = std::strlen(buf);
    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r' || buf[len - 1] == ' '))
        buf[--len] = '\0';

    if (std::strcmp(buf, "POINTCLOUD_2D") != 0) {
        std::fclose(f);
        return false;
    }

    // Read point data
    while (std::fgets(buf, sizeof(buf), f)) {
        // Skip empty lines and comments
        if (buf[0] == '\n' || buf[0] == '\r' || buf[0] == '#') continue;

        float x, y;
        if (std::sscanf(buf, "%f %f", &x, &y) == 2) {
            points_.push_back(x);
            points_.push_back(y);
        }
    }

    std::fclose(f);
    return !points_.empty();
}

bool PointCloud2D::writeFile(const std::string& path) const
{
    FILE* f = std::fopen(path.c_str(), "w");
    if (!f) return false;

    std::fprintf(f, "POINTCLOUD_2D\n");
    for (size_t i = 0; i < points_.size(); i += 2) {
        std::fprintf(f, "%.15f %.15f\n",
                     static_cast<double>(points_[i]),
                     static_cast<double>(points_[i + 1]));
    }

    std::fclose(f);
    return true;
}

// =======================================================================
//  PointCloud3D
// =======================================================================

PointCloud3D::PointCloud3D(std::vector<float> points)
    : points_(std::move(points))
{
}

void PointCloud3D::getPoint(int i, float& x, float& y, float& z) const
{
    x = points_[i * 3];
    y = points_[i * 3 + 1];
    z = points_[i * 3 + 2];
}

void PointCloud3D::setPoint(int i, float x, float y, float z)
{
    points_[i * 3]     = x;
    points_[i * 3 + 1] = y;
    points_[i * 3 + 2] = z;
}

void PointCloud3D::addPoint(float x, float y, float z)
{
    points_.push_back(x);
    points_.push_back(y);
    points_.push_back(z);
}

void PointCloud3D::boundingBox(float& min_x, float& min_y, float& min_z,
                                float& max_x, float& max_y, float& max_z) const
{
    if (points_.empty()) {
        min_x = min_y = min_z = max_x = max_y = max_z = 0.0f;
        return;
    }
    min_x = max_x = points_[0];
    min_y = max_y = points_[1];
    min_z = max_z = points_[2];
    for (size_t i = 3; i < points_.size(); i += 3) {
        float x = points_[i];
        float y = points_[i + 1];
        float z = points_[i + 2];
        if (x < min_x) min_x = x;
        if (y < min_y) min_y = y;
        if (z < min_z) min_z = z;
        if (x > max_x) max_x = x;
        if (y > max_y) max_y = y;
        if (z > max_z) max_z = z;
    }
}

std::string PointCloud3D::info() const
{
    float min_x, min_y, min_z, max_x, max_y, max_z;
    boundingBox(min_x, min_y, min_z, max_x, max_y, max_z);
    float dx = max_x - min_x;
    float dy = max_y - min_y;
    float dz = max_z - min_z;

    std::ostringstream oss;
    oss << "PointCloud3D: " << numPoints() << " points"
        << "\nBBox: [" << min_x << ", " << min_y << ", " << min_z << "]"
        << " x [" << max_x << ", " << max_y << ", " << max_z << "]"
        << "\nExtent: " << dx << " x " << dy << " x " << dz;
    return oss.str();
}

bool PointCloud3D::readFile(const std::string& path)
{
    FILE* f = std::fopen(path.c_str(), "r");
    if (!f) return false;

    points_.clear();

    char buf[512];
    // Read header
    if (!std::fgets(buf, sizeof(buf), f)) {
        std::fclose(f);
        return false;
    }
    size_t len = std::strlen(buf);
    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r' || buf[len - 1] == ' '))
        buf[--len] = '\0';

    if (std::strcmp(buf, "POINTCLOUD_3D") != 0) {
        std::fclose(f);
        return false;
    }

    // Read point data
    while (std::fgets(buf, sizeof(buf), f)) {
        if (buf[0] == '\n' || buf[0] == '\r' || buf[0] == '#') continue;

        float x, y, z;
        if (std::sscanf(buf, "%f %f %f", &x, &y, &z) == 3) {
            points_.push_back(x);
            points_.push_back(y);
            points_.push_back(z);
        }
    }

    std::fclose(f);
    return !points_.empty();
}

bool PointCloud3D::writeFile(const std::string& path) const
{
    FILE* f = std::fopen(path.c_str(), "w");
    if (!f) return false;

    std::fprintf(f, "POINTCLOUD_3D\n");
    for (size_t i = 0; i < points_.size(); i += 3) {
        std::fprintf(f, "%.15f %.15f %.15f\n",
                     static_cast<double>(points_[i]),
                     static_cast<double>(points_[i + 1]),
                     static_cast<double>(points_[i + 2]));
    }

    std::fclose(f);
    return true;
}
