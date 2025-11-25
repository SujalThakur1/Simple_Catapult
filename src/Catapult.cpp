#include "Catapult.h"
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <cmath>
#include <array>

Catapult::Catapult()
    : armAngle(-0.03f),
      isAnimating(false),
      animationTime(0.0f),
      pivotPoint(0.0f, 0.0f, 0.0f),
      armLength(0.0f)
{
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    updateArmAngleFromSpeed();
    rebuildGeometry();
}

// Rebuilds the entire catapult mesh geometry from scratch
void Catapult::rebuildGeometry()
{
    std::vector<float> vertices;

    // Helper lambda: creates a box with proper normals for all 6 faces
    auto addBox = [&](float x, float y, float z, float width, float height, float depth)
    {
        float hw = width / 2.0f, hh = height / 2.0f, hd = depth / 2.0f;

        vertices.insert(vertices.end(), {x - hw, y - hh, z + hd, 0, 0, 1,
                                         x + hw, y - hh, z + hd, 0, 0, 1,
                                         x + hw, y + hh, z + hd, 0, 0, 1});
        vertices.insert(vertices.end(), {x + hw, y + hh, z + hd, 0, 0, 1,
                                         x - hw, y + hh, z + hd, 0, 0, 1,
                                         x - hw, y - hh, z + hd, 0, 0, 1});

        vertices.insert(vertices.end(), {x - hw, y - hh, z - hd, 0, 0, -1,
                                         x - hw, y + hh, z - hd, 0, 0, -1,
                                         x + hw, y + hh, z - hd, 0, 0, -1});
        vertices.insert(vertices.end(), {x + hw, y + hh, z - hd, 0, 0, -1,
                                         x + hw, y - hh, z - hd, 0, 0, -1,
                                         x - hw, y - hh, z - hd, 0, 0, -1});

        vertices.insert(vertices.end(), {x - hw, y - hh, z - hd, -1, 0, 0,
                                         x - hw, y - hh, z + hd, -1, 0, 0,
                                         x - hw, y + hh, z + hd, -1, 0, 0});
        vertices.insert(vertices.end(), {x - hw, y + hh, z + hd, -1, 0, 0,
                                         x - hw, y + hh, z - hd, -1, 0, 0,
                                         x - hw, y - hh, z - hd, -1, 0, 0});

        vertices.insert(vertices.end(), {x + hw, y - hh, z - hd, 1, 0, 0,
                                         x + hw, y + hh, z - hd, 1, 0, 0,
                                         x + hw, y + hh, z + hd, 1, 0, 0});
        vertices.insert(vertices.end(), {x + hw, y + hh, z + hd, 1, 0, 0,
                                         x + hw, y - hh, z + hd, 1, 0, 0,
                                         x + hw, y - hh, z - hd, 1, 0, 0});

        vertices.insert(vertices.end(), {x - hw, y + hh, z - hd, 0, 1, 0,
                                         x - hw, y + hh, z + hd, 0, 1, 0,
                                         x + hw, y + hh, z + hd, 0, 1, 0});
        vertices.insert(vertices.end(), {x + hw, y + hh, z + hd, 0, 1, 0,
                                         x + hw, y + hh, z - hd, 0, 1, 0,
                                         x - hw, y + hh, z - hd, 0, 1, 0});

        vertices.insert(vertices.end(), {x - hw, y - hh, z - hd, 0, -1, 0,
                                         x + hw, y - hh, z - hd, 0, -1, 0,
                                         x + hw, y - hh, z + hd, 0, -1, 0});
        vertices.insert(vertices.end(), {x + hw, y - hh, z + hd, 0, -1, 0,
                                         x - hw, y - hh, z + hd, 0, -1, 0,
                                         x - hw, y - hh, z - hd, 0, -1, 0});
    };

    // Helper lambda: creates a hollow box frame (only edges, not solid)
    auto addHollowBox = [&](float x, float y, float z, float width, float height, float depth, float bt)
    {
        float innerW = width - 2.0f * bt;
        float innerD = depth - 2.0f * bt;
        if (innerW < 0.0f)
            innerW = 0.0f;
        if (innerD < 0.0f)
            innerD = 0.0f;

        // Create 4 edge beams (front, back, left, right) to form hollow frame
        addBox(x, y, z + depth / 2.0f - bt / 2.0f, innerW, height, bt);
        addBox(x, y, z - depth / 2.0f + bt / 2.0f, innerW, height, bt);
        addBox(x - width / 2.0f + bt / 2.0f, y, z, bt, height, innerD);
        addBox(x + width / 2.0f - bt / 2.0f, y, z, bt, height, innerD);
    };

    // Helper lambda: creates a cylinder aligned along Z-axis (for wheels)
    auto addCylinderZ = [&](float x, float y, float z, float radius, float length, int segments)
    {
        float angleStep = 2.0f * 3.1415926535f / segments;
        float z1 = z - length / 2.0f, z2 = z + length / 2.0f;
        for (int i = 0; i < segments; ++i)
        {
            float a1 = i * angleStep, a2 = (i + 1) * angleStep;
            float cx1 = x + radius * cosf(a1), cy1 = y + radius * sinf(a1);
            float cx2 = x + radius * cosf(a2), cy2 = y + radius * sinf(a2);

            // Normals point radially outward from center
            float nx1 = cosf(a1), ny1 = sinf(a1);
            float nx2 = cosf(a2), ny2 = sinf(a2);

            vertices.insert(vertices.end(), {x, y, z2, 0, 0, 1,
                                             cx1, cy1, z2, 0, 0, 1,
                                             cx2, cy2, z2, 0, 0, 1});

            vertices.insert(vertices.end(), {x, y, z1, 0, 0, -1,
                                             cx2, cy2, z1, 0, 0, -1,
                                             cx1, cy1, z1, 0, 0, -1});
            vertices.insert(vertices.end(), {cx1, cy1, z1, nx1, ny1, 0,
                                             cx2, cy2, z1, nx2, ny2, 0,
                                             cx2, cy2, z2, nx2, ny2, 0});
            vertices.insert(vertices.end(), {cx2, cy2, z2, nx2, ny2, 0,
                                             cx1, cy1, z2, nx1, ny1, 0,
                                             cx1, cy1, z1, nx1, ny1, 0});
        }
    };

    // Build horizontal tier planks (base structure)
    int tiers = tierCount < 1 ? 1 : tierCount;
    float borderThickness = 0.02f;
    for (int i = 0; i < tiers; ++i)
    {
        float y = baseCenterY + i * (plankHeight + tierGap);
        int oldSize = vertices.size();
        addHollowBox(baseCenterX, y, baseCenterZ, plankWidthX, plankHeight, plankDepthZ, borderThickness);
        vertexCounts[i] = (vertices.size() - oldSize) / 6; // Store vertex count (6 floats per vertex: xyz + normal)
    }

    int oldSize = vertices.size();
    addHollowBox(verticalPlank1X, verticalPlank1Y, verticalPlank1Z, verticalPlank1Width, verticalPlank1Height, verticalPlank1Depth, borderThickness);
    vertexCounts[tiers] = (vertices.size() - oldSize) / 6;

    oldSize = vertices.size();
    addHollowBox(verticalPlank2X, verticalPlank2Y, verticalPlank2Z, verticalPlank2Width, verticalPlank2Height, verticalPlank2Depth, borderThickness);
    vertexCounts[tiers + 1] = (vertices.size() - oldSize) / 6;

    oldSize = vertices.size();
    addBox(horizontalPlankX, horizontalPlankY, horizontalPlankZ, horizontalPlankWidth, horizontalPlankHeight, horizontalPlankDepth);
    vertexCounts[tiers + 2] = (vertices.size() - oldSize) / 6;

    oldSize = vertices.size();
    addBox(ropeX, ropeY, ropeZ, ropeWidth, ropeHeight, ropeDepth);
    vertexCounts[tiers + 3] = (vertices.size() - oldSize) / 6;

    // Throwing arm: pivot (left end) stays fixed, bucket end rotates around it
    float fixedPivotX = armPivotX;
    float fixedPivotY = armPivotY;
    float fixedPivotZ = armPivotZ;

    // Calculate moving end position based on current arm angle
    // Negative cos/sin because arm extends in negative X direction
    float movingEndX = fixedPivotX - armDepth * cosf(armAngle);
    float movingEndY = fixedPivotY - armDepth * sinf(armAngle);
    float movingEndZ = fixedPivotZ;
    auto addRotatedBox = [&](float fixedX, float fixedY, float fixedZ, float length, float width, float height, float angle)
    {
        float hw = width / 2.0f, hh = height / 2.0f;
        float cos_a = cosf(angle), sin_a = sinf(angle);

        float corners[8][3] = {
            {0, -hh, -hw}, {length, -hh, -hw}, {length, hh, -hw}, {0, hh, -hw}, {0, -hh, hw}, {length, -hh, hw}, {length, hh, hw}, {0, hh, hw}};

        // Rotate box around fixed pivot (Z-axis rotation in X-Y plane)
        for (int i = 0; i < 8; ++i)
        {
            float x = corners[i][0];
            float y = corners[i][1];
            float z = corners[i][2];

            float rotatedX = -x * cos_a - y * sin_a;
            float rotatedY = -x * sin_a + y * cos_a;
            float rotatedZ = z;
            corners[i][0] = fixedX + rotatedX;
            corners[i][1] = fixedY + rotatedY;
            corners[i][2] = fixedZ + rotatedZ;
        }

        // Helper to rotate normal vector
        auto rotateNormal = [&](float nx, float ny, float nz) -> std::array<float, 3>
        {
            float rx = -nx * cos_a - ny * sin_a;
            float ry = -nx * sin_a + ny * cos_a;
            float rz = nz;
            return {rx, ry, rz};
        };

        auto n_front = rotateNormal(0, 0, -1);
        vertices.insert(vertices.end(), {corners[0][0], corners[0][1], corners[0][2], n_front[0], n_front[1], n_front[2],
                                         corners[1][0], corners[1][1], corners[1][2], n_front[0], n_front[1], n_front[2],
                                         corners[2][0], corners[2][1], corners[2][2], n_front[0], n_front[1], n_front[2]});
        vertices.insert(vertices.end(), {corners[2][0], corners[2][1], corners[2][2], n_front[0], n_front[1], n_front[2],
                                         corners[3][0], corners[3][1], corners[3][2], n_front[0], n_front[1], n_front[2],
                                         corners[0][0], corners[0][1], corners[0][2], n_front[0], n_front[1], n_front[2]});

        auto n_back = rotateNormal(0, 0, 1);
        vertices.insert(vertices.end(), {corners[4][0], corners[4][1], corners[4][2], n_back[0], n_back[1], n_back[2],
                                         corners[7][0], corners[7][1], corners[7][2], n_back[0], n_back[1], n_back[2],
                                         corners[6][0], corners[6][1], corners[6][2], n_back[0], n_back[1], n_back[2]});
        vertices.insert(vertices.end(), {corners[6][0], corners[6][1], corners[6][2], n_back[0], n_back[1], n_back[2],
                                         corners[5][0], corners[5][1], corners[5][2], n_back[0], n_back[1], n_back[2],
                                         corners[4][0], corners[4][1], corners[4][2], n_back[0], n_back[1], n_back[2]});

        auto n_left = rotateNormal(-1, 0, 0);
        vertices.insert(vertices.end(), {corners[0][0], corners[0][1], corners[0][2], n_left[0], n_left[1], n_left[2],
                                         corners[3][0], corners[3][1], corners[3][2], n_left[0], n_left[1], n_left[2],
                                         corners[7][0], corners[7][1], corners[7][2], n_left[0], n_left[1], n_left[2]});
        vertices.insert(vertices.end(), {corners[7][0], corners[7][1], corners[7][2], n_left[0], n_left[1], n_left[2],
                                         corners[4][0], corners[4][1], corners[4][2], n_left[0], n_left[1], n_left[2],
                                         corners[0][0], corners[0][1], corners[0][2], n_left[0], n_left[1], n_left[2]});

        auto n_right = rotateNormal(1, 0, 0);
        vertices.insert(vertices.end(), {corners[1][0], corners[1][1], corners[1][2], n_right[0], n_right[1], n_right[2],
                                         corners[5][0], corners[5][1], corners[5][2], n_right[0], n_right[1], n_right[2],
                                         corners[6][0], corners[6][1], corners[6][2], n_right[0], n_right[1], n_right[2]});
        vertices.insert(vertices.end(), {corners[6][0], corners[6][1], corners[6][2], n_right[0], n_right[1], n_right[2],
                                         corners[2][0], corners[2][1], corners[2][2], n_right[0], n_right[1], n_right[2],
                                         corners[1][0], corners[1][1], corners[1][2], n_right[0], n_right[1], n_right[2]});

        auto n_top = rotateNormal(0, 1, 0);
        vertices.insert(vertices.end(), {corners[3][0], corners[3][1], corners[3][2], n_top[0], n_top[1], n_top[2],
                                         corners[2][0], corners[2][1], corners[2][2], n_top[0], n_top[1], n_top[2],
                                         corners[6][0], corners[6][1], corners[6][2], n_top[0], n_top[1], n_top[2]});
        vertices.insert(vertices.end(), {corners[6][0], corners[6][1], corners[6][2], n_top[0], n_top[1], n_top[2],
                                         corners[7][0], corners[7][1], corners[7][2], n_top[0], n_top[1], n_top[2],
                                         corners[3][0], corners[3][1], corners[3][2], n_top[0], n_top[1], n_top[2]});

        auto n_bottom = rotateNormal(0, -1, 0);
        vertices.insert(vertices.end(), {corners[0][0], corners[0][1], corners[0][2], n_bottom[0], n_bottom[1], n_bottom[2],
                                         corners[4][0], corners[4][1], corners[4][2], n_bottom[0], n_bottom[1], n_bottom[2],
                                         corners[5][0], corners[5][1], corners[5][2], n_bottom[0], n_bottom[1], n_bottom[2]});
        vertices.insert(vertices.end(), {corners[5][0], corners[5][1], corners[5][2], n_bottom[0], n_bottom[1], n_bottom[2],
                                         corners[1][0], corners[1][1], corners[1][2], n_bottom[0], n_bottom[1], n_bottom[2],
                                         corners[0][0], corners[0][1], corners[0][2], n_bottom[0], n_bottom[1], n_bottom[2]});
    };

    oldSize = vertices.size();
    addRotatedBox(fixedPivotX, fixedPivotY, fixedPivotZ, armDepth, armHeight, armWidth, armAngle);
    vertexCounts[tiers + 4] = (vertices.size() - oldSize) / 6;

    // Bowl position: offset from arm end toward pivot (pulls bucket closer to stick)
    float armDirX = -cosf(armAngle);
    float armDirY = -sinf(armAngle);
    float connectionOffset = 0.16f; // Distance to pull bucket back toward pivot
    float bucketCenterX = movingEndX + armDirX * connectionOffset + bucketOffsetX;
    float bucketCenterY = movingEndY + armDirY * connectionOffset + bucketOffsetY;
    float bucketCenterZ = movingEndZ + bucketOffsetZ;

    float bucketX = bucketCenterX;
    float bucketY = bucketCenterY;
    float bucketZ = bucketCenterZ;
    // Creates outer surface of curved bowl (hemisphere-like shape)
    auto addBowlOuter = [&](float centerX, float centerY, float centerZ, float w, float h, float d, float rotAngle) -> int
    {
        int startSize = vertices.size();
        int segments = 24;      // Angular resolution
        int heightSegments = 8; // Vertical resolution
        float cos_a = cosf(rotAngle), sin_a = sinf(rotAngle);

        auto rotateAndTranslate = [&](float px, float py, float pz) -> std::array<float, 3>
        {
            float dx = px - centerX;
            float dy = py - centerY;
            float dz = pz - centerZ;

            float rotatedX = centerX - dx * cos_a - dy * sin_a;
            float rotatedY = centerY - dx * sin_a + dy * cos_a;
            float rotatedZ = centerZ + dz;

            return {rotatedX, rotatedY, rotatedZ};
        };

        // Helper to rotate normal vector
        auto rotateNormal = [&](float nx, float ny, float nz) -> std::array<float, 3>
        {
            float rx = -nx * cos_a - ny * sin_a;
            float ry = -nx * sin_a + ny * cos_a;
            float rz = nz;
            return {rx, ry, rz};
        };

        float radiusX = w / 2.0f;
        float radiusZ = d / 2.0f;
        float depth = h;
        float bottomY = centerY - depth / 2.0f;
        for (int layer = 0; layer < heightSegments; ++layer)
        {
            for (int i = 0; i < segments; ++i)
            {
                float angle1 = (i / float(segments)) * 2.0f * 3.1415926535f;
                float angle2 = ((i + 1) / float(segments)) * 2.0f * 3.1415926535f;

                float t1 = layer / float(heightSegments);
                float t2 = (layer + 1) / float(heightSegments);

                float curve1 = sinf(t1 * 1.57f);
                float curve2 = sinf(t2 * 1.57f);
                float r1 = curve1;
                float r2 = curve2;
                float y1 = bottomY + t1 * depth;
                float y2 = bottomY + t2 * depth;
                float x1_1 = radiusX * r1 * cosf(angle1);
                float z1_1 = radiusZ * r1 * sinf(angle1);
                float x1_2 = radiusX * r1 * cosf(angle2);
                float z1_2 = radiusZ * r1 * sinf(angle2);

                float x2_1 = radiusX * r2 * cosf(angle1);
                float z2_1 = radiusZ * r2 * sinf(angle1);
                float x2_2 = radiusX * r2 * cosf(angle2);
                float z2_2 = radiusZ * r2 * sinf(angle2);

                // Calculate normals: derivative of sine curve gives proper surface normals
                float dcurve1 = cosf(t1 * 1.57f); // Derivative of sin curve for normal calculation
                float dcurve2 = cosf(t2 * 1.57f);
                float nx1_1 = cosf(angle1) * r1;
                float nz1_1 = sinf(angle1) * r1;
                float ny1 = dcurve1; // Vertical component from curve derivative
                glm::vec3 norm1_1 = glm::normalize(glm::vec3(nx1_1, ny1, nz1_1));

                float nx1_2 = cosf(angle2) * r1;
                float nz1_2 = sinf(angle2) * r1;
                glm::vec3 norm1_2 = glm::normalize(glm::vec3(nx1_2, ny1, nz1_2));

                float nx2_1 = cosf(angle1) * r2;
                float nz2_1 = sinf(angle1) * r2;
                float ny2 = dcurve2;
                glm::vec3 norm2_1 = glm::normalize(glm::vec3(nx2_1, ny2, nz2_1));

                float nx2_2 = cosf(angle2) * r2;
                float nz2_2 = sinf(angle2) * r2;
                glm::vec3 norm2_2 = glm::normalize(glm::vec3(nx2_2, ny2, nz2_2));

                auto n1 = rotateNormal(norm1_1.x, norm1_1.y, norm1_1.z);
                auto n2 = rotateNormal(norm1_2.x, norm1_2.y, norm1_2.z);
                auto n3 = rotateNormal(norm2_2.x, norm2_2.y, norm2_2.z);
                auto n4 = rotateNormal(norm2_1.x, norm2_1.y, norm2_1.z);
                auto v1 = rotateAndTranslate(centerX + x1_1, y1, centerZ + z1_1);
                auto v2 = rotateAndTranslate(centerX + x1_2, y1, centerZ + z1_2);
                auto v3 = rotateAndTranslate(centerX + x2_2, y2, centerZ + z2_2);
                auto v4 = rotateAndTranslate(centerX + x2_1, y2, centerZ + z2_1);

                vertices.insert(vertices.end(), {v1[0], v1[1], v1[2], n1[0], n1[1], n1[2],
                                                 v2[0], v2[1], v2[2], n2[0], n2[1], n2[2],
                                                 v3[0], v3[1], v3[2], n3[0], n3[1], n3[2]});
                vertices.insert(vertices.end(), {v3[0], v3[1], v3[2], n3[0], n3[1], n3[2],
                                                 v4[0], v4[1], v4[2], n4[0], n4[1], n4[2],
                                                 v1[0], v1[1], v1[2], n1[0], n1[1], n1[2]});
            }
        }

        return (vertices.size() - startSize) / 6;
    };

    auto addBowlInner = [&](float centerX, float centerY, float centerZ, float w, float h, float d, float bt, float rotAngle)
    {
        int segments = 24;
        int heightSegments = 8;
        float cos_a = cosf(rotAngle), sin_a = sinf(rotAngle);

        auto rotateAndTranslate = [&](float px, float py, float pz) -> std::array<float, 3>
        {
            float dx = px - centerX;
            float dy = py - centerY;
            float dz = pz - centerZ;

            float rotatedX = centerX - dx * cos_a - dy * sin_a;
            float rotatedY = centerY - dx * sin_a + dy * cos_a;
            float rotatedZ = centerZ + dz;

            return {rotatedX, rotatedY, rotatedZ};
        };

        // Helper to rotate normal vector
        auto rotateNormal = [&](float nx, float ny, float nz) -> std::array<float, 3>
        {
            float rx = -nx * cos_a - ny * sin_a;
            float ry = -nx * sin_a + ny * cos_a;
            float rz = nz;
            return {rx, ry, rz};
        };

        float radiusX = w / 2.0f;
        float radiusZ = d / 2.0f;
        float depth = h;
        float bottomY = centerY - depth / 2.0f;

        // Ensure minimum offset to prevent z-fighting (overlapping surfaces)
        float minOffset = 0.001f;
        float effectiveThickness = (bt > minOffset) ? bt : minOffset;
        float innerRadiusX = radiusX - effectiveThickness;
        float innerRadiusZ = radiusZ - effectiveThickness;

        for (int layer = 0; layer < heightSegments; ++layer)
        {
            for (int i = 0; i < segments; ++i)
            {
                float angle1 = (i / float(segments)) * 2.0f * 3.1415926535f;
                float angle2 = ((i + 1) / float(segments)) * 2.0f * 3.1415926535f;

                float t1 = layer / float(heightSegments);
                float t2 = (layer + 1) / float(heightSegments);

                float curve1 = sinf(t1 * 1.57f);
                float curve2 = sinf(t2 * 1.57f);
                float r1 = curve1;
                float r2 = curve2;
                float y1 = bottomY + t1 * depth;
                float y2 = bottomY + t2 * depth;

                float ix1_1 = innerRadiusX * r1 * cosf(angle1);
                float iz1_1 = innerRadiusZ * r1 * sinf(angle1);
                float ix1_2 = innerRadiusX * r1 * cosf(angle2);
                float iz1_2 = innerRadiusZ * r1 * sinf(angle2);

                float ix2_1 = innerRadiusX * r2 * cosf(angle1);
                float iz2_1 = innerRadiusZ * r2 * sinf(angle1);
                float ix2_2 = innerRadiusX * r2 * cosf(angle2);
                float iz2_2 = innerRadiusZ * r2 * sinf(angle2);

                float dcurve1 = cosf(t1 * 1.57f);
                float dcurve2 = cosf(t2 * 1.57f);
                // Inner normals point INWARD (negated) for correct lighting
                float inx1_1 = -cosf(angle1) * r1;
                float inz1_1 = -sinf(angle1) * r1;
                float iny1 = -dcurve1;
                glm::vec3 inorm1_1 = glm::normalize(glm::vec3(inx1_1, iny1, inz1_1));

                float inx1_2 = -cosf(angle2) * r1;
                float inz1_2 = -sinf(angle2) * r1;
                glm::vec3 inorm1_2 = glm::normalize(glm::vec3(inx1_2, iny1, inz1_2));

                float inx2_1 = -cosf(angle1) * r2;
                float inz2_1 = -sinf(angle1) * r2;
                float iny2 = -dcurve2;
                glm::vec3 inorm2_1 = glm::normalize(glm::vec3(inx2_1, iny2, inz2_1));

                float inx2_2 = -cosf(angle2) * r2;
                float inz2_2 = -sinf(angle2) * r2;
                glm::vec3 inorm2_2 = glm::normalize(glm::vec3(inx2_2, iny2, inz2_2));

                auto in1 = rotateNormal(inorm1_1.x, inorm1_1.y, inorm1_1.z);
                auto in2 = rotateNormal(inorm1_2.x, inorm1_2.y, inorm1_2.z);
                auto in3 = rotateNormal(inorm2_2.x, inorm2_2.y, inorm2_2.z);
                auto in4 = rotateNormal(inorm2_1.x, inorm2_1.y, inorm2_1.z);
                auto iv1 = rotateAndTranslate(centerX + ix1_1, y1 + bt, centerZ + iz1_1);
                auto iv2 = rotateAndTranslate(centerX + ix1_2, y1 + bt, centerZ + iz1_2);
                auto iv3 = rotateAndTranslate(centerX + ix2_2, y2, centerZ + iz2_2);
                auto iv4 = rotateAndTranslate(centerX + ix2_1, y2, centerZ + iz2_1);

                vertices.insert(vertices.end(), {iv1[0], iv1[1], iv1[2], in1[0], in1[1], in1[2],
                                                 iv4[0], iv4[1], iv4[2], in4[0], in4[1], in4[2],
                                                 iv3[0], iv3[1], iv3[2], in3[0], in3[1], in3[2]});
                vertices.insert(vertices.end(), {iv3[0], iv3[1], iv3[2], in3[0], in3[1], in3[2],
                                                 iv2[0], iv2[1], iv2[2], in2[0], in2[1], in2[2],
                                                 iv1[0], iv1[1], iv1[2], in1[0], in1[1], in1[2]});
            }
        }

        // Add rim at top to connect outer and inner surfaces
        float topY = centerY + depth / 2.0f;
        auto rimNormal = rotateNormal(0, 1, 0);

        for (int i = 0; i < segments; ++i)
        {
            float angle1 = (i / float(segments)) * 2.0f * 3.1415926535f;
            float angle2 = ((i + 1) / float(segments)) * 2.0f * 3.1415926535f;

            auto outerV1 = rotateAndTranslate(centerX + radiusX * cosf(angle1), topY, centerZ + radiusZ * sinf(angle1));
            auto outerV2 = rotateAndTranslate(centerX + radiusX * cosf(angle2), topY, centerZ + radiusZ * sinf(angle2));
            auto innerV1 = rotateAndTranslate(centerX + innerRadiusX * cosf(angle1), topY, centerZ + innerRadiusZ * sinf(angle1));
            auto innerV2 = rotateAndTranslate(centerX + innerRadiusX * cosf(angle2), topY, centerZ + innerRadiusZ * sinf(angle2));

            vertices.insert(vertices.end(), {outerV1[0], outerV1[1], outerV1[2], rimNormal[0], rimNormal[1], rimNormal[2],
                                             outerV2[0], outerV2[1], outerV2[2], rimNormal[0], rimNormal[1], rimNormal[2],
                                             innerV2[0], innerV2[1], innerV2[2], rimNormal[0], rimNormal[1], rimNormal[2]});
            vertices.insert(vertices.end(), {innerV2[0], innerV2[1], innerV2[2], rimNormal[0], rimNormal[1], rimNormal[2],
                                             innerV1[0], innerV1[1], innerV1[2], rimNormal[0], rimNormal[1], rimNormal[2],
                                             outerV1[0], outerV1[1], outerV1[2], rimNormal[0], rimNormal[1], rimNormal[2]});
        }
    };

    oldSize = vertices.size();
    addBowlInner(bucketX, bucketY, bucketZ, bucketWidth, bucketHeight, bucketDepth, bucketWallThickness, armAngle);
    vertexCounts[tiers + 5] = (vertices.size() - oldSize) / 6;

    oldSize = vertices.size();
    addBowlOuter(bucketX, bucketY, bucketZ, bucketWidth, bucketHeight, bucketDepth, armAngle);
    vertexCounts[tiers + 6] = (vertices.size() - oldSize) / 6;

    // Rubber band: connects arm to horizontal plank (provides upward pull force)
    float rubberArmX = armPivotX - rubberAttachArmOffset * cosf(armAngle);
    float rubberArmY = armPivotY - rubberAttachArmOffset * sinf(armAngle);
    float rubberArmZ = armPivotZ;

    float rubberPlankX = horizontalPlankX;
    float rubberPlankY = horizontalPlankY - horizontalPlankHeight / 2.0f; // Bottom of plank
    float rubberPlankZ = horizontalPlankZ;
    // Creates a stretched box between two points (for rubber bands/ropes)
    auto addStretchedBox = [&](float x1, float y1, float z1, float x2, float y2, float z2, float thickness)
    {
        float dx = x2 - x1, dy = y2 - y1, dz = z2 - z1;
        float length = sqrtf(dx * dx + dy * dy + dz * dz);
        float ndx = dx / length, ndy = dy / length, ndz = dz / length;
        float ht = thickness / 2.0f;
        glm::vec3 dir(ndx, ndy, ndz);
        glm::vec3 up(0, 1, 0);
        if (fabsf(ndy) > 0.99f) // Handle near-vertical case to avoid parallel vectors
            up = glm::vec3(1, 0, 0);

        // Create orthogonal basis for box cross-section
        glm::vec3 right = glm::normalize(glm::cross(dir, up));
        glm::vec3 forward = glm::normalize(glm::cross(right, dir));

        glm::vec3 p1(x1, y1, z1);
        glm::vec3 p2(x2, y2, z2);

        glm::vec3 corners[8] = {
            p1 - right * ht - forward * ht,
            p1 + right * ht - forward * ht,
            p1 + right * ht + forward * ht,
            p1 - right * ht + forward * ht,
            p2 - right * ht - forward * ht,
            p2 + right * ht - forward * ht,
            p2 + right * ht + forward * ht,
            p2 - right * ht + forward * ht};

        glm::vec3 n_front = -forward;
        glm::vec3 n_back = forward;
        glm::vec3 n_left = -right;
        glm::vec3 n_right = right;
        glm::vec3 n_start = -dir;
        glm::vec3 n_end = dir;
        vertices.insert(vertices.end(), {corners[0].x, corners[0].y, corners[0].z, n_front.x, n_front.y, n_front.z,
                                         corners[1].x, corners[1].y, corners[1].z, n_front.x, n_front.y, n_front.z,
                                         corners[2].x, corners[2].y, corners[2].z, n_front.x, n_front.y, n_front.z});
        vertices.insert(vertices.end(), {corners[2].x, corners[2].y, corners[2].z, n_front.x, n_front.y, n_front.z,
                                         corners[3].x, corners[3].y, corners[3].z, n_front.x, n_front.y, n_front.z,
                                         corners[0].x, corners[0].y, corners[0].z, n_front.x, n_front.y, n_front.z});

        vertices.insert(vertices.end(), {corners[4].x, corners[4].y, corners[4].z, n_back.x, n_back.y, n_back.z,
                                         corners[7].x, corners[7].y, corners[7].z, n_back.x, n_back.y, n_back.z,
                                         corners[6].x, corners[6].y, corners[6].z, n_back.x, n_back.y, n_back.z});
        vertices.insert(vertices.end(), {corners[6].x, corners[6].y, corners[6].z, n_back.x, n_back.y, n_back.z,
                                         corners[5].x, corners[5].y, corners[5].z, n_back.x, n_back.y, n_back.z,
                                         corners[4].x, corners[4].y, corners[4].z, n_back.x, n_back.y, n_back.z});

        vertices.insert(vertices.end(), {corners[0].x, corners[0].y, corners[0].z, n_left.x, n_left.y, n_left.z,
                                         corners[3].x, corners[3].y, corners[3].z, n_left.x, n_left.y, n_left.z,
                                         corners[7].x, corners[7].y, corners[7].z, n_left.x, n_left.y, n_left.z});
        vertices.insert(vertices.end(), {corners[7].x, corners[7].y, corners[7].z, n_left.x, n_left.y, n_left.z,
                                         corners[4].x, corners[4].y, corners[4].z, n_left.x, n_left.y, n_left.z,
                                         corners[0].x, corners[0].y, corners[0].z, n_left.x, n_left.y, n_left.z});

        vertices.insert(vertices.end(), {corners[1].x, corners[1].y, corners[1].z, n_right.x, n_right.y, n_right.z,
                                         corners[5].x, corners[5].y, corners[5].z, n_right.x, n_right.y, n_right.z,
                                         corners[6].x, corners[6].y, corners[6].z, n_right.x, n_right.y, n_right.z});
        vertices.insert(vertices.end(), {corners[6].x, corners[6].y, corners[6].z, n_right.x, n_right.y, n_right.z,
                                         corners[2].x, corners[2].y, corners[2].z, n_right.x, n_right.y, n_right.z,
                                         corners[1].x, corners[1].y, corners[1].z, n_right.x, n_right.y, n_right.z});

        vertices.insert(vertices.end(), {corners[3].x, corners[3].y, corners[3].z, forward.x, forward.y, forward.z,
                                         corners[2].x, corners[2].y, corners[2].z, forward.x, forward.y, forward.z,
                                         corners[6].x, corners[6].y, corners[6].z, forward.x, forward.y, forward.z});
        vertices.insert(vertices.end(), {corners[6].x, corners[6].y, corners[6].z, forward.x, forward.y, forward.z,
                                         corners[7].x, corners[7].y, corners[7].z, forward.x, forward.y, forward.z,
                                         corners[3].x, corners[3].y, corners[3].z, forward.x, forward.y, forward.z});
        vertices.insert(vertices.end(), {corners[0].x, corners[0].y, corners[0].z, -forward.x, -forward.y, -forward.z,
                                         corners[4].x, corners[4].y, corners[4].z, -forward.x, -forward.y, -forward.z,
                                         corners[5].x, corners[5].y, corners[5].z, -forward.x, -forward.y, -forward.z});
        vertices.insert(vertices.end(), {corners[5].x, corners[5].y, corners[5].z, -forward.x, -forward.y, -forward.z,
                                         corners[1].x, corners[1].y, corners[1].z, -forward.x, -forward.y, -forward.z,
                                         corners[0].x, corners[0].y, corners[0].z, -forward.x, -forward.y, -forward.z});
    };

    oldSize = vertices.size();
    if (rubberBandsVisible)
        addStretchedBox(rubberArmX, rubberArmY, rubberArmZ, rubberPlankX, rubberPlankY, rubberPlankZ, rubberThickness);
    vertexCounts[tiers + 7] = (vertices.size() - oldSize) / 6;

    vertexCounts[tiers + 8] = 0;

    // Release rope: disappears when fired
    oldSize = vertices.size();
    if (releaseRopeAttached)
    {
        float ropeArmX = armPivotX - ropeAttachArmOffset * cosf(armAngle) + ropeAttachArmXOffset;
        float ropeArmY = armPivotY - ropeAttachArmOffset * sinf(armAngle) + ropeAttachArmYOffset;
        float ropeArmZ = armPivotZ + ropeAttachArmZOffset;

        float ropeBaseX = ropeBaseAttachX;
        float ropeBaseY = ropeBaseAttachY;
        float ropeBaseZ = ropeBaseAttachZ;

        addStretchedBox(ropeArmX, ropeArmY, ropeArmZ, ropeBaseX, ropeBaseY, ropeBaseZ, releaseRopeThickness);
    }
    vertexCounts[tiers + 9] = (vertices.size() - oldSize) / 6;

    float xoff = wheelHalfWidthX;
    float zoff = wheelHalfDepthZ;
    int segments = 28;

    addCylinderZ(-xoff, -0.2f, zoff, wheelRadius, wheelThickness, segments);
    addCylinderZ(xoff, -0.2f, zoff, wheelRadius, wheelThickness, segments);
    addCylinderZ(-xoff, -0.2f, -zoff, wheelRadius, wheelThickness, segments);
    addCylinderZ(xoff, -0.2f, -zoff, wheelRadius, wheelThickness, segments);

    // Store vertex counts for wheels (each wheel has same vertex count)
    int perWheel = segments * 12; // 12 triangles per segment (caps + sides)
    int wheelStart = tiers + 10;
    for (int w = 0; w < 4; ++w)
        vertexCounts[wheelStart + w] = perWheel;
    for (int k = wheelStart + 4; k < 20; ++k)
        vertexCounts[k] = 0; // Zero out unused vertex count slots

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

// Returns world-space position of bucket with terrain transformations applied
glm::vec3 Catapult::getBucketPositionWorld(float terrainHeight, const glm::vec3 &terrainNormal) const
{
    // Get local bucket position
    float movingEndX = armPivotX - armDepth * cosf(armAngle);
    float movingEndY = armPivotY - armDepth * sinf(armAngle);
    float movingEndZ = armPivotZ;

    float armDirX = -cosf(armAngle);
    float armDirY = -sinf(armAngle);
    float connectionOffset = 0.16f;

    float bucketX = movingEndX + armDirX * connectionOffset + bucketOffsetX;
    float bucketY = movingEndY + armDirY * connectionOffset + bucketOffsetY;
    float bucketZ = movingEndZ + bucketOffsetZ;

    // Build transformation matrix matching catapult.draw()
    glm::mat4 transform = glm::mat4(1.0f);
    transform = glm::translate(transform, position);

    // Add terrain height + wheel offset
    float wheelY = -0.2f;
    float wheelBottomOffset = -(wheelY - wheelRadius);
    transform = glm::translate(transform, glm::vec3(0.0f, terrainHeight + wheelBottomOffset, 0.0f));

    // Rotate to match terrain slope
    glm::vec3 up(0.0f, 1.0f, 0.0f);
    glm::vec3 normal = glm::normalize(terrainNormal);
    glm::vec3 axis = glm::cross(up, normal);
    float angle = acosf(glm::clamp(glm::dot(up, normal), -1.0f, 1.0f));

    if (glm::length(axis) > 0.001f && angle > 0.001f)
    {
        axis = glm::normalize(axis);
        transform = glm::rotate(transform, angle, axis);
    }

    // Rotate around Y axis for steering
    transform = glm::rotate(transform, rotation, glm::vec3(0.0f, 1.0f, 0.0f));

    // Transform bucket position
    glm::vec3 localBucketPos(bucketX, bucketY, bucketZ);
    glm::vec4 transformedPos = transform * glm::vec4(localBucketPos, 1.0f);

    return glm::vec3(transformedPos);
}

// Initiates firing sequence: starts arm animation and cuts release rope
void Catapult::fire()
{
    if (!isAnimating)
    {
        isAnimating = true;
        animationTime = 0.0f;
        armAngle = startAngle;
        releaseRopeAttached = false; // Cut rope makes it disappear visually
    }
}

void Catapult::reset()
{
    isAnimating = false;
    animationTime = 0.0f;
    updateArmAngleFromSpeed();
    releaseRopeAttached = true;
    rebuildGeometry();
}

// Updates arm animation during firing sequence
void Catapult::update(float deltaTime)
{
    if (isAnimating)
    {
        animationTime += deltaTime;

        if (animationTime >= animationDuration)
        {
            armAngle = endAngle; // Animation complete
            isAnimating = false;
            rebuildGeometry();
        }
        else
        {
            // Interpolate arm angle with ease-out cubic for smooth motion
            float t = animationTime / animationDuration;
            t = 1.0f - powf(1.0f - t, 3.0f); // Ease-out cubic easing
            armAngle = startAngle + (endAngle - startAngle) * t;
            rebuildGeometry(); // Update geometry to reflect new arm position
        }
    }
}

// Updates arm angle based on current speed setting (0.0 to 1.0)
// Lower speed = higher angle (more pulled back), higher speed = lower angle (less pulled back)
void Catapult::updateArmAngleFromSpeed()
{
    float targetAngle = minSpeedAngle + (maxSpeedAngle - minSpeedAngle) * speed;

    if (!isAnimating)
    {
        armAngle = targetAngle;
        startAngle = targetAngle; // Starting angle for next fire matches current angle
        rebuildGeometry();
    }
}

void Catapult::increaseSpeed()
{
    if (!isAnimating)
    {
        speed += speedStep;
        if (speed > 1.0f)
            speed = 1.0f;
        updateArmAngleFromSpeed();
    }
}

void Catapult::decreaseSpeed()
{
    if (!isAnimating)
    {
        speed -= speedStep;
        if (speed < 0.0f)
            speed = 0.0f;
        updateArmAngleFromSpeed();
    }
}

float Catapult::getLaunchSpeed() const
{
    return minLaunchSpeed + (maxLaunchSpeed - minLaunchSpeed) * speed;
}

void Catapult::rotateLeft(float deltaTime)
{
    rotation += rotationSpeed * deltaTime;
    frontWheelSteerAngle += wheelSteerSpeed * deltaTime;
    if (frontWheelSteerAngle > maxWheelSteerAngle)
        frontWheelSteerAngle = maxWheelSteerAngle;
}

void Catapult::rotateRight(float deltaTime)
{
    rotation -= rotationSpeed * deltaTime;
    frontWheelSteerAngle -= wheelSteerSpeed * deltaTime;
    if (frontWheelSteerAngle < -maxWheelSteerAngle)
        frontWheelSteerAngle = -maxWheelSteerAngle;
}

void Catapult::moveForward(float deltaTime)
{
    glm::vec3 forward = getForwardDirection();
    position += forward * moveSpeed * deltaTime;
}

void Catapult::moveBackward(float deltaTime)
{
    glm::vec3 forward = getForwardDirection();
    position -= forward * moveSpeed * deltaTime;
}

// Returns forward direction vector based on catapult's Y-axis rotation
// Negative X because catapult faces -X direction at rotation = 0
glm::vec3 Catapult::getForwardDirection() const
{
    return glm::vec3(-cosf(rotation), 0.0f, sinf(rotation));
}

// Gradually returns front wheels to center position when not turning
void Catapult::updateWheelSteering(float deltaTime, bool isTurning)
{
    if (!isTurning)
    {
        float returnSpeed = 5.0f; // Radians per second return speed
        if (frontWheelSteerAngle > 0.01f)
        {
            frontWheelSteerAngle -= returnSpeed * deltaTime;
            if (frontWheelSteerAngle < 0.0f)
                frontWheelSteerAngle = 0.0f;
        }
        else if (frontWheelSteerAngle < -0.01f)
        {
            frontWheelSteerAngle += returnSpeed * deltaTime;
            if (frontWheelSteerAngle > 0.0f)
                frontWheelSteerAngle = 0.0f;
        }
    }
}

// Draws the entire catapult using stored vertex data
void Catapult::draw(unsigned int shaderProgram, float height, const glm::vec3 &terrainNormal)
{
    glUseProgram(shaderProgram);
    unsigned int modelLoc = glGetUniformLocation(shaderProgram, "model");
    unsigned int colorLoc = glGetUniformLocation(shaderProgram, "objectColor");

    // Build model matrix: translate to position, apply terrain height, rotate to match terrain slope, then rotate around Y
    // Wheels are positioned at Y = -0.2f relative to catapult origin
    // Wheel bottom is at Y = -0.2f - wheelRadius = -0.2f - 0.18f = -0.38f
    // To make wheels sit on terrain, we need to offset by 0.38f
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, position);

    // Offset to put bottom of wheels at terrain height
    float wheelY = -0.2f;                              // Wheel center Y position
    float wheelBottomOffset = -(wheelY - wheelRadius); // = -(-0.2f - 0.18f) = 0.38f
    model = glm::translate(model, glm::vec3(0.0f, height + wheelBottomOffset, 0.0f));

    // Rotate catapult to match terrain slope
    // Calculate rotation from up vector (0,1,0) to terrain normal
    glm::vec3 up(0.0f, 1.0f, 0.0f);
    glm::vec3 normal = glm::normalize(terrainNormal);

    // Calculate rotation axis and angle
    glm::vec3 axis = glm::cross(up, normal);
    float angle = acosf(glm::clamp(glm::dot(up, normal), -1.0f, 1.0f));

    // Only rotate if there's a significant slope (avoid division by zero)
    if (glm::length(axis) > 0.001f && angle > 0.001f)
    {
        axis = glm::normalize(axis);
        model = glm::rotate(model, angle, axis);
    }

    // Rotate around Y axis for steering
    model = glm::rotate(model, rotation, glm::vec3(0.0f, 1.0f, 0.0f));
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

    glBindVertexArray(VAO);

    int vertexOffset = 0;
    glUniform3f(colorLoc, 0.75f, 0.55f, 0.35f);
    int tiers = tierCount < 1 ? 1 : tierCount;
    for (int i = 0; i < tiers; ++i)
    {
        glDrawArrays(GL_TRIANGLES, vertexOffset, vertexCounts[i]);
        vertexOffset += vertexCounts[i];
    }

    glUniform3f(colorLoc, 0.75f, 0.55f, 0.35f);
    glDrawArrays(GL_TRIANGLES, vertexOffset, vertexCounts[tiers]);
    vertexOffset += vertexCounts[tiers];
    glDrawArrays(GL_TRIANGLES, vertexOffset, vertexCounts[tiers + 1]);
    vertexOffset += vertexCounts[tiers + 1];

    glUniform3f(colorLoc, 0.75f, 0.55f, 0.35f);
    glDrawArrays(GL_TRIANGLES, vertexOffset, vertexCounts[tiers + 2]);
    vertexOffset += vertexCounts[tiers + 2];

    glUniform3f(colorLoc, 0.5f, 0.35f, 0.2f);
    glDrawArrays(GL_TRIANGLES, vertexOffset, vertexCounts[tiers + 3]);
    vertexOffset += vertexCounts[tiers + 3];

    glUniform3f(colorLoc, 0.5f, 0.0f, 0.2f);
    glDrawArrays(GL_TRIANGLES, vertexOffset, vertexCounts[tiers + 4]);
    vertexOffset += vertexCounts[tiers + 4];

    glUniform3f(colorLoc, 0.5f, 0.5f, 0.5f);
    glDrawArrays(GL_TRIANGLES, vertexOffset, vertexCounts[tiers + 5]);
    vertexOffset += vertexCounts[tiers + 5];

    glUniform3f(colorLoc, 0.0f, 0.0f, 0.0f);
    glDrawArrays(GL_TRIANGLES, vertexOffset, vertexCounts[tiers + 6]);
    vertexOffset += vertexCounts[tiers + 6];

    glUniform3f(colorLoc, 0.3f, 0.2f, 0.1f);
    glDrawArrays(GL_TRIANGLES, vertexOffset, vertexCounts[tiers + 7]);
    vertexOffset += vertexCounts[tiers + 7];
    vertexOffset += vertexCounts[tiers + 8];

    glUniform3f(colorLoc, 1.0f, 1.0f, 1.0f);
    glDrawArrays(GL_TRIANGLES, vertexOffset, vertexCounts[tiers + 9]);
    vertexOffset += vertexCounts[tiers + 9];

    glUniform3f(colorLoc, 0.8f, 0.1f, 0.1f);

    float xoff = wheelHalfWidthX;
    float zoff = wheelHalfDepthZ;
    glm::vec3 wheelPositions[4] = {
        glm::vec3(-xoff, -0.2f, zoff),
        glm::vec3(xoff, -0.2f, zoff),
        glm::vec3(-xoff, -0.2f, -zoff),
        glm::vec3(xoff, -0.2f, -zoff)};

    for (int i = 0; i < 4; ++i)
    {
        // Apply steering rotation to front wheels (wheels 1 and 3)
        if (i == 1 || i == 3)
        {
            // Create separate transform for steered wheels
            // Use same terrain height + wheel offset + terrain slope as main catapult body
            glm::mat4 wheelModel = glm::mat4(1.0f);
            wheelModel = glm::translate(wheelModel, position);
            float wheelY = -0.2f;
            float wheelBottomOffset = -(wheelY - wheelRadius);
            wheelModel = glm::translate(wheelModel, glm::vec3(0.0f, height + wheelBottomOffset, 0.0f));

            // Rotate to match terrain slope (same as main body)
            glm::vec3 up(0.0f, 1.0f, 0.0f);
            glm::vec3 normal = glm::normalize(terrainNormal);
            glm::vec3 axis = glm::cross(up, normal);
            float angle = acosf(glm::clamp(glm::dot(up, normal), -1.0f, 1.0f));
            if (glm::length(axis) > 0.001f && angle > 0.001f)
            {
                axis = glm::normalize(axis);
                wheelModel = glm::rotate(wheelModel, angle, axis);
            }

            wheelModel = glm::rotate(wheelModel, rotation, glm::vec3(0.0f, 1.0f, 0.0f));
            wheelModel = glm::translate(wheelModel, wheelPositions[i]);
            wheelModel = glm::rotate(wheelModel, frontWheelSteerAngle, glm::vec3(0.0f, 1.0f, 0.0f));
            wheelModel = glm::translate(wheelModel, -wheelPositions[i]); // Rotate around wheel center
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(wheelModel));
        }

        glDrawArrays(GL_TRIANGLES, vertexOffset, vertexCounts[tiers + 10 + i]);
        vertexOffset += vertexCounts[tiers + 10 + i];

        if (i == 1 || i == 3)
        {
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        }
    }
}
