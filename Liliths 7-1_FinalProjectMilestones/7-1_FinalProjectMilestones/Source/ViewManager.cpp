///////////////////////////////////////////////////////////////////////////////
// viewmanager.cpp
// ============
// manage the viewing of 3D objects within the viewport
//
//  AUTHOR: Brian Battersby - SNHU Instructor / Computer Science
//  Created for CS-330-Computational Graphics and Visualization, Nov. 1st, 2023
///////////////////////////////////////////////////////////////////////////////

#include "ViewManager.h"

#include <iostream>

// GLM Math Header inclusions
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>    

// declaration of the global variables and defines
namespace
{
    // Window size (fixed for this assignment; resize handling optional)
    const int WINDOW_WIDTH = 1000;
    const int WINDOW_HEIGHT = 800;

    // Uniform names
    const char* g_ViewName = "view";
    const char* g_ProjectionName = "projection";

    // Camera used to view/interact with the 3D scene
    Camera* g_pCamera = nullptr;

    // Mouse tracking
    float gLastX = WINDOW_WIDTH * 0.5f;
    float gLastY = WINDOW_HEIGHT * 0.5f;
    bool  gFirstMouse = true;

    // Frame timing
    float gDeltaTime = 0.0f;
    float gLastFrame = 0.0f;

    // Ortho vs Perspective
    bool bOrthographicProjection = false;

    // Movement speed control (adjusted by mouse scroll)
    float gMoveSpeed = 2.5f;          // base m/s
    const float kMinSpeed = 0.5f;
    const float kMaxSpeed = 15.0f;

    // Debounce for projection toggles
    bool gWasPDown = false;
    bool gWasODown = false;

    // Save/restore perspective camera when entering/leaving Ortho
    bool  gSavedCamValid = false;
    glm::vec3 gSavedPos{}, gSavedFront{}, gSavedUp{};
}

/***********************************************************
 *  ViewManager()
 *
 *  The constructor for the class
 ***********************************************************/
ViewManager::ViewManager(ShaderManager* pShaderManager)
{
    // initialize the member variables
    m_pShaderManager = pShaderManager;
    m_pWindow = NULL;

    // Camera with a seated/desk vantage
    g_pCamera = new Camera();
    g_pCamera->Position = glm::vec3(0.0f, 0.5f, 2.0f);
    g_pCamera->Front = glm::vec3(0.0f, -0.15f, -1.0f);
    g_pCamera->Up = glm::vec3(0.0f, 1.0f, 0.0f);
    g_pCamera->Zoom = 45.0f; // FOV for perspective
}

/***********************************************************
 *  ~ViewManager()
 *
 *  The destructor for the class
 ***********************************************************/
ViewManager::~ViewManager()
{
    m_pShaderManager = NULL;
    m_pWindow = NULL;
    if (NULL != g_pCamera)
    {
        delete g_pCamera;
        g_pCamera = NULL;
    }
}

/***********************************************************
 *  CreateDisplayWindow()
 *
 *  This method is used to create the main display window.
 ***********************************************************/
GLFWwindow* ViewManager::CreateDisplayWindow(const char* windowTitle)
{
    GLFWwindow* window = glfwCreateWindow(
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        windowTitle,
        NULL, NULL);

    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return NULL;
    }
    glfwMakeContextCurrent(window);

    // OPTIONAL: capture cursor for FPS-style look
    // glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // callbacks
    glfwSetCursorPosCallback(window, &ViewManager::Mouse_Position_Callback);
    glfwSetScrollCallback(window, &ViewManager::Mouse_Scroll_Callback);

    // enable blending for transparent rendering
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_pWindow = window;
    return window;
}

/***********************************************************
 *  Mouse_Position_Callback()
 *
 *  Called by GLFW whenever the mouse moves in the window.
 *  Implements FPS-style look (yaw/pitch).
 ***********************************************************/
void ViewManager::Mouse_Position_Callback(GLFWwindow* /*window*/, double xMousePos, double yMousePos)
{
    float xpos = static_cast<float>(xMousePos);
    float ypos = static_cast<float>(yMousePos);

    if (gFirstMouse)
    {
        gLastX = xpos;
        gLastY = ypos;
        gFirstMouse = false;
    }

    float xoffset = xpos - gLastX;
    float yoffset = gLastY - ypos; // reversed: y ranges top->bottom
    gLastX = xpos;
    gLastY = ypos;

    if (g_pCamera)
    {
        // Delegate to typical learnopengl-style camera
        g_pCamera->ProcessMouseMovement(xoffset, yoffset);
    }
}

/***********************************************************
 *  Mouse_Scroll_Callback()
 *
 *  Called by GLFW whenever the scroll wheel moves.
 *  Here it adjusts movement speed (not FOV).
 ***********************************************************/
void ViewManager::Mouse_Scroll_Callback(GLFWwindow* /*window*/, double /*xoffset*/, double yoffset)
{
    // Increase/decrease speed smoothly, clamp to a friendly range
    gMoveSpeed += static_cast<float>(yoffset) * 0.5f;
    if (gMoveSpeed < kMinSpeed) gMoveSpeed = kMinSpeed;
    if (gMoveSpeed > kMaxSpeed) gMoveSpeed = kMaxSpeed;

    // If your Camera exposes MovementSpeed, keep it in sync
    if (g_pCamera)
    {
        g_pCamera->MovementSpeed = gMoveSpeed;
    }
}

