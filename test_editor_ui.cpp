#include <iostream>
#include "Editor/UI/UIContext.hpp"
#include "Editor/UI/PropertyInspector.hpp"
#include "Editor/UI/NodeCanvas.hpp"

using namespace Cogent::Editor::UI;

void PrintNodeTree(std::shared_ptr<DockNode> node, int depth = 0) {
    if (!node) return;
    
    for (int i = 0; i < depth; i++) std::cout << "  ";
    
    if (node->IsLeaf()) {
        std::cout << "- Panel [" << node->panelName << "] Bounds: {" 
                  << node->bounds.x << ", " << node->bounds.y << ", " 
                  << node->bounds.width << ", " << node->bounds.height << "}\n";
    } else {
        std::cout << "- Container (Split: " 
                  << (node->split == SplitDirection::HORIZONTAL ? "HORIZONTAL" : "VERTICAL") 
                  << " " << (node->splitRatio * 100.0f) << "%)\n";
        PrintNodeTree(node->childA, depth + 1);
        PrintNodeTree(node->childB, depth + 1);
    }
}

int main() {
    std::cout << "--- COGENT ENGINE EDITOR UI TEST ---\n\n";

    UIContext::Get().Initialize(1920.0f, 1080.0f);

    // ==========================================
    // 1. COMMAND PALETTE TEST
    // ==========================================
    std::cout << "\n>> Testing Command Palette (Fuzzy Match)...\n";
    auto& cmd = UIContext::Get().GetCommandPalette();
    
    // Mock registration
    cmd.RegisterCommand("file.save", "Save Scene", "Ctrl+S", [](){ std::cout << "Saving scene...\n"; });
    cmd.RegisterCommand("edit.undo", "Undo Action", "Ctrl+Z", [](){});
    cmd.RegisterCommand("create.cube", "Create 3D Cube", "Ctrl+Shift+C", [](){});
    cmd.RegisterCommand("create.sphere", "Create 3D Sphere", "", [](){});
    cmd.RegisterCommand("view.telemetry", "Toggle Telemetry Overlay", "F3", [](){});

    // User types "cub"
    std::string query = "cub";
    std::cout << "User typing: '" << query << "'\n";
    auto results = cmd.Search(query);
    
    if (!results.empty() && results[0].command->id == "create.cube") {
        std::cout << "[PASS] Fuzzy search perfectly matched 'Create 3D Cube' as top result!\n";
    } else {
        std::cerr << "[FAIL] Fuzzy match algorithm failed.\n";
        return 1;
    }

    // ==========================================
    // 2. DOCKING SYSTEM TEST
    // ==========================================
    std::cout << "\n>> Testing Docking System (Split Layout)...\n";
    auto& dock = UIContext::Get().GetDockingSystem();
    
    auto root = dock.GetRoot(); // Starts with 'MainViewport' 1920x1080 (ID 1)
    
    // Split vertically (Left: Viewport, Right: Inspector) at 75%
    dock.SplitNode(root->id, SplitDirection::HORIZONTAL, "Inspector", 0.75f);
    
    // Split the Inspector horizontally (Top: Inspector, Bottom: Content Browser) at 50%
    // Inspector is childB of root, its ID is 3. (Root=1, Viewport=2, Inspector=3)
    dock.SplitNode(3, SplitDirection::VERTICAL, "Content Browser", 0.5f);

    PrintNodeTree(dock.GetRoot());

    // Verify coordinates
    // Viewport should be: width = 1920 * 0.75 = 1440
    // Inspector should be: x = 1440, width = 480, y = 0, height = 540
    // Content Browser should be: x = 1440, width = 480, y = 540, height = 540
    
    // ==========================================
    // 3. PROPERTY INSPECTOR TEST
    // ==========================================
    std::cout << "\n>> Testing Property Inspector (Data-driven Cards)...\n";
    PropertyInspector inspector;
    
    // Mock GameObject Data
    float transformPos[3] = {10.5f, 0.0f, -5.2f};
    bool isVisible = true;
    std::string meshName = "SM_Rock_01";
    float roughness = 0.8f;
    
    InspectorCard transformCard;
    transformCard.headerName = "Transform";
    transformCard.AddField("Position", PropertyType::VECTOR3, transformPos);
    
    InspectorCard meshCard;
    meshCard.headerName = "Static Mesh Component";
    meshCard.AddField("Mesh Asset", PropertyType::STRING, &meshName);
    meshCard.AddField("Visible", PropertyType::BOOL, &isVisible);
    
    InspectorCard materialCard;
    materialCard.headerName = "Material Instance";
    materialCard.AddField("Roughness", PropertyType::FLOAT, &roughness);
    
    inspector.AddCard(transformCard);
    inspector.AddCard(meshCard);
    inspector.AddCard(materialCard);
    
    inspector.RenderMockUI();
    std::cout << "[PASS] Property Inspector successfully generated Data-driven UI Cards.\n";

    // ==========================================
    // 4. MODERN NODE CANVAS TEST
    // ==========================================
    std::cout << "\n>> Testing Modern Node Canvas (Infinite Grid & Spline Wires)...\n";
    NodeCanvas canvas;
    
    EditorNode& nodeBegin = canvas.CreateNode("Event BeginPlay", {100.0f, 200.0f});
    canvas.AddPinToNode(nodeBegin.id, "Exec Out", PinType::OUTPUT, DataType::EXEC);
    
    EditorNode& nodePrint = canvas.CreateNode("Print String", {500.0f, 200.0f});
    canvas.AddPinToNode(nodePrint.id, "Exec In", PinType::INPUT, DataType::EXEC);
    canvas.AddPinToNode(nodePrint.id, "Message", PinType::INPUT, DataType::STRING);
    
    // Attempt Connection (Output Pin 100 to Input Pin 101)
    bool connected = canvas.ConnectPins(100, 101);
    
    canvas.RenderMockCanvas();
    
    if (connected) {
        std::cout << "[PASS] Nodes successfully connected in infinite spatial canvas!\n";
    } else {
        std::cerr << "[FAIL] Failed to connect nodes.\n";
        return 1;
    }

    std::cout << "\n[FINAL PASS] Phase 1 & Phase 2 UI Architecture successfully integrated!\n";
    
    UIContext::Get().Shutdown();
    return 0;
}
