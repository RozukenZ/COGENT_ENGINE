#include "UIContext.hpp"
#include <iostream>

namespace Cogent::Editor::UI {

    void UIContext::Initialize(float screenWidth, float screenHeight) {
        _commandPalette = std::make_unique<CommandPalette>();
        _dockingSystem = std::make_unique<DockingSystem>(screenWidth, screenHeight);
        
        std::cout << "[UI Context] GPU-Accelerated UI Pipeline Initialized.\n";
        std::cout << "[UI Context] Base Resolution: " << screenWidth << " x " << screenHeight << "\n";
    }

    void UIContext::Shutdown() {
        _commandPalette.reset();
        _dockingSystem.reset();
        std::cout << "[UI Context] Shutdown.\n";
    }

}
