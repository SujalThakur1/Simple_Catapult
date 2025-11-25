#pragma once
#include <GL/glew.h>
#include <glm/glm.hpp>

class Catapult
{
public:
    Catapult();
    void draw(unsigned int shaderProgram, float height = 0.0f, const glm::vec3 &terrainNormal = glm::vec3(0.0f, 1.0f, 0.0f));
    void update(float deltaTime); // Update animation
    void fire();                  // Trigger arm animation
    void reset();                 // Reset catapult to initial state (reattach release rope, reset arm)
    float getArmAngle() const { return armAngle; }
    bool isFiring() const { return isAnimating; }
    glm::vec3 getBucketPositionWorld(float terrainHeight, const glm::vec3 &terrainNormal) const; // Get bucket position with terrain transformations

    // Speed control (affects arm angle and launch power)
    void increaseSpeed();                    // Increase launch speed (up arrow)
    void decreaseSpeed();                    // Decrease launch speed (down arrow)
    float getSpeed() const { return speed; } // Get current speed (0.0 to 1.0)
    float getLaunchSpeed() const;            // Get actual launch velocity

    // Movement and rotation controls
    void rotateLeft(float deltaTime);                          // Rotate catapult left
    void rotateRight(float deltaTime);                         // Rotate catapult right
    void moveForward(float deltaTime);                         // Move catapult forward
    void moveBackward(float deltaTime);                        // Move catapult backward
    void updateWheelSteering(float deltaTime, bool isTurning); // Update wheel steering angle
    float getRotation() const { return rotation; }
    void setRotation(float newRotation) { rotation = newRotation; } // Set rotation in radians
    glm::vec3 getPosition() const { return position; }
    void setPosition(const glm::vec3 &newPosition) { position = newPosition; }
    glm::vec3 getForwardDirection() const; // Get forward direction based on rotation

    // Health system
    float getHealth() const { return health; }
    float getMaxHealth() const { return maxHealth; }
    void setHealth(float newHealth) { health = glm::clamp(newHealth, 0.0f, maxHealth); }
    void setMaxHealth(float newMaxHealth) { maxHealth = newMaxHealth; }
    void takeDamage(float damage)
    {
        health -= damage;
        if (health < 0.0f)
            health = 0.0f;
    }
    bool isAlive() const { return health > 0.0f; }

    // Configuration (call anytime; geometry will rebuild automatically)
    void setBaseDimensions(float widthX, float depthZ, float heightY)
    {
        plankWidthX = widthX;
        plankDepthZ = depthZ;
        plankHeight = heightY;
        rebuildGeometry();
    }
    void setTiering(int count, float gapY)
    {
        tierCount = count;
        tierGap = gapY;
        rebuildGeometry();
    }
    void setBaseCenterY(float y)
    {
        baseCenterY = y;
        rebuildGeometry();
    }
    void setBaseCenterZ(float z)
    {
        baseCenterZ = z;
        rebuildGeometry();
    }
    void setBaseCenterX(float x)
    {
        baseCenterX = x;
        rebuildGeometry();
    }

    // Wheel spacing and size (rectangle formed by 4 wheels)
    void setWheelSpacing(float halfWidthX, float halfDepthZ)
    {
        wheelHalfWidthX = halfWidthX; // distance from center to left/right wheels
        wheelHalfDepthZ = halfDepthZ; // distance from center to front/back wheels
        rebuildGeometry();
    }
    void setWheelSize(float radius, float thickness)
    {
        wheelRadius = radius;
        wheelThickness = thickness;
        rebuildGeometry();
    }

    // Vertical plank positions (you can set X, Y, Z for each plank)
    void setVerticalPlank1Position(float x, float y, float z)
    {
        verticalPlank1X = x;
        verticalPlank1Y = y;
        verticalPlank1Z = z;
        rebuildGeometry();
    }
    void setVerticalPlank2Position(float x, float y, float z)
    {
        verticalPlank2X = x;
        verticalPlank2Y = y;
        verticalPlank2Z = z;
        rebuildGeometry();
    }

    // Vertical plank dimensions (width, height, depth for each plank)
    void setVerticalPlank1Size(float width, float height, float depth)
    {
        verticalPlank1Width = width;
        verticalPlank1Height = height;
        verticalPlank1Depth = depth;
        rebuildGeometry();
    }
    void setVerticalPlank2Size(float width, float height, float depth)
    {
        verticalPlank2Width = width;
        verticalPlank2Height = height;
        verticalPlank2Depth = depth;
        rebuildGeometry();
    }

