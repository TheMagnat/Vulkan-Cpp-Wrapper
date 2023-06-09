
#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <VulkanObjects/Instance.hpp>
#include <VulkanObjects/Surface.hpp>
#include <VulkanObjects/Device.hpp>
#include <VulkanObjects/Allocator.hpp>
#include <VulkanObjects/SwapChain.hpp>
#include <VulkanObjects/RenderPass.hpp>
#include <VulkanObjects/CommandPool.hpp>
#include <VulkanObjects/CommandBuffers.hpp>
#include <VulkanObjects/SynchronisationObjects.hpp>

//Debug
#include <VulkanObjects/DebugMessenger.hpp>


//Generator
#include <VulkanObjects/Shader.hpp>
#include <VulkanObjects/GraphicsPipeline.hpp>

bool validationDebugLayerActivated = true;

class VulkanWrapper {

    public:
        VulkanWrapper(GLFWwindow* window, uint16_t framesInFlight, bool depthCheck = false)
            : framesInFlight_(framesInFlight), depthCheck_(depthCheck), window_(window), instance_(validationDebugLayerActivated), debugMessenger_(instance_, validationDebugLayerActivated), surface_(window_, instance_), device_(instance_, surface_, validationDebugLayerActivated), swapChain_(window, surface_, device_, depthCheck_),
            renderPass_(device_, swapChain_, depthCheck_), commandPool_(surface_, device_), commandBuffers_(framesInFlight_, device_, commandPool_), syncObjs_(framesInFlight, device_)
        {

            swapChain_.initializeFramebuffers(renderPass_);
        }

        //If true, recording started
        //If false, failed  to start drawing
        VkCommandBuffer beginRecordingDraw() {

            //We wait the fence to be signaled...
            vkWaitForFences(device_.get(), 1, &syncObjs_.inFlightFences[currentFrame_], VK_TRUE, UINT64_MAX);

            VkResult result = vkAcquireNextImageKHR(device_.get(), swapChain_.get(), UINT64_MAX, syncObjs_.imageAvailableSemaphores[currentFrame_], VK_NULL_HANDLE, &currentDrawingTargetImageIndex_);

            if (result == VK_ERROR_OUT_OF_DATE_KHR) {
                recreateGraphicWindow();
                return nullptr;
            } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
                throw std::runtime_error("failed to acquire swap chain image!");
            }

            //...Then we set it to the un-signaled state. Note: we un-signal it only if we're sure that we will submit work with it
            vkResetFences(device_.get(), 1, &syncObjs_.inFlightFences[currentFrame_]);
            
            //TODO DE ICI
            
            //TODO: Voir l'ordre ?
            //TODO: LE DEPLACER
            // updateUniformBuffer(currentFrame_);

            //Selected command buffer to store the draw calls
            VkCommandBuffer commandBuffer = commandBuffers_.get()[currentFrame_];

            //We now reset and start recording the command buffer
            vkResetCommandBuffer(commandBuffer, 0);
            beginRecordingCommandBuffer(commandBuffer);

