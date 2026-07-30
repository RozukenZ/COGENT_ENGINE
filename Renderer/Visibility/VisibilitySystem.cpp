#include "VisibilitySystem.hpp"
#include <algorithm>

namespace Cogent::Renderer {

    void VisibilitySystem::update(const glm::mat4& viewProj, const glm::vec3& cameraPos) {
        PROFILE_FUNCTION();
        _frustum.update(viewProj);
        _cameraPos = cameraPos;
    }

    void VisibilitySystem::cull(const std::vector<GameObject>& allObjects, std::vector<const GameObject*>& outVisibleObjects) {
        PROFILE_FUNCTION();
        outVisibleObjects.clear();
        outVisibleObjects.reserve(allObjects.size());

        for (const auto& obj : allObjects) {
            Math::AABB box;
            box.min = obj.aabbMin;
            box.max = obj.aabbMax;

            // 1. Distance Culling
            glm::vec3 pos = glm::vec3(obj.model[3]);
            float dist = glm::distance(_cameraPos, pos);
            if (dist > MAX_DRAW_DISTANCE) {
                continue; 
            }

            // 2. Frustum Culling
            if (_frustum.checkAABB(box)) {
                outVisibleObjects.push_back(&obj);
            }
        }
    }

    void VisibilitySystem::cullParallel(const std::vector<GameObject>& allObjects, std::vector<const GameObject*>& outVisibleObjects) {
        PROFILE_FUNCTION();
        outVisibleObjects.clear();
        outVisibleObjects.reserve(allObjects.size());

        size_t totalObjects = allObjects.size();
        if (totalObjects == 0) return;

        // If too few objects, fallback to single-thread to avoid overhead
        if (totalObjects < CHUNK_SIZE) {
            cull(allObjects, outVisibleObjects);
            return;
        }

        size_t numChunks = (totalObjects + CHUNK_SIZE - 1) / CHUNK_SIZE;
        CullResult globalResult;
        globalResult.visibleObjects.reserve(totalObjects);

        auto& jobSystem = Threading::JobSystem::Get();
        std::vector<Threading::JobHandle> handles;
        handles.reserve(numChunks);

        for (size_t i = 0; i < numChunks; ++i) {
            size_t startIndex = i * CHUNK_SIZE;
            size_t endIndex = std::min(startIndex + CHUNK_SIZE, totalObjects);

            handles.push_back(jobSystem.Execute([this, &allObjects, startIndex, endIndex, &globalResult]() {
                // Thread-local storage to avoid locking mutex on every object
                std::vector<const GameObject*> localVisible;
                localVisible.reserve(endIndex - startIndex);

                for (size_t j = startIndex; j < endIndex; ++j) {
                    const auto& obj = allObjects[j];
                    Math::AABB box;
                    box.min = obj.aabbMin;
                    box.max = obj.aabbMax;

                    glm::vec3 pos = glm::vec3(obj.model[3]);
                    float dist = glm::distance(_cameraPos, pos);
                    if (dist > MAX_DRAW_DISTANCE) {
                        continue; 
                    }

                    if (_frustum.checkAABB(box)) {
                        localVisible.push_back(&obj);
                    }
                }

                // Batch merge to global result
                if (!localVisible.empty()) {
                    std::lock_guard<std::mutex> lock(globalResult.mutex);
                    globalResult.visibleObjects.insert(globalResult.visibleObjects.end(), localVisible.begin(), localVisible.end());
                }
            }));
        }

        jobSystem.WaitAll();

        outVisibleObjects = std::move(globalResult.visibleObjects);
    }
}
