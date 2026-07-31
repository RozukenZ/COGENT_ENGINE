#pragma once
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

namespace Cogent::Editor::UI {

    struct EditorCommand {
        std::string id;
        std::string displayName;
        std::string shortcut;
        std::function<void()> executeCallback;
    };

    struct CommandSearchResult {
        const EditorCommand* command;
        int matchScore;
    };

    class CommandPalette {
    public:
        // Register a command into the engine
        void RegisterCommand(const std::string& id, const std::string& displayName, const std::string& shortcut, std::function<void()> callback);

        // Fuzzy match search for quick navigation
        std::vector<CommandSearchResult> Search(const std::string& query);

        // Execute by ID
        bool Execute(const std::string& id);

    private:
        std::unordered_map<std::string, EditorCommand> _commandRegistry;

        // Basic string matching score algorithm
        int CalculateFuzzyScore(const std::string& query, const std::string& target);
    };

}
