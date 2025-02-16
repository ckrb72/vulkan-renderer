#pragma once
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <string>

struct queue_families
{
    uint32_t graphics;
    uint32_t present;
    uint8_t has_graphics;
    uint8_t has_present;
};

struct vk_swapchain
{
    VkSwapchainKHR swapchain;
    std::vector<VkImage> images;
    std::vector<VkImageView> image_views;
    VkSurfaceFormatKHR format;
    VkPresentModeKHR present_mode;
    VkExtent2D extent;
    uint32_t image_count;
};

struct vk_context
{
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkDevice logical_device;
    VkSurfaceKHR surface;
    VkDebugUtilsMessengerEXT debug_messenger;
    vk_swapchain swapchain;
};

struct vk_shader
{
    VkShaderModule vertex;
    VkShaderModule fragment;
};

struct vk_pipeline_config
{
    vk_shader shader;
    VkRenderPass renderpass;
    // vertex attribute stuff
    // uniform layout stuff (will likely make this it's own struct that you have to free yourself so it can be reused among many pipelines)
};

struct vk_dynamic_pipeline
{
    VkPipeline pipeline;
    VkPipelineLayout layout;
};

struct vk_dynamic_framebuffer
{
    std::vector<VkImageView> color_buffers;
    std::vector<VkImageView> depth_buffers;
    std::vector<VkImageView> stencil_buffers;
};

struct vk_pipeline
{
    VkPipeline pipeline;
    VkPipelineLayout layout;    // Storing this here for now but will likely move this out so layouts can be reused among many pipelines
    VkRenderPass renderpass;
};


struct vk_command_pool
{
    VkCommandPool command_pool;
    std::vector<VkCommandBuffer> buffers;
};

// Initializes vulkan and places all important context specific data in context
// 0 - success
// -1 - failure
int vk_init(vk_context* context, GLFWwindow* window);

// Deinitializes vulkan and frees context data;
int vk_terminate(vk_context* context);

int vk_shader_create(const std::string& vert_path, const std::string& frag_path, vk_context& context, vk_shader* shader);
void vk_shader_destroy(vk_context& context, vk_shader& shader);

int vk_pipeline_create(vk_context& context, vk_pipeline_config& config, vk_pipeline* pipeline);
int vk_pipeline_destroy(vk_context& context, vk_pipeline& pipeline);

int vk_dynamic_pipeline_create(vk_context& context, vk_pipeline_config& config, vk_dynamic_pipeline* pipeline);
int vk_dynamic_pipeline_destroy(vk_context& context, vk_dynamic_pipeline& pipeline);

queue_families vk_get_device_queues(const VkPhysicalDevice& physical_device, vk_context& context);

int vk_command_pool_create(vk_context& context, vk_command_pool* pool, uint32_t queue_index);
int vk_command_pool_add_buffers(vk_context& context, vk_command_pool& pool, uint32_t n);
int vk_command_pool_destroy(vk_context& context, vk_command_pool& pool);