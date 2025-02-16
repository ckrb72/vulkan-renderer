#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include "vklib.h"

const uint32_t WIN_WIDTH = 1920;
const uint32_t WIN_HEIGHT = 1080;

const uint32_t MAX_FRAMES_IN_FLIGHT = 2;

std::vector<VkSemaphore> image_available(MAX_FRAMES_IN_FLIGHT);
std::vector<VkSemaphore> render_finished(MAX_FRAMES_IN_FLIGHT);
std::vector<VkFence> in_flight(MAX_FRAMES_IN_FLIGHT);

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    GLFWwindow* window = glfwCreateWindow(WIN_WIDTH, WIN_HEIGHT, "Vulkan Test", NULL, NULL);

    vk_context context{};
    vk_init(&context, window);

    vk_shader shader{};
    vk_shader_create("../vert.spv", "../frag.spv", context, &shader);

    vk_pipeline_config pipeline_config{};
    pipeline_config.shader = shader;

    vk_dynamic_pipeline pipeline{};
    vk_dynamic_pipeline_create(context, pipeline_config, &pipeline);

    vk_shader_destroy(context, shader);


    // Don't really need these rn because we are rendering directly to the swapchain images...
    // Just here to show how to do it but would need these if rendering to other color attachments / images
    /*std::vector<vk_dynamic_framebuffer> framebuffers(context.swapchain.image_views.size());

    for(int i = 0; i < framebuffers.size(); i++)
    {
        framebuffers[i].color_buffers.resize(1);
        framebuffers[i].color_buffers.push_back(context.swapchain.image_views[i]);
    }*/

    queue_families queues = vk_get_device_queues(context.physical_device, context);

    vk_command_pool command_pool{};
    vk_command_pool_create(context, &command_pool, queues.graphics);

    vk_command_pool_add_buffers(context, command_pool, MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphore_info{};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    
    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        if(vkCreateSemaphore(context.logical_device, &semaphore_info, NULL, &image_available[i]) != VK_SUCCESS ||
        vkCreateSemaphore(context.logical_device, &semaphore_info, NULL, &render_finished[i]) != VK_SUCCESS ||
        vkCreateFence(context.logical_device, &fence_info, NULL, &in_flight[i]) != VK_SUCCESS)
        {
            std::cerr << "Failed to create synchronization objects" << std::endl;
            return -1;
        }
    }

    VkQueue graphics_queue;
    VkQueue present_queue;

    vkGetDeviceQueue(context.logical_device, queues.graphics, 0, &graphics_queue);
    vkGetDeviceQueue(context.logical_device, queues.present, 0, &present_queue);

    uint32_t current_frame = 0;


    PFN_vkCmdBeginRenderingKHR vkCmdBeginRenderingKHR_ext = (PFN_vkCmdBeginRenderingKHR)vkGetInstanceProcAddr(context.instance, "vkCmdBeginRenderingKHR");
    PFN_vkCmdEndRenderingKHR vkCmdEndRenderingKHR_ext = (PFN_vkCmdEndRenderingKHR)vkGetInstanceProcAddr(context.instance, "vkCmdEndRenderingKHR");

    if(!vkCmdBeginRenderingKHR_ext || !vkCmdEndRenderingKHR_ext)
    {
        std::cerr << "Failed to load rendering procs" << std::endl;
        return -1;
    }

    while(!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        vkWaitForFences(context.logical_device, 1, &in_flight[current_frame], VK_TRUE, UINT64_MAX);

        uint32_t image_index;
        VkResult acquire_result = vkAcquireNextImageKHR(context.logical_device, context.swapchain.swapchain, UINT64_MAX, image_available[current_frame], VK_NULL_HANDLE, &image_index);
        vkResetFences(context.logical_device, 1, &in_flight[current_frame]);

        vkResetCommandBuffer(command_pool.buffers[current_frame], 0);

        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        if(vkBeginCommandBuffer(command_pool.buffers[current_frame], &begin_info) != VK_SUCCESS)
        {
            std::cerr << "Failed to start command buffer" << std::endl;
            return -1;
        }

        VkRenderingAttachmentInfoKHR color_attachment{};
        color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
        color_attachment.imageView = context.swapchain.image_views[image_index];
        color_attachment.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
        color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment.clearValue = {{{0.3f, 0.3f, 0.3f, 1.0f}}};


        VkRenderingInfoKHR rendering_info{};
        rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
        rendering_info.renderArea = VkRect2D{VkOffset2D{}, context.swapchain.extent};
        rendering_info.layerCount = 1;
        rendering_info.colorAttachmentCount = 1;
        rendering_info.pColorAttachments = &color_attachment;

        vkCmdBeginRenderingKHR_ext(command_pool.buffers[current_frame], &rendering_info);

        // Rendering commands here
        vkCmdBindPipeline(command_pool.buffers[current_frame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(context.swapchain.extent.width);
        viewport.height = static_cast<float>(context.swapchain.extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(command_pool.buffers[current_frame], 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = context.swapchain.extent;
        vkCmdSetScissor(command_pool.buffers[current_frame], 0, 1, &scissor);

        vkCmdDraw(command_pool.buffers[current_frame], 3, 1, 0, 0);

        vkCmdEndRenderingKHR_ext(command_pool.buffers[current_frame]);

        if(vkEndCommandBuffer(command_pool.buffers[current_frame]) != VK_SUCCESS)
        {
            std::cerr << "Failed to end command buffer" << std::endl;
            return -1;
        }

        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        
        VkSemaphore wait_semaphores[] = { image_available[current_frame] };
        VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = wait_semaphores;
        submit_info.pWaitDstStageMask = wait_stages;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &command_pool.buffers[current_frame];

        VkSemaphore signal_semaphores[] = { render_finished[current_frame] };
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = signal_semaphores;

        if(vkQueueSubmit(graphics_queue, 1, &submit_info, in_flight[current_frame]) != VK_SUCCESS)
        {
            std::cerr << "Failed to submit to graphics queue" << std::endl;
            return -1;
        }

        VkPresentInfoKHR present_info{};
        present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores = signal_semaphores;

        VkSwapchainKHR swapchains[] = { context.swapchain.swapchain };
        present_info.swapchainCount = 1;
        present_info.pSwapchains = swapchains;
        present_info.pImageIndices = &image_index;
        present_info.pResults = NULL;

        VkResult present_result = vkQueuePresentKHR(present_queue, &present_info);

        current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;

    }

    vkDeviceWaitIdle(context.logical_device);

    for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vkDestroySemaphore(context.logical_device, image_available[i], NULL);
        vkDestroySemaphore(context.logical_device, render_finished[i], NULL);
        vkDestroyFence(context.logical_device, in_flight[i], NULL);
    }

    vk_command_pool_destroy(context, command_pool);
    vk_dynamic_pipeline_destroy(context, pipeline);
    vk_terminate(&context);

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
