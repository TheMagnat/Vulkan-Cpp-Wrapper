
#include <vulkan/vulkan.h>

#include <VulkanObjects/Helper/Buffer.hpp>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <vector>
#include <array>
#include <numeric>

struct VertexData {

    VertexData(VkDevice device, VkPhysicalDevice physicalDevice, std::vector<uint32_t> const& attributesSize)
        : device_(device), physicalDevice_(physicalDevice), attributesSize_(attributesSize)
    {
        totalSize_ = std::reduce(attributesSize_.begin(), attributesSize_.end());
        generateBindingDescription();
        generateAttributeDescriptions();
    }

    ~VertexData() {
        if (vertexBuffer_) {
            vkDestroyBuffer(device_, vertexBuffer_, nullptr);
            vertexBuffer_ = nullptr;
        }

		if (vertexBufferMemory_) {
            vkFreeMemory(device_, vertexBufferMemory_, nullptr);
            vertexBufferMemory_ = nullptr;
        }

        if (indexBuffer_) {
            vkDestroyBuffer(device_, indexBuffer_, nullptr);
            indexBuffer_ = nullptr;
        }

		if (indexBufferMemory_) {
            vkFreeMemory(device_, indexBufferMemory_, nullptr);
            indexBufferMemory_ = nullptr;
        }
    }

    void setDevice(VkDevice device, VkPhysicalDevice physicalDevice) {
        device_ = device;
        physicalDevice_ = physicalDevice;
    }

    void bind(VkCommandBuffer commandBuffer) const {
        VkBuffer vertexBuffers[] = {vertexBuffer_};
		VkDeviceSize offsets[] = {0};
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer_, 0, VK_INDEX_TYPE_UINT32);
    }

    void draw(VkCommandBuffer commandBuffer) const {
        //Command buffer, number of indices, number of instances (commonly 1), vertices start index offset, instances start index offset
		vkCmdDrawIndexed(commandBuffer, indicesSize_, 1, 0, 0, 0);
    }

    VkResult setData(std::vector<float> const& vertices, std::vector<uint32_t> const& indices, VkCommandPool commandPool, VkQueue queue) {

        void* data;

        //// Create vertex buffer
        VkDeviceSize vertexBufferSize = sizeof(float) * vertices.size();

		VkBuffer stagingVertexBuffer;
		VkDeviceMemory stagingVertexBufferMemory;
		Buffer::create(device_, physicalDevice_, vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingVertexBuffer, stagingVertexBufferMemory);

		vkMapMemory(device_, stagingVertexBufferMemory, 0, vertexBufferSize, 0, &data);
		memcpy(data, vertices.data(), (size_t) vertexBufferSize);

		vkUnmapMemory(device_, stagingVertexBufferMemory);

		Buffer::create(device_, physicalDevice_, vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer_, vertexBufferMemory_);

		Buffer::copy(device_, commandPool, queue, stagingVertexBuffer, vertexBuffer_, vertexBufferSize);

		vkDestroyBuffer(device_, stagingVertexBuffer, nullptr);
		vkFreeMemory(device_, stagingVertexBufferMemory, nullptr);

        //// Create index buffer
        VkDeviceSize indexBufferSize = sizeof(uint32_t) * indices.size();

        VkBuffer stagingIndexBuffer;
        VkDeviceMemory stagingIndexBufferMemory;
        Buffer::create(device_, physicalDevice_, indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingIndexBuffer, stagingIndexBufferMemory);

        vkMapMemory(device_, stagingIndexBufferMemory, 0, indexBufferSize, 0, &data);
        memcpy(data, indices.data(), (size_t) indexBufferSize);
        vkUnmapMemory(device_, stagingIndexBufferMemory);

        Buffer::create(device_, physicalDevice_, indexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer_, indexBufferMemory_);

        Buffer::copy(device_, commandPool, queue, stagingIndexBuffer, indexBuffer_, indexBufferSize);

        vkDestroyBuffer(device_, stagingIndexBuffer, nullptr);
        vkFreeMemory(device_, stagingIndexBufferMemory, nullptr);

        indicesSize_ = indices.size();

        return VK_SUCCESS;

    }
    
    VkVertexInputBindingDescription const& getBindingDescription() const {
        return bindingDescription_;
    }

    std::vector<VkVertexInputAttributeDescription> const& getAttributeDescriptions() const {
        return attributeDescriptions_;
    }

    void generateBindingDescription() {
        
        bindingDescription_.binding = 0;
        bindingDescription_.stride = sizeof(float) * totalSize_;
        bindingDescription_.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    }

    void generateAttributeDescriptions() {

        attributeDescriptions_.resize( attributesSize_.size() );

        for (uint32_t i = 0, acc = 0; i < attributeDescriptions_.size(); ++i) {
            
            uint32_t numberOfFloat = attributesSize_[i];

            attributeDescriptions_[i].binding = 0;
            attributeDescriptions_[i].location = i;

            if      (numberOfFloat == 1) attributeDescriptions_[i].format = VK_FORMAT_R32_SFLOAT;
            else if (numberOfFloat == 2) attributeDescriptions_[i].format = VK_FORMAT_R32G32_SFLOAT;
            else if (numberOfFloat == 3) attributeDescriptions_[i].format = VK_FORMAT_R32G32B32_SFLOAT;
            else if (numberOfFloat == 4) attributeDescriptions_[i].format = VK_FORMAT_R32G32B32A32_SFLOAT;
            else throw std::runtime_error("One of the attributes size is not legal !");
            
            attributeDescriptions_[i].offset = acc * sizeof(float);

            acc += numberOfFloat;

        }

    }

    private:

        VkDevice device_;
        VkPhysicalDevice physicalDevice_;

        VkBuffer vertexBuffer_ = nullptr;
        VkDeviceMemory vertexBufferMemory_ = nullptr;

        VkBuffer indexBuffer_ = nullptr;
        VkDeviceMemory indexBufferMemory_ = nullptr;


        std::vector<uint32_t> attributesSize_;
        uint32_t totalSize_;
        uint32_t indicesSize_ = 0;

        VkVertexInputBindingDescription bindingDescription_;
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions_;

};
