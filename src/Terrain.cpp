#include "Terrain.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include "../third_party/stb_image.h"
#include "PathUtils.h"

Terrain::Terrain(float size, int divisions, glm::vec3 offset)
{
    terrainSize = size;
    terrainOffset = offset;
    VAO = 0;
    VBO = 0;
    terrainTexture = 0;
    vertexCount = 0;

    // Create a flat terrain plane with proper UV coordinates for texture tiling
    std::vector<float> vertices;
    float halfSize = size / 2.0f;
    float step = size / divisions;

    // Create a grid of quads for the terrain
    for (int i = 0; i < divisions; ++i)
    {
        for (int j = 0; j < divisions; ++j)
        {
            // Apply offset to shift terrain from center
            float x0 = -halfSize + i * step + offset.x;
            float x1 = x0 + step;
            float z0 = -halfSize + j * step + offset.z;
            float z1 = z0 + step;

            // Flat terrain - all Y values are 0
            float y = 0.0f;

            // Calculate UV coordinates for texture tiling
            float uvScale = size / 10.0f; 
            float u0 = (x0 + halfSize) / uvScale;
            float u1 = (x1 + halfSize) / uvScale;
            float v0 = (z0 + halfSize) / uvScale;
            float v1 = (z1 + halfSize) / uvScale;

            // First triangle
            // Vertex 1
            vertices.insert(vertices.end(), {x0, y, z0, 0.0f, 1.0f, 0.0f, u0, v0});
            // Vertex 2
            vertices.insert(vertices.end(), {x1, y, z0, 0.0f, 1.0f, 0.0f, u1, v0});
            // Vertex 3
            vertices.insert(vertices.end(), {x1, y, z1, 0.0f, 1.0f, 0.0f, u1, v1});

            // Second triangle
            // Vertex 1
            vertices.insert(vertices.end(), {x0, y, z0, 0.0f, 1.0f, 0.0f, u0, v0});
            // Vertex 2
            vertices.insert(vertices.end(), {x1, y, z1, 0.0f, 1.0f, 0.0f, u1, v1});
            // Vertex 3
            vertices.insert(vertices.end(), {x0, y, z1, 0.0f, 1.0f, 0.0f, u0, v1});
        }
    }

    vertexCount = vertices.size() / 8; 

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    // Normal attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // Texture coordinate attribute
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    // Load texture from the Poly Haven texture folder
    loadTerrainTexture();

    // Load and place trees
    loadTrees();
    placeTrees(30); // Place trees in structured lines

    // Load and place rock walls
    loadRockWalls();
    placeRockWalls();

    std::cout << "Flat terrain created with " << divisions << "x" << divisions << " divisions" << std::endl;
}

Terrain::~Terrain()
{
    if (VAO != 0)
    {
        glDeleteVertexArrays(1, &VAO);
        VAO = 0;
    }
    if (VBO != 0)
    {
        glDeleteBuffers(1, &VBO);
        VBO = 0;
    }
    if (terrainTexture != 0)
    {
        glDeleteTextures(1, &terrainTexture);
        terrainTexture = 0;
    }

    // Clean up tree models
    for (auto *model : treeModels)
    {
        delete model;
    }
    treeModels.clear();
    trees.clear();

    // Clean up rock wall models
    for (auto *model : rockWallModels)
    {
        delete model;
    }
    rockWallModels.clear();
    rockWalls.clear();
}

void Terrain::loadTerrainTexture()
{
    // Load the diffuse texture from Poly Haven texture folder
    std::string texturePath = FindImagePath("Terrain/brown_mud_leaves_01_1k/textures/brown_mud_leaves_01_diff_1k.png");

    glGenTextures(1, &terrainTexture);
    glBindTexture(GL_TEXTURE_2D, terrainTexture);

    // Set texture wrapping and filtering
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Load image using stb_image
    int width, height, nrComponents;
    unsigned char *data = stbi_load(texturePath.c_str(), &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum format;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;

        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        std::cout << "Terrain texture loaded: " << texturePath << " (" << width << "x" << height << ")" << std::endl;
        stbi_image_free(data);
    }
    else
    {
        std::cerr << "Failed to load terrain texture: " << texturePath << std::endl;
        glDeleteTextures(1, &terrainTexture);
        terrainTexture = 0;
    }

    glBindTexture(GL_TEXTURE_2D, 0);
}

