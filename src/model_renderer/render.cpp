#include <iostream>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <filesystem>
#include <iostream>
#include <future>

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
#include "SharedEnum.hpp"

int width = 1600, height = 900;
float lastTime;
Camera *camera = nullptr;

float loaderProgress = 0.0f;
int inputValue = 0;
double threshold = 0.05;

bool renderModel = false;
bool shouldSetCamera = false;

// List of available models
std::vector<std::string> modelPaths;
std::string selectedModelPath;

// Variables for model selection
int selectedModelIndex = -1;   // Default: no model selected
Model *currentModel = nullptr; // Pointer to the currently loaded model

std::future<std::string> segmentationFuture; // Future to track segmentation progress
bool isProcessing = false;                   // Flag to track processing state

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

Render::Render(std::function<std::string(const std::string &, Enums::CentroidInit, Enums::KInit, Enums::MetricMethod, int, double)> segmentationCallback)
    : segmentationCallback(std::move(segmentationCallback))
{
    std::cout << "Shader program initialized!" << std::endl;
}

void Render::renderFile(const std::string &fileName)
{
    selectedModelPath = fileName;

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
        lightDiffuse(1.2f, 1.2f, 1.2f),
        lightSpecular(2.0f, 2.0f, 2.0f),
        lightAmbient(0.3f, 0.3f, 0.3f);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    bool shouldClose = false;

    try
    {
        while (!glfwWindowShouldClose(window) && !shouldClose)
        {
            // Clear the screen
            glClearColor(0.85f, 0.85, 0.85, 1.0f);
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

                glm::vec3 modelCenter = currentModel->getBoundingBoxCenter();
                glm::vec3 modelSize = currentModel->getBoundingBoxSize();

                if (shouldSetCamera)
                {
                    camera->setCenter(glm::vec3(-modelCenter.x, -modelCenter.y, -modelCenter.z));
                    shouldSetCamera = false;
                }

                glm::mat4 model = glm::translate(glm::identity<glm::mat4>(), -modelCenter);
                float diagonal = glm::length(modelSize);

                glm::mat4 projection = glm::perspective(glm::radians(camera->getFOV()),
                                                        (float)width / height, std::max(0.1f, diagonal / 100.0f), diagonal * 10.0f);

                if (program)
                {
                    program->setMVPMatrices(model, camera->getViewMatrix(), projection);
                    program->setVec3("eyePos", camera->getPosition());
                    program->setVec3("light.position", lightPos);
                    program->setVec3("light.ambient", lightAmbient);
                    program->setVec3("light.diffuse", lightDiffuse);
                    program->setVec3("light.specular", lightSpecular);
                    program->setFloat("material.shininess", 64.0f);
                }

                currentModel->draw();

                // Step 2: Numerical Input for the task (e.g., number of iterations or parameters)

                ImGui::Text("Enter the number of clusters (parameter k, 0 if unknow)");

                // Input field with range enforcement
                if (ImGui::InputInt("Enter Value", &inputValue))
                {
                    inputValue = std::clamp(inputValue, 0, 10);
                }

                ImGui::Text("Enter the threshold value");

                if (ImGui::InputDouble("Enter Threshold", &threshold, 0.01, 0.1, "%.2f"))
                {
                    threshold = std::max(threshold, 0.05);
                }

                // Initialization method dropdown
                const Enums::CentroidInit initMethods[] = {Enums::CentroidInit::RANDOM, Enums::CentroidInit::KDE, Enums::CentroidInit::MOSTDISTANT, Enums::CentroidInit::KDE3D};
                static Enums::CentroidInit selectedInitMethod = Enums::CentroidInit::RANDOM;
                static Enums::KInit selectedKInitMethod = Enums::KInit::ELBOW_METHOD;
                static Enums::MetricMethod selectedMetricMethod = Enums::MetricMethod::DIJKSTRA;

                ImGui::Text("Select Initialization Method for Centroids:");

                if (ImGui::BeginCombo("Initialization Method", Enums::toString(selectedInitMethod).c_str()))
                {
                    for (const auto &method : initMethods)
                    {
                        bool isSelected = (selectedInitMethod == method);
                        if (ImGui::Selectable(Enums::toString(method).c_str(), isSelected))
                        {
                            selectedInitMethod = method; // Store the selected value
                        }

                        if (isSelected)
                        {
                            ImGui::SetItemDefaultFocus(); // Set the default focus on the selected item
                        }
                    }

                    ImGui::EndCombo();
                }

                ImGui::Text("Select Metric Method:");

                const Enums::MetricMethod initMetricMethods[] = {Enums::MetricMethod::DIJKSTRA, Enums::MetricMethod::EUCLIDEAN, Enums::MetricMethod::HEAT};

                if (ImGui::BeginCombo("Metric Method", Enums::toString(selectedMetricMethod).c_str()))
                {
                    for (const auto &method : initMetricMethods)
                    {
                        bool isSelected = (selectedMetricMethod == method);
                        if (ImGui::Selectable(Enums::toString(method).c_str(), isSelected))
                        {
                            selectedMetricMethod = method; // Store the selected value
                        }

                        if (isSelected)
                        {
                            ImGui::SetItemDefaultFocus(); // Set the default focus on the selected item
                        }
                    }

                    ImGui::EndCombo();
                }

                // Method for 'k' initialization, shown only if num_clusters == 0
                if (inputValue == 0)
                {
                    const Enums::KInit initMethods[] = {Enums::KInit::ELBOW_METHOD, Enums::KInit::KDE_METHOD, Enums::KInit::SILHOUETTE_METHOD};

                    ImGui::Text("Select Method for k Initialization:");
                    if (ImGui::BeginCombo("k Initialization Method", Enums::toString(selectedKInitMethod).c_str()))
                    {
                        for (const auto &method : initMethods)
                        {
                            bool isSelected = (selectedKInitMethod == method);
                            if (ImGui::Selectable(Enums::toString(method).c_str(), isSelected))
                            {
                                selectedKInitMethod = method; // Store the selected index
                            }

                            if (isSelected)
                            {
                                ImGui::SetItemDefaultFocus(); // Set the default focus on the selected item
                            }
                        }

                        ImGui::EndCombo();
                    }
                }

                // Step 3: Start Button to trigger loading process
                if (ImGui::Button("Start") && !isProcessing)
                {
                    loaderProgress = 0.0f; // Reset progress
                    isProcessing = true;   // Mark segmentation as ongoing

                    std::cout << "Starting segmentation..." << std::endl;

                    // Launch segmentation asynchronously in another thread (non-blocking)
                    segmentationFuture = std::async(std::launch::async, [&]()
                                                    { return segmentationCallback(selectedModelPath,
                                                                                  selectedInitMethod,
                                                                                  selectedKInitMethod,
                                                                                  selectedMetricMethod,
                                                                                  inputValue,
                                                                                  threshold); });
                }

                // Show loader while segmentation is ongoing
                if (isProcessing)
                {
                    // Make the overlay cover the entire screen
                    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
                    ImGui::SetNextWindowPos(ImVec2(0, 0));

                    ImGui::Begin("##LoaderOverlay", nullptr,
                                 ImGuiWindowFlags_NoTitleBar |
                                     ImGuiWindowFlags_NoResize |
                                     ImGuiWindowFlags_NoMove |
                                     ImGuiWindowFlags_NoScrollbar |
                                     ImGuiWindowFlags_NoInputs |
                                     ImGuiWindowFlags_NoBackground);

                    // Draw semi-transparent dark background
                    ImDrawList *drawList = ImGui::GetForegroundDrawList();
                    ImVec2 screenSize = ImGui::GetIO().DisplaySize;
                    drawList->AddRectFilled(ImVec2(0, 0), screenSize, IM_COL32(0, 0, 0, 150)); // Grey overlay (alpha 150)

                    // Position the loader at the center
                    ImVec2 center = ImVec2(screenSize.x * 0.5f, screenSize.y * 0.5f);

                    // Show loading text
                    ImGui::SetCursorPos(ImVec2(center.x - 40, center.y - 20));
                    ImGui::Text("Processing...");

                    // Draw a loading spinner
                    static float angle = 0.0f;
                    angle += ImGui::GetIO().DeltaTime * 3.0f; // Rotate spinner over time

                    float radius = 20.0f;
                    int numSegments = 16;
                    for (int i = 0; i < numSegments; i++)
                    {
                        float start = (float)i / (float)numSegments * 2.0f * 3.14159265f;
                        float end = (float)(i + 1) / (float)numSegments * 2.0f * 3.14159265f;
                        float alpha = (float)i / (float)numSegments;

                        ImVec2 p1 = ImVec2(center.x + cosf(start + angle) * radius, center.y + sinf(start + angle) * radius);
                        ImVec2 p2 = ImVec2(center.x + cosf(end + angle) * radius, center.y + sinf(end + angle) * radius);

                        drawList->AddLine(p1, p2, IM_COL32(255, 255, 255, (int)(alpha * 255)), 3.0f);
                    }

                    ImGui::End();
                }

                // Check if segmentation is finished
                if (isProcessing && segmentationFuture.valid() && segmentationFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
                {
                    std::string outputFile = segmentationFuture.get(); // Retrieve segmentation result
                    isProcessing = false;                              // Reset processing flag

                    if (!outputFile.empty())
                    {
                        std::cout << "Segmentation completed. Rendering file: " << outputFile << std::endl;
                        renderFile(outputFile); // Render the segmented model
                    }
                    else
                    {
                        std::cerr << "Segmentation failed!" << std::endl;
                    }
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
            }
            else
            {
                // Model Selector UI (grid view or list view)
                ImGui::Text("Select a Model to Load:");

                // Search box for filtering models
                static char searchBuffer[128] = "";
                ImGui::InputText("Search", searchBuffer, IM_ARRAYSIZE(searchBuffer));

                // Filtered model paths based on the search query
                std::vector<std::string> filteredModelPaths;
                for (const auto &modelPath : modelPaths)
                {
                    std::string fileName = std::filesystem::path(modelPath).filename().string();
                    if (strstr(fileName.c_str(), searchBuffer)) // Case-insensitive search
                    {
                        filteredModelPaths.push_back(modelPath);
                    }
                }
                // Dropdown list for selecting a model
                std::string currentModelName = selectedModelIndex != -1 ? std::filesystem::path(selectedModelPath).filename().string() : "";
                const char *currentModelNameCStr = currentModelName.empty() ? "Select a model" : currentModelName.c_str();

                if (ImGui::BeginCombo("Models", currentModelNameCStr))
                {
                    for (int i = 0; i < filteredModelPaths.size(); ++i)
                    {
                        const bool isSelected = (selectedModelIndex == i);
                        std::string fileName = std::filesystem::path(filteredModelPaths[i]).filename().string();

                        if (ImGui::Selectable(fileName.c_str(), isSelected))
                        {
                            selectedModelIndex = i;
                            selectedModelPath = filteredModelPaths[i];
                        }

                        // Highlight the selected item
                        if (isSelected)
                        {
                            ImGui::SetItemDefaultFocus();
                        }
                    }

                    ImGui::EndCombo();
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

    shouldSetCamera = true;
    renderModel = true;
}

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
