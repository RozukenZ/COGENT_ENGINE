#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace Cogent::Editor::UI {

    enum class PropertyType {
        FLOAT,
        INT,
        BOOL,
        STRING,
        VECTOR3,
        COLOR
    };

    struct PropertyField {
        std::string name;
        PropertyType type;
        void* dataPtr;
        bool isExpanded = true;
    };

    // Card-based visual container for a component (e.g., Transform, Material)
    struct InspectorCard {
        std::string headerName;
        std::vector<PropertyField> fields;
        bool isCollapsed = false;
        
        void AddField(const std::string& name, PropertyType type, void* dataPtr) {
            fields.push_back({name, type, dataPtr, true});
        }
    };

    class PropertyInspector {
    public:
        // Registers a new Card into the inspector view
        void AddCard(const InspectorCard& card);
        
        // Simulates rendering the data-driven UI
        void RenderMockUI() const;

        // Render actual ImGui widgets
        void RenderImGui();

        // Clears all cards
        void Clear();

    private:
        std::vector<InspectorCard> _cards;
    };

}
