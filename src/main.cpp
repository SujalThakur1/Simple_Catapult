#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <sstream>
#include <filesystem>

#include "Catapult.h"
#include "Camera.h"
#include "Terrain.h"
#include "Projectile.h"
#include "Skybox.h"
#include "Zombie.h"
#include "PathUtils.h"
#include <vector>
#include <map>

// ===== Globals =====
Camera camera(glm::vec3(-4.0f, 1.50f, -0.10f), glm::vec3(0.0f, 1.0f, 0.0f), -5.0f, -15.0f);
float lastX = 400, lastY = 300;
bool firstMouse = true;
float deltaTime = 0.0f;
float lastFrame = 0.0f;
bool mousePressed = false;

// Camera mode system (3 zoom levels)
enum CameraMode
{
    ZOOM_OUT_1 = 0, // Medium distance
    ZOOM_OUT_2 = 1, // Far distance
    ZOOM_OUT_3 = 2  // Very far distance
};
CameraMode currentCameraMode = ZOOM_OUT_1;
bool cameraModeKeyPressed = false;

// Free-look camera mode
bool freeLookMode = false;
float freeLookSpeed = 10.0f; // Movement speed for free-look mode

// Camera follow system - smooth interpolation
glm::vec3 targetCameraPosition = glm::vec3(0.0f, 0.0f, 0.0f);
float cameraFollowSpeed = 5.0f;
float targetCameraYaw = 0.0f;
float targetCameraPitch = 0.0f;
float cameraRotationFollowSpeed = 3.0f;
float cameraRotationFollowSpeedFast = 15.0f;
float lastCatapultRotation = 0.0f;
float currentRotationSpeed = 3.0f;
float rotationSpeedTransitionRate = 8.0f;

// Camera follow projectile mode
bool cameraFollowProjectile = false;
float projectileFollowDistance = 5.0f;
float projectileFollowHeight = 2.0f;
float projectileFollowZoom = 60.0f;

// Projectile and Catapult
Projectile *bomb = nullptr;
Catapult *catapultPtr = nullptr;

// Zombies
std::vector<Zombie *> zombies;

// Default white texture for fallback (to avoid OpenGL texture errors)
unsigned int defaultWhiteTexture = 0;

// ===== Input Callbacks =====
void processInput(GLFWwindow *window, Terrain &terrain)
{
    // Check for catapult movement keys - if pressed, exit free-look mode
    bool catapultMoving = (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS ||
                           glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS ||
                           glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS ||
                           glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS);

    if (catapultMoving && freeLookMode)
    {
        // Exit free-look mode and reset camera to catapult position
        freeLookMode = false;
    }

    // Free-look camera controls (Shift + WASD/QE)
    bool shiftPressed = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                         glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);

    if (shiftPressed)
    {
        // Enter free-look mode
        if (!freeLookMode)
        {
            freeLookMode = true;
            firstMouse = true; // Reset mouse for smooth transition
        }

        // Free-look movement
        camera.MovementSpeed = freeLookSpeed;

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            camera.ProcessKeyboard(FORWARD, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            camera.ProcessKeyboard(BACKWARD, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            camera.ProcessKeyboard(LEFT, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            camera.ProcessKeyboard(RIGHT, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
            camera.ProcessKeyboard(DOWN, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
            camera.ProcessKeyboard(UP, deltaTime);
    }
    else
    {
        // Exit free-look mode when shift is released
        if (freeLookMode && !catapultMoving)
        {
            freeLookMode = false;
        }
    }

    // Camera mode toggle (P key) - cycle through 3 zoom levels (only in follow mode)
    if (!freeLookMode && glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS)
    {
        if (!cameraModeKeyPressed)
        {
            currentCameraMode = (CameraMode)((currentCameraMode + 1) % 3);
            cameraModeKeyPressed = true;
        }
    }
    else
    {
        cameraModeKeyPressed = false;
    }

    // Catapult controls
    if (catapultPtr)
    {
        bool isTurning = false;

        // Catapult rotation (left/right arrows)
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
        {
            catapultPtr->rotateLeft(deltaTime);
            isTurning = true;
        }
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
        {
            catapultPtr->rotateRight(deltaTime);
            isTurning = true;
        }

        // Catapult movement (UP/DOWN arrows) with collision detection
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
        {
            glm::vec3 oldPos = catapultPtr->getPosition();
            catapultPtr->moveBackward(deltaTime);
            glm::vec3 newPos = catapultPtr->getPosition();

            // Check collision with trees and walls (catapult collision radius ~0.8 units for tighter collision)
            if (terrain.checkTreeCollision(newPos.x, newPos.z, 0.8f) ||
                terrain.checkWallCollision(newPos.x, newPos.z, 0.8f))
            {
                // Revert position if collision detected
                catapultPtr->setPosition(oldPos);
            }
        }
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
        {
            glm::vec3 oldPos = catapultPtr->getPosition();
            catapultPtr->moveForward(deltaTime);
            glm::vec3 newPos = catapultPtr->getPosition();

            // Check collision with trees and walls (catapult collision radius ~0.8 units for tighter collision)
            if (terrain.checkTreeCollision(newPos.x, newPos.z, 0.8f) ||
                terrain.checkWallCollision(newPos.x, newPos.z, 0.8f))
            {
                // Revert position if collision detected
                catapultPtr->setPosition(oldPos);
            }
        }

        // Update wheel steering
        catapultPtr->updateWheelSteering(deltaTime, isTurning);

        // Catapult projectile speed controls (+/- keys)
        static float speedChangeTimer = 0.0f;
        static const float speedChangeInterval = 0.05f; // Adjust speed every 0.05 seconds when holding

        speedChangeTimer += deltaTime;

        // Increase speed while holding +
        if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_KP_ADD) == GLFW_PRESS)
        {
            if (speedChangeTimer >= speedChangeInterval)
            {
                catapultPtr->increaseSpeed();
                speedChangeTimer = 0.0f; // Reset timer
            }
        }
        // Decrease speed while holding - (MINUS key)
        else if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS)
        {
            if (speedChangeTimer >= speedChangeInterval)
            {
                catapultPtr->decreaseSpeed();
                speedChangeTimer = 0.0f; // Reset timer
            }
        }
        else
        {
            // Reset timer when no key is pressed
            speedChangeTimer = 0.0f;
        }
    }
}

void mouse_callback(GLFWwindow *window, double xpos, double ypos)
{
    // In free-look mode, always allow mouse movement
    // In follow mode, only allow mouse movement when mouse button is pressed
    if (!freeLookMode && !mousePressed)
        return;

    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    camera.ProcessMouseMovement(xoffset, yoffset);
}

void mouse_button_callback(GLFWwindow *window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT)
    {
        if (action == GLFW_PRESS)
        {
            mousePressed = true;
            firstMouse = true;
        }
        else if (action == GLFW_RELEASE)
            mousePressed = false;
    }
}

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(yoffset);
}