void Terrain::draw(unsigned int shaderProgram)
{
    // Set model matrix with offset (terrain can be shifted from origin)
    unsigned int modelLoc = glGetUniformLocation(shaderProgram, "model");
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, terrainOffset);
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

    // Bind and use terrain texture
    if (terrainTexture != 0)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, terrainTexture);
        glUniform1i(glGetUniformLocation(shaderProgram, "texture_diffuse1"), 0);
        glUniform1i(glGetUniformLocation(shaderProgram, "useTexture"), 1);
    }
    else
    {
        glUniform1i(glGetUniformLocation(shaderProgram, "useTexture"), 0);
    }

    // Draw the terrain plane
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, vertexCount);
    glBindVertexArray(0);

    // Draw trees
    // Set brown color for bark
    unsigned int colorLoc = glGetUniformLocation(shaderProgram, "objectColor");
    glUniform3f(colorLoc, 0.4f, 0.25f, 0.15f); // Brown color for bark/wood

    for (const auto &tree : trees)
    {
        if (tree.model)
        {
            // Calculate model matrix for tree
            glm::mat4 modelMatrix = glm::mat4(1.0f);

            // Translate to tree position
            float treeY = getHeight(tree.position.x, tree.position.z);
            modelMatrix = glm::translate(modelMatrix, glm::vec3(tree.position.x, treeY, tree.position.z));

            // Rotate around Y axis
            modelMatrix = glm::rotate(modelMatrix, tree.rotation, glm::vec3(0.0f, 1.0f, 0.0f));

            // Scale the tree
            modelMatrix = glm::scale(modelMatrix, glm::vec3(tree.scale));

            // Set model matrix uniform
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(modelMatrix));

            // Draw the tree model
            tree.model->Draw(shaderProgram);
        }
    }

    // Draw rock walls
    glUniform3f(colorLoc, 0.5f, 0.5f, 0.5f); 

    for (const auto &wall : rockWalls)
    {
        if (wall.model)
        {
            // Calculate model matrix for rock wall
            glm::mat4 modelMatrix = glm::mat4(1.0f);

            // Translate to wall position
            float wallY = getHeight(wall.position.x, wall.position.z);
            modelMatrix = glm::translate(modelMatrix, glm::vec3(wall.position.x, wallY, wall.position.z));

            // Rotate around Y axis
            modelMatrix = glm::rotate(modelMatrix, wall.rotation, glm::vec3(0.0f, 1.0f, 0.0f));

            // Scale the wall
            modelMatrix = glm::scale(modelMatrix, glm::vec3(wall.scale));

            // Set model matrix uniform
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(modelMatrix));

            // Draw the rock wall model
            wall.model->Draw(shaderProgram);
        }
    }
}

float Terrain::getHeight(float x, float z) const
{
    // Flat terrain - always return 0 (adjusted for offset)
    return 0.0f + terrainOffset.y;
}

glm::vec3 Terrain::getNormal(float x, float z) const
{
    // Flat terrain - normal always points straight up
    return glm::vec3(0.0f, 1.0f, 0.0f);
}

bool Terrain::checkTreeCollision(float x, float z, float radius) const
{
    // Check if position (x, z) with given radius collides with any tree
    for (const auto &tree : trees)
    {
        if (tree.model)
        {
            // Calculate distance from position to tree center
            float dx = x - tree.position.x;
            float dz = z - tree.position.z;
            float distance = sqrtf(dx * dx + dz * dz);

            // Estimate tree collision radius based on scale
            float treeRadius = 0.8f * tree.scale; // Adjust this value to make collision tighter/looser

            // Check if circles overlap
            if (distance < (radius + treeRadius))
            {
                return true; // Collision detected
            }
        }
    }
    return false; // No collision
}

