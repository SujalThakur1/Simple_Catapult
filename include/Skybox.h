#pragma once
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>

class Skybox
{
public:
    Skybox(const std::string &hdrPath);
    ~Skybox();

    void Draw(const glm::mat4 &view, const glm::mat4 &projection, float deltaTime = 0.0f);
    void Update(float deltaTime);

private:
    unsigned int cubemapTexture;
    unsigned int skyboxVAO, skyboxVBO;
    unsigned int shaderProgram;

    void loadHDRTexture(const std::string &path);
    void setupSkyboxCube();
    unsigned int compileSkyboxShader();
    void convertEquirectangularToCubemap();

    // HDR data
    float *hdrData;
    int hdrWidth, hdrHeight, hdrChannels;

    // Rotation for animated skybox
    float rotationAngle;
    float rotationSpeed; // radians per second
};
