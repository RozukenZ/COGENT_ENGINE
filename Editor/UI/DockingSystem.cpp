#include "DockingSystem.hpp"
#include <iostream>

namespace Cogent::Editor::UI {

    DockingSystem::DockingSystem(float screenWidth, float screenHeight) {
        _root = std::make_shared<DockNode>();
        _root->id = _nodeIdCounter++;
        _root->bounds = {0.0f, 0.0f, screenWidth, screenHeight};
        _root->panelName = "MainViewport";
    }

    std::shared_ptr<DockNode> DockingSystem::FindNode(std::shared_ptr<DockNode> node, uint32_t id) {
        if (!node) return nullptr;
        if (node->id == id) return node;
        
        if (auto found = FindNode(node->childA, id)) return found;
        if (auto found = FindNode(node->childB, id)) return found;
        
        return nullptr;
    }

    bool DockingSystem::SplitNode(uint32_t targetNodeId, SplitDirection direction, const std::string& newPanelName, float ratio) {
        auto node = FindNode(_root, targetNodeId);
        if (!node || !node->IsLeaf()) return false;

        node->split = direction;
        node->splitRatio = ratio;
        
        // Child A takes the existing panel
        node->childA = std::make_shared<DockNode>();
        node->childA->id = _nodeIdCounter++;
        node->childA->panelName = node->panelName;
        
        // Child B takes the new panel
        node->childB = std::make_shared<DockNode>();
        node->childB->id = _nodeIdCounter++;
        node->childB->panelName = newPanelName;
        
        node->panelName = ""; // It is now a container, not a panel

        UpdateBounds(node, node->bounds);
        return true;
    }

    void DockingSystem::UpdateBounds(std::shared_ptr<DockNode> node, const Rect& bounds) {
        if (!node) return;
        node->bounds = bounds;

        if (node->IsLeaf()) return;

        Rect boundsA = bounds;
        Rect boundsB = bounds;

        if (node->split == SplitDirection::HORIZONTAL) {
            float splitWidth = bounds.width * node->splitRatio;
            boundsA.width = splitWidth;
            boundsB.x += splitWidth;
            boundsB.width -= splitWidth;
        } else if (node->split == SplitDirection::VERTICAL) {
            float splitHeight = bounds.height * node->splitRatio;
            boundsA.height = splitHeight;
            boundsB.y += splitHeight;
            boundsB.height -= splitHeight;
        }

        UpdateBounds(node->childA, boundsA);
        UpdateBounds(node->childB, boundsB);
    }

    void DockingSystem::RecalculateLayout(float screenWidth, float screenHeight) {
        if (_root) {
            UpdateBounds(_root, {0.0f, 0.0f, screenWidth, screenHeight});
        }
    }
}
