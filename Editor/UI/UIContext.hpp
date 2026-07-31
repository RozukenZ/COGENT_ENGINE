#pragma once
#include <memory>
#include "CommandPalette.hpp"
#include "DockingSystem.hpp"

namespace Cogent::Editor::UI {

    class UIContext {
    public:
        static UIContext& Get() {
            static UIContext instance;
            return instance;
        }

        void Initialize(float screenWidth, float screenHeight);
        void Shutdown();

        CommandPalette& GetCommandPalette() { return *_commandPalette; }
        DockingSystem& GetDockingSystem() { return *_dockingSystem; }

    private:
        UIContext() = default;

        std::unique_ptr<CommandPalette> _commandPalette;
        std::unique_ptr<DockingSystem> _dockingSystem;
    };

}
