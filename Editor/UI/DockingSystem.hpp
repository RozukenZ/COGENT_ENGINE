#pragma once
#include <string>
#include <vector>
#include <memory>

namespace Cogent::Editor::UI {

    struct Rect {
        float x, y, width, height;
    };

    enum class SplitDirection {
        NONE,
        HORIZONTAL,
        VERTICAL
    };

    // A node in the Binary Space Partitioning tree representing dockable panels
    struct DockNode {
        uint32_t id = 0;
        Rect bounds;
        
        SplitDirection split = SplitDirection::NONE;
        float splitRatio = 0.5f; // 0.0 to 1.0
        
        std::shared_ptr<DockNode> childA = nullptr;
        std::shared_ptr<DockNode> childB = nullptr;
        
        std::string panelName; // e.g., "Viewport", "Inspector", if this is a leaf node
        
        bool IsLeaf() const { return split == SplitDirection::NONE; }
    };

    class DockingSystem {
    public:
        DockingSystem(float screenWidth, float screenHeight);

        // Root of the workspace
        std::shared_ptr<DockNode> GetRoot() const { return _root; }

        // Split a specific leaf node into two
        bool SplitNode(uint32_t targetNodeId, SplitDirection direction, const std::string& newPanelName, float ratio = 0.5f);

        // Traverse tree to calculate new bounds when window resizes
        void RecalculateLayout(float screenWidth, float screenHeight);

    private:
        std::shared_ptr<DockNode> _root;
        uint32_t _nodeIdCounter = 1;

        std::shared_ptr<DockNode> FindNode(std::shared_ptr<DockNode> node, uint32_t id);
        void UpdateBounds(std::shared_ptr<DockNode> node, const Rect& bounds);
    };

}