            return commandBuffer;

        }

        void endRecordingDraw() {

            //Current command buffer
            VkCommandBuffer commandBuffer = commandBuffers_.get()[currentFrame_];

            endRecordingCommandBuffer(commandBuffer);

            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &commandBuffer;

            VkSemaphore waitSemaphores[] = {syncObjs_.imageAvailableSemaphores[currentFrame_]};
            VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = waitSemaphores;
            submitInfo.pWaitDstStageMask = waitStages;

            VkSemaphore signalSemaphores[] = {syncObjs_.renderFinishedSemaphores[currentFrame_]};
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = signalSemaphores;

            if (vkQueueSubmit(device_.getGraphicsQueue(), 1, &submitInfo, syncObjs_.inFlightFences[currentFrame_]) != VK_SUCCESS) {
                throw std::runtime_error("failed to submit draw command buffer !");
            }

            VkPresentInfoKHR presentInfo{};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = signalSemaphores;


            VkSwapchainKHR swapChains[] = {swapChain_.get()};
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = swapChains;
            presentInfo.pImageIndices = &currentDrawingTargetImageIndex_;

            presentInfo.pResults = nullptr; // Optionnel

            VkResult result = vkQueuePresentKHR(device_.getPresentQueue(), &presentInfo);
            if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized_) {
                framebufferResized_ = false;
                recreateGraphicWindow();
            } else if (result != VK_SUCCESS) {
                throw std::runtime_error("failed to present swap chain image!");
            }

            // Set the currentFrame to the next available frame
            currentFrame_ = (currentFrame_ + 1) % framesInFlight_;

        }
        
        void beginRecordingCommandBuffer(VkCommandBuffer commandBuffer) {
            
            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = 0; // Optional
            beginInfo.pInheritanceInfo = nullptr; // Optional

            if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
                throw std::runtime_error("failed to begin recording command buffer!");
            }

            VkRenderPassBeginInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = renderPass_.get();
            renderPassInfo.framebuffer = swapChain_.getFramebuffers()[currentDrawingTargetImageIndex_];

            renderPassInfo.renderArea.offset = {0, 0};
            renderPassInfo.renderArea.extent = swapChain_.getExtent();

            std::vector<VkClearValue> clearValues(1 + depthCheck_);
            clearValues[0] = {{0.5f, 0.5f, 0.5f, 1.0f}};
            if (depthCheck_) clearValues[1] = {1.0f, 0};

            renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
            renderPassInfo.pClearValues = clearValues.data();

            vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            //ONLY if dynamic viewport and scissor activated during fixed pipeline's function specification
            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = static_cast<float>(swapChain_.getExtent().width);
            viewport.height = static_cast<float>(swapChain_.getExtent().height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
            
            // VkRect2D scissor{};
            // scissor.offset = {0, 0};
            // scissor.extent = swapChainExtent;
            // vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        }

        void endRecordingCommandBuffer(VkCommandBuffer commandBuffer) {
            vkCmdEndRenderPass(commandBuffer);

            if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
                throw std::runtime_error("failed to record command buffer !");
            }
        }

        VkExtent2D const& getExtent() const {
            return swapChain_.getExtent();
        }

        uint32_t getCurrentFrame() const {
            return currentFrame_;
        }

        // Generator
        Shader generateShader(const std::string& vertexFilename, const std::string& fragmentFilename) const {
            return Shader(&device_, framesInFlight_, vertexFilename, fragmentFilename);
        }

        GraphicsPipeline generateGraphicsPipeline(Shader const& shader, std::vector<uint32_t> const& vertexAttributesSize) const {
            return GraphicsPipeline(device_, shader, swapChain_.getExtent(), renderPass_, vertexAttributesSize, depthCheck_);
        }

        VertexData generateVertexData() const {
            return VertexData(&device_, commandPool_.get());
        }

        Texture generateTexture(std::vector<uint8_t> const& textureData, Texture::TextureInformations const& textureInformations) const {
            return Texture(&device_, commandPool_.get(), device_.getGraphicsQueue(), textureData, textureInformations);
        }

        void waitIdle() const {
            vkDeviceWaitIdle(device_.get());
        }

        VmaTotalStatistics getMemoryStatistics() {
            VmaTotalStatistics stats;
            vmaCalculateStatistics (device_.getAllocator(), &stats);
            return stats;
        }

        //To be called when window get resized
        void gotResized() {
            framebufferResized_ = true;
        }

        void saveGraphicPipeline(GraphicsPipeline* pipeline) {
            savedPipeline_ = pipeline;
        }

        void recreateGraphicWindow() {

            int width = 0, height = 0;
            glfwGetFramebufferSize(window_, &width, &height);
            while (width == 0 || height == 0) {
                //THIS MAKE THE PROGRAM SLEEP
                glfwGetFramebufferSize(window_, &width, &height);
                glfwWaitEvents();
            }

            vkDeviceWaitIdle(device_.get());
            
            renderPass_.clean();
            swapChain_.clean();
            
            swapChain_.initializeSwapChain(window_, surface_, device_);
            swapChain_.initializeImageViews();
            if (depthCheck_) swapChain_.initializeDepthResources(device_);

            renderPass_.initializeRenderPass(swapChain_, depthCheck_);

            swapChain_.initializeFramebuffers(renderPass_);

            if (savedPipeline_) {

                savedPipeline_->clean();
                savedPipeline_->initialize(swapChain_.getExtent(), renderPass_);

            }

        }

    private:
        //Variables
        uint16_t framesInFlight_;
        bool depthCheck_;

        bool framebufferResized_ = false;
        uint32_t currentFrame_ = 0;
        uint32_t currentDrawingTargetImageIndex_;

        //GLFW
        GLFWwindow* window_;

        //Vulkan
        Instance instance_;
        DebugMessenger debugMessenger_;
        Surface surface_;

        Device device_;

        SwapChain swapChain_;

        RenderPass renderPass_;

        CommandPool commandPool_;
        CommandBuffers commandBuffers_;

        SynchronisationObjects syncObjs_;

        //Save
        GraphicsPipeline* savedPipeline_ = nullptr;
};

