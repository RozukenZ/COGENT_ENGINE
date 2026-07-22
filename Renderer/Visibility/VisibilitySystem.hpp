#pragma once
#include <vector>
#include <glm/glm.hpp>
#include "../../Core/Math/Frustum.hpp"
#include "../../Core/Types.hpp" // For GameObject

namespace Cogent::Renderer {

    class VisibilitySystem {
    public:
        void update(const glm::mat4& viewProj);
        
        // Culls objects and populates 'visibleObjects' list
        void cull(const std::vector<GameObject>& allObjects, std::vector<const GameObject*>& visibleObjects);

        const Math::Frustum& getFrustum() const { return _frustum; }

    private:
        Math::Frustum _frustum;
    };
}
