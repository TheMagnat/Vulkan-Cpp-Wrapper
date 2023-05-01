#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#define GLM_FORCE_RADIANS
#include <glm/mat4x4.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <optional>
#include <set>
#include <memory>

#include <VulkanObjects/Configuration.hpp>
#include <VulkanObjects/Helper/Checker.hpp>
#include <VulkanObjects/Helper/Getter.hpp>
#include <VulkanObjects/Helper/PhysicalDevices.hpp>
#include <VulkanObjects/Helper/QueueFamily.hpp>
#include <VulkanObjects/Helper/SwapChain.hpp>
#include <VulkanObjects/Helper/ShaderHelper.hpp>
#include <VulkanObjects/Helper/Buffer.hpp>
#include <VulkanObjects/Helper/Command.hpp>
#include <VulkanObjects/Helper/Image.hpp>
#include <VulkanObjects/VertexData.hpp>
#include <VulkanObjects/Shader.hpp>

#include <VulkanObjects/DebugMessenger.hpp>


const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

const uint16_t MAX_FRAMES_IN_FLIGHT = 2;

#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif


const std::vector<float> vertices = {
    -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
    0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
    0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
    -0.5f, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f
};

const std::vector<uint32_t> indices = {
    0, 1, 2, 2, 3, 0
};

struct UniformBufferObject {
	glm::vec2 foo;
    alignas(16) glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

class HelloTriangleApplication {
public:

	HelloTriangleApplication() : vertexData_(nullptr, nullptr, {2, 3, 2}), testShader_(nullptr, nullptr, MAX_FRAMES_IN_FLIGHT) {}

    void run() {

		auto test = sizeof(UniformBufferObject);

		//GLFW
		initWindow();

		//VULKAN INIT INSTANCE
        initVulkan();
		
        mainLoop();
        
		//VULKAN AND GLFW DESTROY
		cleanup();
    }

	void mainLoop() {
		while (!glfwWindowShouldClose(window)) {
			glfwPollEvents();
			drawFrame();
		}

		vkDeviceWaitIdle(device);
    }

private:

	VertexData vertexData_;

	void drawFrame() {
		
		//We wait the fence to be signaled...
    	vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

		uint32_t imageIndex;
		VkResult result = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			recreateSwapChain();
			return;
		} else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
			throw std::runtime_error("failed to acquire swap chain image!");
		}

		//...Then we set it to the un-signaled state. Note: we un-signal it only if we're sure that we will submit work with it
		vkResetFences(device, 1, &inFlightFences[currentFrame]);
		

		updateUniformBuffer(currentFrame);

		//We now reset and record again the command
		vkResetCommandBuffer(commandBuffers[currentFrame], 0);
        recordCommandBuffer(commandBuffers[currentFrame], imageIndex);

		

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

		VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
		VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;

		VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
			throw std::runtime_error("failed to submit draw command buffer !");
		}

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;


		VkSwapchainKHR swapChains[] = {swapChain};
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &imageIndex;

		presentInfo.pResults = nullptr; // Optionnel

		result = vkQueuePresentKHR(presentQueue, &presentInfo);
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
			framebufferResized = false;
			recreateSwapChain();
		} else if (result != VK_SUCCESS) {
			throw std::runtime_error("failed to present swap chain image!");
		}

		// Set the currentFrame to the next available frame
		currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

	}

	void updateUniformBuffer(uint32_t currentImage) {

		static auto startTime = std::chrono::high_resolution_clock::now();

		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

		UniformBufferObject ubo{};
		ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.proj = glm::perspective(glm::radians(45.0f), swapChainExtent.width / (float) swapChainExtent.height, 0.1f, 10.0f);
		ubo.proj[1][1] *= -1;

		// memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));

		glm::vec3 color(1.0f, 0.0f, 0.0f);

		testShader_.updateUniform(0, currentImage, &ubo, sizeof(ubo));
		testShader_.updateUniform(1, currentImage, &color, sizeof(color));
		testShader_.updateUniform(2, currentImage, &time, sizeof(time));

	}

	GLFWwindow* window;
	VkInstance instance;

	//Surface object, it will receive rendered images
	VkSurfaceKHR surface;

	//Note: this object will be destructed automaticaly by the instance destructor
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

	//Logical device
	VkDevice device;

	//Queues (note: the queues are implicitly cleaned up when the device is destroyed)
	VkQueue graphicsQueue;
	VkQueue presentQueue;

	//The swap chain. This will receive and present to the surface the rendered images
	VkSwapchainKHR swapChain;
	std::vector<VkImage> swapChainImages;
	std::vector<VkImageView> swapChainImageViews;

	//Store the formant and the extent
	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;
	
	VkRenderPass renderPass;

	//Shaders uniforms
	

	//The full graphic pipeline
	VkPipeline graphicsPipeline;
	
	// VkBuffer vertexBuffer;
	// VkDeviceMemory vertexBufferMemory;

	std::vector<VkFramebuffer> swapChainFramebuffers;

	// Manage the memory for the command buffers
	VkCommandPool commandPool;

	// These objects will store all the command before indicating Vulkan to execute them
	std::vector<VkCommandBuffer> commandBuffers;

	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	std::vector<VkFence> inFlightFences;

	// Uniform buffers
	Shader testShader_;

	// Texture
	// VkImage textureImage;
	// VkDeviceMemory textureImageMemory;
	// VkImageView textureImageView;
	// VkSampler textureSampler;

	bool framebufferResized = false;

	//Store the current available frame to start a draw
	uint32_t currentFrame = 0;

	//To show debug message in the standard output
	DebugMessenger debugMessenger_;


	void initWindow() {
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		//glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
		glfwSetWindowUserPointer(window, this);

		glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    }

	static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
		auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window));
		app->framebufferResized = true;
	}

    void initVulkan() {
		createInstance();

		//Activate the validation layer messenger if debugging
		if (enableValidationLayers) debugMessenger_.setup(instance);

		createSurface();

		pickPhysicalDevice();
		createLogicalDevice();

		//Swap chain and images
		createSwapChain();
		createImageViews();
		
		createRenderPass();

		createCommandPool();

		//Uniforms
		testShader_.setDevice(device, physicalDevice);
		
		// createUniformBuffers();
		auto testvaluelol = sizeof(glm::vec3);
		testShader_.addUniformBufferObjects({
			{0, sizeof(UniformBufferObject), VK_SHADER_STAGE_VERTEX_BIT},
			{1, sizeof(glm::vec3), VK_SHADER_STAGE_VERTEX_BIT},
			{2, sizeof(float), VK_SHADER_STAGE_VERTEX_BIT}
		});

		//Texture
		createTextureImage();
		// createTextureImageView();
		// createTextureSampler();

		testShader_.generateBindingsAndSets();
		// createDescriptorSetLayout();
		// createDescriptorPool();
		// createDescriptorSets();

		createGraphicsPipeline();

		createFramebuffers();

		

		vertexData_.setDevice(device, physicalDevice);




		createVertexBuffer();
		

		createCommandBuffers();

		createSyncObjects();
    }


	// void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {

	// 	VkCommandBuffer commandBuffer = Command::beginSingleTimeCommands(device, commandPool);

	// 	VkImageMemoryBarrier barrier{};
	// 	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	// 	barrier.oldLayout = oldLayout;
	// 	barrier.newLayout = newLayout;

	// 	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	// 	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

	// 	barrier.image = image;
	// 	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	// 	barrier.subresourceRange.baseMipLevel = 0;
	// 	barrier.subresourceRange.levelCount = 1;
	// 	barrier.subresourceRange.baseArrayLayer = 0;
	// 	barrier.subresourceRange.layerCount = 1;

	// 	VkPipelineStageFlags sourceStage;
	// 	VkPipelineStageFlags destinationStage;

	// 	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {

	// 		barrier.srcAccessMask = 0;
	// 		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

	// 		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	// 		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

	// 	} else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {

	// 		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	// 		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	// 		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	// 		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

	// 	}
	// 	else {
	// 		throw std::invalid_argument("Layout transition not supported !");
	// 	}

	// 	vkCmdPipelineBarrier(
	// 		commandBuffer,
	// 		sourceStage, destinationStage,
	// 		0,
	// 		0, nullptr,
	// 		0, nullptr,
	// 		1, &barrier
	// 	);

	// 	Command::endSingleTimeCommands(device, commandPool, graphicsQueue, commandBuffer);
	
	// }

	void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
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

		testShader_.addTexture(commandPool, graphicsQueue, texture, {3, texWidth, texHeight, VK_SHADER_STAGE_FRAGMENT_BIT});


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

		vertexData_.setData(vertices, indices, commandPool, graphicsQueue);

	}


	void recreateSwapChain() {

		int width = 0, height = 0;
		glfwGetFramebufferSize(window, &width, &height);
		while (width == 0 || height == 0) {
			glfwGetFramebufferSize(window, &width, &height);
			glfwWaitEvents();
		}

		vkDeviceWaitIdle(device);

		cleanupSwapChain();
		
		createSwapChain();
		createImageViews();
		createFramebuffers();
	}

	void createSyncObjects() {

		imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {

			if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
				vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
				vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS)  {

				throw std::runtime_error("Failed to create syncronization objects for a frame !");
			}

		}

	}

	void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = 0; // Optional
		beginInfo.pInheritanceInfo = nullptr; // Optional

		if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
			throw std::runtime_error("failed to begin recording command buffer!");
		}

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = renderPass;
		renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];

		renderPassInfo.renderArea.offset = {0, 0};
		renderPassInfo.renderArea.extent = swapChainExtent;

		VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
		renderPassInfo.clearValueCount = 1;
		renderPassInfo.pClearValues = &clearColor;

		vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);


		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);



		//ONLY if dynamic viewport and scissor activated during fixed pipeline's function specification
		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(swapChainExtent.width);
		viewport.height = static_cast<float>(swapChainExtent.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

		// VkRect2D scissor{};
		// scissor.offset = {0, 0};
		// scissor.extent = swapChainExtent;
		// vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
		
		//Uniforms
		// vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);
		testShader_.bind(commandBuffer, currentFrame);


		//Vertex Buffer part
		vertexData_.bind(commandBuffer);
		vertexData_.draw(commandBuffer);

		vkCmdEndRenderPass(commandBuffer);

		if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
			throw std::runtime_error("failed to record command buffer !");
		}

	}

	void createCommandBuffers() {
		commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = commandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = (uint32_t) commandBuffers.size();

		if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
			throw std::runtime_error("Failed to allocate command buffers !");
		}

	}

	void createCommandPool() {
		QueueFamily::QueueFamilyIndices queueFamilyIndices = QueueFamily::findQueueFamilies(physicalDevice, surface);

		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // Allow te rewrite command buffer every frame
		poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

		if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create command pool !");
		}

	}

	void createFramebuffers() {

		swapChainFramebuffers.resize(swapChainImageViews.size());

		for (size_t i = 0; i < swapChainImageViews.size(); i++) {
			VkImageView attachments[] = {
				swapChainImageViews[i]
			};

			VkFramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = renderPass;
			framebufferInfo.attachmentCount = 1;
			framebufferInfo.pAttachments = attachments;
			framebufferInfo.width = swapChainExtent.width;
			framebufferInfo.height = swapChainExtent.height;
			framebufferInfo.layers = 1;

			if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
				throw std::runtime_error("échec de la création d'un framebuffer!");
			}
		}

	}


	void createRenderPass() {

		// Attachment description
		VkAttachmentDescription colorAttachment{};
		colorAttachment.format = swapChainImageFormat;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;

		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		// Subpasses
		VkAttachmentReference colorAttachmentRef{};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;

		// Dependency, to configure when to wait for acquisition (here we only start waiting the acquisition in the color writing)
		VkSubpassDependency dependency{};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;

		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;

		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		// The render pass
		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = 1;
		renderPassInfo.pAttachments = &colorAttachment;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;


		if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create render pass !");
		}


	}

	void createGraphicsPipeline() {

		std::vector<char> vertShaderCode = ShaderHelper::readFile("Shaders/main.vert.spv");
		std::vector<char> fragShaderCode = ShaderHelper::readFile("Shaders/main.frag.spv");

		VkShaderModule vertShaderModule = ShaderHelper::createShaderModule(device, vertShaderCode);
    	VkShaderModule fragShaderModule = ShaderHelper::createShaderModule(device, fragShaderCode);

		// Vertex hader
		VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
		vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;

		vertShaderStageInfo.module = vertShaderModule;
		vertShaderStageInfo.pName = "main";
		vertShaderStageInfo.pSpecializationInfo = nullptr;

		// Fragment Shader
		VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
		fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;

		fragShaderStageInfo.module = fragShaderModule;
		fragShaderStageInfo.pName = "main";
		fragShaderStageInfo.pSpecializationInfo = nullptr;

		// Dynamic shaders array
		VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

		//// Static part

		//Vertex Buffer part
		VkVertexInputBindingDescription const& bindingDescription = vertexData_.getBindingDescription();
		std::vector<VkVertexInputAttributeDescription> const& attributeDescriptions = vertexData_.getAttributeDescriptions();


		/// Describe the structure of the data vertices (Leur type, le décalage entre donnée etc)
		VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.pVertexBindingDescriptions = &bindingDescription; // Optional

		vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
		vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data(); // Optional
		
		/// Describe how to link vertices together (We can change to line here for example)
		VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		/// Viewport. It describe where we should draw on the frameBuffer
		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float) swapChainExtent.width;
		viewport.height = (float) swapChainExtent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		// Mask filter, only the pixels within will be drawn
		VkRect2D scissor{};
		scissor.offset = {0, 0};
		scissor.extent = swapChainExtent;

		// Merging viewport and scissor to create one unique viewport
		VkPipelineViewportStateCreateInfo viewportState{};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports = &viewport;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &scissor;

		/// Rasterizer
		VkPipelineRasterizationStateCreateInfo rasterizer{};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = VK_FALSE; //If true, geometries that are not in between the far and near planes will be clamped
		rasterizer.rasterizerDiscardEnable = VK_FALSE; //If true, every geometry will be discarded

		/**
		 * VK_POLYGON_MODE_FILL : Fill the polygons with fragments
		 * VK_POLYGON_MODE_LINE : Only draw lines
		 * VK_POLYGON_MODE_POINT : Only draw points
		 */
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;

		rasterizer.lineWidth = 1.0f; //Define the thickness of the lines. If not 1.0, the extension "wideLines" should be activated

		rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; //Clockwise or not to evaluate front face

		//Parameters to alter the depth
		rasterizer.depthBiasEnable = VK_FALSE;
		rasterizer.depthBiasConstantFactor = 0.0f; // Optional
		rasterizer.depthBiasClamp = 0.0f; // Optional
		rasterizer.depthBiasSlopeFactor = 0.0f; // Optional


		/// Multi-sampling (It allow anti-aliasing)

		// Deactivated :
		VkPipelineMultisampleStateCreateInfo multisampling{};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampling.minSampleShading = 1.0f; // Optional
		multisampling.pSampleMask = nullptr; // Optional
		multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
		multisampling.alphaToOneEnable = VK_FALSE; // Optional

		/// Color blending (Configure how we combine colors that are already present in the framebuffer)

		// One per framebuffer
		// Deactivated :
		VkPipelineColorBlendAttachmentState colorBlendAttachment{};
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_FALSE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

		// Global color blend settings
		VkPipelineColorBlendStateCreateInfo colorBlending{};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional

		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;

		colorBlending.blendConstants[0] = 0.0f; // Optional
		colorBlending.blendConstants[1] = 0.0f; // Optional
		colorBlending.blendConstants[2] = 0.0f; // Optional
		colorBlending.blendConstants[3] = 0.0f; // Optional

		std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT
        };

		VkPipelineDynamicStateCreateInfo dynamicState{};
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
		dynamicState.pDynamicStates = dynamicStates.data();


		/// Pipeline Layout (This will indicates our need in uniforms)
		// VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		// pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		// pipelineLayoutInfo.setLayoutCount = 1;
		// pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
		// pipelineLayoutInfo.pushConstantRangeCount = 0;    // Optional
		// pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

		// if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
		// 	throw std::runtime_error("Failed to create the pipeline layout !");
		// }


		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = shaderStages;


		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pDepthStencilState = nullptr; // Optionel
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.pDynamicState = &dynamicState; // Optionel

		pipelineInfo.layout = testShader_.getPipelineLayout();

		pipelineInfo.renderPass = renderPass;
		pipelineInfo.subpass = 0;

		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
		pipelineInfo.basePipelineIndex = -1; // Optional

		if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create the graphic pipeline !");
		}

		//Now delete the shader modules since we don't need them anymore
		vkDestroyShaderModule(device, fragShaderModule, nullptr);
	    vkDestroyShaderModule(device, vertShaderModule, nullptr);

	}

	void createImageViews() {

		swapChainImageViews.resize(swapChainImages.size());

		for (size_t i = 0; i < swapChainImages.size(); i++) {
			swapChainImageViews[i] = Image::createImageView(device, swapChainImages[i], swapChainImageFormat);
		}

	}

	void createSwapChain() {

		SwapChain::SwapChainSupportDetails swapChainSupport = SwapChain::querySwapChainSupport(physicalDevice, surface);

		VkSurfaceFormatKHR surfaceFormat = SwapChain::chooseSwapSurfaceFormat(swapChainSupport.formats);
		VkPresentModeKHR presentMode = SwapChain::chooseSwapPresentMode(swapChainSupport.presentModes);
		VkExtent2D extent = SwapChain::chooseSwapExtent(swapChainSupport.capabilities, window);

		//TODO: voir ce qu'il se passe si on change le imageCount

		// Selection the number of images that will contain our swap chain.
		uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
		if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
			imageCount = swapChainSupport.capabilities.maxImageCount;
		}

		// Fill the information structure to create the swap chain
		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = surface; //We specify the surface on which we will render

		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1; //This is always 1 except for application that require multiple images on one render like stereoscopic 3D
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; //This mean we will directly render to the image, but we could also render on a separate image first to do post-processing, in this case VK_IMAGE_USAGE_TRANSFER_DST_BIT could be used


		// Specify how to handle swap chain images that are getting used from multiple queues families
		QueueFamily::QueueFamilyIndices indices = QueueFamily::findQueueFamilies(physicalDevice, surface);
		uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};
		
		if (indices.graphicsFamily != indices.presentFamily) {
			//If we're using two different queue, we will have to use concurrent mode
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		} else {
			//If they are both the same queue, no need to use concurrent mode
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.queueFamilyIndexCount = 0; // Optional
			createInfo.pQueueFamilyIndices = nullptr; // Optional
		}

		// Transformation applied to the images, capabilities.currentTransform mean no transformation applied
		createInfo.preTransform = swapChainSupport.capabilities.currentTransform;

		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE; //If we want to ignore pixels that are obscured by other windows for example. True give the best performances

		//If the swap chain get recreated, we want to store here a pointer to the old one
		createInfo.oldSwapchain = VK_NULL_HANDLE;

		// GENERATE the swap chain
		if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
			throw std::runtime_error("failed to create swap chain !");
		}

		// Retrieve the images handles
		vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);

		swapChainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

		// Store the formant and the extent
		swapChainImageFormat = surfaceFormat.format;
		swapChainExtent = extent;

	}

	void createSurface() {
		if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
			throw std::runtime_error("failed to create window surface !");
		}
	}

	void createLogicalDevice() {
		QueueFamily::QueueFamilyIndices indices = QueueFamily::findQueueFamilies(physicalDevice, surface);

		//The requested queue families
		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

		float queuePriority = 1.0f;
		for (uint32_t queueFamily : uniqueQueueFamilies) {
			VkDeviceQueueCreateInfo queueCreateInfo{};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;

			//Priority of the queue, from 0.0 (low) to 1.0 (Max)
			queueCreateInfo.pQueuePriorities = &queuePriority;

			//Add the queue informations to the vector of queue informations
			queueCreateInfos.push_back(queueCreateInfo);
		}

		
		//Specify the special features of the device we want to use in the queue (can be empty)
		VkPhysicalDeviceFeatures deviceFeatures{};
		deviceFeatures.samplerAnisotropy = VK_TRUE;

		//Logical device informations
		VkDeviceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

		createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		createInfo.pQueueCreateInfos = queueCreateInfos.data();

		createInfo.pEnabledFeatures = &deviceFeatures;

		createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
		createInfo.ppEnabledExtensionNames = deviceExtensions.data();
		

		//This may be useless, validations layers are now useless in device since they use now the same as the instance validation layer
		if (enableValidationLayers) {
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();
		} else {
			createInfo.enabledLayerCount = 0;
		}

		if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
			throw std::runtime_error("failed to create logical device!");
		}

		//Retrieve the queues we want to use and keep a pointer to them
		vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
		vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);

	}

	void pickPhysicalDevice() {

		physicalDevice = PhysicalDevices::getBestPhysicalDevice(instance, surface);

		if (physicalDevice == VK_NULL_HANDLE) {
			throw std::runtime_error("failed to find a suitable GPU !");
		}
		
	}

	void createInstance() {

		if (enableValidationLayers && !Checker::validationLayerSupport()) {
			throw std::runtime_error("validation layers requested, but not available!");
		}

		//Generate the informations of the application for the vulkan instance (stored in the vulkan instance create info structure)
		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Hello Triangle";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;


		//Code to get a list of all supported availableExtensions
		std::vector<VkExtensionProperties> availableExtensions = Getter::availableExtensions();
		
		// std::cout << "available extensions:\n";
		// for (const auto& extension : availableExtensions) {
		// 	std::cout << '\t' << extension.extensionName << '\n';
		// }

		std::vector<const char*> requiredExtensions = Getter::requiredExtensions(enableValidationLayers);

		//And verify if the required are availables
		if (Checker::requiredExtensionAreAvailable(requiredExtensions, availableExtensions) == false) {
			throw std::runtime_error("Required extensions are not available.");
		}
		
		//Generate the informations for the creation of the vulkan instance then create it
		VkInstanceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;
		createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
		createInfo.ppEnabledExtensionNames = requiredExtensions.data();

		//If enabled, add the validation layers
		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
		if (enableValidationLayers) {
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();

			debugCreateInfo = DebugMessenger::generateCreateInfo();
        	createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo; //Note: cast inutile ?
		}
		else {
			createInfo.enabledLayerCount = 0;
			createInfo.pNext = nullptr;
		}

		if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
			throw std::runtime_error("failed to create instance!");
		}

	}

    
	void cleanupSwapChain() {
		for (size_t i = 0; i < swapChainFramebuffers.size(); i++) {
			vkDestroyFramebuffer(device, swapChainFramebuffers[i], nullptr);
		}

		for (size_t i = 0; i < swapChainImageViews.size(); i++) {
			vkDestroyImageView(device, swapChainImageViews[i], nullptr);
		}

		vkDestroySwapchainKHR(device, swapChain, nullptr);
	}

    void cleanup() {

		cleanupSwapChain();

		// vkDestroySampler(device, textureSampler, nullptr);
		// vkDestroyImageView(device, textureImageView, nullptr);
		// vkDestroyImage(device, textureImage, nullptr);
		// vkFreeMemory(device, textureImageMemory, nullptr);

		//TODO: ne pas détruire ailleur que dans le destructeur
		testShader_.~Shader();
		vertexData_.~VertexData();

		vkDestroyPipeline(device, graphicsPipeline, nullptr);

		vkDestroyRenderPass(device, renderPass, nullptr);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
			vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
			vkDestroyFence(device, inFlightFences[i], nullptr);
		}

		vkDestroyCommandPool(device, commandPool, nullptr);

		vkDestroyDevice(device, nullptr);
		
		if (enableValidationLayers) {
			debugMessenger_.clean();
		}

		//We destroy the surface then the instance
		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyInstance(instance, nullptr);

		glfwDestroyWindow(window);

		glfwTerminate();
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
