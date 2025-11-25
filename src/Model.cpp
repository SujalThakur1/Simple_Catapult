#include "Model.h"
#include <iostream>
#include <map>
#include <algorithm>
#include <limits>
#include <cstring>
#include <cstddef>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include "../third_party/stb_image.h"
#include "PathUtils.h"

// Static member initialization for shared texture cache
std::vector<Texture> Model::textures_loaded;

// Mesh implementation
Mesh::Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices, std::vector<Texture> textures)
    : vertices(vertices), indices(indices), textures(textures)
{
    setupMesh();
}

void Mesh::setupMesh()
{
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

    // Vertex positions
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)0);
    // Vertex normals
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, Normal));
    // Vertex texture coords
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, TexCoords));
    // Bone IDs
    glEnableVertexAttribArray(3);
    glVertexAttribIPointer(3, 4, GL_INT, sizeof(Vertex), (void *)offsetof(Vertex, BoneIDs));
    // Bone weights
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, Weights));

    glBindVertexArray(0);
}

void Mesh::Draw(unsigned int shaderProgram)
{
    // Only load diffuse textures (shader only uses texture_diffuse1)
    bool hasTextures = false;
    for (unsigned int i = 0; i < textures.size(); i++)
    {
        if (textures[i].type == "texture_diffuse")
        {
            glActiveTexture(GL_TEXTURE0);
            glUniform1i(glGetUniformLocation(shaderProgram, "texture_diffuse1"), 0);
            glBindTexture(GL_TEXTURE_2D, textures[i].id);
            hasTextures = true;
            break; // Only use first diffuse texture
        }
    }

    // Set useTexture uniform
    glUniform1i(glGetUniformLocation(shaderProgram, "useTexture"), hasTextures ? 1 : 0);

    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    // Reset texture binding after drawing
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

// Model implementation
Model::Model(const std::string &path)
    : importer(new Assimp::Importer()), scene(nullptr),
      numBones(0), globalInverseTransform(glm::mat4(1.0f)),
      animationScene(nullptr), animationImporter(nullptr),
      animationTime(0.0f), hasAnimation(false)
{
    loadModel(path);
}

Model::~Model()
{
    delete importer;
    if (animationImporter)
        delete animationImporter;
}

void Model::Draw(unsigned int shaderProgram)
{
    // Set bone matrices if animation is active
    if (hasAnimation && animationScene && animationScene->HasAnimations())
    {
        glUniform1i(glGetUniformLocation(shaderProgram, "useAnimation"), 1);

        glm::mat4 boneMatrices[100];
        for (unsigned int i = 0; i < boneInfo.size() && i < 100; i++)
        {
            boneMatrices[i] = boneInfo[i].finalTransformation;
        }
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "gBones"), boneInfo.size(), GL_FALSE, glm::value_ptr(boneMatrices[0]));
    }
    else
    {
        glUniform1i(glGetUniformLocation(shaderProgram, "useAnimation"), 0);
    }

    for (unsigned int i = 0; i < meshes.size(); i++)
        meshes[i].Draw(shaderProgram);
}

void Model::loadModel(const std::string &path)
{
    scene = importer->ReadFile(path.c_str(), aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace | aiProcess_GenNormals | aiProcess_LimitBoneWeights);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    {
        std::cerr << "ERROR::ASSIMP:: " << importer->GetErrorString() << std::endl;
        return;
    }
    directory = path.substr(0, path.find_last_of('/'));

    globalInverseTransform = aiMatrix4x4ToGlm(scene->mRootNode->mTransformation);
    globalInverseTransform = glm::inverse(globalInverseTransform);

    calculateBounds(scene);
    processNode(scene->mRootNode, scene);
}

void Model::processNode(aiNode *node, const aiScene *scene)
{
    for (unsigned int i = 0; i < node->mNumMeshes; i++)
    {
        aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];
        meshes.push_back(processMesh(mesh, scene));
    }

    for (unsigned int i = 0; i < node->mNumChildren; i++)
    {
        processNode(node->mChildren[i], scene);
    }
}