    // Horizontal plank (additional plank, horizontal orientation)
    void setHorizontalPlankPosition(float x, float y, float z)
    {
        horizontalPlankX = x;
        horizontalPlankY = y;
        horizontalPlankZ = z;
        rebuildGeometry();
    }
    void setHorizontalPlankSize(float width, float height, float depth)
    {
        horizontalPlankWidth = width;
        horizontalPlankHeight = height;
        horizontalPlankDepth = depth;
        rebuildGeometry();
    }

    // Rope controls
    void setRopePosition(float x, float y, float z)
    {
        ropeX = x;
        ropeY = y;
        ropeZ = z;
        rebuildGeometry();
    }
    void setRopeSize(float width, float height, float depth)
    {
        ropeWidth = width;
        ropeHeight = height;
        ropeDepth = depth;
        rebuildGeometry();
    }

    // Throwing arm controls
    void setArmPivotPosition(float x, float y, float z)
    {
        armPivotX = x;
        armPivotY = y;
        armPivotZ = z;
        rebuildGeometry();
    }
    void setArmSize(float width, float height, float depth)
    {
        armWidth = width;
        armHeight = height;
        armDepth = depth;
        rebuildGeometry();
    }
    void setArmLength(float length)
    {
        armLength = length;
        rebuildGeometry();
    }
    void setArmRotation(float angle) // Angle in radians
    {
        armAngle = angle;
        rebuildGeometry();
    }
    float getArmRotation() const { return armAngle; }

    // Animation controls
    void setAnimationDuration(float duration) { animationDuration = duration; }
    void setStartAngle(float angle)
    {
        startAngle = angle;
        if (!isAnimating) // Only update armAngle if not currently animating
        {
            armAngle = startAngle;
        }
        rebuildGeometry();
    }
    void setEndAngle(float angle) { endAngle = angle; } // End angle
    float getEndAngle() const { return endAngle; }      // Get current end angle setting
    float getStartAngle() const { return startAngle; }  // Get current start angle

    // Convenience: Set angle in degrees
    void setEndAngleDegrees(float degrees) { endAngle = degrees * 3.1415926535f / 180.0f; }
    void setStartAngleDegrees(float degrees)
    {
        startAngle = degrees * 3.1415926535f / 180.0f;
        if (!isAnimating) // Only update armAngle if not currently animating
        {
            armAngle = startAngle;
        }
        rebuildGeometry();
    }
    float getEndAngleDegrees() const { return endAngle * 180.0f / 3.1415926535f; }
    float getStartAngleDegrees() const { return startAngle * 180.0f / 3.1415926535f; }

    // Bucket/Spoon controls
    void setBucketPosition(float x, float y, float z) // Position relative to arm end
    {
        bucketOffsetX = x;
        bucketOffsetY = y;
        bucketOffsetZ = z;
        rebuildGeometry();
    }
    void setBucketSize(float width, float height, float depth)
    {
        bucketWidth = width;
        bucketHeight = height;
        bucketDepth = depth;
        rebuildGeometry();
    }
    void setBucketWallThickness(float thickness)
    {
        bucketWallThickness = thickness;
        rebuildGeometry();
    }

    // Rope base attachment controls
    void setRopeBaseAttachPosition(float x, float y, float z)
    {
        ropeBaseAttachX = x;
        ropeBaseAttachY = y;
        ropeBaseAttachZ = z;
        rebuildGeometry();
    }
    void setRopeBaseAttachX(float x)
    {
        ropeBaseAttachX = x;
        rebuildGeometry();
    }
    void setRopeBaseAttachY(float y)
    {
        ropeBaseAttachY = y;
        rebuildGeometry();
    }
    void setRopeBaseAttachZ(float z)
    {
        ropeBaseAttachZ = z;
        rebuildGeometry();
    }

    // Rope arm attachment controls (where rope connects to arm)
    void setRopeAttachArmPosition(float xOffset, float yOffset, float zOffset)
    {
        ropeAttachArmXOffset = xOffset;
        ropeAttachArmYOffset = yOffset;
        ropeAttachArmZOffset = zOffset;
        rebuildGeometry();
    }
    void setRopeAttachArmXOffset(float xOffset)
    {
        ropeAttachArmXOffset = xOffset;
        rebuildGeometry();
    }
    void setRopeAttachArmYOffset(float yOffset)
    {
        ropeAttachArmYOffset = yOffset;
        rebuildGeometry();
    }
    void setRopeAttachArmZOffset(float zOffset)
    {
        ropeAttachArmZOffset = zOffset;
        rebuildGeometry();
    }
    void setRopeAttachArmOffset(float offset)
    {
        ropeAttachArmOffset = offset;
        rebuildGeometry();
    }

private:
    unsigned int VAO, VBO;
    float armAngle;      // Current arm rotation angle
    bool isAnimating;    // Whether arm is currently animating
    float animationTime; // Time elapsed in animation

