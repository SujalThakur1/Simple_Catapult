#include "Skybox.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <glm/gtc/type_ptr.hpp>

// STB_IMAGE_IMPLEMENTATION is now in stb_image_impl.cpp
#include "../third_party/stb_image.h"

// Skybox cube vertices
float skyboxVertices[] = {
    // positions
    -1.0f, 1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f,
    1.0f, -1.0f, -1.0f,
    1.0f, -1.0f, -1.0f,
    1.0f, 1.0f, -1.0f,
    -1.0f, 1.0f, -1.0f,

    -1.0f, -1.0f, 1.0f,
    -1.0f, -1.0f, -1.0f,
    -1.0f, 1.0f, -1.0f,
    -1.0f, 1.0f, -1.0f,
    -1.0f, 1.0f, 1.0f,
    -1.0f, -1.0f, 1.0f,

    1.0f, -1.0f, -1.0f,
    1.0f, -1.0f, 1.0f,
    1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, -1.0f,
    1.0f, -1.0f, -1.0f,

    -1.0f, -1.0f, 1.0f,
    -1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f,
    1.0f, -1.0f, 1.0f,
    -1.0f, -1.0f, 1.0f,

    -1.0f, 1.0f, -1.0f,
    1.0f, 1.0f, -1.0f,
    1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f,
    -1.0f, 1.0f, 1.0f,
    -1.0f, 1.0f, -1.0f,

    -1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f, 1.0f,
    1.0f, -1.0f, -1.0f,
    1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f, 1.0f,
    1.0f, -1.0f, 1.0f};

Skybox::Skybox(const std::string &hdrPath)
    : cubemapTexture(0), skyboxVAO(0), skyboxVBO(0), shaderProgram(0),
      hdrData(nullptr), hdrWidth(0), hdrHeight(0), hdrChannels(0),
      rotationAngle(0.0f), rotationSpeed(0.02f) // Slow rotation for cloud movement effect
{
    loadHDRTexture(hdrPath);
    convertEquirectangularToCubemap();
    setupSkyboxCube();
    shaderProgram = compileSkyboxShader();

    // Clean up HDR data after conversion
    if (hdrData)
    {
        stbi_image_free(hdrData);
        hdrData = nullptr;
    }
}

Skybox::~Skybox()
{
    if (cubemapTexture != 0)
        glDeleteTextures(1, &cubemapTexture);
    if (skyboxVAO != 0)
        glDeleteVertexArrays(1, &skyboxVAO);
    if (skyboxVBO != 0)
        glDeleteBuffers(1, &skyboxVBO);
    if (shaderProgram != 0)
        glDeleteProgram(shaderProgram);
    if (hdrData)
        stbi_image_free(hdrData);
}

void Skybox::loadHDRTexture(const std::string &path)
{
    // Try multiple paths (depending on where executable is run from)
    std::vector<std::string> pathsToTry = {
        path,            // Original path
        "../" + path,    // One level up (if run from build/)
        "../../" + path, // Two levels up
        "images/" + (path.find("images/") != std::string::npos ? path.substr(path.find("images/") + 7) : path)};

    stbi_set_flip_vertically_on_load(true);

    for (const auto &tryPath : pathsToTry)
    {
        hdrData = stbi_loadf(tryPath.c_str(), &hdrWidth, &hdrHeight, &hdrChannels, 0);
        if (hdrData)
        {
            std::cout << "Loaded HDR from: " << tryPath << std::endl;
            std::cout << "HDR dimensions: " << hdrWidth << "x" << hdrHeight << " channels: " << hdrChannels << std::endl;
            return;
        }
    }

    std::cerr << "Failed to load HDR image. Tried paths:" << std::endl;
    for (const auto &tryPath : pathsToTry)
    {
        std::cerr << "  - " << tryPath << std::endl;
    }
}

void Skybox::convertEquirectangularToCubemap()
{
    if (!hdrData || hdrWidth == 0 || hdrHeight == 0)
    {
        std::cerr << "Cannot convert: HDR data not loaded" << std::endl;
        return;
    }

    // Calculate cubemap size based on input resolution, but cap at reasonable maximum
    // For 2K HDR (2048x1024), use 512; for 4K (4096x2048), use 1024; for 8K (8192x4096), use 2048
    int cubemapSize = std::min(2048, std::max(512, hdrWidth / 4));
    // Round to nearest power of 2 for better GPU performance
    int powerOf2 = 512;
    while (powerOf2 < cubemapSize && powerOf2 < 2048)
        powerOf2 *= 2;
    cubemapSize = powerOf2;

    std::cout << "Converting HDR (" << hdrWidth << "x" << hdrHeight << ") to cubemap ("
              << cubemapSize << "x" << cubemapSize << " per face)" << std::endl;

    // Create framebuffer for rendering to cubemap
    unsigned int captureFBO, captureRBO;
    glGenFramebuffers(1, &captureFBO);
    glGenRenderbuffers(1, &captureRBO);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, cubemapSize, cubemapSize);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);

    // Generate cubemap texture
    glGenTextures(1, &cubemapTexture);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
    for (unsigned int i = 0; i < 6; ++i)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F,
                     cubemapSize, cubemapSize, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Create equirectangular to cubemap conversion shader
    const char *equirectVertexShader = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
out vec3 WorldPos;
uniform mat4 projection;
uniform mat4 view;
void main() {
    WorldPos = aPos;
    gl_Position = projection * view * vec4(WorldPos, 1.0);
}
)";

    const char *equirectFragmentShader = R"(