Mesh Model::processMesh(aiMesh *mesh, const aiScene *scene)
{
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    std::vector<Texture> textures;

    // Calculate mesh bounds to help determine if it's bark (trunk) or leaves
    float minY = std::numeric_limits<float>::max();
    float maxY = std::numeric_limits<float>::lowest();
    float avgY = 0.0f;
    float meshHeight = 0.0f;
    float meshWidth = 0.0f;

    // Process vertices
    for (unsigned int i = 0; i < mesh->mNumVertices; i++)
    {
        Vertex vertex;
        glm::vec3 vector;
        vector.x = mesh->mVertices[i].x;
        vector.y = mesh->mVertices[i].y;
        vector.z = mesh->mVertices[i].z;
        vertex.Position = vector;

        // Track Y bounds for mesh analysis
        if (vector.y < minY)
            minY = vector.y;
        if (vector.y > maxY)
            maxY = vector.y;
        avgY += vector.y;

        if (mesh->HasNormals())
        {
            vector.x = mesh->mNormals[i].x;
            vector.y = mesh->mNormals[i].y;
            vector.z = mesh->mNormals[i].z;
            vertex.Normal = vector;
        }

        if (mesh->mTextureCoords[0])
        {
            glm::vec2 vec;
            vec.x = mesh->mTextureCoords[0][i].x;
            vec.y = mesh->mTextureCoords[0][i].y;
            vertex.TexCoords = vec;
        }
        else
            vertex.TexCoords = glm::vec2(0.0f, 0.0f);

        vertices.push_back(vertex);
    }

    // Calculate mesh properties for texture selection
    if (mesh->mNumVertices > 0)
    {
        avgY /= mesh->mNumVertices;
        meshHeight = maxY - minY;
        // Estimate width from vertex spread
        float minX = std::numeric_limits<float>::max(), maxX = std::numeric_limits<float>::lowest();
        float minZ = std::numeric_limits<float>::max(), maxZ = std::numeric_limits<float>::lowest();
        for (unsigned int i = 0; i < mesh->mNumVertices; i++)
        {
            if (mesh->mVertices[i].x < minX)
                minX = mesh->mVertices[i].x;
            if (mesh->mVertices[i].x > maxX)
                maxX = mesh->mVertices[i].x;
            if (mesh->mVertices[i].z < minZ)
                minZ = mesh->mVertices[i].z;
            if (mesh->mVertices[i].z > maxZ)
                maxZ = mesh->mVertices[i].z;
        }
        meshWidth = std::max(maxX - minX, maxZ - minZ);
    }

    // Load bone data
    std::vector<unsigned int> boneIDs;
    std::vector<float> boneWeights;
    loadBones(mesh, boneIDs, boneWeights);

    // Assign bone data to vertices
    for (unsigned int i = 0; i < vertices.size(); i++)
    {
        if (i < boneIDs.size() / 4)
        {
            vertices[i].BoneIDs[0] = boneIDs[i * 4 + 0];
            vertices[i].BoneIDs[1] = boneIDs[i * 4 + 1];
            vertices[i].BoneIDs[2] = boneIDs[i * 4 + 2];
            vertices[i].BoneIDs[3] = boneIDs[i * 4 + 3];
            vertices[i].Weights[0] = boneWeights[i * 4 + 0];
            vertices[i].Weights[1] = boneWeights[i * 4 + 1];
            vertices[i].Weights[2] = boneWeights[i * 4 + 2];
            vertices[i].Weights[3] = boneWeights[i * 4 + 3];
        }
    }

    // Process indices
    for (unsigned int i = 0; i < mesh->mNumFaces; i++)
    {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++)
            indices.push_back(face.mIndices[j]);
    }

    // Process materials
    if (mesh->mMaterialIndex >= 0)
    {
        aiMaterial *material = scene->mMaterials[mesh->mMaterialIndex];

        // Map material names to texture folders
        std::string matName = material->GetName().C_Str();
        std::transform(matName.begin(), matName.end(), matName.begin(), ::tolower);

        // Get material index for tree models with generic material names
        unsigned int materialIndex = mesh->mMaterialIndex;
        bool isTreeModel = directory.find("Tree") != std::string::npos;

        std::vector<Texture> diffuseMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse");

        // ALWAYS try to load from default textures folder for FBX models (but skip for tree models)
        // Also check for RockWall models and load from brown folder
        if (diffuseMaps.empty())
        {
            bool isTreeModel = directory.find("Tree") != std::string::npos;
            bool isRockWallModel = directory.find("RockWall") != std::string::npos;

            if (isRockWallModel)
            {
                // Load rock wall textures from brown folder
                std::string textureFolder = directory + "/brown/";
                Texture texture;

                // Load diffuse texture (Base Color)
                std::string textureFile = "stonewall_Base_Color.png";
                texture.id = TextureFromFile(textureFile.c_str(), textureFolder);
                if (texture.id != 0)
                {
                    texture.type = "texture_diffuse";
                    texture.path = textureFolder + textureFile;
                    diffuseMaps.push_back(texture);
                }
            }
            else if (!isTreeModel)
            {
                std::string textureFolder = directory + "/textures default/";
                Texture texture;
                std::vector<std::string> textureFiles = {
                    "DefaultMaterial_Base_Color2.png",
                    "DefaultMaterial_Mixed_AO.png"};
                for (const auto &texFile : textureFiles)
                {
                    texture.id = TextureFromFile(texFile.c_str(), textureFolder);
                    if (texture.id != 0)
                    {
                        texture.type = "texture_diffuse";
                        texture.path = textureFolder + texFile;
                        diffuseMaps.push_back(texture);
                        break;
                    }
                }
            }
        }

        // If still no textures, try to load from folders based on material name
        if (diffuseMaps.empty())
        {
            std::string textureFolder = directory + "/";
            std::string textureFile = "";

            // Tree materials - textures are in the textures folder
            if (matName.find("flower") != std::string::npos)
            {
                // Use flowers color texture from textures folder
                textureFolder = directory + "/textures/";
                textureFile = "gleditsia triacanthos flowers color.jpg";
            }
            else if (matName.find("leaf") != std::string::npos)
            {
                // Use leaf color texture from textures folder
                textureFolder = directory + "/textures/";
                textureFile = "gleditsia triacanthos leaf color a1.jpg";
            }
            else if (matName.find("stem") != std::string::npos)
            {
                // Use stem texture from textures folder
                textureFolder = directory + "/textures/";
                textureFile = "gleditsia triacanthos stem.jpg";
            }
            else if (matName.find("bean") != std::string::npos)
            {
                // Use beans color texture from textures folder
                textureFolder = directory + "/textures/";
                textureFile = "gleditsia triacanthos beans color.jpg";
            }
            else if (matName.find("bark") != std::string::npos)
            {
                // Use bark color texture from textures folder
                textureFolder = directory + "/textures/";
                textureFile = "gleditsia triacanthos bark a1.jpg";
            }
            else
            {
                // For tree models with unknown materials, use material index to assign textures
                if (isTreeModel)
                {
                    textureFolder = directory + "/textures/";
                    // Use material index to cycle through different texture types
                    // This ensures different materials get different textures
                    unsigned int textureType = materialIndex % 5; // Cycle through 5 types

                    if (textureType == 0)
                    {
                        // Material 0, 5, 10... -> Bark
                        textureFile = "gleditsia triacanthos bark a1.jpg";
                    }
                    else if (textureType == 1)
                    {
                        // Material 1, 6, 11... -> Leaf
                        textureFile = "gleditsia triacanthos leaf color a1.jpg";
                    }
                    else if (textureType == 2)
                    {
                        // Material 2, 7, 12... -> Leaf variant
                        textureFile = "gleditsia triacanthos leaf color a2.jpg";
                    }
                    else if (textureType == 3)
                    {
                        // Material 3, 8, 13... -> Beans
                        textureFile = "gleditsia triacanthos beans color.jpg";
                    }
                    else
                    {
                        // Material 4, 9, 14... -> Flowers
                        textureFile = "gleditsia triacanthos flowers color.jpg";
                    }
                }
                else
                {
                    // Default fallback: try default textures folder for any unknown material
                    textureFolder += "textures default/";
                    textureFile = "DefaultMaterial_Base_Color2.png";
                }
            }

            if (!textureFile.empty())
            {
                Texture texture;
                texture.id = TextureFromFile(textureFile.c_str(), textureFolder);
                if (texture.id != 0)
                {
                    texture.type = "texture_diffuse";
                    texture.path = textureFile;
                    diffuseMaps.push_back(texture);
                }
                else
                {
                    // Try alternative names based on material type
                    std::vector<std::string> alternatives;
                    bool isTreeModel = directory.find("Tree") != std::string::npos;

                    // Tree material fallbacks - use textures from textures folder
                    if (matName.find("flower") != std::string::npos)
                    {
                        alternatives = {
                            "gleditsia triacanthos flowers color.jpg"};
                    }
                    else if (matName.find("leaf") != std::string::npos)
                    {
                        alternatives = {
                            "gleditsia triacanthos leaf color a2.jpg",
                            "gleditsia triacanthos leaf color b1.jpg",
                            "gleditsia triacanthos leaf color b2.jpg",
                            "gleditsia triacanthos leaf color a1.jpg"};
                    }
                    else if (matName.find("stem") != std::string::npos)
                    {
                        alternatives = {
                            "gleditsia triacanthos stem.jpg"};
                    }
                    else if (matName.find("bean") != std::string::npos)
                    {
                        alternatives = {
                            "gleditsia triacanthos beans color.jpg"};
                    }
                    else if (matName.find("bark") != std::string::npos)
                    {
                        alternatives = {
                            "gleditsia triacanthos bark a2.jpg",
                            "gleditsia triacanthos bark2 a1.jpg",
                            "gleditsia triacanthos bark a1.jpg"};
                    }
                    else if (isTreeModel)
                    {
                        // For tree models with unknown materials, use material index to determine texture alternatives
                        unsigned int textureType = materialIndex % 5;

                        if (textureType == 0)
                        {
                            // Bark alternatives
                            alternatives = {
                                "gleditsia triacanthos bark a2.jpg",
                                "gleditsia triacanthos bark2 a1.jpg",
                                "gleditsia triacanthos bark a1.jpg"};
                        }
                        else if (textureType == 1 || textureType == 2)
                        {
                            // Leaf alternatives
                            alternatives = {
                                "gleditsia triacanthos leaf color a2.jpg",
                                "gleditsia triacanthos leaf color b1.jpg",
                                "gleditsia triacanthos leaf color b2.jpg",
                                "gleditsia triacanthos leaf color a1.jpg"};
                        }
                        else if (textureType == 3)
                        {
                            // Beans alternatives
                            alternatives = {
                                "gleditsia triacanthos beans color.jpg"};
                        }
                        else
                        {
                            // Flowers alternatives
                            alternatives = {
                                "gleditsia triacanthos flowers color.jpg"};
                        }
                    }
                    else
                    {
                        // Default fallbacks for other materials (zombie textures)
                        alternatives = {
                            "DefaultMaterial_Base_Color2.png",
                            "DefaultMaterial_Mixed_AO.png"};
                    }

                    // Update texture folder for tree models to use textures subfolder
                    std::string altTextureFolder = textureFolder;
                    if (isTreeModel && textureFolder.find("/textures/") == std::string::npos)
                    {
                        altTextureFolder = directory + "/textures/";
                    }

                    for (const auto &alt : alternatives)
                    {
                        texture.id = TextureFromFile(alt.c_str(), altTextureFolder);
                        if (texture.id != 0)
                        {
                            texture.type = "texture_diffuse";
                            texture.path = altTextureFolder + alt;
                            diffuseMaps.push_back(texture);
                            break;
                        }
                    }
                }
            }
        }

        // Final fallback: if still no textures, try default folder (but skip for tree models)
        if (diffuseMaps.empty())
        {
            // Check if this is a tree model (Tree directory)
            bool isTreeModel = directory.find("Tree") != std::string::npos;

            if (!isTreeModel)
            {
                std::string textureFolder = directory + "/textures default/";
                Texture texture;
                std::vector<std::string> fallbacks = {
                    "DefaultMaterial_Base_Color2.png",
                    "DefaultMaterial_Mixed_AO.png"};
                for (const auto &fallback : fallbacks)
                {
                    texture.id = TextureFromFile(fallback.c_str(), textureFolder);
                    if (texture.id != 0)
                    {
                        texture.type = "texture_diffuse";
                        texture.path = textureFolder + fallback;
                        diffuseMaps.push_back(texture);
                        break;
                    }
                }
            }
        }

        textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
    }
    else
    {
        // No material index - try loading default texture anyway (but skip for tree models)
        bool isTreeModel = directory.find("Tree") != std::string::npos;
        if (!isTreeModel)
        {
            std::string textureFolder = directory + "/textures default/";
            Texture texture;
            texture.id = TextureFromFile("DefaultMaterial_Base_Color2.png", textureFolder);
            if (texture.id != 0)
            {
                texture.type = "texture_diffuse";
                texture.path = textureFolder + "DefaultMaterial_Base_Color2.png"; // Store full path
                textures.push_back(texture);
            }
        }
    }

    return Mesh(vertices, indices, textures);
}