// Check collision with rock walls using rectangular bounding boxes
bool Terrain::checkWallCollision(float x, float z, float radius) const
{
    for (const auto &wall : rockWalls)
    {
        if (!wall.model)
            continue;

        // Get wall model size and scale
        glm::vec3 wallSize = wall.model->getSize();
        float scaledWidth = wallSize.x * wall.scale;
        float scaledHeight = wallSize.y * wall.scale;
        float scaledDepth = wallSize.z * wall.scale;

        // Build wall transformation matrix
        glm::mat4 wallTransform = glm::mat4(1.0f);
        wallTransform = glm::translate(wallTransform, wall.position);
        wallTransform = glm::rotate(wallTransform, wall.rotation, glm::vec3(0.0f, 1.0f, 0.0f));
        wallTransform = glm::scale(wallTransform, glm::vec3(wall.scale));

        // Get wall center in world space
        glm::vec3 wallCenter = glm::vec3(wallTransform * glm::vec4(wall.model->getCenter(), 1.0f));

        // Transform the collision point to wall's local space for easier checking
        glm::mat4 wallInverse = glm::inverse(wallTransform);
        glm::vec3 localPoint = glm::vec3(wallInverse * glm::vec4(x, wallCenter.y, z, 1.0f));

        // Get wall model center
        glm::vec3 modelCenter = wall.model->getCenter();

        // Calculate half extents in local space
        float halfWidth = scaledWidth / (2.0f * wall.scale);
        float halfDepth = scaledDepth / (2.0f * wall.scale);

        // Check collision using AABB (Axis-Aligned Bounding Box) in local space
        // Add radius buffer around the wall (reduced for tighter collision)
        float radiusBuffer = radius / wall.scale * 0.6f; // Reduced by 40% for tighter collision

        float minX = modelCenter.x - halfWidth - radiusBuffer;
        float maxX = modelCenter.x + halfWidth + radiusBuffer;
        float minZ = modelCenter.z - halfDepth - radiusBuffer;
        float maxZ = modelCenter.z + halfDepth + radiusBuffer;

        // Check if point is inside the expanded bounding box
        if (localPoint.x >= minX && localPoint.x <= maxX &&
            localPoint.z >= minZ && localPoint.z <= maxZ)
        {
            return true; // Collision detected
        }
    }

    return false; // No collision
}

// Resolve wall collision and return adjusted position
glm::vec3 Terrain::resolveWallCollision(float x, float z, float radius, const glm::vec3 &velocity) const
{
    glm::vec3 adjustedPos(x, 0.0f, z);

    for (const auto &wall : rockWalls)
    {
        if (!wall.model)
            continue;

        // Get wall model size and scale
        glm::vec3 wallSize = wall.model->getSize();
        float scaledWidth = wallSize.x * wall.scale;
        float scaledHeight = wallSize.y * wall.scale;
        float scaledDepth = wallSize.z * wall.scale;

        // Build wall transformation matrix
        glm::mat4 wallTransform = glm::mat4(1.0f);
        wallTransform = glm::translate(wallTransform, wall.position);
        wallTransform = glm::rotate(wallTransform, wall.rotation, glm::vec3(0.0f, 1.0f, 0.0f));
        wallTransform = glm::scale(wallTransform, glm::vec3(wall.scale));

        // Get wall center in world space
        glm::vec3 wallCenter = glm::vec3(wallTransform * glm::vec4(wall.model->getCenter(), 1.0f));

        // Transform the collision point to wall's local space
        glm::mat4 wallInverse = glm::inverse(wallTransform);
        glm::vec3 localPoint = glm::vec3(wallInverse * glm::vec4(x, wallCenter.y, z, 1.0f));

        // Get wall model center
        glm::vec3 modelCenter = wall.model->getCenter();

        // Calculate half extents in local space
        float halfWidth = scaledWidth / (2.0f * wall.scale);
        float halfDepth = scaledDepth / (2.0f * wall.scale);

        // Add radius buffer (reduced for tighter collision)
        float radiusBuffer = radius / wall.scale * 0.6f; // Reduced by 40% for tighter collision

        float minX = modelCenter.x - halfWidth - radiusBuffer;
        float maxX = modelCenter.x + halfWidth + radiusBuffer;
        float minZ = modelCenter.z - halfDepth - radiusBuffer;
        float maxZ = modelCenter.z + halfDepth + radiusBuffer;

        // Check if point is inside the expanded bounding box
        if (localPoint.x >= minX && localPoint.x <= maxX &&
            localPoint.z >= minZ && localPoint.z <= maxZ)
        {
            // Collision detected - push out to nearest edge

            // Calculate distances to each edge
            float distToMinX = localPoint.x - minX;
            float distToMaxX = maxX - localPoint.x;
            float distToMinZ = localPoint.z - minZ;
            float distToMaxZ = maxZ - localPoint.z;

            // Find the smallest distance (nearest edge)
            float minDist = distToMinX;
            int nearestEdge = 0; // 0=minX, 1=maxX, 2=minZ, 3=maxZ

            if (distToMaxX < minDist)
            {
                minDist = distToMaxX;
                nearestEdge = 1;
            }
            if (distToMinZ < minDist)
            {
                minDist = distToMinZ;
                nearestEdge = 2;
            }
            if (distToMaxZ < minDist)
            {
                minDist = distToMaxZ;
                nearestEdge = 3;
            }

            // Push out from nearest edge in local space
            glm::vec3 correctedLocalPoint = localPoint;
            switch (nearestEdge)
            {
            case 0: // Push left
                correctedLocalPoint.x = minX - 0.01f;
                break;
            case 1: // Push right
                correctedLocalPoint.x = maxX + 0.01f;
                break;
            case 2: // Push back
                correctedLocalPoint.z = minZ - 0.01f;
                break;
            case 3: // Push forward
                correctedLocalPoint.z = maxZ + 0.01f;
                break;
            }

            // Transform back to world space
            glm::vec3 correctedWorldPoint = glm::vec3(wallTransform * glm::vec4(correctedLocalPoint, 1.0f));
            adjustedPos.x = correctedWorldPoint.x;
            adjustedPos.z = correctedWorldPoint.z;
        }
    }

    return adjustedPos;
}

