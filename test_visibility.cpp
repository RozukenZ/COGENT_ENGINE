#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include "Core/Types.hpp"
#include "Core/Threading/JobSystem.hpp"
#include "Renderer/Visibility/VisibilitySystem.hpp"

using namespace Cogent;
using namespace Cogent::Renderer;
using namespace Cogent::Threading;

void GenerateMockObjects(std::vector<GameObject>& objects, size_t count) {
    objects.reserve(count);
    
    std::mt19937 rng(1337);
    std::uniform_real_distribution<float> distPos(-10000.0f, 10000.0f);
    std::uniform_real_distribution<float> distSize(1.0f, 50.0f);

    for (size_t i = 0; i < count; ++i) {
        GameObject obj;
        glm::vec3 pos = glm::vec3(distPos(rng), distPos(rng), distPos(rng));
        obj.model = glm::translate(glm::mat4(1.0f), pos);
        
        float size = distSize(rng);
        obj.aabbMin = pos - glm::vec3(size);
        obj.aabbMax = pos + glm::vec3(size);
        
        objects.push_back(obj);
    }
}

int main() {
    std::cout << "--- COGENT ENGINE VISIBILITY STRESS TEST ---\n";
    
    // 1. Setup Job System
    JobSystem::Get().Initialize();
    std::cout << "[INFO] JobSystem initialized.\n";

    // 2. Generate 1,000,000 objects
    size_t numObjects = 1'000'000;
    std::vector<GameObject> allObjects;
    std::cout << "[INFO] Generating " << numObjects << " mock GameObjects...\n";
    GenerateMockObjects(allObjects, numObjects);

    // 3. Setup Camera and Visibility System
    VisibilitySystem visSystem;
    
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), 16.0f / 9.0f, 0.1f, 5000.0f);
    glm::mat4 view = glm::lookAt(glm::vec3(0, 0, 0), glm::vec3(0, 0, -1), glm::vec3(0, 1, 0));
    visSystem.update(projection * view, glm::vec3(0, 0, 0));

    std::vector<const GameObject*> visibleObjectsSingle;
    std::vector<const GameObject*> visibleObjectsMulti;

    // 4. Test Single Thread
    std::cout << "\n[TEST] Running Single-Threaded Culling...\n";
    auto startSingle = std::chrono::high_resolution_clock::now();
    
    visSystem.cull(allObjects, visibleObjectsSingle);
    
    auto endSingle = std::chrono::high_resolution_clock::now();
    double timeSingle = std::chrono::duration<double, std::milli>(endSingle - startSingle).count();
    
    std::cout << "   -> Visible Objects: " << visibleObjectsSingle.size() << "\n";
    std::cout << "   -> Time Taken: " << timeSingle << " ms\n";

    // 5. Test Multi Thread
    std::cout << "\n[TEST] Running Multi-Threaded Culling...\n";
    auto startMulti = std::chrono::high_resolution_clock::now();
    
    visSystem.cullParallel(allObjects, visibleObjectsMulti);
    
    auto endMulti = std::chrono::high_resolution_clock::now();
    double timeMulti = std::chrono::duration<double, std::milli>(endMulti - startMulti).count();
    
    std::cout << "   -> Visible Objects: " << visibleObjectsMulti.size() << "\n";
    std::cout << "   -> Time Taken: " << timeMulti << " ms\n";

    // 6. Conclusion
    double speedup = timeSingle / timeMulti;
    std::cout << "\n--- RESULTS ---\n";
    std::cout << "Multithreading Speedup: " << speedup << "x\n";
    
    if (visibleObjectsSingle.size() != visibleObjectsMulti.size()) {
        std::cerr << "[FAIL] Output mismatch! Single: " << visibleObjectsSingle.size() << " vs Multi: " << visibleObjectsMulti.size() << "\n";
        return 1;
    }
    
    std::cout << "[PASS] All visibility tests passed.\n";

    JobSystem::Get().Shutdown();
    return 0;
}