std::vector<Texture> Model::loadMaterialTextures(aiMaterial *mat, aiTextureType type, std::string typeName)
{
    std::vector<Texture> textures;
    bool isTreeModel = directory.find("Tree") != std::string::npos;

    for (unsigned int i = 0; i < mat->GetTextureCount(type); i++)
    {
        aiString str;
        mat->GetTexture(type, i, &str);
        bool skip = false;
        std::string texturePath = str.C_Str();

        // For tree models, extract filename from Windows paths and use textures folder
        std::string textureFile = texturePath;
        std::string textureDir = directory;

        if (isTreeModel)
        {
            // Extract just the filename from Windows path (C:\Users\...\filename.jpg -> filename.jpg)
            size_t lastSlash = texturePath.find_last_of("\\/");
            if (lastSlash != std::string::npos)
            {
                textureFile = texturePath.substr(lastSlash + 1);
            }

            // Map reflect/mask textures to color textures for tree models
            // MTL files sometimes reference reflect.jpg or mask.jpg files, but we only use color textures
            // This ensures we always load the correct diffuse color texture
            if (textureFile.find("bark reflect") != std::string::npos)
            {
                // Use bark color texture instead of reflect
                textureFile = "gleditsia triacanthos bark a1.jpg";
            }
            else if (textureFile.find("bark2") != std::string::npos)
            {
                // bark2 a1.jpg is already a color texture, use it as is
                if (textureFile.find("color") == std::string::npos &&
                    textureFile.find("bark2 a1") == std::string::npos)
                {
                    textureFile = "gleditsia triacanthos bark2 a1.jpg";
                }
            }
            else if (textureFile.find("leaf") != std::string::npos &&
                     textureFile.find("mask") != std::string::npos)
            {
                // If leaf mask, use leaf color instead
                if (textureFile.find("color b1") != std::string::npos)
                {
                    textureFile = "gleditsia triacanthos leaf color b1.jpg";
                }
                else
                {
                    textureFile = "gleditsia triacanthos leaf color a1.jpg";
                }
            }
            else if (textureFile.find("flowers") != std::string::npos &&
                     textureFile.find("mask") != std::string::npos)
            {
                // If flowers mask, use flowers color instead
                textureFile = "gleditsia triacanthos flowers color.jpg";
            }
            else if (textureFile.find("beans") != std::string::npos &&
                     textureFile.find("mask") != std::string::npos)
            {
                // If beans mask, use beans color instead
                textureFile = "gleditsia triacanthos beans color.jpg";
            }

            // Use textures subfolder for tree models
            textureDir = directory + "/textures/";
        }

        // Build full path for comparison
        std::string fullTexturePath = textureDir + textureFile;

        // Check if we've already loaded this texture (by full path)
        for (unsigned int j = 0; j < textures_loaded.size(); j++)
        {
            if (textures_loaded[j].path == fullTexturePath)
            {
                textures.push_back(textures_loaded[j]);
                skip = true;
                break;
            }
        }
        if (!skip)
        {
            Texture texture;
            texture.id = TextureFromFile(textureFile.c_str(), textureDir);
            if (texture.id != 0)
            {
                texture.type = typeName;
                texture.path = fullTexturePath;
                textures.push_back(texture);
                textures_loaded.push_back(texture);
            }
        }
    }
    return textures;
}

