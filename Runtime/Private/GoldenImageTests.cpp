#include "Runtime/Public/GoldenImageTests.h"
#include "Runtime/Public/EngineRuntime.h"
#include "Runtime/Public/GPUSceneTests.h"
#include "RenderingEngine/Runtime/engine/EngineLoop.h"
#include "Rendering/Core/Renderer.h"
#include "Scene/SceneManager.h"
#include "Core/Logger.h"
#include "stb/stb_image.h"
#include <iostream>
#include <filesystem>
#include <cmath>

namespace eng::renderer {

bool RunGoldenImageTests() noexcept {
    LOG_INFO("================================================================================");
    LOG_INFO("                         RUNNING GOLDEN IMAGE COMPARISON TEST                   ");
    LOG_INFO("================================================================================");

    // 1. Setup minimal headless loop
    eng::runtime::EngineRuntime runtime;
    int argc = 1;
    char* argv[] = { (char*)"Application.exe" };
    
    // We manually initialize the systems to avoid full editor window loops
    // But since the loop requires window and swapchain, let's let EngineRuntime initialize normally
    // but run headless/test behavior.
    // Wait, EngineLoop normally creates a window. Is that fine? Yes, it will pop a window briefly or run.
    // Let's load the scene, position camera, render frames, and compare.
    
    LOG_INFO("[GoldenTest] Initializing engine runtime...");
    if (!runtime.Initialize(argc, argv)) {
        LOG_ERROR("[GoldenTest] Failed to initialize EngineRuntime.");
        return false;
    }

    auto* loop = static_cast<eng::runtime::EngineLoop*>(runtime.GetContext().renderer);
    if (!loop) {
        LOG_ERROR("[GoldenTest] EngineLoop is not initialized.");
        return false;
    }

    auto* sceneMgr = static_cast<SceneManager*>(runtime.GetContext().scenes);
    if (!sceneMgr) {
        LOG_ERROR("[GoldenTest] SceneManager is not initialized.");
        return false;
    }

    LOG_INFO("[GoldenTest] Loading test scene Assets/Scenes/RendererTest_RVG.omnixscene...");
    sceneMgr->LoadScene("Assets/Scenes/RendererTest_RVG.omnixscene");

    // Tick scene manager to load and transition scene to active state
    sceneMgr->Update(0.016f); // state: Loading -> ReadyToSwitch
    sceneMgr->Update(0.016f); // state: ReadyToSwitch -> Running (and Switches Scene)

    // Retrieve the active renderer
    auto* renderer = loop->GetSceneRenderer();
    if (!renderer) {
        LOG_ERROR("[GoldenTest] Renderer instance is null.");
        return false;
    }

    // Run GPU scene tests (G9 Page Streaming + G10 Visibility attachments validation)
    if (!RunGPUSceneTests(renderer->resources, renderer->gpuScene, renderer)) {
        LOG_ERROR("[GoldenTest] GPUScene tests failed.");
        return false;
    }

    // Bind the active scene to the renderer
    renderer->SetActiveScene(sceneMgr->GetActiveScene());

    // Adjust camera to a stable reference angle
    renderer->camera.position = glm::vec3(0.0f, 5.0f, 10.0f);
    renderer->camera.target = glm::vec3(0.0f, 1.0f, 0.0f);
    renderer->camera.up = glm::vec3(0.0f, 1.0f, 0.0f);

    // Warm up render loop by ticking several times
    LOG_INFO("[GoldenTest] Warming up render loop (rendering 10 frames)...");
    for (int i = 0; i < 10; ++i) {
        loop->Tick();
    }

    std::string testPath = "golden_test_run.png";
    std::string refPath = "Assets/Textures/golden_reference.png";

    LOG_INFO("[GoldenTest] Capturing current frame...");
    if (!renderer->CaptureScreenshot(testPath)) {
        LOG_ERROR("[GoldenTest] Failed to capture screenshot.");
        return false;
    }

    // Check if reference exists
    if (!std::filesystem::exists(refPath)) {
        LOG_INFO("[GoldenTest] Reference image not found. Creating reference from current run...");
        try {
            std::filesystem::copy_file(testPath, refPath, std::filesystem::copy_options::overwrite_existing);
            LOG_INFO("[GoldenTest] Created golden reference image at: " + refPath);
            LOG_INFO("[GoldenTest] Re-run the test to validate.");
            return true;
        } catch (const std::exception& e) {
            LOG_ERROR(std::string("[GoldenTest] Failed to copy reference image: ") + e.what());
            return false;
        }
    }

    // Load both images
    LOG_INFO("[GoldenTest] Comparing test run against golden reference...");
    int w1, h1, c1;
    unsigned char* dataTest = stbi_load(testPath.c_str(), &w1, &h1, &c1, 4);
    if (!dataTest) {
        LOG_ERROR("[GoldenTest] Failed to load test screenshot: " + testPath);
        return false;
    }

    int w2, h2, c2;
    unsigned char* dataRef = stbi_load(refPath.c_str(), &w2, &h2, &c2, 4);
    if (!dataRef) {
        stbi_image_free(dataTest);
        LOG_ERROR("[GoldenTest] Failed to load golden reference: " + refPath);
        return false;
    }

    if (w1 != w2 || h1 != h2) {
        LOG_ERROR("[GoldenTest] Image dimensions do not match! Test: " + std::to_string(w1) + "x" + std::to_string(h1) +
                  ", Ref: " + std::to_string(w2) + "x" + std::to_string(h2));
        stbi_image_free(dataTest);
        stbi_image_free(dataRef);
        return false;
    }

    // Count mismatching pixels
    uint64_t totalPixels = w1 * h1;
    uint64_t mismatchedPixels = 0;
    
    // Configurable tolerance metrics
    // We allow minor color differences (threshold of 8 out of 255 per channel)
    // and overall mismatched pixel ratio of < 1.0% (for driver/GPU variance)
    const int colorThreshold = 8;
    const double maxMismatchedRatio = 0.01; // 1%

    for (int y = 0; y < h1; ++y) {
        for (int x = 0; x < w1; ++x) {
            int idx = (y * w1 + x) * 4;
            int rDiff = std::abs(dataTest[idx + 0] - dataRef[idx + 0]);
            int gDiff = std::abs(dataTest[idx + 1] - dataRef[idx + 1]);
            int bDiff = std::abs(dataTest[idx + 2] - dataRef[idx + 2]);
            
            if (rDiff > colorThreshold || gDiff > colorThreshold || bDiff > colorThreshold) {
                mismatchedPixels++;
            }
        }
    }

    stbi_image_free(dataTest);
    stbi_image_free(dataRef);

    double mismatchRatio = (double)mismatchedPixels / (double)totalPixels;
    LOG_INFO("[GoldenTest] Comparison results: mismatched pixels = " + std::to_string(mismatchedPixels) + 
              "/" + std::to_string(totalPixels) + " (" + std::to_string(mismatchRatio * 100.0) + "%)");

    if (mismatchRatio > maxMismatchedRatio) {
        LOG_ERROR("[GoldenTest] TEST FAILED: Mismatch ratio exceeds tolerance of " + std::to_string(maxMismatchedRatio * 100.0) + "%");
        return false;
    }

    LOG_INFO("[GoldenTest] TEST PASSED: Mismatch ratio within acceptable tolerance.");
    return true;
}

} // namespace eng::renderer
