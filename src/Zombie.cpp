#include "Zombie.h"
#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <iostream>
#include "PathUtils.h"

// Static animation cache initialization
std::map<std::string, bool> Zombie::animationCacheLoaded;

Zombie::Zombie(const std::string &modelPath,
               const glm::vec3 &position,
               float scale,
               float speed,
               ZombieBehavior behavior,
               float detectionRadius,
               bool isBoss)
    : position(position), rotation(0.0f), speed(speed), scale(scale), alive(true), isBoss(isBoss),
      behavior(behavior), detectionRadius(detectionRadius), isChasing(false), attackRange(2.0f),
      runSpeedMultiplier(1.5f), 
      patrolPointA(position), patrolPointB(position), currentPatrolTarget(position), patrolTowardsB(true),
      currentAnimState(ZombieAnimationState::IDLE), animationTime(0.0f), animationSpeedMultiplier(1.0f),
      walkCycle(0.0f), walkSpeed(8.0f), isMoving(false),
      health(100.0f), maxHealth(100.0f) // Default health: 100, boss zombies can have more
{
    model = new Model(modelPath);

    // Initialize animation cache on first zombie creation
    if (animationCacheLoaded.empty())
    {
        initializeAnimationCache();
    }

    // Determine and set initial animation based on behavior
    ZombieAnimationState initialState;
    if (behavior == ZombieBehavior::IDLE)
        initialState = ZombieAnimationState::IDLE;
    else if (behavior == ZombieBehavior::PATROL)
        initialState = ZombieAnimationState::WALKING;
    else
        // Default to IDLE if unknown behavior
        initialState = ZombieAnimationState::IDLE;

    // Force set the initial state (bypass the state check)
    currentAnimState = initialState;

    // Load the initial animation into the model
    std::string animPath;
    switch (initialState)
    {
    case ZombieAnimationState::IDLE:
        animPath = FindImagePath("zombie/animation/Zombie Idle2.fbx");
        break;
    case ZombieAnimationState::WALKING:
        animPath = FindImagePath("zombie/animation/Zombie Walk2.fbx");
        break;
    case ZombieAnimationState::RUNNING:
    case ZombieAnimationState::ATTACKING:
        animPath = FindImagePath("zombie/animation/Zombie Running2.fbx");
        break;
    }

    // Load animation into model (Model::LoadAnimation handles the actual loading)
    bool isFirstLoad = (animationCacheLoaded.find(animPath) == animationCacheLoaded.end() || !animationCacheLoaded[animPath]);

    model->LoadAnimation(animPath);

    if (isFirstLoad)
    {
        animationCacheLoaded[animPath] = true;
        std::cout << "Loaded animation: " << animPath << std::endl;
    }

    std::cout << "Zombie initialized with animation: " << animPath << std::endl;
}

Zombie::~Zombie()
{
    delete model;
}

void Zombie::initializeAnimationCache()
{
    // Pre-load all animations into the cache (only called once)
    std::vector<std::string> animationPaths = {
        FindImagePath("zombie/animation/Zombie Idle2.fbx"),
        FindImagePath("zombie/animation/Zombie Walk2.fbx"),
        FindImagePath("zombie/animation/Zombie Running2.fbx"),
        FindImagePath("zombie/animation/Zombie Attack (2).fbx")};

    // Mark all animations as available (they'll be loaded on first use)
    for (const auto &path : animationPaths)
    {
        animationCacheLoaded[path] = false; 
    }
}

void Zombie::cleanupAnimationCache()
{
    animationCacheLoaded.clear();
}