unsigned int Model::TextureFromFile(const char *path, const std::string &directory)
{
    std::string filename;
    if (directory.back() == '/')
        filename = directory + std::string(path);
    else
        filename = directory + '/' + std::string(path);

    // Check if we've already loaded this texture (texture caching to avoid duplicate loads)
    for (unsigned int i = 0; i < textures_loaded.size(); i++)
    {
        // Compare full paths to see if this texture was already loaded
        std::string loadedFullPath = textures_loaded[i].path;
        if (loadedFullPath == filename && textures_loaded[i].id != 0)
        {
            return textures_loaded[i].id; // Return existing texture ID (reuse cached texture)
        }
    }

    // Create new OpenGL texture object
    unsigned int textureID = 0;
    glGenTextures(1, &textureID);

    // Load image data from file using stb_image
    int width, height, nrComponents;
    unsigned char *data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 0);

    if (data)
    {
        // Determine OpenGL format based on number of color components
        GLenum format;
        if (nrComponents == 1)
            format = GL_RED; // Grayscale
        else if (nrComponents == 3)
            format = GL_RGB; // RGB
        else if (nrComponents == 4)
            format = GL_RGBA; // RGBA with alpha

        // Upload texture data to GPU
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D); // Generate mipmaps for better quality at different distances

        // Set texture parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);                   // Repeat texture horizontally
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);                   // Repeat texture vertically
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); // Minification filter
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);               // Magnification filter

        // Free image data from CPU memory (now on GPU)
        stbi_image_free(data);

        // Store texture info in cache for future reuse
        Texture tex;
        tex.id = textureID;
        tex.path = filename; // Store full path for cache lookup
        tex.type = "texture_diffuse";
        textures_loaded.push_back(tex);
    }
    else
    {
        std::cerr << "Failed to load texture: " << filename << std::endl;
        glDeleteTextures(1, &textureID);
        textureID = 0;
    }

    return textureID;
}

