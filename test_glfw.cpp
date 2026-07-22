#include <iostream>
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

int main() {
    std::cout << "Init GLFW..." << std::endl;
    if (!glfwInit()) {
        std::cerr << "Failed to init GLFW!" << std::endl;
        return 1;
    }
    std::cout << "GLFW Initialized. Checking Vulkan support..." << std::endl;
    if (!glfwVulkanSupported()) {
        std::cerr << "Vulkan not supported!" << std::endl;
        return 1;
    }
    
    std::cout << "Getting extensions..." << std::endl;
    uint32_t count = 0;
    const char** exts = glfwGetRequiredInstanceExtensions(&count);
    
    std::cout << "Extensions: " << count << std::endl;
    for (uint32_t i = 0; i < count; i++) {
        std::cout << " - " << exts[i] << std::endl;
    }
    
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Test GLFW";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = count;
    createInfo.ppEnabledExtensionNames = exts;

    VkInstance instance;
    std::cout << "Calling vkCreateInstance..." << std::endl;
    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        std::cerr << "vkCreateInstance failed!" << std::endl;
        return 1;
    }
    std::cout << "vkCreateInstance success!" << std::endl;
    
    glfwTerminate();
    std::cout << "Success!" << std::endl;
    return 0;
}
