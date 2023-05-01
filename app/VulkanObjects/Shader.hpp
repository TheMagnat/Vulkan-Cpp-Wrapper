
#pragma once

#include <vulkan/vulkan.h>

#include <VulkanObjects/Texture.hpp>

#include <VulkanObjects/Helper/Buffer.hpp>

#include <vector>

struct UniformInformations {
    uint32_t binding;
    VkDeviceSize bufferSize;
    VkShaderStageFlags flags;
};

struct UniformBufferWrapper {

    UniformBufferWrapper(uint16_t nbFrames, UniformInformations const& uniformInformationP)
        : informations(uniformInformationP), uniformBuffers(nbFrames), uniformBuffersMemory(nbFrames), uniformBuffersMapped(nbFrames) {}

    void allocate(VkDevice device, VkPhysicalDevice physicalDevice) {

        for (size_t i = 0; i < uniformBuffers.size(); i++) {
			Buffer::create(device, physicalDevice, informations.bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i], uniformBuffersMemory[i]);

			vkMapMemory(device, uniformBuffersMemory[i], 0, informations.bufferSize, 0, &uniformBuffersMapped[i]);
		}

        allocated_ = true;

    }

    void deallocate(VkDevice device) {
        if (allocated_) {

            for (size_t i = 0; i < uniformBuffers.size(); i++) {
                vkDestroyBuffer(device, uniformBuffers[i], nullptr);
                vkFreeMemory(device, uniformBuffersMemory[i], nullptr);
            }

            allocated_ = false;
        }
    }

    UniformInformations informations;

    //Uniforms memory (Vulkan objects)
    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;

    bool allocated_ = false;

};

class Shader {

    public:
        Shader(VkDevice device, VkPhysicalDevice physicalDevice, uint16_t nbFrames)
            : device_(device), physicalDevice_(physicalDevice), nbFrames_(nbFrames) {

        }

        ~Shader() {
            
            for (UniformBufferWrapper& uniformBufferWrapper : uniformBufferWrappers_)
                uniformBufferWrapper.deallocate(device_);

            if(descriptorPool_){
                vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
                descriptorPool_ = nullptr;
            }

            if(descriptorSetLayout_){
                vkDestroyDescriptorSetLayout(device_, descriptorSetLayout_, nullptr);
                descriptorSetLayout_ = nullptr;
            }
            
            if(pipelineLayout_){
		        vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
                pipelineLayout_ = nullptr;
            }

        }

        void setDevice(VkDevice device, VkPhysicalDevice physicalDevice) {
            device_ = device;
            physicalDevice_ = physicalDevice;
        }

        //Add uniforms to the shader structure and allocate the memory for them
        void addUniformBufferObjects(std::vector<UniformInformations> const& uniformsInformation) {

            for (UniformInformations const& uniformInformation : uniformsInformation) {
                uniformBufferWrappers_.emplace_back(nbFrames_, uniformInformation);
                uniformBufferWrappers_.back().allocate(device_, physicalDevice_);
            }

            nbUniforms_ += uniformsInformation.size();

        }

        void addTexture(VkCommandPool commandPool, VkQueue queue, std::vector<uint8_t> const& texture, Texture::TextureInformations const& textureInformations) {

            textures_.emplace_back(
                device_, physicalDevice_, commandPool, queue,
                texture, textureInformations
            );
            
        }


        void generateBindingsAndSets() {
            createDescriptorSetLayout();
            createPipelineLayout();
            createDescriptorPool();
            createDescriptorSets();
        }

        ///  -> Finishing after all uniform creation
        //CPU
        void createDescriptorSetLayout() {

            std::vector<VkDescriptorSetLayoutBinding> layoutBindings(nbUniforms_ + textures_.size());

            // Set the uniforms layout bindings
            for (size_t i = 0; i < nbUniforms_; ++i) {

                layoutBindings[i].binding = uniformBufferWrappers_[i].informations.binding;
                layoutBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                layoutBindings[i].descriptorCount = 1; //If array, put more than 1
                layoutBindings[i].pImmutableSamplers = nullptr; // Optional

                // Precise in which shader we will use the uniform
                layoutBindings[i].stageFlags = uniformBufferWrappers_[i].informations.flags;

            }

            // Set the texture layout bindings
            for (size_t i = 0; i < textures_.size(); ++i) {
                
                size_t currentIndex = nbUniforms_ + i;

                Texture::TextureInformations const& currentTextureInformations = textures_[i].getInformations();

                layoutBindings[currentIndex].binding = currentTextureInformations.binding;
                layoutBindings[currentIndex].descriptorCount = 1;
                layoutBindings[currentIndex].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                layoutBindings[currentIndex].pImmutableSamplers = nullptr;

                // Precise in which shader we will use the texture
                layoutBindings[currentIndex].stageFlags = currentTextureInformations.flags;

            }

            VkDescriptorSetLayoutCreateInfo layoutInfo{};
            layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layoutInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
            layoutInfo.pBindings = layoutBindings.data();

            if (vkCreateDescriptorSetLayout(device_, &layoutInfo, nullptr, &descriptorSetLayout_) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create a descriptor set !");
            }

        }

        void createPipelineLayout() {

            VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
            pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutInfo.setLayoutCount = 1;
            pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout_;
            pipelineLayoutInfo.pushConstantRangeCount = 0;    // Optional
            pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

            if (vkCreatePipelineLayout(device_, &pipelineLayoutInfo, nullptr, &pipelineLayout_) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create the pipeline layout !");
            }
        }