void Model::calculateBounds(const aiScene *scene)
{
    if (scene->mNumMeshes == 0)
    {
        modelSize = glm::vec3(1.0f);
        modelCenter = glm::vec3(0.0f);
        return;
    }

    float minX = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float minY = std::numeric_limits<float>::max();
    float maxY = std::numeric_limits<float>::lowest();
    float minZ = std::numeric_limits<float>::max();
    float maxZ = std::numeric_limits<float>::lowest();

    for (unsigned int i = 0; i < scene->mNumMeshes; i++)
    {
        aiMesh *mesh = scene->mMeshes[i];
        for (unsigned int j = 0; j < mesh->mNumVertices; j++)
        {
            minX = std::min(minX, mesh->mVertices[j].x);
            maxX = std::max(maxX, mesh->mVertices[j].x);
            minY = std::min(minY, mesh->mVertices[j].y);
            maxY = std::max(maxY, mesh->mVertices[j].y);
            minZ = std::min(minZ, mesh->mVertices[j].z);
            maxZ = std::max(maxZ, mesh->mVertices[j].z);
        }
    }

    modelSize = glm::vec3(maxX - minX, maxY - minY, maxZ - minZ);
    modelCenter = glm::vec3((minX + maxX) / 2.0f, (minY + maxY) / 2.0f, (minZ + maxZ) / 2.0f);
}

