#ifndef POINTCLOUD_H
#define POINTCLOUD_H

#include <string>
#include <vector>
#include <cstdint>

// -----------------------------------------------------------------------
// PointCloud2D — stores 2D point data as interleaved x,y pairs.
//
// File format (.p2d):
//   POINTCLOUD_2D
//   x0 y0
//   x1 y1
//   ...
// -----------------------------------------------------------------------
class PointCloud2D {
public:
    PointCloud2D() = default;
    PointCloud2D(const PointCloud2D&) = default;
    PointCloud2D& operator=(const PointCloud2D&) = default;
    PointCloud2D(PointCloud2D&&) noexcept = default;
    PointCloud2D& operator=(PointCloud2D&&) noexcept = default;

    /// Construct from vector of interleaved x,y values (must have even size).
    explicit PointCloud2D(std::vector<float> points);

    // ---- Data access ----------------------------------------------------
    const std::vector<float>& points() const { return points_; }
    std::vector<float>& points() { return points_; }

    int numPoints() const { return static_cast<int>(points_.size() / 2); }
    bool empty() const { return points_.empty(); }

    void getPoint(int i, float& x, float& y) const;
    void setPoint(int i, float x, float y);

    void clear() { points_.clear(); }
    void reserve(int n) { points_.reserve(n * 2); }
    void addPoint(float x, float y);

    // ---- Bounding box ---------------------------------------------------
    /// Compute axis-aligned bounding box. Returns {min_x, min_y, max_x, max_y}.
    void boundingBox(float& min_x, float& min_y,
                     float& max_x, float& max_y) const;

    // ---- Info string ----------------------------------------------------
    std::string info() const;

    // ---- I/O ------------------------------------------------------------
    /// Read from a .p2d file. Returns true on success.
    bool readFile(const std::string& path);

    /// Write to a .p2d file. Returns true on success.
    bool writeFile(const std::string& path) const;

private:
    std::vector<float> points_;  // interleaved x,y per point
};

// -----------------------------------------------------------------------
// PointCloud3D — stores 3D point data as interleaved x,y,z triples.
//
// File format (.p3d):
//   POINTCLOUD_3D
//   x0 y0 z0
//   x1 y1 z1
//   ...
// -----------------------------------------------------------------------
class PointCloud3D {
public:
    PointCloud3D() = default;
    PointCloud3D(const PointCloud3D&) = default;
    PointCloud3D& operator=(const PointCloud3D&) = default;
    PointCloud3D(PointCloud3D&&) noexcept = default;
    PointCloud3D& operator=(PointCloud3D&&) noexcept = default;

    /// Construct from vector of interleaved x,y,z values (size must be multiple of 3).
    explicit PointCloud3D(std::vector<float> points);

    // ---- Data access ----------------------------------------------------
    const std::vector<float>& points() const { return points_; }
    std::vector<float>& points() { return points_; }

    int numPoints() const { return static_cast<int>(points_.size() / 3); }
    bool empty() const { return points_.empty(); }

    void getPoint(int i, float& x, float& y, float& z) const;
    void setPoint(int i, float x, float y, float z);

    void clear() { points_.clear(); }
    void reserve(int n) { points_.reserve(n * 3); }
    void addPoint(float x, float y, float z);

    // ---- Bounding box ---------------------------------------------------
    void boundingBox(float& min_x, float& min_y, float& min_z,
                     float& max_x, float& max_y, float& max_z) const;

    // ---- Info string ----------------------------------------------------
    std::string info() const;

    // ---- I/O ------------------------------------------------------------
    /// Read from a .p3d file. Returns true on success.
    bool readFile(const std::string& path);

    /// Write to a .p3d file. Returns true on success.
    bool writeFile(const std::string& path) const;

private:
    std::vector<float> points_;  // interleaved x,y,z per point
};

#endif // POINTCLOUD_H
