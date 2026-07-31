#include "NodeCanvas.hpp"
#include <iostream>
#include <imgui.h>

namespace Cogent::Editor::UI {

    EditorNode& NodeCanvas::CreateNode(const std::string& title, Vec2 position) {
        EditorNode node;
        node.id = _nodeIdCounter++;
        node.title = title;
        node.position = position;
        _nodes.push_back(node);
        return _nodes.back();
    }

    EditorNode* NodeCanvas::FindNode(uint32_t id) {
        for (auto& node : _nodes) {
            if (node.id == id) return &node;
        }
        return nullptr;
    }

    NodePin* NodeCanvas::FindPin(uint32_t id) {
        for (auto& node : _nodes) {
            for (auto& pin : node.inputs) {
                if (pin.id == id) return &pin;
            }
            for (auto& pin : node.outputs) {
                if (pin.id == id) return &pin;
            }
        }
        return nullptr;
    }

    void NodeCanvas::AddPinToNode(uint32_t nodeId, const std::string& name, PinType type, DataType dataType) {
        EditorNode* node = FindNode(nodeId);
        if (!node) return;

        NodePin pin;
        pin.id = _pinIdCounter++;
        pin.name = name;
        pin.type = type;
        pin.dataType = dataType;
        
        // Mock pin spatial location logic
        pin.canvasPosition = { node->position.x + (type == PinType::INPUT ? 0.0f : 200.0f), node->position.y + 40.0f + (node->inputs.size() + node->outputs.size()) * 30.0f };

        if (type == PinType::INPUT) {
            node->inputs.push_back(pin);
        } else {
            node->outputs.push_back(pin);
        }
    }

    bool NodeCanvas::ConnectPins(uint32_t outPinId, uint32_t inPinId) {
        NodePin* outPin = FindPin(outPinId);
        NodePin* inPin = FindPin(inPinId);

        if (!outPin || !inPin) return false;
        if (outPin->type != PinType::OUTPUT || inPin->type != PinType::INPUT) return false;
        
        // Very basic data type check (allow exec to exec, data to data)
        if (outPin->dataType != inPin->dataType) {
            std::cerr << "[ERROR] Cannot connect pins of different data types!\n";
            return false;
        }

        outPin->isConnected = true;
        inPin->isConnected = true;
        
        _wires.push_back({outPinId, inPinId});
        return true;
    }

    std::string DataTypeToString(DataType type) {
        switch(type) {
            case DataType::EXEC: return "EXEC";
            case DataType::FLOAT: return "FLOAT";
            case DataType::VECTOR3: return "VECTOR3";
            case DataType::BOOL: return "BOOL";
            case DataType::OBJECT: return "OBJECT";
            case DataType::STRING: return "STRING";
            default: return "UNKNOWN";
        }
    }

    void NodeCanvas::RenderMockCanvas() const {
        std::cout << "\n[NODE CANVAS] Rendering " << _nodes.size() << " Nodes and " << _wires.size() << " Wires\n";
        
        for (const auto& node : _nodes) {
            std::cout << "  [NODE] " << node.title << " @(" << node.position.x << ", " << node.position.y << ")\n";
            for (const auto& pin : node.inputs) {
                std::cout << "     <- (IN:" << DataTypeToString(pin.dataType) << ") " << pin.name << " [ID:" << pin.id << "]\n";
            }
            for (const auto& pin : node.outputs) {
                std::cout << "     -> (OUT:" << DataTypeToString(pin.dataType) << ") " << pin.name << " [ID:" << pin.id << "]\n";
            }
        }
        
        for (const auto& wire : _wires) {
            std::cout << "  [WIRE] (ID:" << wire.outputPinId << ") ======> (ID:" << wire.inputPinId << ")\n";
        }
    }

    void NodeCanvas::RenderImGui() {
        ImGui::Begin("NVS Studio - Node Canvas");
        
        // Canvas Setup
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();      // ImDrawList API uses screen coordinates
        ImVec2 canvas_sz = ImGui::GetContentRegionAvail();   // Resize canvas to what's available
        if (canvas_sz.x < 50.0f) canvas_sz.x = 50.0f;
        if (canvas_sz.y < 50.0f) canvas_sz.y = 50.0f;
        ImVec2 canvas_p1 = ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);

        // Draw border and background
        ImGuiIO& io = ImGui::GetIO();
        draw_list->AddRectFilled(canvas_p0, canvas_p1, IM_COL32(40, 40, 40, 255));
        draw_list->AddRect(canvas_p0, canvas_p1, IM_COL32(255, 255, 255, 50));

