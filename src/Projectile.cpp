#include "Projectile.h"
#include "Terrain.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>
#include <cmath>

Projectile::Projectile(glm::vec3 startPos)
    : position(startPos), velocity(0.0f), isLaunched(false), hasHit(false), damageApplied(false),
      impactTime(0.0f), isAnimating(false), impactPosition(0.0f), hasShattered(false),
      baseDamage(50.0f), innerRadius(2.0f), outerRadius(5.0f), outerDamageMultiplier(0.3f)
{
    initMesh();
}

void Projectile::launch(glm::vec3 initialVelocity)
{
    velocity = initialVelocity;
    isLaunched = true;
    hasHit = false;
    damageApplied = false;
    isAnimating = false;
    impactTime = 0.0f;
    hasShattered = false;
    fragments.clear();
}

void Projectile::update(float deltaTime, Terrain *terrain)
{
    if (isAnimating)
    {
        // Update impact animation
        impactTime += deltaTime;
        const float animationDuration = 2.0f; // Animation lasts 2 seconds (fragments fade out)

        // Update shatter fragments
        updateFragments(deltaTime);

        if (impactTime >= animationDuration)
        {
            isAnimating = false;
            isLaunched = false;
            fragments.clear(); // Clean up fragments
        }
        return;
    }

    if (isLaunched && !hasHit)
    {
        velocity += glm::vec3(0.0f, -9.81f, 0.0f) * deltaTime; // gravity
        position += velocity * deltaTime;

        // Check collision with terrain and obstacles
        if (checkCollision(terrain))
        {
            // Collision detected, start impact animation
            startImpactAnimation(position);
        }
    }
}

bool Projectile::checkCollision(Terrain *terrain)
{
    if (!terrain || hasHit)
        return false;

    // Check ground collision
    float terrainHeight = terrain->getHeight(position.x, position.z);
    if (position.y <= terrainHeight + 0.15f) // 0.15f is projectile radius
    {
        position.y = terrainHeight + 0.15f;
        hasHit = true;
        return true;
    }

    // Check collision with trees
    if (terrain->checkTreeCollision(position.x, position.z, 0.15f))
    {
        hasHit = true;
        return true;
    }

    // Check collision with walls
    if (terrain->checkWallCollision(position.x, position.z, 0.15f))
    {
        hasHit = true;
        return true;
    }

    return false;
}

void Projectile::startImpactAnimation(glm::vec3 hitPosition)
{
    impactPosition = hitPosition;
    position = hitPosition;
    velocity = glm::vec3(0.0f);
    isAnimating = true;
    impactTime = 0.0f;
    hasShattered = false;

    // Create shatter effect
    createShatterEffect(hitPosition);
}

void Projectile::createShatterEffect(glm::vec3 hitPosition)
{
    if (hasShattered)
        return; // Already created

    hasShattered = true;
    fragments.clear();

    // Create 24 fragments that spread outward in all directions
    int numFragments = 24;
    for (int i = 0; i < numFragments; i++)
    {
        Fragment frag;
        frag.position = hitPosition;

        // Better distribution: use golden angle spiral for even distribution
        float goldenAngle = 2.399963229728653f;              // Golden angle in radians
        float theta = i * goldenAngle;                       // Spiral angle
        float y = 1.0f - (2.0f * i) / (numFragments - 1.0f); // Distribute from -1 to 1
        float radius = sqrtf(1.0f - y * y);                  // Radius on sphere

        // Random speed variation for more dynamic effect
        float speed = 3.0f + (i % 7) * 0.4f; // Speed between 3.0 and 5.4

        // Calculate direction vector
        frag.velocity = glm::vec3(
            radius * cosf(theta) * speed,
            y * speed + 0.5f, // Add upward bias
            radius * sinf(theta) * speed);

        frag.life = 1.0f;                    // Full life
        frag.size = 0.08f + (i % 4) * 0.03f; // Size between 0.08 and 0.17 (more visible)

        fragments.push_back(frag);
    }
}

void Projectile::updateFragments(float deltaTime)
{
    for (auto &frag : fragments)
    {
        // Update position
        frag.position += frag.velocity * deltaTime;

        // Apply gravity
        frag.velocity += glm::vec3(0.0f, -9.81f, 0.0f) * deltaTime;

        // Fade out over time
        frag.life -= deltaTime / 2.0f; // Fade out over 2 seconds
        if (frag.life < 0.0f)
            frag.life = 0.0f;

        // Slow down over time (friction)
        frag.velocity *= 0.98f;
    }
}