void Zombie::setAnimationState(ZombieAnimationState state)
{
    if (currentAnimState != state)
    {
        // Special handling for switching to attack: stop movement and cut animation immediately
        if (state == ZombieAnimationState::ATTACKING && currentAnimState == ZombieAnimationState::RUNNING)
        {
            // Stop movement immediately when switching to attack
            isMoving = false;
            // Force immediate animation cut - don't wait for running animation to finish
            animationTime = 0.0f; // Reset before state change
        }

        currentAnimState = state;
        animationTime = 0.0f; // Reset animation time when changing states (cuts current animation)

        // Load the new animation into the model (this also resets the model's animation time)
        std::string animPath;
        switch (state)
        {
        case ZombieAnimationState::IDLE:
            animPath = FindImagePath("zombie/animation/Zombie Idle2.fbx");
            break;
        case ZombieAnimationState::WALKING:
            animPath = FindImagePath("zombie/animation/Zombie Walk2.fbx");
            break;
        case ZombieAnimationState::RUNNING:
            animPath = FindImagePath("zombie/animation/Zombie Running2.fbx");
            break;
        case ZombieAnimationState::ATTACKING:
            animPath = FindImagePath("zombie/animation/Zombie Attack (2).fbx");
            break;
        }

        // Load animation into model (Model::LoadAnimation handles the actual loading)
        bool isFirstLoad = (animationCacheLoaded.find(animPath) == animationCacheLoaded.end() || !animationCacheLoaded[animPath]);

        model->LoadAnimation(animPath);

        if (isFirstLoad)
        {
            animationCacheLoaded[animPath] = true;
            std::cout << "Loaded animation: " << animPath << std::endl;
        }
    }
}

void Zombie::setPatrolPoints(const glm::vec3 &pointA, const glm::vec3 &pointB)
{
    patrolPointA = pointA;
    patrolPointB = pointB;
    currentPatrolTarget = pointB;
    patrolTowardsB = true;
}

void Zombie::update(float deltaTime, const glm::vec3 &targetPosition, float terrainHeight, float distanceToCatapult)
{
    if (!alive)
        return;

    // Update based on behavior type
    switch (behavior)
    {
    case ZombieBehavior::IDLE:
        updateIdle(deltaTime, targetPosition, terrainHeight, distanceToCatapult);
        break;
    case ZombieBehavior::PATROL:
        updatePatrol(deltaTime, targetPosition, terrainHeight, distanceToCatapult);
        break;
    }

    // Update animation based on current state
    updateAnimation(deltaTime);
}

void Zombie::updateAnimation(float deltaTime)
{
    // Calculate animation speed based on zombie state and movement
    float stateSpeedMultiplier = 1.0f;

    switch (currentAnimState)
    {
    case ZombieAnimationState::IDLE:
        // Idle animation plays at normal speed
        stateSpeedMultiplier = 1.0f;
        break;

    case ZombieAnimationState::WALKING:
        // Walking animation speed matches movement speed
        stateSpeedMultiplier = speed / 2.0f; 
        break;

    case ZombieAnimationState::RUNNING:
        // Running animation speed matches movement speed
        stateSpeedMultiplier = speed / 4.5f;
        break;

    case ZombieAnimationState::ATTACKING:
        // Attack animation plays at consistent speed (not tied to movement)
        stateSpeedMultiplier = 1.0f;
        break;
    }

    // Apply both the state-based multiplier and the user-configurable multiplier
    float finalMultiplier = stateSpeedMultiplier * animationSpeedMultiplier;

    // Update the model's animation with adjusted delta time
    model->UpdateAnimation(deltaTime * finalMultiplier);
}

void Zombie::updateIdle(float deltaTime, const glm::vec3 &targetPosition, float terrainHeight, float distanceToCatapult)
{
    // Check if catapult is within attack range - IMMEDIATE switch, cut running animation
    if (distanceToCatapult <= attackRange)
    {
        // Attack! Stop everything immediately and cut running animation
        isChasing = false;
        isMoving = false;

        // Immediately switch to attack animation
        setAnimationState(ZombieAnimationState::ATTACKING);

        position.y = terrainHeight; // Keep on ground

        // Ensure zombie faces the target when attacking
        glm::vec3 direction = targetPosition - position;
        direction.y = 0.0f;
        if (glm::length(direction) > 0.01f)
        {
            direction = glm::normalize(direction);
            float angle = atan2f(direction.x, direction.z);
            rotation.y = angle;
        }
    }
    // Check if catapult is within detection radius
    else if (distanceToCatapult <= detectionRadius)
    {
        // Chase the catapult (running)
        isChasing = true;
        setAnimationState(ZombieAnimationState::RUNNING);
        moveTowardsTarget(deltaTime, targetPosition, terrainHeight);
    }
    else
    {
        // Stand idle
        isChasing = false;
        isMoving = false;
        setAnimationState(ZombieAnimationState::IDLE);
        position.y = terrainHeight; // Keep on ground
    }
}

