#pragma once
#include <string>
#include <vector>
#include <memory>

namespace Cogent::Editor::UI {

    struct Vec2 { float x, y; };

    enum class PinType {
        INPUT,
        OUTPUT
    };

    enum class DataType {
        EXEC,
        FLOAT,
        VECTOR3,
        BOOL,
        OBJECT,
        STRING
    };

    struct NodePin {
        uint32_t id;
        std::string name;
        PinType type;
        DataType dataType;
        bool isConnected = false;
        Vec2 canvasPosition; // World position of the pin on canvas
    };

    struct NodeWire {
        uint32_t outputPinId;
        uint32_t inputPinId;
    };

    struct EditorNode {
        uint32_t id;
        std::string title;
        Vec2 position; // Position on infinite canvas
        std::vector<NodePin> inputs;
        std::vector<NodePin> outputs;
    };

    class NodeCanvas {
    public:
        // Core operations
        EditorNode& CreateNode(const std::string& title, Vec2 position);
        void AddPinToNode(uint32_t nodeId, const std::string& name, PinType type, DataType dataType);
        
        // Connects Output to Input
        bool ConnectPins(uint32_t outPinId, uint32_t inPinId);
        
        // Simulates Canvas UI state
        void RenderMockCanvas() const;

        // Render actual ImGui 2D canvas
        void RenderImGui();

    private:
        std::vector<EditorNode> _nodes;
        std::vector<NodeWire> _wires;
        uint32_t _nodeIdCounter = 1;
        uint32_t _pinIdCounter = 100;
        
        // Panning and Zooming State
        Vec2 _scrolling = {0.0f, 0.0f};

        EditorNode* FindNode(uint32_t id);
        NodePin* FindPin(uint32_t id);
    };

}