// Animation functions
void Model::LoadAnimation(const std::string& animationPath)
{
    std::string fixedPath = FindImagePath(animationPath);
    if (animationImporter) delete animationImporter;
    animationImporter = new Assimp::Importer();

    animationScene = animationImporter->ReadFile(fixedPath.c_str(),
        aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_LimitBoneWeights);

    if (!animationScene || !animationScene->HasAnimations())
    {
        hasAnimation = false;
        std::cerr << "FAILED to load animation: " << fixedPath << std::endl;
        return;
    }

    // THIS IS THE MISSING PART — REBUILD BONE INFO FROM ANIMATION FILE!
    boneInfo.clear();
    boneMapping.clear();
    numBones = 0;
    globalInverseTransform = aiMatrix4x4ToGlm(animationScene->mRootNode->mTransformation);
    globalInverseTransform = glm::inverse(globalInverseTransform);

    // Process all meshes in the animation file to extract bones
    for (unsigned int i = 0; i < animationScene->mNumMeshes; i++)
    {
        aiMesh* mesh = animationScene->mMeshes[i];
        std::vector<unsigned int> dummyIDs;
        std::vector<float> dummyWeights;
        loadBones(mesh, dummyIDs, dummyWeights);  // This fills boneInfo and boneMapping
    }

    hasAnimation = true;
    animationTime = 0.0f;
    std::cout << "Animation loaded + BONE DATA REBUILT: " << fixedPath << std::endl;
}

void Model::UpdateAnimation(float deltaTime)
{
    if (!hasAnimation || !animationScene || !animationScene->HasAnimations())
        return;

    aiAnimation *animation = animationScene->mAnimations[0];
    float ticksPerSecond = animation->mTicksPerSecond != 0 ? animation->mTicksPerSecond : 25.0f;
    static bool printed = false;
    if (!printed)
    {
        std::cout << "Animation Info:" << std::endl;
        std::cout << "  Duration: " << animation->mDuration << " ticks" << std::endl;
        std::cout << "  Ticks/Second: " << ticksPerSecond << std::endl;
        std::cout << "  Time in seconds: " << (animation->mDuration / ticksPerSecond) << "s" << std::endl;
        std::cout << "  Approximate frames (at 30fps): " << (animation->mDuration / ticksPerSecond * 30.0f) << std::endl;
        printed = true;
    }
    animationTime += deltaTime * ticksPerSecond;

    float duration = animation->mDuration;
    float blendWindow = duration * 0.05f;
    float loopBlendFactor = 0.0f;

    if (animationTime >= duration - blendWindow)
    {
        // We're in the blend zone - calculate blend factor
        float timeFromEnd = duration - animationTime;
        loopBlendFactor = 1.0f - (timeFromEnd / blendWindow);              // 0 at start of blend, 1 at end
        loopBlendFactor = std::max(0.0f, std::min(1.0f, loopBlendFactor)); // Clamp between 0 and 1
    }

    // Wrap animation time using fmod
    if (duration > 0.0f)
    {
        animationTime = fmod(animationTime, duration);
        if (animationTime < 0.0f)
            animationTime += duration;
    }

    // Update bone matrices with loop blending
    // This recursively processes the bone hierarchy and calculates final bone transformations
    glm::mat4 identity(1.0f);
    readNodeHierarchy(animationTime, animationScene->mRootNode, identity, loopBlendFactor);
}

void Model::loadBones(const aiMesh *mesh, std::vector<unsigned int> &boneIDs, std::vector<float> &boneWeights)
{
    // Initialize bone data arrays (each vertex can be influenced by up to 4 bones)
    boneIDs.resize(mesh->mNumVertices * 4, 0);
    boneWeights.resize(mesh->mNumVertices * 4, 0.0f);

    // Process each bone in the mesh
    for (unsigned int i = 0; i < mesh->mNumBones; i++)
    {
        unsigned int boneIndex = 0;
        std::string boneName(mesh->mBones[i]->mName.data);

        // Check if we've seen this bone before (bone mapping for shared bones across meshes)
        if (boneMapping.find(boneName) == boneMapping.end())
        {
            // New bone - add it to our mapping
            boneIndex = numBones;
            numBones++;
            BoneInfo bi;
            bi.offset = aiMatrix4x4ToGlm(mesh->mBones[i]->mOffsetMatrix); // Store bone's offset matrix
            boneInfo.push_back(bi);
            boneMapping[boneName] = boneIndex;
        }
        else
        {
            // Bone already exists - reuse its index
            boneIndex = boneMapping[boneName];
        }

        // Assign bone weights to vertices influenced by this bone
        for (unsigned int j = 0; j < mesh->mBones[i]->mNumWeights; j++)
        {
            unsigned int vertexID = mesh->mBones[i]->mWeights[j].mVertexId;
            float weight = mesh->mBones[i]->mWeights[j].mWeight;

            // Find an empty slot (up to 4 bones per vertex)
            for (unsigned int k = 0; k < 4; k++)
            {
                if (boneWeights[vertexID * 4 + k] == 0.0f)
                {
                    boneIDs[vertexID * 4 + k] = boneIndex;
                    boneWeights[vertexID * 4 + k] = weight;
                    break;
                }
            }
        }
    }
}

