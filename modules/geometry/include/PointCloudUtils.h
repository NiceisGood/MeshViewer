#ifndef POINTCLOUDUTILS_H
#define POINTCLOUDUTILS_H

#include "PointCloud.h"

// -----------------------------------------------------------------------
// PointCloudUtils — utility functions for point cloud generation.
// -----------------------------------------------------------------------
class PointCloudUtils {
public:
    /// Generate a random 2D point cloud within the specified bounds.
    /// @param count  Number of points to generate.
    /// @param xMin, xMax  X-axis range.
    /// @param yMin, yMax  Y-axis range.
    /// @param seed  Random seed (0 = use random device).
    static PointCloud2D generateRandom2D(int count,
                                          float xMin, float xMax,
                                          float yMin, float yMax,
                                          unsigned int seed = 0);

    /// Generate a random 3D point cloud within the specified bounds.
    /// @param count  Number of points to generate.
    /// @param xMin, xMax  X-axis range.
    /// @param yMin, yMax  Y-axis range.
    /// @param zMin, zMax  Z-axis range.
    /// @param seed  Random seed (0 = use random device).
    static PointCloud3D generateRandom3D(int count,
                                          float xMin, float xMax,
                                          float yMin, float yMax,
                                          float zMin, float zMax,
                                          unsigned int seed = 0);

    /// Generate a random 2D point cloud uniformly distributed inside a circular disc.
    /// @param count  Number of points to generate.
    /// @param cx, cy  Center of the disc.
    /// @param radius  Radius of the disc.
    /// @param seed  Random seed (0 = use random device).
    static PointCloud2D generateRandomInCircle(int count,
                                                float cx, float cy,
                                                float radius,
                                                unsigned int seed = 0);

    /// Generate a random 3D point cloud uniformly distributed inside a sphere.
    /// @param count  Number of points to generate.
    /// @param cx, cy, cz  Center of the sphere.
    /// @param radius  Radius of the sphere.
    /// @param seed  Random seed (0 = use random device).
    static PointCloud3D generateRandomInSphere(int count,
                                                float cx, float cy, float cz,
                                                float radius,
                                                unsigned int seed = 0);
};

#endif // POINTCLOUDUTILS_H
