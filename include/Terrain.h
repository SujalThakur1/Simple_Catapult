#pragma once
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include "Model.h"

struct TreeInstance
{
    Model *model;
    glm::vec3 position;
    float rotation; // Rotation around Y axis
    float scale;    // Scale factor for variation
};

struct RockWallInstance
{
    Model *model;
    glm::vec3 position;
    float rotation; // Rotation around Y axis
    float scale;    // Scale factor for variation
};

class Terrain
{
public:
    Terrain(float size = 10.0f, int divisions = 20, glm::vec3 offset = glm::vec3(0.0f));
    ~Terrain();
    void draw(unsigned int shaderProgram);
    float getHeight(float x, float z) const;                                                         // Get terrain height at position (x, z)
    glm::vec3 getNormal(float x, float z) const;                                                     // Get terrain normal at position (x, z) for slope calculation
    bool checkTreeCollision(float x, float z, float radius = 0.5f) const;                            // Check if position collides with any tree
    bool checkWallCollision(float x, float z, float radius) const;                                   // Check collision with rock walls
    glm::vec3 resolveWallCollision(float x, float z, float radius, const glm::vec3 &velocity) const; // Resolve wall collision and return adjusted position

private:
    unsigned int VAO, VBO;
    unsigned int vertexCount;
    unsigned int terrainTexture;
    float terrainSize;
    glm::vec3 terrainOffset; // Offset to shift terrain from center

    // Trees
    std::vector<TreeInstance> trees;
    std::vector<Model *> treeModels; // Store models for cleanup

    // Rock Walls
    std::vector<RockWallInstance> rockWalls;
    std::vector<Model *> rockWallModels; // Store models for cleanup

    void loadTerrainTexture();
    void loadTrees();
    void placeTrees(int numTrees = 100);
    void loadRockWalls();
    void placeRockWalls();
};