void Model::readNodeHierarchy(float animationTime, const aiNode *node, const glm::mat4 &parentTransform, float loopBlendFactor)
{
    std::string nodeName(node->mName.data);
    glm::mat4 nodeTransform = aiMatrix4x4ToGlm(node->mTransformation);

    const aiAnimation *animation = animationScene->mAnimations[0];
    const aiNodeAnim *nodeAnim = nullptr;

    for (unsigned int i = 0; i < animation->mNumChannels; i++)
    {
        const aiNodeAnim *channel = animation->mChannels[i];
        if (std::string(channel->mNodeName.data) == nodeName)
        {
            nodeAnim = channel;
            break;
        }
    }

    if (nodeAnim)
    {
        // Calculate pose at current time by interpolating between keyframes
        aiVector3D scaling = calcInterpolatedScaling(animationTime, nodeAnim);
        aiQuaternion rotation = calcInterpolatedRotation(animationTime, nodeAnim);
        aiVector3D translation = calcInterpolatedPosition(animationTime, nodeAnim);

        std::string rootNodeName(animationScene->mRootNode->mName.data);
        if (nodeName == rootNodeName ||
            nodeName.find("Root") != std::string::npos ||
            nodeName.find("root") != std::string::npos ||
            nodeName.find("ROOT") != std::string::npos)
        {
            translation = aiVector3D(0.0f, 0.0f, 0.0f); // Zero out root translation
        }

        if (loopBlendFactor > 0.0f)
        {
            // Get pose at time 0 (start of animation) for blending
            aiVector3D scalingStart = calcInterpolatedScaling(0.0f, nodeAnim);
            aiQuaternion rotationStart = calcInterpolatedRotation(0.0f, nodeAnim);
            aiVector3D translationStart = calcInterpolatedPosition(0.0f, nodeAnim);

            // Linear blend between current pose and start pose for scaling and translation
            scaling = aiVector3D(
                scaling.x * (1.0f - loopBlendFactor) + scalingStart.x * loopBlendFactor,
                scaling.y * (1.0f - loopBlendFactor) + scalingStart.y * loopBlendFactor,
                scaling.z * (1.0f - loopBlendFactor) + scalingStart.z * loopBlendFactor);

            translation = aiVector3D(
                translation.x * (1.0f - loopBlendFactor) + translationStart.x * loopBlendFactor,
                translation.y * (1.0f - loopBlendFactor) + translationStart.y * loopBlendFactor,
                translation.z * (1.0f - loopBlendFactor) + translationStart.z * loopBlendFactor);

            // Also zero out root translation after blending (maintain manual position control)
            std::string rootNodeName(animationScene->mRootNode->mName.data);
            if (nodeName == rootNodeName ||
                nodeName.find("Root") != std::string::npos ||
                nodeName.find("root") != std::string::npos ||
                nodeName.find("ROOT") != std::string::npos)
            {
                translation = aiVector3D(0.0f, 0.0f, 0.0f);
            }

            // Spherical linear interpolation (SLERP) for rotation (smooth rotation blending)
            aiQuaternion blendedRotation;
            aiQuaternion::Interpolate(blendedRotation, rotation, rotationStart, loopBlendFactor);
            blendedRotation.Normalize();
            rotation = blendedRotation;
        }

        glm::mat4 scalingM = glm::scale(glm::mat4(1.0f), aiVector3DToGlm(scaling));
        glm::mat4 rotationM = glm::mat4_cast(aiQuaternionToGlm(rotation));
        glm::mat4 translationM = glm::translate(glm::mat4(1.0f), aiVector3DToGlm(translation));

        nodeTransform = translationM * rotationM * scalingM;
    }

    glm::mat4 globalTransform = parentTransform * nodeTransform;

    if (boneMapping.find(nodeName) != boneMapping.end())
    {
        unsigned int boneIndex = boneMapping[nodeName];
        boneInfo[boneIndex].finalTransformation = globalInverseTransform * globalTransform * boneInfo[boneIndex].offset;
    }

    for (unsigned int i = 0; i < node->mNumChildren; i++)
    {
        readNodeHierarchy(animationTime, node->mChildren[i], globalTransform, loopBlendFactor);
    }
}

// Find the position keyframe index for the given animation time
// Returns the index of the keyframe just before the current time
unsigned int Model::findPosition(float animationTime, const aiNodeAnim *nodeAnim)
{
    for (unsigned int i = 0; i < nodeAnim->mNumPositionKeys - 1; i++)
    {
        if (animationTime < nodeAnim->mPositionKeys[i + 1].mTime)
            return i;
    }
    return 0;
}

// Find the rotation keyframe index for the given animation time
// Returns the index of the keyframe just before the current time
unsigned int Model::findRotation(float animationTime, const aiNodeAnim *nodeAnim)
{
    for (unsigned int i = 0; i < nodeAnim->mNumRotationKeys - 1; i++)
    {
        if (animationTime < nodeAnim->mRotationKeys[i + 1].mTime)
            return i;
    }
    return 0;
}

// Find the scaling keyframe index for the given animation time
// Returns the index of the keyframe just before the current time
unsigned int Model::findScaling(float animationTime, const aiNodeAnim *nodeAnim)
{
    for (unsigned int i = 0; i < nodeAnim->mNumScalingKeys - 1; i++)
    {
        if (animationTime < nodeAnim->mScalingKeys[i + 1].mTime)
            return i;
    }
    return 0;
}

