#pragma once

#include <vulkan/vulkan.h>

#include <iostream>
#include <cassert>


//TODO: a mettre dans un namespace vide dans le .cpp
VkResult getInstanceAndCreateDebugMessenger(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}


class DebugMessenger {

    public:
        DebugMessenger() {}

    //Static calls
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugMessengerCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData) {

		std::cerr << "Validation layer: " << pCallbackData->pMessage << std::endl;

		return VK_FALSE;
	}

    static VkDebugUtilsMessengerCreateInfoEXT generateCreateInfo() {
		VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		//createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = debugMessengerCallback;

		return createInfo;
	}

    

    //Setup and clean phase
    void setup(VkInstance instance) {
        
        assert(instance != nullptr);
        assert(debugMessenger_ == nullptr);

        instance_ = instance;

		VkDebugUtilsMessengerCreateInfoEXT createInfo = DebugMessenger::generateCreateInfo();

		if (getInstanceAndCreateDebugMessenger(instance, &createInfo, nullptr, &debugMessenger_) != VK_SUCCESS) {
			throw std::runtime_error("failed to set up debug messenger!");
		}

	}

    void clean(const VkAllocationCallbacks* pAllocator = nullptr) {

        assert(instance_ != nullptr);

        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr) {
            func(instance_, debugMessenger_, pAllocator);
        }
    }

    

    private:
        VkDebugUtilsMessengerEXT debugMessenger_ = nullptr;

        //Save a pointer to the Vulkan instance
        VkInstance instance_ = nullptr;

};
