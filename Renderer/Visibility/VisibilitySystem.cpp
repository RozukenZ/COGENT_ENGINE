#include "VisibilitySystem.hpp"
#include "../../Core/Math/Frustum.hpp"

namespace Cogent::Renderer {

    void VisibilitySystem::update(const glm::mat4& viewProj) {
        _frustum.update(viewProj);
    }

    void VisibilitySystem::cull(const std::vector<GameObject>& allObjects, std::vector<const GameObject*>& visibleObjects) {
        visibleObjects.clear();
        visibleObjects.reserve(allObjects.size());

        for (const auto& obj : allObjects) {
            Math::AABB box;
            box.min = obj.aabbMin;
            box.max = obj.aabbMax;

            if (_frustum.checkAABB(box)) {
                visibleObjects.push_back(&obj);
            }
        }
    }
}
