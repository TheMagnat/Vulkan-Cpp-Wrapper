
#pragma once

#include <vulkan/vulkan.h>

#include <VulkanObjects/Device.hpp>
#include <VulkanObjects/CommandPool.hpp>

#include <vector>
#include <stdexcept>

class CommandBuffers {

    public:
        CommandBuffers(uint16_t framesInFlight, Device const& device, CommandPool const& commandPool) {
            initializeCommandBuffers(framesInFlight, device, commandPool);
        };

        void initializeCommandBuffers(uint16_t framesInFlight, Device const& device, CommandPool const& commandPool) {
            commandBuffers.resize(framesInFlight);

            VkCommandBufferAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocInfo.commandPool = commandPool.get();
            allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocInfo.commandBufferCount = (uint32_t) commandBuffers.size();

            if (vkAllocateCommandBuffers(device.get(), &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
                throw std::runtime_error("Failed to allocate command buffers !");
            }

        }

        std::vector<VkCommandBuffer> const& get() const {
            return commandBuffers;
        }
        

    private:
	    std::vector<VkCommandBuffer> commandBuffers;


};