// ===== Health Display HUD =====
void renderHealthBar(GLFWwindow *window, unsigned int shaderProgram, float health, float maxHealth, const glm::mat4 &originalProjection, const glm::mat4 &originalView)
{
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    // Store current state
    GLboolean depthTestEnabled;
    glGetBooleanv(GL_DEPTH_TEST, &depthTestEnabled);
    glDisable(GL_DEPTH_TEST);

    // Use shader program
    glUseProgram(shaderProgram);

    // Create orthographic projection for 2D HUD
    glm::mat4 projection = glm::ortho(0.0f, (float)width, (float)height, 0.0f, -1.0f, 1.0f);
    glm::mat4 view = glm::mat4(1.0f);

    // Health bar dimensions
    float barWidth = 400.0f;
    float barHeight = 30.0f;
    float barX = (width - barWidth) / 2.0f;
    float barY = 20.0f;

    float healthPercent = health / maxHealth;

    // Create simple quad VAO for health bar
    static unsigned int healthBarVAO = 0, healthBarVBO = 0;
    static bool initialized = false;

    if (!initialized)
    {
        glGenVertexArrays(1, &healthBarVAO);
        glGenBuffers(1, &healthBarVBO);
        initialized = true;
    }

    // Set uniforms
    unsigned int hudProjLoc = glGetUniformLocation(shaderProgram, "projection");
    unsigned int hudViewLoc = glGetUniformLocation(shaderProgram, "view");
    unsigned int hudModelLoc = glGetUniformLocation(shaderProgram, "model");
    unsigned int hudColorLoc = glGetUniformLocation(shaderProgram, "objectColor");

    glUniformMatrix4fv(hudProjLoc, 1, GL_FALSE, glm::value_ptr(projection));
    glUniformMatrix4fv(hudViewLoc, 1, GL_FALSE, glm::value_ptr(view));
    glUniform1i(glGetUniformLocation(shaderProgram, "useTexture"), 0);

    // Draw background (dark red/black)
    float bgVertices[] = {
        barX, barY, 0.0f, 0.0f, 1.0f, 0.0f,
        barX + barWidth, barY, 0.0f, 0.0f, 1.0f, 0.0f,
        barX + barWidth, barY + barHeight, 0.0f, 0.0f, 1.0f, 0.0f,
        barX, barY, 0.0f, 0.0f, 1.0f, 0.0f,
        barX + barWidth, barY + barHeight, 0.0f, 0.0f, 1.0f, 0.0f,
        barX, barY + barHeight, 0.0f, 0.0f, 1.0f, 0.0f};

    glBindVertexArray(healthBarVAO);
    glBindBuffer(GL_ARRAY_BUFFER, healthBarVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(bgVertices), bgVertices, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glm::mat4 hudModel = glm::mat4(1.0f);
    glUniformMatrix4fv(hudModelLoc, 1, GL_FALSE, glm::value_ptr(hudModel));
    glUniform3f(hudColorLoc, 0.2f, 0.0f, 0.0f);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Draw health bar (green to red gradient)
    float healthBarWidth = barWidth * healthPercent;
    float healthColorR, healthColorG, healthColorB;
    if (healthPercent > 0.5f)
    {
        // Green to yellow
        healthColorG = 1.0f;
        healthColorR = 2.0f * (1.0f - healthPercent);
        healthColorB = 0.0f;
    }
    else
    {
        // Yellow to red
        healthColorR = 1.0f;
        healthColorG = 2.0f * healthPercent;
        healthColorB = 0.0f;
    }

    float healthVertices[] = {
        barX, barY, 0.0f, 0.0f, 1.0f, 0.0f,
        barX + healthBarWidth, barY, 0.0f, 0.0f, 1.0f, 0.0f,
        barX + healthBarWidth, barY + barHeight, 0.0f, 0.0f, 1.0f, 0.0f,
        barX, barY, 0.0f, 0.0f, 1.0f, 0.0f,
        barX + healthBarWidth, barY + barHeight, 0.0f, 0.0f, 1.0f, 0.0f,
        barX, barY + barHeight, 0.0f, 0.0f, 1.0f, 0.0f};

    glBufferData(GL_ARRAY_BUFFER, sizeof(healthVertices), healthVertices, GL_DYNAMIC_DRAW);
    glUniform3f(hudColorLoc, healthColorR, healthColorG, healthColorB);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Draw border (white outline)
    float borderThickness = 2.0f;
    float borderVertices[] = {
        // Top
        barX, barY, 0.0f, 0.0f, 1.0f, 0.0f,
        barX + barWidth, barY, 0.0f, 0.0f, 1.0f, 0.0f,
        barX + barWidth, barY + borderThickness, 0.0f, 0.0f, 1.0f, 0.0f,
        barX, barY, 0.0f, 0.0f, 1.0f, 0.0f,
        barX + barWidth, barY + borderThickness, 0.0f, 0.0f, 1.0f, 0.0f,
        barX, barY + borderThickness, 0.0f, 0.0f, 1.0f, 0.0f,
        // Bottom
        barX, barY + barHeight - borderThickness, 0.0f, 0.0f, 1.0f, 0.0f,
        barX + barWidth, barY + barHeight - borderThickness, 0.0f, 0.0f, 1.0f, 0.0f,
        barX + barWidth, barY + barHeight, 0.0f, 0.0f, 1.0f, 0.0f,
        barX, barY + barHeight - borderThickness, 0.0f, 0.0f, 1.0f, 0.0f,
        barX + barWidth, barY + barHeight, 0.0f, 0.0f, 1.0f, 0.0f,
        barX, barY + barHeight, 0.0f, 0.0f, 1.0f, 0.0f,
        // Left
        barX, barY, 0.0f, 0.0f, 1.0f, 0.0f,
        barX + borderThickness, barY, 0.0f, 0.0f, 1.0f, 0.0f,
        barX + borderThickness, barY + barHeight, 0.0f, 0.0f, 1.0f, 0.0f,
        barX, barY, 0.0f, 0.0f, 1.0f, 0.0f,
        barX + borderThickness, barY + barHeight, 0.0f, 0.0f, 1.0f, 0.0f,
        barX, barY + barHeight, 0.0f, 0.0f, 1.0f, 0.0f,
        // Right
        barX + barWidth - borderThickness, barY, 0.0f, 0.0f, 1.0f, 0.0f,
        barX + barWidth, barY, 0.0f, 0.0f, 1.0f, 0.0f,
        barX + barWidth, barY + barHeight, 0.0f, 0.0f, 1.0f, 0.0f,
        barX + barWidth - borderThickness, barY, 0.0f, 0.0f, 1.0f, 0.0f,
        barX + barWidth, barY + barHeight, 0.0f, 0.0f, 1.0f, 0.0f,
        barX + barWidth - borderThickness, barY + barHeight, 0.0f, 0.0f, 1.0f, 0.0f};

    glBufferData(GL_ARRAY_BUFFER, sizeof(borderVertices), borderVertices, GL_DYNAMIC_DRAW);
    glUniform3f(hudColorLoc, 1.0f, 1.0f, 1.0f);
    glDrawArrays(GL_TRIANGLES, 0, 24);

    glBindVertexArray(0);

    // Restore original projection and view matrices
    unsigned int restoreProjLoc = glGetUniformLocation(shaderProgram, "projection");
    unsigned int restoreViewLoc = glGetUniformLocation(shaderProgram, "view");
    glUniformMatrix4fv(restoreProjLoc, 1, GL_FALSE, glm::value_ptr(originalProjection));
    glUniformMatrix4fv(restoreViewLoc, 1, GL_FALSE, glm::value_ptr(originalView));

    // Restore depth test state
    if (depthTestEnabled)
    {
        glEnable(GL_DEPTH_TEST);
    }
}

// ===== Shader Helpers =====
std::string loadShaderSource(const std::string &path)
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

unsigned int compileShader(const std::string &vertexPath, const std::string &fragmentPath)
{
    std::string vertexCode = loadShaderSource(vertexPath);
    std::string fragmentCode = loadShaderSource(fragmentPath);
    const char *vShaderCode = vertexCode.c_str();
    const char *fShaderCode = fragmentCode.c_str();

    unsigned int vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex, 1, &vShaderCode, NULL);
    glCompileShader(vertex);

    unsigned int fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment, 1, &fShaderCode, NULL);
    glCompileShader(fragment);

    unsigned int program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);

    glDeleteShader(vertex);
    glDeleteShader(fragment);

    return program;
}

void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
    glViewport(0, 0, width, height);
}