    glm::vec3 pivotPoint; // Arm pivot point
    float armLength_old;  // Length of arm from pivot to bucket
    int vertexCounts[20]; // Vertex counts for each component

    // Base/tier configuration
    float plankWidthX = 2.25f;
    float plankDepthZ = 0.62f;
    float plankHeight = 0.08f;
    int tierCount = 1;
    float tierGap = 0.02f;
    float baseCenterY = -0.22f;
    float baseCenterZ = 0.0f;
    float baseCenterX = -0.25f;

    // Wheel configuration (spacing and size)
    float wheelHalfWidthX = 0.7f;
    float wheelHalfDepthZ = 0.35f;
    float wheelRadius = 0.18f;
    float wheelThickness = 0.08f;

    // Vertical plank positions
    float verticalPlank1X = 0.0f;
    float verticalPlank1Y = 0.35f;
    float verticalPlank1Z = 0.30f;
    float verticalPlank2X = 0.0f;
    float verticalPlank2Y = 0.35f;
    float verticalPlank2Z = -0.30f;

    // Vertical plank dimensions
    float verticalPlank1Width = 0.1f;
    float verticalPlank1Height = 1.15f;
    float verticalPlank1Depth = 0.01f;
    float verticalPlank2Width = 0.1f;
    float verticalPlank2Height = 1.15f;
    float verticalPlank2Depth = 0.01f;

    // Horizontal stick (solid, horizontal along Z axis)
    float horizontalPlankX = 0.0f;
    float horizontalPlankY = 0.90f;
    float horizontalPlankZ = 0.0f;
    float horizontalPlankWidth = 0.08f;
    float horizontalPlankHeight = 0.08f;
    float horizontalPlankDepth = 0.59f;

    // Rope (horizontal along Z axis, much thicker so visible)
    float ropeX = 0.0f;
    float ropeY = -0.2f;
    float ropeZ = 0.0f;
    float ropeWidth = 0.03f;
    float ropeHeight = 0.03f;
    float ropeDepth = 0.59f;

    // Throwing arm
    float armPivotX = 0.0f;
    float armPivotY = -0.2f;
    float armPivotZ = 0.0f;
    float armWidth = 0.04f;
    float armHeight = 0.04f;
    float armDepth = 1.5f;
    float armLength = 2.9f;

    // Animation parameters
    float startAngle = -1.2f;
    float endAngle = -1.5f;
    float animationDuration = 0.5f;

    // Speed control parameters
    float speed = 1.0f;
    float minSpeedAngle = -1.1f;
    float maxSpeedAngle = -0.03f;
    float minLaunchSpeed = 5.0f;
    float maxLaunchSpeed = 15.0f;
    float speedStep = 0.05f;

    // Bucket/Spoon at end of arm
    float bucketOffsetX = 0.0f;
    float bucketOffsetY = 0.0f;
    float bucketOffsetZ = 0.0f;
    float bucketWidth = 0.4f;
    float bucketHeight = 0.2f;
    float bucketDepth = 0.4f;
    float bucketWallThickness = 0.00f;

    // Rubber band configuration
    float rubberThickness = 0.015f;
    float rubberAttachArmOffset = 1.1f;
    bool rubberBandsVisible = true;

    // Release rope configuration
    float releaseRopeThickness = 0.02f;
    bool releaseRopeAttached = true;
    float ropeAttachArmOffset = 1.36f;

    // Rope arm attachment offsets
    float ropeAttachArmXOffset = 0.0f;
    float ropeAttachArmYOffset = 0.0f;
    float ropeAttachArmZOffset = 0.0f;

    // Rope base attachment point
    float ropeBaseAttachX = -1.365f;
    float ropeBaseAttachY = -0.22f;
    float ropeBaseAttachZ = 0.0f;

    // Movement and rotation state
    glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f);
    float rotation = 0.0f;
    float rotationSpeed = 1.5f;
    float moveSpeed = 2.0f;

    // Front wheel steering
    float frontWheelSteerAngle = 0.0f;
    float maxWheelSteerAngle = 0.4f;
    float wheelSteerSpeed = 3.0f;

    // Health system
    float health = 100.0f;
    float maxHealth = 100.0f;

    void rebuildGeometry();
    void updateArmAngleFromSpeed();
};
