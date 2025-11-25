#ifndef ZOMBIE_H
#define ZOMBIE_H

#include "Model.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <string>
#include <map>

// Zombie behavior types
enum class ZombieBehavior
{
    IDLE,  // Stands still, only chases when catapult is in range
    PATROL // Moves between two points, chases when catapult is in range
};

// Zombie animation states
enum class ZombieAnimationState
{
    IDLE,
    WALKING,
    RUNNING,
    ATTACKING
};

class Zombie
{
public:
    // Enhanced constructor with configurable settings
    Zombie(const std::string &modelPath,
           const glm::vec3 &position,
           float scale = 0.002f,
           float speed = 1.0f,
           ZombieBehavior behavior = ZombieBehavior::IDLE,
           float detectionRadius = 10.0f,
           bool isBoss = false);
    ~Zombie();

    // Update with catapult distance checking
    void update(float deltaTime, const glm::vec3 &targetPosition, float terrainHeight, float distanceToCatapult);
    void draw(unsigned int shaderProgram);

    // Animation control
    void setAnimationState(ZombieAnimationState state);

    // Static animation cache to share animations between all zombies
    static void initializeAnimationCache();
    static void cleanupAnimationCache();

    // Getters and setters
    glm::vec3 getPosition() const { return position; }
    void setPosition(const glm::vec3 &newPosition) { position = newPosition; }
    bool isAlive() const { return alive; }
    void setAlive(bool state) { alive = state; }

    // Health system
    float getHealth() const { return health; }
    float getMaxHealth() const { return maxHealth; }
    void setHealth(float newHealth)
    {
        health = newHealth;
        if (health <= 0.0f)
        {
            health = 0.0f;
            alive = false;
        }
    }
    void setMaxHealth(float newMaxHealth) { maxHealth = newMaxHealth; }
    void takeDamage(float damage)
    {
        health -= damage;
        if (health <= 0.0f)
        {
            health = 0.0f;
            alive = false;
        }
    }
    float getSpeed() const { return speed; }
    void setSpeed(float newSpeed) { speed = newSpeed; }
    float getScale() const { return scale; }
    void setScale(float newScale) { scale = newScale; }
    bool getIsBoss() const { return isBoss; }

    // Animation speed control
    void setAnimationSpeedMultiplier(float multiplier) { animationSpeedMultiplier = multiplier; }
    float getAnimationSpeedMultiplier() const { return animationSpeedMultiplier; }

    // Run speed control
    void setRunSpeedMultiplier(float multiplier) { runSpeedMultiplier = multiplier; }
    float getRunSpeedMultiplier() const { return runSpeedMultiplier; }

    // Rotation control
    void setRotationY(float angle) { rotation.y = angle; }
    float getRotationY() const { return rotation.y; }

    // Patrol settings (only used if behavior is PATROL)
    void setPatrolPoints(const glm::vec3 &pointA, const glm::vec3 &pointB);
    void setBehavior(ZombieBehavior newBehavior) { behavior = newBehavior; }
    void setDetectionRadius(float radius) { detectionRadius = radius; }

    // Check if zombie is currently attacking
    bool isAttacking() const { return currentAnimState == ZombieAnimationState::ATTACKING; }

private:
    Model *model;
    glm::vec3 position;
    glm::vec3 rotation;
    float speed;
    float scale;
    bool alive;
    bool isBoss;

    // Health system
    float health;
    float maxHealth;

    // Behavior system
    ZombieBehavior behavior;
    float detectionRadius;    // How close catapult needs to be before zombie reacts
    bool isChasing;           // Currently chasing the catapult
    float attackRange;        // Distance at which zombie attacks instead of chasing
    float runSpeedMultiplier; // Speed multiplier when running (default 1.5 = 50% faster)

    // Patrol system (for PATROL behavior)
    glm::vec3 patrolPointA;
    glm::vec3 patrolPointB;
    glm::vec3 currentPatrolTarget;
    bool patrolTowardsB; // true = moving to B, false = moving to A

    // Animation system
    ZombieAnimationState currentAnimState;
    float animationTime;
    float animationSpeedMultiplier;

    // Static animation cache (shared between all zombies)
    static std::map<std::string, bool> animationCacheLoaded;

    // Animation variables
    float walkCycle;
    float walkSpeed;
    bool isMoving;

    // Movement functions
    void updateIdle(float deltaTime, const glm::vec3 &targetPosition, float terrainHeight, float distanceToCatapult);
    void updatePatrol(float deltaTime, const glm::vec3 &targetPosition, float terrainHeight, float distanceToCatapult);
    void moveTowardsTarget(float deltaTime, const glm::vec3 &targetPosition, float terrainHeight);

    // Animation helpers
    void updateAnimation(float deltaTime);
};

#endif