void Projectile::drawFragments(unsigned int shaderProgram)
{
    unsigned int colorLoc = glGetUniformLocation(shaderProgram, "objectColor");

    for (const auto &frag : fragments)
    {
        if (frag.life <= 0.0f)
            continue; // Skip dead fragments

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, frag.position);

        // Scale based on life (fade out)
        float scale = frag.size * frag.life;
        model = glm::scale(model, glm::vec3(scale));

        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));

        // Set color - start with rock color, fade to darker as life decreases
        float lifeFactor = frag.life;
        float r = 0.35f * lifeFactor;
        float g = 0.3f * lifeFactor;
        float b = 0.25f * lifeFactor;
        glUniform3f(colorLoc, r, g, b);

        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, vertexCount);
        glBindVertexArray(0);
    }
}

float Projectile::calculateDamage(float distanceFromImpact) const
{
    if (distanceFromImpact <= innerRadius)
    {
        // Full damage in inner circle
        return baseDamage;
    }
    else if (distanceFromImpact <= outerRadius)
    {
        // Reduced damage in outer circle (linear falloff)
        float t = (distanceFromImpact - innerRadius) / (outerRadius - innerRadius);
        float damageMultiplier = 1.0f - (t * (1.0f - outerDamageMultiplier));
        return baseDamage * damageMultiplier;
    }
    else
    {
        // No damage outside outer circle
        return 0.0f;
    }
}

void Projectile::draw(unsigned int shaderProgram)
{
    if (isAnimating && hasShattered)
    {
        // Draw shatter fragments instead of main projectile
        drawFragments(shaderProgram);
    }
    else if (!isAnimating)
    {
        // Draw normal projectile
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, position);

        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));

        unsigned int colorLoc = glGetUniformLocation(shaderProgram, "objectColor");
        glUniform3f(colorLoc, 0.35f, 0.3f, 0.25f); // Rock color

        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, vertexCount); // sphere vertices
        glBindVertexArray(0);
    }
}

// ----- Initialize sphere mesh (rock-like projectile) -----
void Projectile::initMesh()
{
    std::vector<float> vertices;
    float radius = 0.15f; // Rock size
    int stacks = 16;      // Vertical divisions
    int slices = 16;      // Horizontal divisions

    // Generate sphere vertices with normals
    for (int i = 0; i <= stacks; ++i)
    {
        float V = i / (float)stacks;
        float phi = V * 3.1415926535f; // 0 to PI

        for (int j = 0; j <= slices; ++j)
        {
            float U = j / (float)slices;
            float theta = U * 2.0f * 3.1415926535f; // 0 to 2*PI

            // Normal (unit sphere direction)
            float nx = cosf(theta) * sinf(phi);
            float ny = cosf(phi);
            float nz = sinf(theta) * sinf(phi);

            // Position
            float x = nx * radius;
            float y = ny * radius;
            float z = nz * radius;

            // Store position + normal (6 floats per vertex)
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);
            vertices.push_back(nx);
            vertices.push_back(ny);
            vertices.push_back(nz);
        }
    }

    // Generate sphere triangles
    std::vector<float> finalVertices;
    for (int i = 0; i < stacks; ++i)
    {
        for (int j = 0; j < slices; ++j)
        {
            int first = i * (slices + 1) + j;
            int second = first + slices + 1;

            // First triangle
            finalVertices.insert(finalVertices.end(),
                                 {vertices[first * 6], vertices[first * 6 + 1], vertices[first * 6 + 2],
                                  vertices[first * 6 + 3], vertices[first * 6 + 4], vertices[first * 6 + 5]});

            finalVertices.insert(finalVertices.end(),
                                 {vertices[second * 6], vertices[second * 6 + 1], vertices[second * 6 + 2],
                                  vertices[second * 6 + 3], vertices[second * 6 + 4], vertices[second * 6 + 5]});

            finalVertices.insert(finalVertices.end(),
                                 {vertices[(first + 1) * 6], vertices[(first + 1) * 6 + 1], vertices[(first + 1) * 6 + 2],
                                  vertices[(first + 1) * 6 + 3], vertices[(first + 1) * 6 + 4], vertices[(first + 1) * 6 + 5]});

            // Second triangle
            finalVertices.insert(finalVertices.end(),
                                 {vertices[second * 6], vertices[second * 6 + 1], vertices[second * 6 + 2],
                                  vertices[second * 6 + 3], vertices[second * 6 + 4], vertices[second * 6 + 5]});

            finalVertices.insert(finalVertices.end(),
                                 {vertices[(second + 1) * 6], vertices[(second + 1) * 6 + 1], vertices[(second + 1) * 6 + 2],
                                  vertices[(second + 1) * 6 + 3], vertices[(second + 1) * 6 + 4], vertices[(second + 1) * 6 + 5]});

            finalVertices.insert(finalVertices.end(),
                                 {vertices[(first + 1) * 6], vertices[(first + 1) * 6 + 1], vertices[(first + 1) * 6 + 2],
                                  vertices[(first + 1) * 6 + 3], vertices[(first + 1) * 6 + 4], vertices[(first + 1) * 6 + 5]});
        }
    }

    vertexCount = finalVertices.size() / 6;

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, finalVertices.size() * sizeof(float), finalVertices.data(), GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    // Normal attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}
