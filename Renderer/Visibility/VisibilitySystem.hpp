#pragma once
#include <vector>
#include <mutex>
#include <atomic>
#include <glm/glm.hpp>
#include "../../Core/Math/Frustum.hpp"
#include "../../Core/Types.hpp"
#include "../../Core/Threading/JobSystem.hpp"
#include "../../Core/Diagnostics/Profiler.hpp"

namespace Cogent::Renderer {

    // Output structure for Data-Oriented Culling
    struct CullResult {
        std::vector<const GameObject*> visibleObjects;
        std::mutex mutex; // Protects push_back during chunk processing
    };

    class VisibilitySystem {
    public:
        VisibilitySystem() = default;
        ~VisibilitySystem() = default;

        void update(const glm::mat4& viewProj, const glm::vec3& cameraPos);
        
        // Single-threaded fallback
        void cull(const std::vector<GameObject>& allObjects, std::vector<const GameObject*>& outVisibleObjects);
        
        // Multi-threaded Data-Oriented Culling via CTS
        void cullParallel(const std::vector<GameObject>& allObjects, std::vector<const GameObject*>& outVisibleObjects);

        const Math::Frustum& getFrustum() const { return _frustum; }

    private:
        Math::Frustum _frustum;
        glm::vec3 _cameraPos;
        
        // Tunable parameters
        const float MAX_DRAW_DISTANCE = 5000.0f;
        const size_t CHUNK_SIZE = 1000; // Objects per job
    };
}
