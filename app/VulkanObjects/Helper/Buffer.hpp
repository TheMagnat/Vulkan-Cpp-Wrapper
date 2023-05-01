
#pragma once

#include <vulkan/vulkan.h>

#include <VulkanObjects/Helper/Command.hpp>

#include <stdexcept>


class Buffer {

    public:

        static void createImage(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) {
            
            VkImageCreateInfo imageInfo{};
            imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imageInfo.imageType = VK_IMAGE_TYPE_2D;
            imageInfo.extent.width = width;
            imageInfo.extent.height = height;
            imageInfo.extent.depth = 1;
            imageInfo.mipLevels = 1;
            imageInfo.arrayLayers = 1;

            imageInfo.format = format;

            //TODO: voir l'impact de changer ça pour "VK_IMAGE_TILING_LINEAR" 
            imageInfo.tiling = tiling;

            imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            
            imageInfo.usage = usage;

            imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            imageInfo.flags = 0; // Optional

            if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create image !");
            }

            VkMemoryRequirements memRequirements;
            vkGetImageMemoryRequirements(device, image, &memRequirements);

            VkMemoryAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.allocationSize = memRequirements.size;
            allocInfo.memoryTypeIndex = Buffer::findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

            if (vkAllocateMemory(device, &allocInfo, nullptr, & imageMemory) != VK_SUCCESS) {
                throw std::runtime_error("Failed to allocate memory for the image !");
            }

            vkBindImageMemory(device, image,  imageMemory, 0);

        }

        static void create(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
            VkBufferCreateInfo bufferInfo{};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = size;
            bufferInfo.usage = usage;
            bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create a buffer !");
            }

            VkMemoryRequirements memRequirements;
            vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

            VkMemoryAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.allocationSize = memRequirements.size;
            allocInfo.memoryTypeIndex = Buffer::findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

            if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
                throw std::runtime_error("Failed to allocate memory !");
            }

            vkBindBufferMemory(device, buffer, bufferMemory, 0);
        }

        static uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
            
            VkPhysicalDeviceMemoryProperties memProperties;
            vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

            for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
                if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                    return i;
                }
            }

            throw std::runtime_error("No type of memory satisfies the buffer !");


        }

        static void copy(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {

            VkCommandBuffer commandBuffer = Command::beginSingleTimeCommands(device, commandPool);

            VkBufferCopy copyRegion{};
            // copyRegion.srcOffset = 0; // Optional
            // copyRegion.dstOffset = 0; // Optional
            copyRegion.size = size;
            vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

            // End recording
            Command::endSingleTimeCommands(device, commandPool, graphicsQueue, commandBuffer);
        }

        static void copyToImage(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
            
            VkCommandBuffer commandBuffer = Command::beginSingleTimeCommands(device, commandPool);

            VkBufferImageCopy region{};
            region.bufferOffset = 0;
            region.bufferRowLength = 0;
            region.bufferImageHeight = 0;

            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.mipLevel = 0;
            region.imageSubresource.baseArrayLayer = 0;
            region.imageSubresource.layerCount = 1;

            region.imageOffset = {0, 0, 0};
            region.imageExtent = {
                width,
                height,
                1
            };

            vkCmdCopyBufferToImage(
                commandBuffer,
                buffer,
                image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &region
            );

            Command::endSingleTimeCommands(device, commandPool, graphicsQueue, commandBuffer);

	}

};