void Terrain::loadTrees()
{
    // Load Tree model
    std::string treePath = FindImagePath("Terrain/Tree/Tree1.obj");

    try
    {
        Model* treeModel = new Model(treePath);
        treeModels.push_back(treeModel);
        std::cout << "Loaded 1 tree model: Tree4.obj" << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Failed to load Tree4.obj: " << e.what() << std::endl;
    }

    if (treeModels.empty())
    {
        std::cerr << "Warning: Tree4.obj failed to load!" << std::endl;
    }
}

void Terrain::placeTrees(int numTrees)
{
    if (treeModels.empty())
    {
        std::cerr << "Cannot place trees: No tree models loaded!" << std::endl;
        return;
    }

    trees.clear();

    // ===== TREE LINE CONFIGURATION =====
    // Configure how trees are placed in structured lines

    // Global settings
    float defaultTreeScale = 0.03f; // Base scale for all trees

    // Line configuration structure
    struct TreeLineConfig
    {
        bool enabled;                 // Enable/disable this line
        float startX;                 // Starting X position
        float startZ;                 // Starting Z position
        float direction;              // 0 = along X axis, PI/2 = along Z axis (in radians)
        float spacing;                // Distance between individual trees
        float groupSpacing;           // Extra space between different tree groups
        std::vector<int> treePattern; // Which tree models to use (0-5) and how many of each
    };

    // Define 4 lines of trees
    std::vector<TreeLineConfig> treeLines = {
        {
            true,     
            23.0f,    
            -13.0f,   
            1.5708f,  
            3.0f,     
            5.0f,     
            {0, 0, 0, 
             0, 0, 0,
             0, 0, 0}
        },

       
        {
            true,     
            8.0f,     
            -13.0f,   
            1.5708f,  
            3.0f,     
            5.0f,     
            {0, 0, 0,
             0, 0, 0,
             0, 0, 0}
        },

        {
            true,     
            -5.0f,    
            -13.0f,   
            1.5708f,  
            3.0f,     
            5.0f,     
            {0, 0, 0,
             0, 0, 0,
             0, 0, 0}
        },

        {
            true,     
            -25.0f,   
            -13.0f,   
            1.5708f,  
            3.0f,     
            5.0f,     
            {0, 0, 0,
             0, 0, 0,
             0, 0, 0}
        }};
    // ===== END OF TREE LINE CONFIGURATION =====

    // Place trees according to line configurations
    for (const auto &line : treeLines)
    {
        if (!line.enabled)
            continue;

        float currentX = line.startX;
        float currentZ = line.startZ;

        // Calculate direction vector
        float dirX = cosf(line.direction);
        float dirZ = sinf(line.direction);

        int previousTreeType = -1;

        for (size_t i = 0; i < line.treePattern.size(); i++)
        {
            int treeIndex = line.treePattern[i];

            // Add extra spacing when changing tree types
            if (previousTreeType != -1 && previousTreeType != treeIndex)
            {
                currentX += dirX * line.groupSpacing;
                currentZ += dirZ * line.groupSpacing;
            }
            previousTreeType = treeIndex;

            // Clamp tree index to valid range (0-5)
            if (treeIndex < 0 || treeIndex >= (int)treeModels.size())
                treeIndex = 0;

            // Get terrain height at this position
            float y = getHeight(currentX, currentZ);

            // Create tree instance
            TreeInstance tree;
            tree.model = treeModels[treeIndex];
            tree.position = glm::vec3(currentX, y, currentZ);
            tree.rotation = 0.0f; // No random rotation for structured lines
            tree.scale = defaultTreeScale;
            trees.push_back(tree);

            // Move to next position
            currentX += dirX * line.spacing;
            currentZ += dirZ * line.spacing;
        }
    }

    std::cout << "Placed " << trees.size() << " trees in structured lines" << std::endl;
}

