#define VMA_IMPLEMENTATION
#define VMA_ASSERT
#define VMA_DEBUG_MARGIN 16
#define VMA_DEBUG_DETECT_CORRUPTION 1
#include <vk_mem_alloc.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/mat4x4.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>


#include <chrono>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>


#include <VulkanObjects/Window.hpp>
#include <VulkanObjects/VulkanWrapper.hpp>
#include <VulkanObjects/VertexData.hpp>
#include <VulkanObjects/Shader.hpp>


const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

const uint16_t MAX_FRAMES_IN_FLIGHT = 2;

#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif


const std::vector<float> vertices = {
    -0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
    0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
    -0.5f, 0.5f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f,

	-0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    0.5f, -0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
    0.5f, 0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
    -0.5f, 0.5f, -0.5f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f

};

const std::vector<uint32_t> indices = {
    0, 1, 2, 2, 3, 0,
    4, 5, 6, 6, 7, 4
};

struct UniformBufferObject {
	glm::vec2 foo;
    alignas(16) glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

bool DEPTH_CHECK = true;

class HelloTriangleApplication {
public:

	HelloTriangleApplication() : window_("Tesvoxel", WIDTH, HEIGHT), vulkanWrapper_(window_.get(), MAX_FRAMES_IN_FLIGHT, DEPTH_CHECK), vertexData_(vulkanWrapper_.generateVertexData()),
	testShader_(vulkanWrapper_.generateShader("Shaders/main.vert.spv", "Shaders/main.frag.spv")), testGraphicsPipeline_()
 {}

    void run() {

		auto test = sizeof(UniformBufferObject);
		
        initVulkan();

        // mainLoop();

		window_.setResizeCallback([this](int width, int height){ resizedCallback(width, height); });

		window_.setGraphicLoop([this](float deltaTime){newDrawFrame(deltaTime);});
		window_.startLoop();
		vulkanWrapper_.waitIdle();
    }

	void mainLoop() {
		while (!glfwWindowShouldClose(window_.get())) {
			glfwPollEvents();
			newDrawFrame(0.0);
		}

		vulkanWrapper_.waitIdle();
    }

	void newDrawFrame(float deltaTime) {

		static float timeFromStart = 0.0f;
		timeFromStart += deltaTime;

		uint32_t currentFrame = vulkanWrapper_.getCurrentFrame();

		VkCommandBuffer currentCommandBuffer = vulkanWrapper_.beginRecordingDraw();
		if (!currentCommandBuffer) {
			std::cout << "Can't acquire new image to start recording a draw." << std::endl;
			return;
		}

		std::cout << "delta time: " << deltaTime << std::endl;

		//Can be here or before "beginRecordingDraw"
		updateUniformBuffer(currentFrame, timeFromStart);
		
		testShader_.recordPushConstant(currentCommandBuffer, &timeFromStart, sizeof(timeFromStart));

		testGraphicsPipeline_.bind(currentCommandBuffer);
		testShader_.bind(currentCommandBuffer, currentFrame);
		vertexData_.bind(currentCommandBuffer);
		vertexData_.draw(currentCommandBuffer);

		//Bind pipeline
		//Bind shader
		//Bind vertexData
		//Draw vertexData

		vulkanWrapper_.endRecordingDraw();

	}

	void updateUniformBuffer(uint32_t currentFrame, float time) {
		
		VkExtent2D const& currentExtent = vulkanWrapper_.getExtent();

		UniformBufferObject ubo{};
		ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.proj = glm::perspective(glm::radians(45.0f), currentExtent.width / (float) currentExtent.height, 0.1f, 10.0f);
		ubo.proj[1][1] *= -1;

		// memcpy(uniformBuffersMapped[currentFrame], &ubo, sizeof(ubo));

		glm::vec3 color(1.0f, 0.0f, 0.0f);

		testShader_.updateUniform(0, currentFrame, &ubo, sizeof(ubo));
		testShader_.updateUniform(1, currentFrame, &color, sizeof(color));
		// testShader_.updateUniform(2, currentFrame, &time, sizeof(time));

	}

	void resizedCallback(int width, int height) {
		vulkanWrapper_.gotResized();
	}

private:
	
	Window window_;
	VulkanWrapper vulkanWrapper_;

	VertexData vertexData_;
	// Uniform buffers
	Shader testShader_;

	//The full graphic pipeline
	GraphicsPipeline testGraphicsPipeline_;

	bool framebufferResized = false;

	//Store the current available frame to start a draw
	uint32_t currentFrame = 0;

    void initVulkan() {
		
		// createUniformBuffers();
		testShader_.addUniformBufferObjects({
			{0, sizeof(UniformBufferObject), VK_SHADER_STAGE_VERTEX_BIT},
			{1, sizeof(glm::vec3), VK_SHADER_STAGE_VERTEX_BIT}
			// {2, sizeof(float), VK_SHADER_STAGE_VERTEX_BIT}
		});


		testShader_.setPushConstant({sizeof(float), VK_SHADER_STAGE_VERTEX_BIT});

		//Texture
		// createTextureImage(); //testShader_.addTexture(...);


		testShader_.generateBindingsAndSets();
		
		testGraphicsPipeline_ = vulkanWrapper_.generateGraphicsPipeline(testShader_, {3, 3, 2});
		vulkanWrapper_.saveGraphicPipeline(&testGraphicsPipeline_);

		createVertexBuffer();


    }


	void createTextureImage() {
		
		//Generate random image
		uint32_t texWidth = 100, texHeight = 100;
		VkDeviceSize imageSize = texWidth * texHeight * 4;

		std::vector<uint8_t> texture(imageSize);
		for(size_t y = 0; y < texHeight; ++y) {
			for(size_t x = 0; x < texWidth; ++x) {
				texture[y * (texWidth * 4) + (x * 4) + 0] = (((x + y)*3) % 201);
				texture[y * (texWidth * 4) + (x * 4) + 1] = (((x + y)*3) % 156);
				texture[y * (texWidth * 4) + (x * 4) + 2] = (((x + y)*3) % 256);
				texture[y * (texWidth * 4) + (x * 4) + 3] = 255;
			}
		}

		testShader_.addTexture( vulkanWrapper_.generateTexture(texture, {3, texWidth, texHeight, VK_SHADER_STAGE_FRAGMENT_BIT}) );


		// VkBuffer stagingBuffer;
		// VkDeviceMemory stagingBufferMemory;

		// Buffer::create(device, physicalDevice, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

		// void* data;
		// vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
		// memcpy(data, texture.data(), static_cast<size_t>(imageSize));
		// vkUnmapMemory(device, stagingBufferMemory);

		// Buffer::createImage(device, physicalDevice, texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory); 

		// // Change the organisation of the image
		// transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		// // Copy the buffer into the image
		// copyBufferToImage(stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));

		// vkDestroyBuffer(device, stagingBuffer, nullptr);
		// vkFreeMemory(device, stagingBufferMemory, nullptr);


	}

	void createVertexBuffer() {
		
		vertexData_.setData(vertices, indices);
	
	}


};



int main() {
    HelloTriangleApplication app;

    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "Exception received : " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
