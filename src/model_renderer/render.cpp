#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <filesystem>
#include <iostream>

#include <plog/Log.h>
#include <plog/Initializers/RollingFileInitializer.h>

#include "shader.hpp"
#include "mesh.hpp"
#include "camera.hpp"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <config.h>
#include "render.hpp"

int width = 1600, height = 900;
float lastTime;
Camera *camera = nullptr;

bool showLoader = false;
float loaderProgress = 0.0f;
int inputValue = 0;

bool renderModel = false;

// List of available models
std::vector<std::string> modelPaths;
std::string selectedModelPath;

// Variables for model selection
int selectedModelIndex = -1;   // Default: no model selected
Model *currentModel = nullptr; // Pointer to the currently loaded model

void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void cursor_position_callback(GLFWwindow *window, double x, double y);
void scroll_callback(GLFWwindow *window, double x, double y);
void handle_keyboard(GLFWwindow *window, float deltaTime);

// Function to populate modelPaths
void populateModelPaths(const std::string &directory)
{
    try
    {
        // Iterate through the directory
        for (const auto &entry : std::filesystem::directory_iterator(directory))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".obj")
            {
                modelPaths.push_back(entry.path().string()); // Add the .obj file to the list
            }
        }
    }
    catch (const std::filesystem::filesystem_error &e)
    {
        std::cerr << "Error reading directory: " << e.what() << std::endl;
    }
}

Render::Render(std::function<void(Render &, const std::string &)> segmentationCallback)
    : segmentationCallback(std::move(segmentationCallback))
{
    std::cout << "Shader program initialized!" << std::endl;
}

void Render::renderFile(const std::string &fileName)
{
    selectedModelPath = fileName;
    renderModel = true; // Switch UI to render state

    loadAndRenderModel(selectedModelPath);

    std::cout << "Rendering segmented file: " << fileName << std::endl;
}

void Render::start()
{
    // Directory to search for .obj files from the main resource folder
    std::string modelsDirectory = MODEL_DIR;

    // Populate the modelPaths vector
    populateModelPaths(modelsDirectory);

    // Debug: Print loaded model paths
    for (const auto &model : modelPaths)
    {
        std::cout << "Loaded model: " << model << std::endl;
    }

    plog::init(plog::warning);

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    // Create window
    GLFWwindow *window = glfwCreateWindow(width, height, "Mesh Viewer", nullptr, nullptr);
    if (!window)
    {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return;
    }
    glfwMakeContextCurrent(window);

    // Init glad
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return;
    }

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // ImGui Initialization
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    program = new ShaderProgram(SHADER_DIR "/common.vert", SHADER_DIR "/phong.frag");

    camera = new ModelRotationCamera({0.0f, 10.0f, 0.0f}, 20.0f);
    camera->setCenter(glm::vec3(0.0f, 0.0f, 0.0f));

    lastTime = glfwGetTime();

    glm::vec3 lightPos(0.0f, 10.0f, 5.0f),
        lightColor(3.0f, 3.0f, 1.0f),
        lightAmbient(1.0f, 1.0f, 1.0f);

    glEnable(GL_DEPTH_TEST);

    bool shouldClose = false;

    try
    {
        while (!glfwWindowShouldClose(window) && !shouldClose)
        {
            // Clear the screen
            glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // ImGui Frame Initialization
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            // ImGui Model Viewer UI
            ImGui::Begin("Model Viewer");

            // If the model is rendered, show the model and control elements
            if (renderModel && currentModel)
            {
                // Step 1: Render the model (existing rendering logic)
                float nowTime = glfwGetTime();
                float deltaTime = nowTime - lastTime;
                lastTime = nowTime;
                handle_keyboard(window, deltaTime);

                camera->setCenter(glm::vec3(0.0f, 0.0f, 0.0f));
                glm::vec3 modelCenter = currentModel->getBoundingBoxCenter();
                glm::mat4 model = glm::translate(glm::identity<glm::mat4>(), -modelCenter);
                glm::mat4 view = camera->getViewMatrix();
                glm::mat4 projection = glm::perspective(glm::radians(camera->getFOV()),
                                                        (float)width / height, 0.1f, 100.0f);

                if (program)
                {
                    program->setMVPMatrices(model, view, projection);
                    program->setVec3("eyePos", camera->getPosition());
                    program->setVec3("light.position", lightPos);
                    program->setVec3("light.ambient", lightAmbient);
                    program->setVec3("light.diffuse", lightColor);
                    program->setVec3("light.specular", lightColor);
                }

                currentModel->draw();

                // Step 2: Numerical Input for the task (e.g., number of iterations or parameters)
                ImGui::InputInt("Enter Value", &inputValue);

                // Step 3: Start Button to trigger loading process
                if (ImGui::Button("Start"))
                {
                    showLoader = true;     // Show loader flag
                    loaderProgress = 0.0f; // Reset progress
                    segmentationCallback(*this, selectedModelPath);
                }

                // Step 4: Back Button to return to model selection
                if (ImGui::Button("Back"))
                {
                    renderModel = false;
                    currentModel = nullptr;
                    selectedModelIndex = -1;
                    selectedModelPath.clear();

                    modelPaths.clear();            // Clear the current list
                    populateModelPaths(MODEL_DIR); // Reload models in the table on back
                }

                // Step 5: Show loader/progress bar while processing
                if (showLoader)
                {
                    ImGui::Text("Processing...");

                    // Show a progress bar
                    ImGui::ProgressBar(loaderProgress, ImVec2(0.0f, 0.0f));

                    if (loaderProgress < 1.0f)
                    {
                        loaderProgress += 0.01f;
                    }
                    else
                    {
                        showLoader = false;
                    }
                }
            }
            else
            {
                // Model Selector UI (grid view or list view)
                ImGui::Text("Select a Model to Load:");

                if (ImGui::BeginTable("ModelGrid", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
                {
                    for (int i = 0; i < modelPaths.size(); ++i)
                    {
                        // Start a new column
                        ImGui::TableNextColumn();

                        // Extract the file name from the path
                        std::string fileName = std::filesystem::path(modelPaths[i]).filename().string();

                        // Center the selectable button in the cell
                        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetColumnWidth() - 100) / 2); // Adjust width (100)

                        // Create a selectable button with a fixed size for each file
                        if (ImGui::Selectable(fileName.c_str(), selectedModelIndex == i, 0, ImVec2(100, 50)))
                        {
                            selectedModelIndex = i;
                            selectedModelPath = modelPaths[i];
                        }
                    }

                    ImGui::EndTable();
                }

                // Render Button
                if (ImGui::Button("Render") && (selectedModelIndex != -1 || (!selectedModelPath.empty())))
                {
                    loadAndRenderModel(selectedModelPath);
                }

                // Close Button
                if (ImGui::Button("Close"))
                {
                    shouldClose = true; // Signal to exit the application
                    glfwSetWindowShouldClose(window, true);
                }
            }

            ImGui::End();

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            glfwSwapBuffers(window);
            glfwPollEvents();
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Exception occurred: " << e.what() << std::endl;
    }
    catch (const char *msg)
    {
        std::cerr << "Error: " << msg << std::endl;
    }
    catch (...)
    {
        std::cerr << "Unknown error occurred!" << std::endl;
    }

    try
    {
        // Cleanup
        if (currentModel)
            delete currentModel;
        delete camera;

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        glfwTerminate();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Cleanup failed: " << e.what() << std::endl;
    }
    catch (const char *msg)
    {
        std::cerr << "Error during cleanup: " << msg << std::endl;
    }
    catch (...)
    {
        std::cerr << "Unknown error during cleanup!" << std::endl;
    }

    return;
}