void Terrain::loadRockWalls()
{
    // Load the rock wall model
    std::string wallPath = FindImagePath("RockWall/stonewallL.exported.obj");

    try
    {
        Model *wallModel = new Model(wallPath);
        rockWallModels.push_back(wallModel);
        std::cout << "Loaded rock wall model: " << wallPath << std::endl;

        // Print rock wall dimensions
        glm::vec3 wallSize = wallModel->getSize();
        glm::vec3 wallCenter = wallModel->getCenter();
        std::cout << "  Rock Wall Size (Width x Height x Depth): "
                  << wallSize.x << " x " << wallSize.y << " x " << wallSize.z << " units" << std::endl;
        std::cout << "  Rock Wall Center: ("
                  << wallCenter.x << ", " << wallCenter.y << ", " << wallCenter.z << ")" << std::endl;
        std::cout << "  Rock Wall Bounds: X["
                  << (wallCenter.x - wallSize.x / 2.0f) << " to " << (wallCenter.x + wallSize.x / 2.0f) << "], "
                  << "Y[" << (wallCenter.y - wallSize.y / 2.0f) << " to " << (wallCenter.y + wallSize.y / 2.0f) << "], "
                  << "Z[" << (wallCenter.z - wallSize.z / 2.0f) << " to " << (wallCenter.z + wallSize.z / 2.0f) << "]" << std::endl;

        float wallScale = 0.0f; 
        glm::vec3 scaledSize = wallSize * wallScale;
        std::cout << "  Scaled Size (with " << wallScale << "x scale): "
                  << scaledSize.x << " x " << scaledSize.y << " x " << scaledSize.z << " units" << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Failed to load rock wall model: " << wallPath << " - " << e.what() << std::endl;
    }

    if (rockWallModels.empty())
    {
        std::cerr << "Warning: No rock wall models loaded!" << std::endl;
    }
    else
    {
        std::cout << "Loaded " << rockWallModels.size() << " rock wall model(s)" << std::endl;
    }
}