#version 330 core
out vec4 FragColor;
in vec3 WorldPos;
uniform sampler2D equirectangularMap;
const vec2 invAtan = vec2(0.1591, 0.3183);
vec2 SampleSphericalMap(vec3 v) {
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}
void main() {
    vec2 uv = SampleSphericalMap(normalize(WorldPos));
    vec3 color = texture(equirectangularMap, uv).rgb;
    FragColor = vec4(color, 1.0);
}
)";

    // Compile conversion shader
    unsigned int equirectProgram = 0;
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &equirectVertexShader, NULL);
    glCompileShader(vertexShader);

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &equirectFragmentShader, NULL);
    glCompileShader(fragmentShader);

    equirectProgram = glCreateProgram();
    glAttachShader(equirectProgram, vertexShader);
    glAttachShader(equirectProgram, fragmentShader);
    glLinkProgram(equirectProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Create equirectangular HDR texture
    unsigned int hdrTexture;
    glGenTextures(1, &hdrTexture);
    glBindTexture(GL_TEXTURE_2D, hdrTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, hdrWidth, hdrHeight, 0, GL_RGB, GL_FLOAT, hdrData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Projection and view matrices for capturing
    glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    glm::mat4 captureViews[] = {
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f))};

    // Create cube for rendering
    unsigned int cubeVAO, cubeVBO;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);

    // Convert HDR equirectangular to cubemap
    glUseProgram(equirectProgram);
    glUniform1i(glGetUniformLocation(equirectProgram, "equirectangularMap"), 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hdrTexture);

    // Store current viewport
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);

    glViewport(0, 0, cubemapSize, cubemapSize);
    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    for (unsigned int i = 0; i < 6; ++i)
    {
        glUniformMatrix4fv(glGetUniformLocation(equirectProgram, "projection"), 1, GL_FALSE, glm::value_ptr(captureProjection));
        glUniformMatrix4fv(glGetUniformLocation(equirectProgram, "view"), 1, GL_FALSE, glm::value_ptr(captureViews[i]));
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, cubemapTexture, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glBindVertexArray(cubeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }
    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Restore viewport (if it was set, otherwise framebuffer_size_callback will set it)
    if (viewport[2] > 0 && viewport[3] > 0)
    {
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    }

    // Cleanup
    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteBuffers(1, &cubeVBO);
    glDeleteTextures(1, &hdrTexture);
    glDeleteProgram(equirectProgram);
    glDeleteFramebuffers(1, &captureFBO);
    glDeleteRenderbuffers(1, &captureRBO);

    std::cout << "Converted HDR equirectangular to cubemap" << std::endl;
}

void Skybox::setupSkyboxCube()
{
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
    glBindVertexArray(0);
}

static std::string loadShaderSource(const std::string &path)
{
    std::ifstream file(path);
    if (!file.is_open())
        file.open("../" + path);

    if (!file.is_open())
    {
        std::cerr << "Error: Shader file not found at '" << path << "' or '../" << path << "'." << std::endl;
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

unsigned int Skybox::compileSkyboxShader()
{
    std::string vertexCode = loadShaderSource("../shaders/skybox_vertex.glsl");
    std::string fragmentCode = loadShaderSource("../shaders/skybox_fragment.glsl");

    if (vertexCode.empty() || fragmentCode.empty())
    {
        std::cerr << "Failed to load skybox shaders" << std::endl;
        return 0;
    }

    const char *vShaderCode = vertexCode.c_str();
    const char *fShaderCode = fragmentCode.c_str();

    unsigned int vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex, 1, &vShaderCode, NULL);
    glCompileShader(vertex);

    // Check compilation
    int success;
    char infoLog[512];
    glGetShaderiv(vertex, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertex, 512, NULL, infoLog);
        std::cerr << "Skybox vertex shader compilation failed:\n"
                  << infoLog << std::endl;
    }

    unsigned int fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment, 1, &fShaderCode, NULL);
    glCompileShader(fragment);

    glGetShaderiv(fragment, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragment, 512, NULL, infoLog);
        std::cerr << "Skybox fragment shader compilation failed:\n"
                  << infoLog << std::endl;
    }

    unsigned int program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);

    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        std::cerr << "Skybox shader linking failed:\n"
                  << infoLog << std::endl;
    }

    glDeleteShader(vertex);
    glDeleteShader(fragment);

    return program;
}

void Skybox::Update(float deltaTime)
{
    rotationAngle += rotationSpeed * deltaTime;
    // Keep angle in reasonable range to avoid precision issues
    if (rotationAngle > 2.0f * 3.14159265359f)
        rotationAngle -= 2.0f * 3.14159265359f;
}

void Skybox::Draw(const glm::mat4 &view, const glm::mat4 &projection, float deltaTime)
{
    if (shaderProgram == 0 || cubemapTexture == 0)
        return;

    // Update rotation
    if (deltaTime > 0.0f)
        Update(deltaTime);

    // Change depth function so depth test passes at 1.0
    glDepthFunc(GL_LEQUAL);
    glUseProgram(shaderProgram);

    // Remove translation from view matrix and apply rotation around Y axis
    glm::mat4 viewNoTranslation = glm::mat4(glm::mat3(view));
    glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), rotationAngle, glm::vec3(0.0f, 1.0f, 0.0f));
    viewNoTranslation = viewNoTranslation * rotation;

    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(viewNoTranslation));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    // Bind cubemap texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
    glUniform1i(glGetUniformLocation(shaderProgram, "skybox"), 0);

    // Draw skybox
    glBindVertexArray(skyboxVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);

    // Reset depth function
    glDepthFunc(GL_LESS);
}