void Render::loadAndRenderModel(const std::string &modelPath)
{
    if (currentModel)
    {
        delete currentModel; // Unload previous model
        currentModel = nullptr;
    }

    camera->resetCamera();

    currentModel = new Model(modelPath, program);
    camera->positionBasedOnObject(*currentModel);

    renderModel = true; // Ensure rendering is triggered
    std::cout << "Loaded and rendering model: " << modelPath << std::endl;
}

// Callback implementations (unchanged)
void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
    ::width = width;
    ::height = height;
    glViewport(0, 0, width, height);
}

void cursor_position_callback(GLFWwindow *window, double x, double y)
{
    bool isImGuiActive = ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) || ImGui::IsAnyItemHovered();

    bool pressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    if (!isImGuiActive && camera != nullptr)
        camera->handleMouseInput(x, y, pressed);
}

void scroll_callback(GLFWwindow *window, double x, double y)
{
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
        return;

    if (camera != nullptr)
        camera->handleScrollInput(y);
}

void handle_keyboard(GLFWwindow *window, float deltaTime)
{
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    {
        if (camera != nullptr)
            camera->handleKeyboardInput(GLFW_KEY_W, deltaTime);
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    {
        if (camera != nullptr)
            camera->handleKeyboardInput(GLFW_KEY_A, deltaTime);
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    {
        if (camera != nullptr)
            camera->handleKeyboardInput(GLFW_KEY_S, deltaTime);
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    {
        if (camera != nullptr)
            camera->handleKeyboardInput(GLFW_KEY_D, deltaTime);
    }
    if (glfwGetKey(window, GLFW_KEY_U) == GLFW_PRESS)
    {
        if (camera != nullptr)
            camera->handleKeyboardInput(GLFW_KEY_U, deltaTime);
    }
    if (glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS)
    {
        if (camera != nullptr)
            camera->handleKeyboardInput(GLFW_KEY_J, deltaTime);
    }
}
