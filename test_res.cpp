#include <iostream>
#include <cassert>
#include "Resources/ResourceManager.hpp"

using namespace Cogent::Resources;

void TestResourceManager() {
    ResourceManager::Get().Init(VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, nullptr);
    
    // Simulate loading texture A
    auto texA1 = ResourceManager::Get().GetTexture("hero.png");
    assert(texA1 != nullptr);
    assert(ResourceManager::Get().GetActiveResourceCount() == 1);
    
    // Simulate another part of engine requesting texture A
    auto texA2 = ResourceManager::Get().GetTexture("hero.png");
    
    // Pointer addresses must match exactly (No duplicate loading)
    assert(texA1.get() == texA2.get());
    
    // Load Texture B
    auto texB = ResourceManager::Get().GetTexture("enemy.png");
    assert(ResourceManager::Get().GetActiveResourceCount() == 2);
    
    // Drop reference to Texture B (it goes out of scope here)
    texB.reset(); 
    
    // Run Garbage Collection
    ResourceManager::Get().UnloadUnused();
    
    // Since texB has use_count == 1 (only held by Manager), it should be erased.
    // texA1 and texA2 are still holding "hero.png" (use_count > 1).
    assert(ResourceManager::Get().GetActiveResourceCount() == 1);
    
    // Drop references to A
    texA1.reset();
    texA2.reset();
    
    // GC again
    ResourceManager::Get().UnloadUnused();
    assert(ResourceManager::Get().GetActiveResourceCount() == 0);
    
    std::cout << "[PASS] Resource Manager GC & Registry Test.\n";
}

int main() {
    std::cout << "--- RESOURCE MANAGER TESTS ---\n";
    TestResourceManager();
    std::cout << "All Resource Manager tests passed successfully.\n";
    return 0;
}
