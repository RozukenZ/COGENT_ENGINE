#include "PropertyInspector.hpp"
#include <iostream>
#include <imgui.h>

namespace Cogent::Editor::UI {

    void PropertyInspector::AddCard(const InspectorCard& card) {
        _cards.push_back(card);
    }

    void PropertyInspector::Clear() {
        _cards.clear();
    }

    void PropertyInspector::RenderMockUI() const {
        for (const auto& card : _cards) {
            std::cout << "[CARD] " << card.headerName << (card.isCollapsed ? " (Collapsed)\n" : "\n");
            
            if (!card.isCollapsed) {
                for (const auto& field : card.fields) {
                    std::cout << "   |-- " << field.name << " : ";
                    
                    if (field.dataPtr == nullptr) {
                        std::cout << "NULL\n";
                        continue;
                    }
                    
                    switch (field.type) {
                        case PropertyType::FLOAT: {
                            float val = *static_cast<float*>(field.dataPtr);
                            std::cout << "[Float Slider] " << val << "\n";
                            break;
                        }
                        case PropertyType::INT: {
                            int val = *static_cast<int*>(field.dataPtr);
                            std::cout << "[Int Drag] " << val << "\n";
                            break;
                        }
                        case PropertyType::BOOL: {
                            bool val = *static_cast<bool*>(field.dataPtr);
                            std::cout << "[Toggle] " << (val ? "ON" : "OFF") << "\n";
                            break;
                        }
                        case PropertyType::STRING: {
                            std::string val = *static_cast<std::string*>(field.dataPtr);
                            std::cout << "[Text Field] \"" << val << "\"\n";
                            break;
                        }
                        case PropertyType::VECTOR3: {
                            float* val = static_cast<float*>(field.dataPtr);
                            std::cout << "[XYZ Fields] X:" << val[0] << " Y:" << val[1] << " Z:" << val[2] << "\n";
                            break;
                        }
                        case PropertyType::COLOR: {
                            float* val = static_cast<float*>(field.dataPtr);
                            std::cout << "[Color Picker] R:" << val[0] << " G:" << val[1] << " B:" << val[2] << "\n";
                            break;
                        }
                    }
                }
            }
            std::cout << "   --------------------------------\n";
        }
    }

    void PropertyInspector::RenderImGui() {
        for (auto& card : _cards) {
            // Modern Card Style Header
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.15f, 0.15f, 0.18f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            
            bool isOpen = ImGui::CollapsingHeader(card.headerName.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
            
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();

            if (isOpen) {
                ImGui::Indent(10.0f);
                for (auto& field : card.fields) {
                    if (field.dataPtr == nullptr) continue;

                    ImGui::PushID(field.dataPtr); // Ensure unique ID per widget based on data pointer
                    ImGui::Columns(2, nullptr, false);
                    ImGui::SetColumnWidth(0, ImGui::GetWindowWidth() * 0.4f);
                    
                    // Field Name
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted(field.name.c_str());
                    
                    ImGui::NextColumn();
                    ImGui::PushItemWidth(-1); // Use full remaining width

                    // Field Widget
                    switch (field.type) {
                        case PropertyType::FLOAT: {
                            float* val = static_cast<float*>(field.dataPtr);
                            ImGui::DragFloat("##v", val, 0.1f);
                            break;
                        }
                        case PropertyType::INT: {
                            int* val = static_cast<int*>(field.dataPtr);
                            ImGui::DragInt("##v", val, 1);
                            break;
                        }
                        case PropertyType::BOOL: {
                            bool* val = static_cast<bool*>(field.dataPtr);
                            ImGui::Checkbox("##v", val);
                            break;
                        }
                        case PropertyType::STRING: {
                            std::string* val = static_cast<std::string*>(field.dataPtr);
                            char buffer[256];
                            strncpy(buffer, val->c_str(), sizeof(buffer) - 1);
                            buffer[sizeof(buffer) - 1] = '\0';
                            if (ImGui::InputText("##v", buffer, sizeof(buffer))) {
                                *val = buffer;
                            }
                            break;
                        }
                        case PropertyType::VECTOR3: {
                            float* val = static_cast<float*>(field.dataPtr);
                            ImGui::DragFloat3("##v", val, 0.1f);
                            break;
                        }
                        case PropertyType::COLOR: {
                            float* val = static_cast<float*>(field.dataPtr);
                            ImGui::ColorEdit3("##v", val);
                            break;
                        }
                    }
                    
                    ImGui::PopItemWidth();
                    ImGui::Columns(1);
                    ImGui::PopID();
                    ImGui::Dummy(ImVec2(0, 4));
                }
                ImGui::Unindent(10.0f);
                ImGui::Dummy(ImVec2(0, 10));
            }
        }
    }

}
