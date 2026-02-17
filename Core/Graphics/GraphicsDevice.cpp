#include "GraphicsDevice.hpp"
#include <set>
#include <string>
#include <algorithm>
#include "../Logger.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

QueueFamilyIndices GraphicsDevice::findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) {
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto& queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) {
            indices.computeFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

        if (presentSupport) {
            indices.presentFamily = i;
        }

        if (indices.isComplete()) break;
        i++;
    }

    return indices;
}

bool GraphicsDevice::isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface) {
    QueueFamilyIndices indices = findQueueFamilies(device, surface);
    
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(device, &props);
    LOG_INFO("Checking GPU: " + std::string(props.deviceName));

    if (!indices.graphicsFamily.has_value()) {
        LOG_WARN("  - Missing Graphics Queue");
    }
    if (!indices.presentFamily.has_value()) {
        LOG_WARN("  - Missing Present Queue");
    }
    if (!indices.computeFamily.has_value()) {
        LOG_WARN("  - Missing Compute Queue");
    }
    
    // Check extensions
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    if (!requiredExtensions.empty()) {
        LOG_WARN("  - Missing Swapchain Extension");
        for (const auto& ext : requiredExtensions) LOG_WARN("    * " + ext);
        return false;
    }

    // Check Swapchain Support (Formats/PresentModes)
    // This requires detailed query logic, avoiding for brevity unless necessary.
    // For now, extensions + queues are sufficient for a "Found" check.

    bool suitable = indices.isComplete() && requiredExtensions.empty();
    if (suitable) LOG_INFO("  - Suitable!");
    else LOG_WARN("  - Not Suitable");
    
    return suitable;
}

void GraphicsDevice::init(VkSurfaceKHR surface) {
    pickPhysicalDevice(surface);
    createLogicalDevice(surface);
    createCommandPool();
}

GraphicsDevice::GraphicsDevice(bool enableValidation) : enableValidationLayers(enableValidation) {
    createInstance();
}

GraphicsDevice::~GraphicsDevice() {
    // Often empty, explicit cleanup preferred
}

void GraphicsDevice::createInstance() {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "COGENT Engine";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "COGENT";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = glfwExtensionCount;
    createInfo.ppEnabledExtensionNames = glfwExtensions;

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan Instance!");
    }
}

void GraphicsDevice::pickPhysicalDevice(VkSurfaceKHR surface) {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0) throw std::runtime_error("Failed to find GPUs with Vulkan support!");

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    int bestScore = -1;
    VkPhysicalDevice bestDevice = VK_NULL_HANDLE;

    for (const auto& dev : devices) {
        if (!isDeviceSuitable(dev, surface)) continue;
        
        VkPhysicalDeviceProperties deviceProperties;
        VkPhysicalDeviceFeatures deviceFeatures;
        vkGetPhysicalDeviceProperties(dev, &deviceProperties);
        vkGetPhysicalDeviceFeatures(dev, &deviceFeatures);

        int score = 0;

        // Discrete GPUs have a significant performance advantage
        if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            score += 1000;
        }

        // Maximum possible size of textures affects graphics quality
        score += deviceProperties.limits.maxImageDimension2D;

        // Application can't function without geometry shaders (if needed, example)
        // if (!deviceFeatures.geometryShader) {
        //     score = 0;
        // }

        LOG_INFO("Found GPU: " + std::string(deviceProperties.deviceName) + " (Score: " + std::to_string(score) + ")");

        if (score > bestScore) {
            bestDevice = dev;
            bestScore = score;
        }
    }
    
    if (bestDevice != VK_NULL_HANDLE) {
        physicalDevice = bestDevice;
    } else {
        throw std::runtime_error("Failed to find a suitable GPU!");
    }
    
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physicalDevice, &props);
    LOG_INFO("RHI: Selected Best GPU: " + std::string(props.deviceName));
}

void GraphicsDevice::createLogicalDevice(VkSurfaceKHR surface) {
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice, surface);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1; // We only need 1 queue for now
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = VK_TRUE; 

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;

    const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Logical Device!");
    }

    vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
}

void GraphicsDevice::createCommandPool() {
    // We assume graphics queue here.
    // In a real RHI we store the index.
    // HACK: Re-find indices. Ideally store them in class.
    // But since we don't have Surface stored, we can't completely re-find.
    // However, for Graphics queue we only need device.
    
    // Better solutions: Store graphicsFamilyIndex in class.
    // For now, let's query just graphics family.
    
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

    int graphicsFamilyIndex = -1;
    for (int i = 0; i < queueFamilies.size(); i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphicsFamilyIndex = i;
            break;
        }
    }
    
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = graphicsFamilyIndex;

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Command Pool!");
    }
}

uint32_t GraphicsDevice::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("Failed to find suitable memory type!");
}
