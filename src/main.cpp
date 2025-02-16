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

    vk_command_pool command_pool{};
    vk_command_pool_create(context, &command_pool, vk_get_device_queues(context.physical_device, context).graphics);

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

    uint32_t current_frame = 0;

    while(!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        /*vkWaitForFences(context.logical_device, 1, &in_flight[current_frame], VK_TRUE, UINT64_MAX);

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
        rendering_info.pColorAttachments = &color_attachment;*/


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
