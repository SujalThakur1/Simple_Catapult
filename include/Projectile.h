#pragma once
#include <glm/glm.hpp>
#include <GL/glew.h>
#include <vector>

class Projectile
{
public:
    glm::vec3 position;
    glm::vec3 velocity;
    bool isLaunched;
    bool hasHit;        // Track if projectile has hit something
    bool damageApplied; // Track if damage has been applied (to prevent multiple applications)

    unsigned int VAO, VBO;
    int vertexCount;

    // Position offsets relative to bucket
    float bucketOffsetX = 0.0f;
    float bucketOffsetY = 0.1f;
    float bucketOffsetZ = 0.0f;

    // Impact animation
    float impactTime;         // Time since impact
    bool isAnimating;         // Whether impact animation is playing
    glm::vec3 impactPosition; // Where the projectile hit

    // Shatter effect - fragments
    struct Fragment
    {
        glm::vec3 position;
        glm::vec3 velocity;
        float life; // Life remaining (0.0 to 1.0)
        float size; // Fragment size
    };
    std::vector<Fragment> fragments; // Shatter fragments
    bool hasShattered;               // Whether shatter effect has been created

    // Damage system
    float baseDamage;            // Base damage of projectile
    float innerRadius;           // Inner circle radius (full damage)
    float outerRadius;           // Outer circle radius (reduced damage)
    float outerDamageMultiplier; // Damage multiplier for outer circle (0.0 to 1.0)

    Projectile(glm::vec3 startPos);

    void launch(glm::vec3 initialVelocity);
    void update(float deltaTime, class Terrain *terrain);
    void draw(unsigned int shaderProgram);

    // Damage calculation based on distance from impact
    float calculateDamage(float distanceFromImpact) const;

    // Check if projectile has hit something
    bool checkCollision(class Terrain *terrain);

    // Create shatter effect
    void createShatterEffect(glm::vec3 hitPosition);

    // Position control methods (relative to bucket)
    void setBucketOffset(float x, float y, float z)
    {
        bucketOffsetX = x;
        bucketOffsetY = y;
        bucketOffsetZ = z;
    }
    void setBucketOffsetX(float x) { bucketOffsetX = x; }
    void setBucketOffsetY(float y) { bucketOffsetY = y; }
    void setBucketOffsetZ(float z) { bucketOffsetZ = z; }

private:
    void initMesh();
    void startImpactAnimation(glm::vec3 hitPosition);
    void updateFragments(float deltaTime);
    void drawFragments(unsigned int shaderProgram);
};
