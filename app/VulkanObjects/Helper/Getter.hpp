
#pragma once

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <vector>
#include <set>

class Getter {

    public:

        static std::vector<const char*> requiredExtensions(bool enableValidationLayers) {
            uint32_t glfwExtensionCount = 0;
            const char** glfwExtensions;
            glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

            std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

            if (enableValidationLayers) {
                extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            }

            return extensions;
        }


        static std::vector<VkExtensionProperties> availableExtensions() {

            uint32_t extensionCount = 0;
            vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

            std::vector<VkExtensionProperties> extensions(extensionCount);

            vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

            return extensions;

        }

};
