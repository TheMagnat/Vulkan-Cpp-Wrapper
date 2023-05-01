#pragma once

#include <vulkan/vulkan.h>

#include <VulkanObjects/Helper/Buffer.hpp>
#include <VulkanObjects/Helper/Image.hpp>

#include <vector>


class Texture {

    public:

        struct TextureInformations {
            uint32_t binding;
            uint32_t width;
            uint32_t height;
            VkShaderStageFlags flags;
        };

        Texture(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue,
            std::vector<uint8_t> const& data, TextureInformations const& textureInformations)
            : device_(device), size_(data.size()), textureInformations_(textureInformations) {
            
            createTextureBuffers(physicalDevice, commandPool, queue, data);
            textureImageView_= Image::createImageView(device_, textureImage_, VK_FORMAT_R8G8B8A8_SRGB);
            createTextureSampler();
        }

        ~Texture() {
            if(textureSampler_) {
                vkDestroySampler(device_, textureSampler_, nullptr);
                textureSampler_ = nullptr;
            }
            if (textureImageView_) {
                vkDestroyImageView(device_, textureImageView_, nullptr);
                textureImageView_ = nullptr;
            }
            if (textureImage_) {
                vkDestroyImage(device_, textureImage_, nullptr);
                textureImage_ = nullptr;
            }
            if (textureImageMemory_) {
                vkFreeMemory(device_, textureImageMemory_, nullptr);
                textureImageMemory_ = nullptr;
            }
        }

        void createTextureBuffers(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue, std::vector<uint8_t> const& data) {
            
            VkBuffer stagingBuffer;
            VkDeviceMemory stagingBufferMemory;

            Buffer::create(device_, physicalDevice, size_, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

            void* mappedMemory;
            vkMapMemory(device_, stagingBufferMemory, 0, size_, 0, &mappedMemory);
            memcpy(mappedMemory, data.data(), static_cast<size_t>(size_));
            vkUnmapMemory(device_, stagingBufferMemory);

            Buffer::createImage(device_, physicalDevice, textureInformations_.width, textureInformations_.height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage_, textureImageMemory_); 

            // Change the organisation of the image to optimize the data reception
            Image::transitionImageLayout(device_, commandPool, queue, textureImage_, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            // Copy the buffer into the image
            Buffer::copyToImage(device_, commandPool, queue, stagingBuffer, textureImage_, textureInformations_.width, textureInformations_.height);

            // Then change again the organisation of the image after the copy to optimize the read in the shader
            Image::transitionImageLayout(device_, commandPool, queue, textureImage_, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            vkDestroyBuffer(device_, stagingBuffer, nullptr);
            vkFreeMemory(device_, stagingBufferMemory, nullptr);

        }

        void createTextureSampler() {

            VkSamplerCreateInfo samplerInfo{};
            samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            samplerInfo.magFilter = VK_FILTER_LINEAR;
            samplerInfo.minFilter = VK_FILTER_LINEAR;

            samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

            samplerInfo.anisotropyEnable = VK_TRUE;
            samplerInfo.maxAnisotropy = 16.0f;

            samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

            //Si vrai coordonnée [0 - size[
            samplerInfo.unnormalizedCoordinates = VK_FALSE;

            samplerInfo.compareEnable = VK_FALSE;
            samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

            samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            samplerInfo.mipLodBias = 0.0f;
            samplerInfo.minLod = 0.0f;
            samplerInfo.maxLod = 0.0f;

            if (vkCreateSampler(device_, &samplerInfo, nullptr, &textureSampler_) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create sampler !");
            }

        }

        TextureInformations const& getInformations() const {
            return textureInformations_;
        }

        //TODO: verify const keyword
        const VkImageView getImageView() const {
            return textureImageView_;
        }

        const VkSampler getSampler() const {
            return textureSampler_;
        }

    private:
        VkDeviceSize size_;
        TextureInformations textureInformations_;
        
        //Saved vulkan objects
        VkDevice device_;
        // VkCommandPool commandPool_;
        // VkQueue queue_;

        VkImage textureImage_;
        VkDeviceMemory textureImageMemory_;
        VkImageView textureImageView_;
        VkSampler textureSampler_;

};