void Zombie::updatePatrol(float deltaTime, const glm::vec3 &targetPosition, float terrainHeight, float distanceToCatapult)
{
    // Check if catapult is within attack range
    if (distanceToCatapult <= attackRange)
    {
       
        isChasing = false;
        isMoving = false;

        // Immediately switch to attack animation
        setAnimationState(ZombieAnimationState::ATTACKING);

        position.y = terrainHeight; // Keep on ground

        // Ensure zombie faces the target when attacking
        glm::vec3 direction = targetPosition - position;
        direction.y = 0.0f;
        if (glm::length(direction) > 0.01f)
        {
            direction = glm::normalize(direction);
            float angle = atan2f(direction.x, direction.z);
            rotation.y = angle;
        }
    }
    // Check if catapult is within detection radius
    else if (distanceToCatapult <= detectionRadius)
    {
        // Chase the catapult (running)
        isChasing = true;
        setAnimationState(ZombieAnimationState::RUNNING);
        moveTowardsTarget(deltaTime, targetPosition, terrainHeight);
    }
    else
    {
        // Patrol between two points (walking)
        isChasing = false;
        setAnimationState(ZombieAnimationState::WALKING);

        glm::vec3 direction = currentPatrolTarget - position;
        direction.y = 0.0f;
        float distanceToTarget = glm::length(direction);

        // If reached patrol point, switch direction
        if (distanceToTarget < 0.5f)
        {
            if (patrolTowardsB)
            {
                currentPatrolTarget = patrolPointA;
                patrolTowardsB = false;
            }
            else
            {
                currentPatrolTarget = patrolPointB;
                patrolTowardsB = true;
            }
        }

        // Move towards current patrol target
        moveTowardsTarget(deltaTime, currentPatrolTarget, terrainHeight);
    }
}

void Zombie::moveTowardsTarget(float deltaTime, const glm::vec3 &targetPosition, float terrainHeight)
{
    glm::vec3 direction = targetPosition - position;
    direction.y = 0.0f; // Keep zombies on the ground

    float distance = glm::length(direction);

    if (distance > 0.1f) // Don't move if very close
    {
        isMoving = true;
        direction = glm::normalize(direction);

        // Calculate effective speed based on animation state
        // Use runSpeedMultiplier when running, normal speed otherwise
        float effectiveSpeed = speed;
        if (currentAnimState == ZombieAnimationState::RUNNING)
        {
            effectiveSpeed = speed * runSpeedMultiplier; // Faster when running!
        }

        // Move towards target (only X and Z, Y will be set by gravity)
        position.x += direction.x * effectiveSpeed * deltaTime;
        position.z += direction.z * effectiveSpeed * deltaTime;

        // Apply gravity - set Y position to terrain height
        position.y = terrainHeight;

        // Rotate zombie to face movement direction
        float angle = atan2f(direction.x, direction.z);
        rotation.y = angle;

        // Update walking animation cycle
        walkCycle += walkSpeed * deltaTime;
        if (walkCycle > 2.0f * 3.14159f)
            walkCycle -= 2.0f * 3.14159f;
    }
    else
    {
        isMoving = false;
        // Gradually stop animation when not moving
        walkCycle *= 0.95f;
    }
}

void Zombie::draw(unsigned int shaderProgram)
{
    if (!alive)
        return;

    glUseProgram(shaderProgram);

    // Build model matrix
    glm::mat4 modelMatrix = glm::mat4(1.0f);

    // Translate to position
    modelMatrix = glm::translate(modelMatrix, position);

    // Rotate to face movement direction
    modelMatrix = glm::rotate(modelMatrix, rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));

    // Scale the zombie (uses configurable scale value)
    modelMatrix = glm::scale(modelMatrix, glm::vec3(scale));

    // Set model matrix uniform
    unsigned int modelLoc = glGetUniformLocation(shaderProgram, "model");
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(modelMatrix));

    // Set color (will be overridden by texture if available)
    unsigned int colorLoc = glGetUniformLocation(shaderProgram, "objectColor");
    glUniform3f(colorLoc, 0.8f, 0.8f, 0.8f);

    // Draw the model
    model->Draw(shaderProgram);
}