// ===== Main =====
int main()
{
    // GLFW Init
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow *window = glfwCreateWindow(800, 600, "Catapult Scene (3D)", NULL, NULL);
    if (!window)
    {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK)
    {
        std::cerr << "Failed to initialize GLEW\n";
        return -1;
    }

    glEnable(GL_DEPTH_TEST);
    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;

    // Create a default white texture to avoid OpenGL texture binding errors
    if (defaultWhiteTexture == 0)
    {
        glGenTextures(1, &defaultWhiteTexture);
        glBindTexture(GL_TEXTURE_2D, defaultWhiteTexture);
        unsigned char whitePixel[] = {255, 255, 255, 255};
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    // ===== Compile Shaders =====
    std::string vertexPath = "../shaders/vertex.glsl";
    std::string fragmentPath = "../shaders/fragment.glsl";
    unsigned int shaderProgram = compileShader(vertexPath, fragmentPath);

    // ===== Create Skybox =====
    Skybox skybox(FindImagePath("Skybox/kloofendal_48d_partly_cloudy_puresky_16k.hdr"));

    // Create bomb AFTER context and GLEW are initialized
    bomb = new Projectile(glm::vec3(0.0f, 0.5f, 0.0f));

    // ===== Sun position for lighting (sun is in skybox) =====
    glm::vec3 sunPosition = glm::vec3(5.0f, 9.0f, -2.0f);
    Terrain terrain(60.0f);
    Catapult catapult;
    catapultPtr = &catapult;

    // ===== CATAPULT INITIAL POSITION AND ROTATION =====
    float catapultStartX = -15.0f;
    float catapultStartZ = 20.0f;
    glm::vec3 catapultStartPosition(catapultStartX, 0.0f, catapultStartZ);
    catapult.setPosition(catapultStartPosition);

    float catapultStartRotation = 1.5708f;
    catapult.setRotation(catapultStartRotation);
    // ===== END OF CATAPULT POSITION =====

    // ===== SET CAMERA INITIAL POSITION TO MATCH CATAPULT =====
    float catapultTerrainHeight = terrain.getHeight(catapultStartX, catapultStartZ);
    glm::vec3 catapultTerrainNormal = terrain.getNormal(catapultStartX, catapultStartZ);

    // Use default camera mode settings for initial position
    float initialCameraDistance = 3.9f; // Default distance (ZOOM_OUT_1)
    float initialCameraHeight = 1.5f;

    // Build catapult transformation matrix (same as in render loop)
    glm::mat4 catapultTransform = glm::mat4(1.0f);
    catapultTransform = glm::translate(catapultTransform, glm::vec3(catapultStartX, 0.0f, catapultStartZ));

    // Apply terrain height offset
    float wheelY = -0.2f;
    float wheelRadius = 0.18f;
    float wheelBottomOffset = -(wheelY - wheelRadius);
    catapultTransform = glm::translate(catapultTransform, glm::vec3(0.0f, catapultTerrainHeight + wheelBottomOffset, 0.0f));

    // Rotate to match terrain slope
    glm::vec3 up(0.0f, 1.0f, 0.0f);
    glm::vec3 normal = glm::normalize(catapultTerrainNormal);
    glm::vec3 axis = glm::cross(up, normal);
    float tiltAngle = acosf(glm::clamp(glm::dot(up, normal), -1.0f, 1.0f));
    if (glm::length(axis) > 0.001f && tiltAngle > 0.001f)
    {
        axis = glm::normalize(axis);
        catapultTransform = glm::rotate(catapultTransform, tiltAngle, axis);
    }

    // Rotate around Y axis for steering
    catapultTransform = glm::rotate(catapultTransform, catapultStartRotation, glm::vec3(0.0f, 1.0f, 0.0f));

    // Calculate camera position in local space (behind the catapult)
    glm::vec3 localCameraPos(-initialCameraDistance, initialCameraHeight, 0.0f);
    glm::vec4 transformedCameraPos = catapultTransform * glm::vec4(localCameraPos, 1.0f);
    glm::vec3 initialCameraPosition = glm::vec3(transformedCameraPos);

    // Calculate catapult center for camera to look at
    glm::vec3 catapultCenterLocal(0.0f, 0.0f, 0.0f);
    glm::vec4 transformedCatapultCenter = catapultTransform * glm::vec4(catapultCenterLocal, 1.0f);
    glm::vec3 catapultCenterWorld = glm::vec3(transformedCatapultCenter);

    // Calculate look direction
    glm::vec3 lookAheadOffset = glm::normalize(glm::vec3(catapultTransform * glm::vec4(1.0f, 0.0f, 0.0f, 0.0f))) * 1.0f;
    glm::vec3 lookTarget = catapultCenterWorld + lookAheadOffset;
    glm::vec3 lookDirection = glm::normalize(lookTarget - initialCameraPosition);

    // Calculate yaw and pitch
    float horizontalLength = sqrtf(lookDirection.x * lookDirection.x + lookDirection.z * lookDirection.z);
    float initialYaw = glm::degrees(atan2f(lookDirection.z, lookDirection.x));
    float clampedY = glm::clamp(lookDirection.y, -1.0f, 1.0f);
    float initialPitch = glm::degrees(asinf(clampedY));

    // Set camera initial position and rotation
    camera.SetPosition(initialCameraPosition);
    camera.SetRotation(initialYaw, initialPitch);
    camera.SetZoom(70.0f); // Default zoom (ZOOM_OUT_1)
    // ===== END OF CAMERA INITIAL POSITION =====

    // ============================================================================
    // ===== ZOMBIE CONFIGURATION SECTION =====
    // ============================================================================
    std::string zombieModelPath = FindImagePath("zombie/uploads_files_2137887_zombie_fbx_rigged.fbx");

    // Zombie configuration structure (used for both initial spawn and respawn)
    struct ZombieConfig
    {
        glm::vec3 position;
        float scale;
        float speed;
        ZombieBehavior behavior;
        float detectionRadius;
        bool isBoss;
        float maxHealth;
        float rotationY;
        glm::vec3 patrolA;
        glm::vec3 patrolB;
    };
    std::vector<ZombieConfig> zombieConfigs;

    // Define all zombie configurations
    // ZOMBIE 1: BOSS (IDLE behavior)
    zombieConfigs.push_back({glm::vec3(15.0f, terrain.getHeight(15.0f, 15.0f), 15.0f), 0.02f, 1.0f, ZombieBehavior::IDLE, 16.0f, true, 200.0f, 3.14159f, glm::vec3(0), glm::vec3(0)});
    // ZOMBIE 2: IDLE
    zombieConfigs.push_back({glm::vec3(-13.0f, terrain.getHeight(-13.0f, -13.0f), -13.0f), 0.01f, 1.0f, ZombieBehavior::IDLE, 8.0f, false, 80.0f, 0.0f, glm::vec3(0), glm::vec3(0)});
    // ZOMBIE 3: IDLE
    zombieConfigs.push_back({glm::vec3(-16.0f, terrain.getHeight(-16.0f, -13.0f), -13.0f), 0.01f, 1.0f, ZombieBehavior::IDLE, 8.0f, false, 100.0f, 0.0f, glm::vec3(0), glm::vec3(0)});
    // ZOMBIE 4: IDLE
    zombieConfigs.push_back({glm::vec3(-10.0f, terrain.getHeight(-10.0f, 10.0f), 10.0f), 0.01f, 1.0f, ZombieBehavior::IDLE, 8.0f, false, 120.0f, 0.0f, glm::vec3(0), glm::vec3(0)});
    // ZOMBIE 5: IDLE
    zombieConfigs.push_back({glm::vec3(-20.0f, terrain.getHeight(-20.0f, 2.0f), 2.0f), 0.01f, 1.0f, ZombieBehavior::IDLE, 8.0f, false, 90.0f, 0.0f, glm::vec3(0), glm::vec3(0)});
    // ZOMBIE 6: PATROL
    zombieConfigs.push_back({glm::vec3(-13.0f, terrain.getHeight(-13.0f, -20.0f), -20.0f), 0.01f, 0.8f, ZombieBehavior::PATROL, 10.0f, false, 70.0f, 0.0f,
                             glm::vec3(-10.0f, terrain.getHeight(-13.0f, -20.0f), -20.0f),
                             glm::vec3(10.0f, terrain.getHeight(-13.0f, -22.0f), -22.0f)});
    // ZOMBIE 7: PATROL
    zombieConfigs.push_back({glm::vec3(11.0f, terrain.getHeight(11.0f, -10.0f), -10.0f), 0.01f, 0.8f, ZombieBehavior::PATROL, 10.0f, false, 85.0f, 0.0f,
                             glm::vec3(11.0f, terrain.getHeight(11.0f, -10.0f), -10.0f),
                             glm::vec3(15.0f, terrain.getHeight(15.0f, -10.0f), -10.0f)});
    // ZOMBIE 8: PATROL
    zombieConfigs.push_back({glm::vec3(20.0f, terrain.getHeight(20.0f, -10.0f), -10.0f), 0.01f, 0.8f, ZombieBehavior::PATROL, 10.0f, false, 75.0f, 0.0f,
                             glm::vec3(20.0f, terrain.getHeight(20.0f, -10.0f), -10.0f),
                             glm::vec3(16.0f, terrain.getHeight(16.0f, -10.0f), -10.0f)});
    // ZOMBIE 9: IDLE
    zombieConfigs.push_back({glm::vec3(10.0f, terrain.getHeight(10.0f, 10.0f), 10.0f), 0.01f, 1.0f, ZombieBehavior::IDLE, 8.0f, false, 110.0f, 3.14159f, glm::vec3(0), glm::vec3(0)});
    // ZOMBIE 10: IDLE
    zombieConfigs.push_back({glm::vec3(20.0f, terrain.getHeight(20.0f, 10.0f), 10.0f), 0.01f, 1.0f, ZombieBehavior::IDLE, 8.0f, false, 95.0f, 3.14159f, glm::vec3(0), glm::vec3(0)});

    // Create zombies from configurations
    for (const auto &config : zombieConfigs)
    {
        glm::vec3 pos = config.position;
        pos.y = terrain.getHeight(pos.x, pos.z);
        Zombie *zombie = new Zombie(
            zombieModelPath,
            pos,
            config.scale,
            config.speed,
            config.behavior,
            config.detectionRadius,
            config.isBoss);
        zombie->setRotationY(config.rotationY);
        zombie->setMaxHealth(config.maxHealth);
        zombie->setHealth(config.maxHealth);
        if (config.behavior == ZombieBehavior::PATROL)
        {
            zombie->setPatrolPoints(config.patrolA, config.patrolB);
        }
        zombies.push_back(zombie);
    }

    std::cout << "Spawned " << zombies.size() << " zombies with custom configurations!" << std::endl;
    // ============================================================================
    // ===== END OF ZOMBIE CONFIGURATION =====
    // ============================================================================

    // Respawn function
    auto respawnEverything = [&]()
    {
        // Reset catapult
        catapult.setPosition(catapultStartPosition);
        catapult.setRotation(catapultStartRotation);
        catapult.setHealth(100.0f);

        // Reset camera
        float initialTerrainHeight = terrain.getHeight(catapultStartX, catapultStartZ);
        glm::vec3 initialTerrainNormal = terrain.getNormal(catapultStartX, catapultStartZ);

        glm::mat4 catapultTransform = glm::mat4(1.0f);
        catapultTransform = glm::translate(catapultTransform, catapultStartPosition);
        float wheelY = -0.2f;
        float wheelRadius = 0.18f;
        float wheelBottomOffset = -(wheelY - wheelRadius);
        catapultTransform = glm::translate(catapultTransform, glm::vec3(0.0f, initialTerrainHeight + wheelBottomOffset, 0.0f));

        glm::vec3 up(0.0f, 1.0f, 0.0f);
        glm::vec3 normal = glm::normalize(initialTerrainNormal);
        glm::vec3 axis = glm::cross(up, normal);
        float tiltAngle = acosf(glm::clamp(glm::dot(up, normal), -1.0f, 1.0f));
        if (glm::length(axis) > 0.001f && tiltAngle > 0.001f)
        {
            axis = glm::normalize(axis);
            catapultTransform = glm::rotate(catapultTransform, tiltAngle, axis);
        }
        catapultTransform = glm::rotate(catapultTransform, catapultStartRotation, glm::vec3(0.0f, 1.0f, 0.0f));

        glm::vec3 localCameraPos(-initialCameraDistance, initialCameraHeight, 0.0f);
        glm::vec4 transformedCameraPos = catapultTransform * glm::vec4(localCameraPos, 1.0f);
        glm::vec3 initialCameraPosition = glm::vec3(transformedCameraPos);

        glm::vec3 catapultCenterLocal(0.0f, 0.0f, 0.0f);
        glm::vec4 transformedCatapultCenter = catapultTransform * glm::vec4(catapultCenterLocal, 1.0f);
        glm::vec3 catapultCenterWorld = glm::vec3(transformedCatapultCenter);

        glm::vec3 lookAheadOffset = glm::normalize(glm::vec3(catapultTransform * glm::vec4(1.0f, 0.0f, 0.0f, 0.0f))) * 1.0f;
        glm::vec3 lookTarget = catapultCenterWorld + lookAheadOffset;
        glm::vec3 lookDirection = glm::normalize(lookTarget - initialCameraPosition);

        float horizontalLength = sqrtf(lookDirection.x * lookDirection.x + lookDirection.z * lookDirection.z);
        float initialYaw = glm::degrees(atan2f(lookDirection.z, lookDirection.x));
        float clampedY = glm::clamp(lookDirection.y, -1.0f, 1.0f);
        float initialPitch = glm::degrees(asinf(clampedY));

        camera.SetPosition(initialCameraPosition);
        camera.SetRotation(initialYaw, initialPitch);
        camera.SetZoom(70.0f);

        // Delete and respawn all zombies
        for (auto *zombie : zombies)
        {
            delete zombie;
        }
        zombies.clear();

        // Respawn zombies from configurations
        for (const auto &config : zombieConfigs)
        {
            glm::vec3 pos = config.position;
            pos.y = terrain.getHeight(pos.x, pos.z);
            Zombie *zombie = new Zombie(
                zombieModelPath,
                pos,
                config.scale,
                config.speed,
                config.behavior,
                config.detectionRadius,
                config.isBoss);
            zombie->setRotationY(config.rotationY);
            zombie->setMaxHealth(config.maxHealth);
            zombie->setHealth(config.maxHealth);
            if (config.behavior == ZombieBehavior::PATROL)
            {
                zombie->setPatrolPoints(config.patrolA, config.patrolB);
            }
            zombies.push_back(zombie);
        }

        // Reset bomb
        if (bomb)
        {
            float initialTerrainHeight = terrain.getHeight(catapultStartX, catapultStartZ);
            glm::vec3 initialTerrainNormal = terrain.getNormal(catapultStartX, catapultStartZ);
            glm::vec3 bucketPos = catapult.getBucketPositionWorld(initialTerrainHeight, initialTerrainNormal);

            float armAngle = catapult.getArmAngle();
            float cos_a = cosf(armAngle);
            float sin_a = sinf(armAngle);

            float localOffsetX = -bomb->bucketOffsetX * cos_a - bomb->bucketOffsetY * sin_a;
            float localOffsetY = -bomb->bucketOffsetX * sin_a + bomb->bucketOffsetY * cos_a;
            float localOffsetZ = bomb->bucketOffsetZ;

            glm::mat4 transform = glm::mat4(1.0f);
            transform = glm::translate(transform, catapultStartPosition);

            float wheelY = -0.2f;
            float wheelRadius = 0.18f;
            float wheelBottomOffset = -(wheelY - wheelRadius);
            transform = glm::translate(transform, glm::vec3(0.0f, initialTerrainHeight + wheelBottomOffset, 0.0f));

            glm::vec3 up(0.0f, 1.0f, 0.0f);
            glm::vec3 normal = glm::normalize(initialTerrainNormal);
            glm::vec3 axis = glm::cross(up, normal);
            float angle = acosf(glm::clamp(glm::dot(up, normal), -1.0f, 1.0f));
            if (glm::length(axis) > 0.001f && angle > 0.001f)
            {
                axis = glm::normalize(axis);
                transform = glm::rotate(transform, angle, axis);
            }

            transform = glm::rotate(transform, catapultStartRotation, glm::vec3(0.0f, 1.0f, 0.0f));

            glm::vec3 localOffset(localOffsetX, localOffsetY, localOffsetZ);
            glm::vec3 worldOffset = glm::vec3(transform * glm::vec4(localOffset, 0.0f));

            bomb->position = bucketPos + worldOffset;
            bomb->isLaunched = false;
            bomb->hasHit = false;
            bomb->damageApplied = false;
            bomb->isAnimating = false;
        }

        std::cout << "Respawned! Health: 100/100" << std::endl;
    };

    // Position bomb initially in bucket
    if (bomb)
    {
        // Get bucket position with all terrain transformations applied
        float initialTerrainHeight = terrain.getHeight(catapult.getPosition().x, catapult.getPosition().z);
        glm::vec3 initialTerrainNormal = terrain.getNormal(catapult.getPosition().x, catapult.getPosition().z);
        glm::vec3 bucketPos = catapult.getBucketPositionWorld(initialTerrainHeight, initialTerrainNormal);

        // Apply bomb offset relative to bucket (same as in render loop)
        float armAngle = catapult.getArmAngle();
        float cos_a = cosf(armAngle);
        float sin_a = sinf(armAngle);

        float localOffsetX = -bomb->bucketOffsetX * cos_a - bomb->bucketOffsetY * sin_a;
        float localOffsetY = -bomb->bucketOffsetX * sin_a + bomb->bucketOffsetY * cos_a;
        float localOffsetZ = bomb->bucketOffsetZ;

        // Build same transformation matrix as bucket
        glm::mat4 transform = glm::mat4(1.0f);
        transform = glm::translate(transform, catapult.getPosition());

        float wheelY = -0.2f;
        float wheelRadius = 0.18f;
        float wheelBottomOffset = -(wheelY - wheelRadius);
        transform = glm::translate(transform, glm::vec3(0.0f, initialTerrainHeight + wheelBottomOffset, 0.0f));

        glm::vec3 up(0.0f, 1.0f, 0.0f);
        glm::vec3 normal = glm::normalize(initialTerrainNormal);
        glm::vec3 axis = glm::cross(up, normal);
        float angle = acosf(glm::clamp(glm::dot(up, normal), -1.0f, 1.0f));
        if (glm::length(axis) > 0.001f && angle > 0.001f)
        {
            axis = glm::normalize(axis);
            transform = glm::rotate(transform, angle, axis);
        }

        transform = glm::rotate(transform, catapult.getRotation(), glm::vec3(0.0f, 1.0f, 0.0f));

        glm::vec3 localOffset(localOffsetX, localOffsetY, localOffsetZ);
        glm::vec3 worldOffset = glm::vec3(transform * glm::vec4(localOffset, 0.0f));

        bomb->position = bucketPos + worldOffset;
    }

    // ===== Render Loop =====
    // Initialize last rotation and rotation speed on first frame
    lastCatapultRotation = catapult.getRotation();
    currentRotationSpeed = cameraRotationFollowSpeed;

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window, terrain);

        // If we just exited free-look mode due to catapult movement, reset camera to catapult position
        static bool wasInFreeLook = false;
        if (wasInFreeLook && !freeLookMode)
        {
            // Reset camera to follow catapult immediately
            glm::vec3 catapultPos = catapult.getPosition();
            float catapultTerrainHeight = terrain.getHeight(catapultPos.x, catapultPos.z);
            glm::vec3 catapultTerrainNormal = terrain.getNormal(catapultPos.x, catapultPos.z);

            // Calculate target camera position behind catapult
            float cameraDistance = 3.9f; // Default distance
            float cameraHeight = 1.5f;

            glm::mat4 catapultTransform = glm::mat4(1.0f);
            catapultTransform = glm::translate(catapultTransform, catapultPos);

            float wheelY = -0.2f;
            float wheelRadius = 0.18f;
            float wheelBottomOffset = -(wheelY - wheelRadius);
            catapultTransform = glm::translate(catapultTransform, glm::vec3(0.0f, catapultTerrainHeight + wheelBottomOffset, 0.0f));

            glm::vec3 up(0.0f, 1.0f, 0.0f);
            glm::vec3 normal = glm::normalize(catapultTerrainNormal);
            glm::vec3 axis = glm::cross(up, normal);
            float tiltAngle = acosf(glm::clamp(glm::dot(up, normal), -1.0f, 1.0f));

            if (glm::length(axis) > 0.001f && tiltAngle > 0.001f)
            {
                axis = glm::normalize(axis);
                catapultTransform = glm::rotate(catapultTransform, tiltAngle, axis);
            }

            catapultTransform = glm::rotate(catapultTransform, catapult.getRotation(), glm::vec3(0.0f, 1.0f, 0.0f));

            glm::vec3 localCameraPos(-cameraDistance, cameraHeight, 0.0f);
            glm::vec4 transformedCameraPos = catapultTransform * glm::vec4(localCameraPos, 1.0f);
            camera.SetPosition(glm::vec3(transformedCameraPos));

            // Reset camera rotation to look at catapult
            glm::vec3 catapultCenterWorld = glm::vec3(catapultTransform * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
            glm::vec3 lookDirection = glm::normalize(catapultCenterWorld - camera.Position);
            float horizontalLength = sqrtf(lookDirection.x * lookDirection.x + lookDirection.z * lookDirection.z);
            float targetYaw = glm::degrees(atan2f(lookDirection.z, lookDirection.x));
            float clampedY = glm::clamp(lookDirection.y, -1.0f, 1.0f);
            float targetPitch = glm::degrees(asinf(clampedY));
            camera.SetRotation(targetYaw, targetPitch);

            firstMouse = true; // Reset mouse for smooth transition
        }
        wasInFreeLook = freeLookMode;

        // Get terrain information for catapult (needed for rendering and camera)
        glm::vec3 catapultPos = catapult.getPosition();
        float catapultRot = catapult.getRotation();
        float catapultTerrainHeight = terrain.getHeight(catapultPos.x, catapultPos.z);
        glm::vec3 catapultTerrainNormal = terrain.getNormal(catapultPos.x, catapultPos.z);

        if (freeLookMode)
        {
            float terrainHeightAtCamera = terrain.getHeight(camera.Position.x, camera.Position.z);
            float minCameraHeight = terrainHeightAtCamera + 0.5f;

            // Check wall collision for free-look camera (reduced radius for tighter collision)
            glm::vec3 currentCameraPos = camera.Position;
            if (terrain.checkWallCollision(currentCameraPos.x, currentCameraPos.z, 0.3f))
            {
                // Wall collision detected - resolve by pushing camera away from wall
                glm::vec3 velocity = glm::vec3(0.0f); // Not needed for resolution in this case
                glm::vec3 resolvedPos = terrain.resolveWallCollision(currentCameraPos.x, currentCameraPos.z, 0.3f, velocity);
                currentCameraPos.x = resolvedPos.x;
                currentCameraPos.z = resolvedPos.z;
            }

            if (currentCameraPos.y < minCameraHeight)
            {
                currentCameraPos.y = minCameraHeight;
            }

            camera.SetPosition(currentCameraPos);

        }
        else if (cameraFollowProjectile && bomb && (bomb->isLaunched || bomb->isAnimating))
        {
            // ===== FOLLOW PROJECTILE MODE =====
            // Camera follows the projectile until it disappears

            glm::vec3 projectilePos = bomb->isAnimating ? bomb->impactPosition : bomb->position;
            glm::vec3 projectileVel = bomb->isAnimating ? glm::vec3(0.0f) : bomb->velocity;

            // Calculate projectile direction (normalized velocity)
            glm::vec3 projectileDir;
            if (glm::length(projectileVel) > 0.1f)
            {
                projectileDir = glm::normalize(projectileVel);
            }
            else
            {
                // If velocity is too small (during animation or at rest), use direction from camera to projectile
                glm::vec3 toProjectile = projectilePos - camera.Position;
                toProjectile.y = 0.0f; // Keep horizontal
                if (glm::length(toProjectile) > 0.1f)
                {
                    projectileDir = glm::normalize(toProjectile);
                }
                else
                {
                    projectileDir = glm::vec3(0.0f, 0.0f, -1.0f); // Default: forward
                }
            }

            // Position camera behind projectile (opposite of velocity direction)
            glm::vec3 cameraOffset = -projectileDir * projectileFollowDistance; // Behind projectile
            cameraOffset.y += projectileFollowHeight;                           // Above projectile

            glm::vec3 targetCameraPos = projectilePos + cameraOffset;

            // Ensure camera doesn't go below ground
            float terrainHeightAtCamera = terrain.getHeight(targetCameraPos.x, targetCameraPos.z);
            float minCameraHeight = terrainHeightAtCamera + 0.5f;
            if (targetCameraPos.y < minCameraHeight)
            {
                targetCameraPos.y = minCameraHeight;
            }

            glm::vec3 currentPos = camera.Position;
            float projectileFollowSpeed = cameraFollowSpeed * 2.0f; // Faster follow for projectile
            glm::vec3 newPos = glm::mix(currentPos, targetCameraPos, projectileFollowSpeed * deltaTime);
            camera.SetPosition(newPos);

            // Make camera look at projectile (or slightly ahead)
            glm::vec3 lookTarget = projectilePos + projectileVel * 0.5f; // Look slightly ahead
            glm::vec3 lookDirection = glm::normalize(lookTarget - newPos);

            // Calculate yaw and pitch
            float horizontalLength = sqrtf(lookDirection.x * lookDirection.x + lookDirection.z * lookDirection.z);
            float targetYaw = glm::degrees(atan2f(lookDirection.z, lookDirection.x));
            float clampedY = glm::clamp(lookDirection.y, -1.0f, 1.0f);
            float targetPitch = glm::degrees(asinf(clampedY));

            // Normalize yaw
            while (targetYaw > 180.0f)
                targetYaw -= 360.0f;
            while (targetYaw < -180.0f)
                targetYaw += 360.0f;

            // Smoothly interpolate camera rotation
            if (!mousePressed)
            {
                float currentYaw = camera.Yaw;
                float yawDiff = targetYaw - currentYaw;
                while (yawDiff > 180.0f)
                    yawDiff -= 360.0f;
                while (yawDiff < -180.0f)
                    yawDiff += 360.0f;

                float currentPitch = camera.Pitch;
                float pitchDiff = targetPitch - currentPitch;

                float newYaw = currentYaw + yawDiff * cameraRotationFollowSpeedFast * deltaTime;
                float newPitch = currentPitch + pitchDiff * cameraRotationFollowSpeedFast * deltaTime;

                camera.SetRotation(newYaw, newPitch);
            }

            // Set zoom for projectile follow
            camera.SetZoom(projectileFollowZoom);
        }
        else
        {
            // Define camera parameters for each mode
            float cameraDistance = 0.0f;
            float cameraHeight = 0.0f;
            float cameraZoom = 45.0f;

            switch (currentCameraMode)
            {
            case ZOOM_OUT_1:
                cameraDistance = 3.9f; // Medium distance
                cameraHeight = 1.5f;
                cameraZoom = 70.0f;
                break;
            case ZOOM_OUT_2:
                cameraDistance = 8.0f; // Far distance
                cameraHeight = 3.0f;
                cameraZoom = 50.0f;
                break;
            case ZOOM_OUT_3:
                cameraDistance = 12.0f; // Very far distance
                cameraHeight = 4.5f;
                cameraZoom = 45.0f;
                break;
            }

            glm::mat4 catapultTransform = glm::mat4(1.0f);
            catapultTransform = glm::translate(catapultTransform, catapultPos);

            float wheelY = -0.2f;
            float wheelRadius = 0.18f;
            float wheelBottomOffset = -(wheelY - wheelRadius);
            catapultTransform = glm::translate(catapultTransform, glm::vec3(0.0f, catapultTerrainHeight + wheelBottomOffset, 0.0f));

            // Rotate to match terrain slope (same as catapult)
            glm::vec3 up(0.0f, 1.0f, 0.0f);
            glm::vec3 normal = glm::normalize(catapultTerrainNormal);
            glm::vec3 axis = glm::cross(up, normal);
            float tiltAngle = acosf(glm::clamp(glm::dot(up, normal), -1.0f, 1.0f));

            if (glm::length(axis) > 0.001f && tiltAngle > 0.001f)
            {
                axis = glm::normalize(axis);
                catapultTransform = glm::rotate(catapultTransform, tiltAngle, axis);
            }

            // Rotate around Y axis for steering
            catapultTransform = glm::rotate(catapultTransform, catapultRot, glm::vec3(0.0f, 1.0f, 0.0f));

            glm::vec3 localCameraPos(-cameraDistance, cameraHeight, 0.0f);

            // Transform camera position using catapult's full transformation
            glm::vec4 transformedCameraPos = catapultTransform * glm::vec4(localCameraPos, 1.0f);
            targetCameraPosition = glm::vec3(transformedCameraPos);

            // Check if target camera position would collide with a wall (reduced radius for tighter collision)
            if (terrain.checkWallCollision(targetCameraPosition.x, targetCameraPosition.z, 0.3f))
            {
                // If collision detected, try to move camera closer to catapult incrementally
                bool foundValidPosition = false;

                // Try reducing distance in steps until we find a valid position
                for (float testDistance = cameraDistance * 0.9f; testDistance > 0.5f; testDistance *= 0.9f)
                {
                    glm::vec3 testLocalCameraPos(-testDistance, cameraHeight, 0.0f);
                    glm::vec4 testTransformedPos = catapultTransform * glm::vec4(testLocalCameraPos, 1.0f);
                    glm::vec3 testCameraPos = glm::vec3(testTransformedPos);

                    if (!terrain.checkWallCollision(testCameraPos.x, testCameraPos.z, 0.3f))
                    {
                        targetCameraPosition = testCameraPos;
                        foundValidPosition = true;
                        break;
                    }
                }

                // If still no valid position, just place camera at catapult position + height
                if (!foundValidPosition)
                {
                    targetCameraPosition = catapultPos + glm::vec3(0.0f, cameraHeight, 0.0f);
                }
            }

            // Calculate catapult center position in world space (with terrain transformations)
            glm::vec3 catapultCenterLocal(0.0f, 0.0f, 0.0f); // Center of catapult in local space
            glm::vec4 transformedCatapultCenter = catapultTransform * glm::vec4(catapultCenterLocal, 1.0f);
            glm::vec3 catapultCenterWorld = glm::vec3(transformedCatapultCenter);

            // Calculate a point slightly ahead of catapult center for camera to look at
            glm::vec3 lookAheadOffset = glm::normalize(glm::vec3(catapultTransform * glm::vec4(1.0f, 0.0f, 0.0f, 0.0f))) * 1.0f;
            glm::vec3 lookTarget = catapultCenterWorld + lookAheadOffset;

            // Smoothly interpolate camera position towards target
            glm::vec3 currentPos = camera.Position;
            glm::vec3 newPos = glm::mix(currentPos, targetCameraPosition, cameraFollowSpeed * deltaTime);

            // Collision detection: prevent camera from going below ground
            float terrainHeightAtCamera = terrain.getHeight(newPos.x, newPos.z);
            float minCameraHeight = terrainHeightAtCamera + 0.5f; // Keep camera at least 0.5 units above ground
            if (newPos.y < minCameraHeight)
            {
                newPos.y = minCameraHeight;
            }

            camera.SetPosition(newPos);

            glm::vec3 lookDirection = glm::normalize(lookTarget - newPos);

            float horizontalLength = sqrtf(lookDirection.x * lookDirection.x + lookDirection.z * lookDirection.z);

            targetCameraYaw = glm::degrees(atan2f(lookDirection.z, lookDirection.x));

            float clampedY = glm::clamp(lookDirection.y, -1.0f, 1.0f);
            targetCameraPitch = glm::degrees(asinf(clampedY));

            // Normalize yaw to [-180, 180] range
            while (targetCameraYaw > 180.0f)
                targetCameraYaw -= 360.0f;
            while (targetCameraYaw < -180.0f)
                targetCameraYaw += 360.0f;

            // Smoothly interpolate camera rotation towards target (only if not using mouse)
            // If mouse is being used, let it control the rotation
            if (!mousePressed)
            {
                float currentYaw = camera.Yaw;
                // Normalize angles to [-180, 180] range for smooth interpolation
                float yawDiff = targetCameraYaw - currentYaw;
                while (yawDiff > 180.0f)
                    yawDiff -= 360.0f;
                while (yawDiff < -180.0f)
                    yawDiff += 360.0f;

                // Check if catapult is rotating (left/right movement)
                float rotationChange = fabsf(catapultRot - lastCatapultRotation);
                float targetRotationSpeed = cameraRotationFollowSpeed;

                // Use a threshold to detect rotation and add some deadzone to prevent jitter
                if (rotationChange > 0.005f)
                {
                    targetRotationSpeed = cameraRotationFollowSpeedFast;
                }

                float speedDiff = targetRotationSpeed - currentRotationSpeed;
                currentRotationSpeed += speedDiff * rotationSpeedTransitionRate * deltaTime;

                // Clamp to valid range
                if (currentRotationSpeed < cameraRotationFollowSpeed)
                    currentRotationSpeed = cameraRotationFollowSpeed;
                if (currentRotationSpeed > cameraRotationFollowSpeedFast)
                    currentRotationSpeed = cameraRotationFollowSpeedFast;

                float newYaw = currentYaw + yawDiff * currentRotationSpeed * deltaTime;

                // Update last rotation for next frame
                lastCatapultRotation = catapultRot;

                // Interpolate pitch to match terrain tilt
                float currentPitch = camera.Pitch;
                float pitchDiff = targetCameraPitch - currentPitch;
                float newPitch = currentPitch + pitchDiff * cameraRotationFollowSpeed * deltaTime;

                camera.SetRotation(newYaw, newPitch);
            }
            // If mouse is pressed, mouse controls rotation (already handled in mouse_callback)

            // Set camera zoom
            camera.SetZoom(cameraZoom);
        }

        // Clear buffers
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 view = camera.GetViewMatrix();
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)width / (float)height, 0.1f, 100.0f);

        // ===== Draw Skybox First =====
        skybox.Draw(view, projection, deltaTime);

        glUseProgram(shaderProgram);

        unsigned int modelLoc = glGetUniformLocation(shaderProgram, "model");
        unsigned int viewLoc = glGetUniformLocation(shaderProgram, "view");
        unsigned int projLoc = glGetUniformLocation(shaderProgram, "projection");
        unsigned int colorLoc = glGetUniformLocation(shaderProgram, "objectColor");

        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

        glUniform1i(glGetUniformLocation(shaderProgram, "useTexture"), 0);
        glUniform1i(glGetUniformLocation(shaderProgram, "texture_diffuse1"), 0);
        glActiveTexture(GL_TEXTURE0);
        // Bind default white texture to avoid "unloadable texture" errors
        glBindTexture(GL_TEXTURE_2D, defaultWhiteTexture);

        // Sun lighting uniforms
        glm::vec3 sunDirection = glm::normalize(sunPosition);
        glm::vec3 sunColor = glm::vec3(1.0f, 0.95f, 0.8f);
        glUniform3fv(glGetUniformLocation(shaderProgram, "sunDirection"), 1, glm::value_ptr(sunDirection));
        glUniform3fv(glGetUniformLocation(shaderProgram, "sunColor"), 1, glm::value_ptr(sunColor));
        glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, glm::value_ptr(camera.Position));
        glUniform3fv(glGetUniformLocation(shaderProgram, "sunPos"), 1, glm::value_ptr(sunPosition));

        // ===== Update Catapult Animation =====
        catapult.update(deltaTime);

        // ===== Update Zombies =====
        static std::map<Zombie *, float> zombieAttackCooldowns;
        const float attackInterval = 1.0f;

        for (auto *zombie : zombies)
        {
            if (zombie && zombie->isAlive())
            {
                // Update cooldown for this zombie
                if (zombieAttackCooldowns.find(zombie) == zombieAttackCooldowns.end())
                {
                    zombieAttackCooldowns[zombie] = 0.0f;
                }
                zombieAttackCooldowns[zombie] -= deltaTime;
                if (zombieAttackCooldowns[zombie] < 0.0f)
                {
                    zombieAttackCooldowns[zombie] = 0.0f;
                }

                // Calculate distance to catapult for behavior decisions
                glm::vec3 zombiePos = zombie->getPosition();
                float distanceToCatapult = glm::length(catapultPos - zombiePos);

                // Get terrain height at zombie position and apply gravity
                float terrainHeight = terrain.getHeight(zombiePos.x, zombiePos.z);

                // Store old position for collision check
                glm::vec3 oldZombiePos = zombiePos;
                zombie->update(deltaTime, catapultPos, terrainHeight, distanceToCatapult);
                glm::vec3 newZombiePos = zombie->getPosition();

                // Check if zombie is attacking and apply damage
                if (zombie->isAttacking() && distanceToCatapult <= 2.5f && zombieAttackCooldowns[zombie] <= 0.0f && catapult.isAlive())
                {
                    float damage = zombie->getIsBoss() ? 20.0f : 10.0f;
                    catapult.takeDamage(damage);
                    zombieAttackCooldowns[zombie] = attackInterval; // Reset cooldown for this zombie
                    std::cout << "Catapult took " << damage << " damage! Health: " << catapult.getHealth() << "/" << catapult.getMaxHealth() << std::endl;
                }

                if (terrain.checkTreeCollision(newZombiePos.x, newZombiePos.z, 0.25f) ||
                    terrain.checkWallCollision(newZombiePos.x, newZombiePos.z, 0.25f))
                {
                    // Try to go around the obstacle
                    glm::vec3 direction = catapultPos - oldZombiePos;
                    direction.y = 0.0f;
                    float dist = glm::length(direction);

                    if (dist > 0.1f)
                    {
                        direction = glm::normalize(direction);

                        // Try moving perpendicular (90 degrees) to avoid obstacle
                        // Try left first
                        glm::vec3 perpLeft = glm::vec3(-direction.z, 0.0f, direction.x);
                        glm::vec3 tryPosLeft = oldZombiePos + perpLeft * zombie->getSpeed() * deltaTime;

                        if (!terrain.checkTreeCollision(tryPosLeft.x, tryPosLeft.z, 0.25f) &&
                            !terrain.checkWallCollision(tryPosLeft.x, tryPosLeft.z, 0.25f))
                        {
                            zombie->setPosition(glm::vec3(tryPosLeft.x, terrainHeight, tryPosLeft.z));
                        }
                        else
                        {
                            glm::vec3 perpRight = glm::vec3(direction.z, 0.0f, -direction.x);
                            glm::vec3 tryPosRight = oldZombiePos + perpRight * zombie->getSpeed() * deltaTime;

                            if (!terrain.checkTreeCollision(tryPosRight.x, tryPosRight.z, 0.25f) &&
                                !terrain.checkWallCollision(tryPosRight.x, tryPosRight.z, 0.25f))
                            {
                                zombie->setPosition(glm::vec3(tryPosRight.x, terrainHeight, tryPosRight.z));
                            }
                            else
                            {
                                zombie->setPosition(glm::vec3(oldZombiePos.x, terrainHeight, oldZombiePos.z));
                            }
                        }
                    }
                    else
                    {
                        zombie->setPosition(glm::vec3(oldZombiePos.x, terrainHeight, oldZombiePos.z));
                    }
                }
            }
        }

        // Check if catapult health reached 0 - respawn everything
        if (!catapult.isAlive())
        {
            respawnEverything();
            zombieAttackCooldowns.clear(); // Reset attack cooldowns
        }

        // Check if all zombies are dead - respawn everything
        bool allZombiesDead = true;
        for (auto *zombie : zombies)
        {
            if (zombie && zombie->isAlive())
            {
                allZombiesDead = false;
                break;
            }
        }
        if (allZombiesDead && zombies.size() > 0)
        {
            respawnEverything();
            zombieAttackCooldowns.clear(); // Reset attack cooldowns
        }

        // Clean up cooldown map for deleted zombies
        auto it = zombieAttackCooldowns.begin();
        while (it != zombieAttackCooldowns.end())
        {
            bool found = false;
            for (auto *zombie : zombies)
            {
                if (zombie == it->first)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                it = zombieAttackCooldowns.erase(it);
            }
            else
            {
                ++it;
            }
        }

        // Check if space was pressed to fire catapult
        static bool spacePressedLastFrame = false;
        bool spacePressedThisFrame = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
        if (spacePressedThisFrame && !spacePressedLastFrame && !catapult.isFiring() && bomb && !bomb->isLaunched)
        {
            catapult.fire();
        }
        spacePressedLastFrame = spacePressedThisFrame;

        // Check if projectile has finished (disappeared) - switch back to catapult follow
        // Wait until both launched is false AND animation is finished
        if (cameraFollowProjectile && bomb && !bomb->isLaunched && !bomb->isAnimating)
        {
            cameraFollowProjectile = false; // Switch back to following catapult
        }

        // ===== Draw Terrain =====
        glUniform3f(colorLoc, 0.4f, 0.3f, 0.2f); // Brown/mud color as fallback
        terrain.draw(shaderProgram);

        // ===== Draw Catapult =====
        glUniform1i(glGetUniformLocation(shaderProgram, "useTexture"), 0);
        glUniform1i(glGetUniformLocation(shaderProgram, "texture_diffuse1"), 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, defaultWhiteTexture);
        // Use terrain height and normal for catapult (gravity applied, tilts with slope)
        catapult.draw(shaderProgram, catapultTerrainHeight, catapultTerrainNormal);

        // ===== Update & Draw Bomb =====
        if (bomb)
        {
            if (!bomb->isLaunched)
            {
                // Get bucket position with all terrain transformations applied
                float catapultTerrainHeight = terrain.getHeight(catapult.getPosition().x, catapult.getPosition().z);
                glm::vec3 catapultTerrainNormal = terrain.getNormal(catapult.getPosition().x, catapult.getPosition().z);
                glm::vec3 bucketPos = catapult.getBucketPositionWorld(catapultTerrainHeight, catapultTerrainNormal);

                // Apply bomb offset relative to bucket (already in world space with all rotations)
                float armAngle = catapult.getArmAngle();
                float cos_a = cosf(armAngle);
                float sin_a = sinf(armAngle);

                // Rotate offsets by arm angle (local to arm)
                float localOffsetX = -bomb->bucketOffsetX * cos_a - bomb->bucketOffsetY * sin_a;
                float localOffsetY = -bomb->bucketOffsetX * sin_a + bomb->bucketOffsetY * cos_a;
                float localOffsetZ = bomb->bucketOffsetZ;

                // Build same transformation matrix as bucket to transform offset
                glm::mat4 transform = glm::mat4(1.0f);
                transform = glm::translate(transform, catapult.getPosition());

                float wheelY = -0.2f;
                float wheelRadius = 0.18f;
                float wheelBottomOffset = -(wheelY - wheelRadius);
                transform = glm::translate(transform, glm::vec3(0.0f, catapultTerrainHeight + wheelBottomOffset, 0.0f));

                // Rotate to match terrain slope
                glm::vec3 up(0.0f, 1.0f, 0.0f);
                glm::vec3 normal = glm::normalize(catapultTerrainNormal);
                glm::vec3 axis = glm::cross(up, normal);
                float angle = acosf(glm::clamp(glm::dot(up, normal), -1.0f, 1.0f));
                if (glm::length(axis) > 0.001f && angle > 0.001f)
                {
                    axis = glm::normalize(axis);
                    transform = glm::rotate(transform, angle, axis);
                }

                // Rotate around Y axis for steering
                transform = glm::rotate(transform, catapult.getRotation(), glm::vec3(0.0f, 1.0f, 0.0f));

                // Transform offset
                glm::vec3 localOffset(localOffsetX, localOffsetY, localOffsetZ);
                glm::vec3 worldOffset = glm::vec3(transform * glm::vec4(localOffset, 0.0f));

                // Apply offset to bucket position
                bomb->position = bucketPos + worldOffset;

                if (catapult.isFiring() && catapult.getArmAngle() < -0.95f && !bomb->isLaunched)
                {

                    float launchSpeed = catapult.getLaunchSpeed();
                    float armAngle = catapult.getArmAngle();

                    float tangentAngle = armAngle - 3.1415926535f / 2.0f;

                    glm::vec3 localLaunchVel(
                        -launchSpeed * cosf(tangentAngle),
                        -launchSpeed * sinf(tangentAngle),
                        0.0f);

                    // Rotate launch velocity by catapult rotation
                    float catapultRotation = catapult.getRotation();
                    glm::mat4 rotationMatrix = glm::rotate(glm::mat4(1.0f), catapultRotation, glm::vec3(0.0f, 1.0f, 0.0f));
                    glm::vec3 rotatedLaunchVel = glm::vec3(rotationMatrix * glm::vec4(localLaunchVel, 0.0f));

                    bomb->launch(rotatedLaunchVel);

                    // Switch camera to follow projectile mode when projectile actually launches
                    if (!freeLookMode)
                    {
                        cameraFollowProjectile = true; // Start following projectile
                    }
                }
            }
            else
            {
                bomb->update(deltaTime, &terrain);

                // Apply damage to zombies when projectile hits (only once)
                if (bomb->hasHit && !bomb->damageApplied)
                {
                    // Apply damage to all zombies within damage radius
                    glm::vec3 impactPos = bomb->impactPosition;

                    for (auto *zombie : zombies)
                    {
                        if (zombie && zombie->isAlive())
                        {
                            glm::vec3 zombiePos = zombie->getPosition();
                            float distance = glm::length(glm::vec3(zombiePos.x - impactPos.x, 0.0f, zombiePos.z - impactPos.z));

                            float damage = bomb->calculateDamage(distance);
                            if (damage > 0.0f)
                            {
                                zombie->takeDamage(damage);
                                std::cout << "Zombie took " << damage << " damage! Health: " << zombie->getHealth() << "/" << zombie->getMaxHealth() << std::endl;
                            }
                        }
                    }

                    // Mark damage as applied so we don't apply it multiple times
                    bomb->damageApplied = true;
                }

                // Auto-reset catapult when projectile lands and animation finishes
                if (!bomb->isLaunched && !bomb->isAnimating && !catapult.isFiring())
                {
                    catapult.reset();
                    bomb->isLaunched = false;
                    bomb->hasHit = false;
                    bomb->damageApplied = false;
                }
            }

            // Reset texture state for bomb
            glUniform1i(glGetUniformLocation(shaderProgram, "useTexture"), 0);
            glUniform1i(glGetUniformLocation(shaderProgram, "texture_diffuse1"), 0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, defaultWhiteTexture);
            glUniform3f(colorLoc, 0.35f, 0.3f, 0.25f);
            bomb->draw(shaderProgram);
        }

        // ===== Draw Zombies =====
        for (auto *zombie : zombies)
        {
            if (zombie && zombie->isAlive())
            {
                zombie->draw(shaderProgram);
            }
        }

        // ===== Draw Health Bar =====
        renderHealthBar(window, shaderProgram, catapult.getHealth(), catapult.getMaxHealth(), projection, view);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup zombies
    for (auto *zombie : zombies)
    {
        delete zombie;
    }
    zombies.clear();

    glfwTerminate();
    return 0;
}
