#include "CommandPalette.hpp"
#include <algorithm>
#include <cctype>

namespace Cogent::Editor::UI {

    void CommandPalette::RegisterCommand(const std::string& id, const std::string& displayName, const std::string& shortcut, std::function<void()> callback) {
        _commandRegistry[id] = {id, displayName, shortcut, callback};
    }

    bool CommandPalette::Execute(const std::string& id) {
        auto it = _commandRegistry.find(id);
        if (it != _commandRegistry.end()) {
            it->second.executeCallback();
            return true;
        }
        return false;
    }

    int CommandPalette::CalculateFuzzyScore(const std::string& query, const std::string& target) {
        if (query.empty()) return 100;
        
        int score = 0;
        size_t queryIdx = 0;
        
        // Very basic fuzzy match: checking if characters exist in order
        for (size_t i = 0; i < target.length() && queryIdx < query.length(); ++i) {
            if (std::tolower(target[i]) == std::tolower(query[queryIdx])) {
                score += 10; // Match reward
                // Consecutive match reward could be added here
                queryIdx++;
            }
        }
        
        // If we didn't match all query chars, return 0
        if (queryIdx < query.length()) return 0;
        
        // Exact prefix match bonus
        if (target.length() >= query.length()) {
            bool prefixMatch = true;
            for (size_t i = 0; i < query.length(); ++i) {
                if (std::tolower(target[i]) != std::tolower(query[i])) {
                    prefixMatch = false;
                    break;
                }
            }
            if (prefixMatch) score += 50;
        }

        return score;
    }

    std::vector<CommandSearchResult> CommandPalette::Search(const std::string& query) {
        std::vector<CommandSearchResult> results;
        
        for (const auto& pair : _commandRegistry) {
            int score = CalculateFuzzyScore(query, pair.second.displayName);
            if (score > 0) {
                results.push_back({&pair.second, score});
            }
        }

        // Sort descending by score
        std::sort(results.begin(), results.end(), [](const CommandSearchResult& a, const CommandSearchResult& b) {
            return a.matchScore > b.matchScore;
        });

        return results;
    }
}