void Terrain::placeRockWalls()
{
    if (rockWallModels.empty())
    {
        std::cerr << "Cannot place rock walls: No rock wall models loaded!" << std::endl;
        return;
    }

    float halfSize = terrainSize / 2.0f;
    float wallOffset = 5.0f;   
    float wallSpacing = 28.0f; 
    float wallScale = 1.5f;    
    float wallHeight = 0.0f;   

    // North edge
    float northEdgeXOffset = 0.0f;  
    float northEdgeZOffset = -3.0f; 

    // South edge
    float southEdgeXOffset = -10.0f; 
    float southEdgeZOffset = 3.0f;   
    // East edge
    float eastEdgeXOffset = -8.0f; 
    float eastEdgeZOffset = 25.0f; 

    // West edge
    float westEdgeXOffset = 5.0f;  
    float westEdgeZOffset = -10.0f; 
    glm::vec3 wallSize = rockWallModels[0]->getSize();
    float wallWidth = wallSize.z * wallScale; 
    float wallWidthHalf = wallWidth / 2.0f;   

    // Middle walls
    bool enableMiddleWalls = true; 
    float middleWallSpacing = 50.0f; 

    // Middle wall segments
    struct MiddleWallConfig
    {
        bool enabled;   
        float x;        
        float z;        
        float rotation; 
        float scale;    
    };

    // Configure middle wall segments 
    // Use middleWallSpacing variable to space them evenly
    std::vector<MiddleWallConfig> middleWalls = {
        {true, 0.0f, 20.0f, 0.0f, 1.5f},
        {true, 0.0f, 0.0f, 0.0f, 1.5f},
    };

    rockWalls.clear();

    for (float x = -halfSize - wallOffset; x < halfSize + wallOffset; x += wallSpacing)
    {
        RockWallInstance wall;
        wall.model = rockWallModels[0];
        // Apply adjustable offsets for north edge
        wall.position = glm::vec3(x + wallWidthHalf + northEdgeXOffset,
                                  wallHeight,
                                  halfSize + wallOffset + northEdgeZOffset);
        wall.rotation = 3.14159f / 2.0f; // Rotated 90 degrees to face east/west (along X axis)
        wall.scale = wallScale;
        rockWalls.push_back(wall);
    }

    // South edge (negative Z) - walls aligned to the LEFT (negative X direction)
    for (float x = -halfSize - wallOffset; x < halfSize + wallOffset; x += wallSpacing)
    {
        RockWallInstance wall;
        wall.model = rockWallModels[0];
        // Apply adjustable offsets for south edge
        wall.position = glm::vec3(x + wallWidthHalf + southEdgeXOffset,
                                  wallHeight,
                                  -halfSize - wallOffset + southEdgeZOffset);
        wall.rotation = -3.14159f / 2.0f; // Rotated -90 degrees to face east/west (along X axis)
        wall.scale = wallScale;
        rockWalls.push_back(wall);
    }

    // East edge
    for (float z = -halfSize - wallOffset; z < halfSize + wallOffset; z += wallSpacing)
    {
        RockWallInstance wall;
        wall.model = rockWallModels[0];
        // Apply adjustable offsets for east edge
        wall.position = glm::vec3(halfSize + wallOffset + eastEdgeXOffset,
                                  wallHeight,
                                  z - wallWidthHalf + eastEdgeZOffset);
        wall.rotation = 0.0f; // Rotated to face north/south (along Z axis, but wall is horizontal)
        wall.scale = wallScale;
        rockWalls.push_back(wall);
    }

    // West edge
    for (float z = -halfSize - wallOffset; z < halfSize + wallOffset; z += wallSpacing)
    {
        RockWallInstance wall;
        wall.model = rockWallModels[0];
        // Apply adjustable offsets for west edge
        wall.position = glm::vec3(-halfSize - wallOffset + westEdgeXOffset,
                                  wallHeight,
                                  z + wallWidthHalf + westEdgeZOffset);
        wall.rotation = 3.14159f; // Rotated 180 degrees to face north/south (along Z axis, but wall is horizontal)
        wall.scale = wallScale;
        rockWalls.push_back(wall);
    }

    // Place middle walls (if enabled) - form a combined big wall
    if (enableMiddleWalls && !rockWallModels.empty())
    {
        int middleWallCount = 0;
        for (const auto &config : middleWalls)
        {
            if (config.enabled)
            {
                RockWallInstance middleWall;
                middleWall.model = rockWallModels[0];
                // Position with adjustable offsets
                middleWall.position = glm::vec3(config.x, wallHeight, config.z);
                middleWall.rotation = config.rotation; // Adjustable rotation
                middleWall.scale = config.scale;
                rockWalls.push_back(middleWall);
                middleWallCount++;
            }
        }
        std::cout << "Placed " << middleWallCount << " middle wall segment(s) to form combined big wall" << std::endl;
    }

    std::cout << "Placed " << rockWalls.size() << " rock wall segments around terrain edges (aligned to one side)" << std::endl;
}