        // Pan Canvas
        if (ImGui::IsWindowHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0.0f)) {
            _scrolling.x += io.MouseDelta.x;
            _scrolling.y += io.MouseDelta.y;
        }

        // Draw Grid
        const float GRID_STEP = 64.0f;
        for (float x = fmodf(_scrolling.x, GRID_STEP); x < canvas_sz.x; x += GRID_STEP)
            draw_list->AddLine(ImVec2(canvas_p0.x + x, canvas_p0.y), ImVec2(canvas_p0.x + x, canvas_p1.y), IM_COL32(200, 200, 200, 40));
        for (float y = fmodf(_scrolling.y, GRID_STEP); y < canvas_sz.y; y += GRID_STEP)
            draw_list->AddLine(ImVec2(canvas_p0.x, canvas_p0.y + y), ImVec2(canvas_p1.x, canvas_p0.y + y), IM_COL32(200, 200, 200, 40));

        // Node offsets
        ImVec2 offset = ImVec2(canvas_p0.x + _scrolling.x, canvas_p0.y + _scrolling.y);

        // Draw Wires (Bezier Curves)
        for (const auto& wire : _wires) {
            NodePin* outPin = FindPin(wire.outputPinId);
            NodePin* inPin = FindPin(wire.inputPinId);
            if (outPin && inPin) {
                ImVec2 p1(offset.x + outPin->canvasPosition.x, offset.y + outPin->canvasPosition.y);
                ImVec2 p2(offset.x + inPin->canvasPosition.x, offset.y + inPin->canvasPosition.y);
                draw_list->AddBezierCubic(p1, ImVec2(p1.x + 50, p1.y), ImVec2(p2.x - 50, p2.y), p2, IM_COL32(200, 200, 100, 255), 3.0f);
            }
        }

        // Draw Nodes
        for (auto& node : _nodes) {
            ImVec2 node_rect_min(offset.x + node.position.x, offset.y + node.position.y);
            ImVec2 node_rect_max(node_rect_min.x + 150.0f, node_rect_min.y + 50.0f + (node.inputs.size() + node.outputs.size()) * 25.0f);

            // Node Background
            draw_list->AddRectFilled(node_rect_min, node_rect_max, IM_COL32(60, 60, 60, 255), 4.0f);
            // Title Header Background
            draw_list->AddRectFilled(node_rect_min, ImVec2(node_rect_max.x, node_rect_min.y + 25.0f), IM_COL32(100, 100, 150, 255), 4.0f, ImDrawFlags_RoundCornersTop);
            draw_list->AddRect(node_rect_min, node_rect_max, IM_COL32(100, 100, 100, 255), 4.0f);

            // Title
            draw_list->AddText(ImVec2(node_rect_min.x + 5.0f, node_rect_min.y + 5.0f), IM_COL32(255, 255, 255, 255), node.title.c_str());

            // Pins
            float currentY = node_rect_min.y + 35.0f;
            for (auto& pin : node.inputs) {
                ImVec2 pin_pos(node_rect_min.x, currentY);
                pin.canvasPosition = {pin_pos.x - offset.x, pin_pos.y - offset.y}; // Update world pos
                draw_list->AddCircleFilled(pin_pos, 5.0f, IM_COL32(150, 150, 150, 255));
                draw_list->AddText(ImVec2(pin_pos.x + 10.0f, pin_pos.y - 7.0f), IM_COL32(200, 200, 200, 255), pin.name.c_str());
                currentY += 25.0f;
            }
            for (auto& pin : node.outputs) {
                ImVec2 pin_pos(node_rect_max.x, currentY);
                pin.canvasPosition = {pin_pos.x - offset.x, pin_pos.y - offset.y}; // Update world pos
                draw_list->AddCircleFilled(pin_pos, 5.0f, IM_COL32(150, 150, 150, 255));
                
                ImVec2 text_size = ImGui::CalcTextSize(pin.name.c_str());
                draw_list->AddText(ImVec2(pin_pos.x - text_size.x - 10.0f, pin_pos.y - 7.0f), IM_COL32(200, 200, 200, 255), pin.name.c_str());
                currentY += 25.0f;
            }
            
            // Allow basic node dragging
            ImGui::SetCursorScreenPos(node_rect_min);
            ImGui::InvisibleButton(node.title.c_str(), ImVec2(node_rect_max.x - node_rect_min.x, 25.0f));
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                node.position.x += io.MouseDelta.x;
                node.position.y += io.MouseDelta.y;
            }
        }

        ImGui::End();
    }
}
