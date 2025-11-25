#ifndef MODEL_H
#define MODEL_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <vector>
#include <map>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <GL/glew.h>

struct Vertex
{
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;
    int BoneIDs[4] = {0, 0, 0, 0};
    float Weights[4] = {0.0f, 0.0f, 0.0f, 0.0f};
};

struct BoneInfo
{
    glm::mat4 offset;
    glm::mat4 finalTransformation;
};

struct Texture
{
    unsigned int id;
    std::string type;
    std::string path;
};

class Mesh
{
public:
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    std::vector<Texture> textures;
    unsigned int VAO, VBO, EBO;

    Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices, std::vector<Texture> textures);
    void Draw(unsigned int shaderProgram);

private:
    void setupMesh();
};

class Model
{
public:
    Model(const std::string &path);
    ~Model();
    void Draw(unsigned int shaderProgram);
    void UpdateAnimation(float deltaTime);
    void LoadAnimation(const std::string &animationPath);
    glm::vec3 getSize() const { return modelSize; }
    glm::vec3 getCenter() const { return modelCenter; }

private:
    std::vector<Mesh> meshes;
    std::string directory;
    static std::vector<Texture> textures_loaded; // Shared across all models
    glm::vec3 modelSize;
    glm::vec3 modelCenter;

    // Animation data
    const aiScene *scene;
    Assimp::Importer *importer;
    std::map<std::string, unsigned int> boneMapping;
    std::vector<BoneInfo> boneInfo;
    unsigned int numBones;
    glm::mat4 globalInverseTransform;

    // Current animation
    const aiScene *animationScene;
    Assimp::Importer *animationImporter;
    float animationTime;
    bool hasAnimation;

    void loadModel(const std::string &path);
    void processNode(aiNode *node, const aiScene *scene);
    Mesh processMesh(aiMesh *mesh, const aiScene *scene);
    void loadBones(const aiMesh *mesh, std::vector<unsigned int> &boneIDs, std::vector<float> &boneWeights);
    void readNodeHierarchy(float animationTime, const aiNode *node, const glm::mat4 &parentTransform, float loopBlendFactor = 0.0f);
    glm::mat4 aiMatrix4x4ToGlm(const aiMatrix4x4 &from);
    glm::vec3 aiVector3DToGlm(const aiVector3D &vec);
    glm::quat aiQuaternionToGlm(const aiQuaternion &quat);
    unsigned int findPosition(float animationTime, const aiNodeAnim *nodeAnim);
    unsigned int findRotation(float animationTime, const aiNodeAnim *nodeAnim);
    unsigned int findScaling(float animationTime, const aiNodeAnim *nodeAnim);
    aiVector3D calcInterpolatedPosition(float animationTime, const aiNodeAnim *nodeAnim);
    aiQuaternion calcInterpolatedRotation(float animationTime, const aiNodeAnim *nodeAnim);
    aiVector3D calcInterpolatedScaling(float animationTime, const aiNodeAnim *nodeAnim);
    std::vector<Texture> loadMaterialTextures(aiMaterial *mat, aiTextureType type, std::string typeName);
    unsigned int TextureFromFile(const char *path, const std::string &directory);
    void calculateBounds(const aiScene *scene);
};

#endif