// Interpolate position between two keyframes based on animation time
// Uses linear interpolation between the current and next position keyframe
aiVector3D Model::calcInterpolatedPosition(float animationTime, const aiNodeAnim *nodeAnim)
{
    // If only one keyframe, return it directly
    if (nodeAnim->mNumPositionKeys == 1)
        return nodeAnim->mPositionKeys[0].mValue;

    // Find the two keyframes to interpolate between
    unsigned int positionIndex = findPosition(animationTime, nodeAnim);
    unsigned int nextPositionIndex = positionIndex + 1;

    // Calculate interpolation factor (0.0 = at start keyframe, 1.0 = at end keyframe)
    float deltaTime = nodeAnim->mPositionKeys[nextPositionIndex].mTime - nodeAnim->mPositionKeys[positionIndex].mTime;
    float factor = (animationTime - nodeAnim->mPositionKeys[positionIndex].mTime) / deltaTime;

    // Linear interpolation between start and end positions
    aiVector3D start = nodeAnim->mPositionKeys[positionIndex].mValue;
    aiVector3D end = nodeAnim->mPositionKeys[nextPositionIndex].mValue;
    aiVector3D delta = end - start;

    return start + factor * delta;
}

// Interpolate rotation between two keyframes using spherical linear interpolation (SLERP)
// SLERP ensures smooth rotation interpolation along the shortest path on the sphere
aiQuaternion Model::calcInterpolatedRotation(float animationTime, const aiNodeAnim *nodeAnim)
{
    // If only one keyframe, return it directly
    if (nodeAnim->mNumRotationKeys == 1)
        return nodeAnim->mRotationKeys[0].mValue;

    // Find the two keyframes to interpolate between
    unsigned int rotationIndex = findRotation(animationTime, nodeAnim);
    unsigned int nextRotationIndex = rotationIndex + 1;

    // Calculate interpolation factor
    float deltaTime = nodeAnim->mRotationKeys[nextRotationIndex].mTime - nodeAnim->mRotationKeys[rotationIndex].mTime;
    float factor = (animationTime - nodeAnim->mRotationKeys[rotationIndex].mTime) / deltaTime;

    // Spherical linear interpolation (SLERP) for smooth rotation
    aiQuaternion start = nodeAnim->mRotationKeys[rotationIndex].mValue;
    aiQuaternion end = nodeAnim->mRotationKeys[nextRotationIndex].mValue;

    aiQuaternion result;
    aiQuaternion::Interpolate(result, start, end, factor);
    result.Normalize(); // Ensure quaternion is normalized

    return result;
}

// Interpolate scaling between two keyframes based on animation time
// Uses linear interpolation between the current and next scaling keyframe
aiVector3D Model::calcInterpolatedScaling(float animationTime, const aiNodeAnim *nodeAnim)
{
    // If only one keyframe, return it directly
    if (nodeAnim->mNumScalingKeys == 1)
        return nodeAnim->mScalingKeys[0].mValue;

    // Find the two keyframes to interpolate between
    unsigned int scalingIndex = findScaling(animationTime, nodeAnim);
    unsigned int nextScalingIndex = scalingIndex + 1;

    // Calculate interpolation factor
    float deltaTime = nodeAnim->mScalingKeys[nextScalingIndex].mTime - nodeAnim->mScalingKeys[scalingIndex].mTime;
    float factor = (animationTime - nodeAnim->mScalingKeys[scalingIndex].mTime) / deltaTime;

    // Linear interpolation between start and end scaling values
    aiVector3D start = nodeAnim->mScalingKeys[scalingIndex].mValue;
    aiVector3D end = nodeAnim->mScalingKeys[nextScalingIndex].mValue;
    aiVector3D delta = end - start;

    return start + factor * delta;
}

glm::mat4 Model::aiMatrix4x4ToGlm(const aiMatrix4x4 &from)
{
    glm::mat4 to;
    to[0][0] = from.a1;
    to[0][1] = from.b1;
    to[0][2] = from.c1;
    to[0][3] = from.d1;
    to[1][0] = from.a2;
    to[1][1] = from.b2;
    to[1][2] = from.c2;
    to[1][3] = from.d2;
    to[2][0] = from.a3;
    to[2][1] = from.b3;
    to[2][2] = from.c3;
    to[2][3] = from.d3;
    to[3][0] = from.a4;
    to[3][1] = from.b4;
    to[3][2] = from.c4;
    to[3][3] = from.d4;
    return to;
}

glm::vec3 Model::aiVector3DToGlm(const aiVector3D &vec)
{
    return glm::vec3(vec.x, vec.y, vec.z);
}

glm::quat Model::aiQuaternionToGlm(const aiQuaternion &quat)
{
    return glm::quat(quat.w, quat.x, quat.y, quat.z);
}