/***********************************************************
 *  ProcessKeyboardEvents()
 *
 *  Poll keyboard for movement and projection toggles.
 ***********************************************************/
void ViewManager::ProcessKeyboardEvents()
{
    // Close on ESC
    if (glfwGetKey(m_pWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(m_pWindow, true);
    }

    if (!g_pCamera) return;

    // --- Basic 6-DOF translation with WASD + QE ---
    // Scale by delta time *and* gMoveSpeed so scroll changes travel rate.
    float dt = gDeltaTime;
    // If your Camera uses its own MovementSpeed, passing dt is fine.
    // Otherwise multiply dt by gMoveSpeed to get a world-speed effect.
    // We'll do both to be robust:
    float dtSpeed = dt * (gMoveSpeed / 2.5f); // normalize against base 2.5

    if (glfwGetKey(m_pWindow, GLFW_KEY_W) == GLFW_PRESS)
        g_pCamera->ProcessKeyboard(FORWARD, dtSpeed);
    if (glfwGetKey(m_pWindow, GLFW_KEY_S) == GLFW_PRESS)
        g_pCamera->ProcessKeyboard(BACKWARD, dtSpeed);
    if (glfwGetKey(m_pWindow, GLFW_KEY_A) == GLFW_PRESS)
        g_pCamera->ProcessKeyboard(LEFT, dtSpeed);
    if (glfwGetKey(m_pWindow, GLFW_KEY_D) == GLFW_PRESS)
        g_pCamera->ProcessKeyboard(RIGHT, dtSpeed);

    // Vertical (up/down) with Q/E
    if (glfwGetKey(m_pWindow, GLFW_KEY_Q) == GLFW_PRESS)
        g_pCamera->ProcessKeyboard(UP, dtSpeed);
    if (glfwGetKey(m_pWindow, GLFW_KEY_E) == GLFW_PRESS)
        g_pCamera->ProcessKeyboard(DOWN, dtSpeed);

    // --- Projection toggles with debounce (tap O/P) ---
    bool pDown = glfwGetKey(m_pWindow, GLFW_KEY_P) == GLFW_PRESS;
    bool oDown = glfwGetKey(m_pWindow, GLFW_KEY_O) == GLFW_PRESS;

    // P => Perspective
    if (pDown && !gWasPDown)
    {
        bOrthographicProjection = false;

        // Restore saved perspective camera, if we had switched from it
        if (gSavedCamValid)
        {
            g_pCamera->Position = gSavedPos;
            g_pCamera->Front = gSavedFront;
            g_pCamera->Up = gSavedUp;
            gSavedCamValid = false;
        }
    }
    gWasPDown = pDown;

    // O => Orthographic
    if (oDown && !gWasODown)
    {
        if (!bOrthographicProjection)
        {
            // Save current perspective camera to restore later
            gSavedCamValid = true;
            gSavedPos = g_pCamera->Position;
            gSavedFront = g_pCamera->Front;
            gSavedUp = g_pCamera->Up;
        }

        bOrthographicProjection = true;

        // Set an inspection camera that looks straight at the scene’s origin.
        // This ensures the bottom plane does not show in orthographic view.
        g_pCamera->Position = glm::vec3(0.0f, 0.0f, 3.0f);
        g_pCamera->Front = glm::vec3(0.0f, 0.0f, -1.0f);
        g_pCamera->Up = glm::vec3(0.0f, 1.0f, 0.0f);
    }
    gWasODown = oDown;
}

/***********************************************************
 *  PrepareSceneView()
 *
 *  Build the per-frame view and projection matrices.
 ***********************************************************/
void ViewManager::PrepareSceneView()
{
    glm::mat4 view;
    glm::mat4 projection;

    // Per-frame timing
    float currentFrame = static_cast<float>(glfwGetTime());
    gDeltaTime = currentFrame - gLastFrame;
    gLastFrame = currentFrame;

    // Keyboard handling (movement + toggles)
    ProcessKeyboardEvents();

    // View from camera
    view = g_pCamera->GetViewMatrix();

    // Projection selection
    float aspect = static_cast<float>(WINDOW_WIDTH) / static_cast<float>(WINDOW_HEIGHT);
    if (!bOrthographicProjection)
    {
        // Perspective projection
        projection = glm::perspective(glm::radians(g_pCamera->Zoom), aspect, 0.1f, 100.0f);
    }
    else
    {
        // Orthographic “inspection” box sized to keep object in frame
        // Tune half-height to frame your build
        const float halfHeight = 1.2f;
        const float halfWidth = halfHeight * aspect;

        // Using a typical right-handed clip space with positive far
        projection = glm::ortho(-halfWidth, halfWidth, -halfHeight, halfHeight, 0.1f, 100.0f);
    }

    // Push matrices and camera position to shader
    if (NULL != m_pShaderManager)
    {
        m_pShaderManager->setMat4Value(g_ViewName, view);
        m_pShaderManager->setMat4Value(g_ProjectionName, projection);
        m_pShaderManager->setVec3Value("viewPosition", g_pCamera->Position);
    }
}