        //GPU
        void createDescriptorPool() {

            std::array<VkDescriptorPoolSize, 2> poolSizes{};
            poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            poolSizes[0].descriptorCount = static_cast<uint32_t>(nbUniforms_ * nbFrames_);
            
            poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            poolSizes[1].descriptorCount = static_cast<uint32_t>(textures_.size() * nbFrames_);

            VkDescriptorPoolCreateInfo poolInfo{};
            poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            
            poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
            poolInfo.pPoolSizes = poolSizes.data();

            //Note: This represent the max values of set that we can allocate.
            //A set is a "set of uniforms" that the shaders will have access to. So here we create 1 set for each frame.
            poolInfo.maxSets = static_cast<uint32_t>(nbFrames_);

            if (vkCreateDescriptorPool(device_, &poolInfo, nullptr, &descriptorPool_) != VK_SUCCESS) {
                throw std::runtime_error("failed to create descriptor pool !");
            }

        }

        void createDescriptorSets() {

            std::vector<VkDescriptorSetLayout> layouts(nbFrames_, descriptorSetLayout_);

            VkDescriptorSetAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool = descriptorPool_;
            allocInfo.descriptorSetCount = static_cast<uint32_t>(nbFrames_);
            allocInfo.pSetLayouts = layouts.data();

            descriptorSets_.resize(nbFrames_);
            if (vkAllocateDescriptorSets(device_, &allocInfo, descriptorSets_.data()) != VK_SUCCESS) {
                throw std::runtime_error("failed to allocate descriptor sets !");
            }

            for (size_t frameIndex = 0; frameIndex < nbFrames_; frameIndex++) {

                std::vector<VkDescriptorBufferInfo> buffersInfos(nbUniforms_);
                std::vector<VkDescriptorImageInfo> imagesInfos(textures_.size());

                std::vector<VkWriteDescriptorSet> writeDescriptors(nbUniforms_ + textures_.size());
                
                // Uniform descriptors
                for (size_t uniformIndex = 0; uniformIndex < nbUniforms_; uniformIndex++) {

                    UniformBufferWrapper const& uniformBufferWrapper = uniformBufferWrappers_[uniformIndex];

                    // VkDescriptorBufferInfo bufferInfo{};
                    buffersInfos[uniformIndex].buffer = uniformBufferWrapper.uniformBuffers[frameIndex];
                    buffersInfos[uniformIndex].offset = 0;
                    buffersInfos[uniformIndex].range = uniformBufferWrapper.informations.bufferSize;

                    // VkWriteDescriptorSet descriptorWrite{};
                    writeDescriptors[uniformIndex].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    writeDescriptors[uniformIndex].dstSet = descriptorSets_[frameIndex];
                    writeDescriptors[uniformIndex].dstBinding = uniformBufferWrapper.informations.binding; //Binding in shader
                    writeDescriptors[uniformIndex].dstArrayElement = 0;

                    writeDescriptors[uniformIndex].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                    writeDescriptors[uniformIndex].descriptorCount = 1;

                    writeDescriptors[uniformIndex].pBufferInfo = &buffersInfos[uniformIndex];

                }

                // Texture descriptors
                for (size_t textureIndex = 0; textureIndex < textures_.size(); ++textureIndex) {
                    
                    size_t currentIndex = nbUniforms_ + textureIndex;

                    Texture const& currentTexture = textures_[textureIndex];

                    imagesInfos[textureIndex].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    imagesInfos[textureIndex].imageView = currentTexture.getImageView();
                    imagesInfos[textureIndex].sampler = currentTexture.getSampler();

                    writeDescriptors[currentIndex].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    writeDescriptors[currentIndex].dstSet = descriptorSets_[frameIndex];
                    writeDescriptors[currentIndex].dstBinding = currentTexture.getInformations().binding;
                    writeDescriptors[currentIndex].dstArrayElement = 0;

                    writeDescriptors[currentIndex].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    writeDescriptors[currentIndex].descriptorCount = 1;

                    writeDescriptors[currentIndex].pImageInfo = &imagesInfos[textureIndex];

                }

                vkUpdateDescriptorSets(device_, static_cast<uint32_t>(writeDescriptors.size()), writeDescriptors.data(), 0, nullptr);

            }

        }
        /// <-

        inline void bind(VkCommandBuffer commandBuffer, uint32_t frameIndex) const {
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &descriptorSets_[frameIndex], 0, nullptr);
        }

        inline void updateUniform(size_t uniformIndex, uint32_t imageIndex, void * date, size_t dataSize) const {
		    memcpy(uniformBufferWrappers_[uniformIndex].uniformBuffersMapped[imageIndex], date, dataSize);
        }

        inline VkPipelineLayout getPipelineLayout() {
            return pipelineLayout_;
        }

    private:

        //Vulkan objects
        VkDevice device_;
        VkPhysicalDevice physicalDevice_;

        //Vulkan uniforms objects
        VkDescriptorSetLayout descriptorSetLayout_;
        VkDescriptorPool descriptorPool_;
        std::vector<VkDescriptorSet> descriptorSets_;
        VkPipelineLayout pipelineLayout_;

        uint16_t nbFrames_;

        //Uniforms memory
        std::vector<UniformBufferWrapper> uniformBufferWrappers_;
        size_t nbUniforms_ = 0;

        //Texture memory
        std::vector<Texture> textures_;
};
