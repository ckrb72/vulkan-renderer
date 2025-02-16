#include "vklib.h"
#include <GLFW/glfw3.h>
#include <vector>
#include <iostream>
#include <algorithm>
#include <fstream>

static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);
queue_families vk_get_device_queues(const VkPhysicalDevice& physical_device, vk_context& context);
static int vk_create_swapchain(vk_context* context, GLFWwindow* window);
static std::vector<char> load_file_bytes(const std::string& path);

int vk_init(vk_context* context, GLFWwindow* window)
{

    if(context == NULL || window == NULL) return -1;

    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.apiVersion = VK_API_VERSION_1_4;
    app_info.pApplicationName = "vulkan-renderer";
    app_info.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
    app_info.pEngineName = "None";
    app_info.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);

    VkInstanceCreateInfo instance_info{};
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo = &app_info;

    uint32_t extension_count = 0;
    const char** glfw_extensions;
    glfw_extensions = glfwGetRequiredInstanceExtensions(&extension_count);
    std::vector<const char*> extensions(glfw_extensions, glfw_extensions + extension_count);
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    std::vector<const char*> validation_layers = { "VK_LAYER_KHRONOS_validation" };

    instance_info.enabledExtensionCount = extensions.size();
    instance_info.ppEnabledExtensionNames = extensions.data();
    instance_info.enabledLayerCount = validation_layers.size();
    instance_info.ppEnabledLayerNames = validation_layers.data();

    VkDebugUtilsMessengerCreateInfoEXT debug_info{};
    debug_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debug_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debug_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debug_info.pfnUserCallback = debug_callback;

    instance_info.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debug_info;
    
    if(vkCreateInstance(&instance_info, NULL, &(context->instance)) != VK_SUCCESS)
    {
        std::cerr << "Failed to create vulkan instance" << std::endl;
        return -1;
    }

    if(CreateDebugUtilsMessengerEXT(context->instance, &debug_info, NULL, &context->debug_messenger) != VK_SUCCESS)
    {
        std::cerr << "Failed to create debug messenger" << std::endl;
        return -1;
    }

    if(glfwCreateWindowSurface(context->instance, window, NULL, &context->surface) != VK_SUCCESS)
    {
        std::cerr << "Failed to create window surface" << std::endl;
        return -1;
    }

    
    // Choose a physical device to use
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(context->instance, &device_count, NULL);
    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(context->instance, &device_count, devices.data());

    std::vector<const char*> required_device_extensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME };

    for(const VkPhysicalDevice& gpu : devices)
    {
        queue_families queues = vk_get_device_queues(gpu, *context);

        uint32_t extension_count;
        vkEnumerateDeviceExtensionProperties(gpu, NULL, &extension_count, NULL);
        std::vector<VkExtensionProperties> available_extensions(extension_count);
        vkEnumerateDeviceExtensionProperties(gpu, NULL, &extension_count, available_extensions.data());

        uint32_t supported = 0;
        for(int i = 0; i < required_device_extensions.size(); i++)
        {
            for(int j = 0; j < available_extensions.size(); j++)
            {
                if(strcmp(required_device_extensions[i], available_extensions[j].extensionName) == 0)
                {
                    supported++;
                }
            }
        }

        uint32_t swapchain_adequate = 0;
        if(supported == required_device_extensions.size())
        {
            uint32_t format_count;
            vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, context->surface, &format_count, NULL);
            std::vector<VkSurfaceFormatKHR> surface_formats(format_count);
            vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, context->surface, &format_count, surface_formats.data());

            uint32_t present_mode_count;
            vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, context->surface, &present_mode_count, NULL);
            std::vector<VkPresentModeKHR> surface_present_modes(present_mode_count);
            vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, context->surface, &present_mode_count, surface_present_modes.data());

            swapchain_adequate = !surface_formats.empty() && !surface_present_modes.empty();
        }

        if(queues.has_graphics && queues.has_present && swapchain_adequate)
        {
            context->physical_device = gpu;
            break;
        }
    }

    if(context->physical_device == VK_NULL_HANDLE)
    {
        std::cerr << "Failed to find adequate physical device" << std::endl;
        return -1;
    }

    queue_families queues = vk_get_device_queues(context->physical_device, *context);

    float queue_priority = 1.0;

    VkDeviceQueueCreateInfo graphics_queue_info{};
    graphics_queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    graphics_queue_info.queueFamilyIndex = queues.graphics;
    graphics_queue_info.queueCount = 1;
    graphics_queue_info.pQueuePriorities = &queue_priority;

    VkDeviceQueueCreateInfo present_queue_info{};
    present_queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    present_queue_info.queueFamilyIndex = queues.present;
    present_queue_info.queueCount = 1;
    present_queue_info.pQueuePriorities = &queue_priority;

    // Make sure we don't repeat the same queue index if present == graphics
    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    queue_create_infos.push_back(graphics_queue_info);
    if(queues.graphics != queues.present)
    {
        queue_create_infos.push_back(present_queue_info);
    }

    VkPhysicalDeviceFeatures device_features{};

    VkDeviceCreateInfo logical_device_info{};
    logical_device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    logical_device_info.pQueueCreateInfos = queue_create_infos.data();
    logical_device_info.queueCreateInfoCount = queue_create_infos.size();
    logical_device_info.pEnabledFeatures = &device_features;
    logical_device_info.enabledExtensionCount = required_device_extensions.size();
    logical_device_info.ppEnabledExtensionNames = required_device_extensions.data();
    logical_device_info.enabledLayerCount = validation_layers.size();
    logical_device_info.ppEnabledLayerNames = validation_layers.data();

    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_rendering_feature{};
    dynamic_rendering_feature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
    dynamic_rendering_feature.dynamicRendering = VK_TRUE;

    logical_device_info.pNext = &dynamic_rendering_feature;

    if(vkCreateDevice(context->physical_device, &logical_device_info, NULL, &(context->logical_device)) != VK_SUCCESS)
    {
        std::cerr << "Failed to create logical device" << std::endl;
        return -1;
    }

    if(vk_create_swapchain(context, window) < 0)
    {
        std::cerr << "Failed to create swapchain" << std::endl;
        return -1;
    }

    return 0;
}

int vk_shader_create(const std::string& vert_path, const std::string& frag_path, vk_context& context, vk_shader* shader)
{
    if(shader == NULL) return -1;

    std::vector<char> vertex_bytes = load_file_bytes(vert_path);

    VkShaderModuleCreateInfo vertex_info{};
    vertex_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vertex_info.codeSize = vertex_bytes.size();
    vertex_info.pCode = (const uint32_t*)vertex_bytes.data();

    if(vkCreateShaderModule(context.logical_device, &vertex_info, NULL, &shader->vertex) != VK_SUCCESS)
    {
        std::cerr << "Failed to create vertex shader module" << std::endl;
        return -1;
    }

    std::vector<char> fragment_bytes = load_file_bytes(frag_path);
    VkShaderModuleCreateInfo fragment_info{};
    fragment_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fragment_info.codeSize = fragment_bytes.size();
    fragment_info.pCode = (const uint32_t*)fragment_bytes.data();

    if(vkCreateShaderModule(context.logical_device, &fragment_info, NULL, &shader->fragment) != VK_SUCCESS)
    {
        std::cerr << "Failed to create fragment shader module" << std::endl;
        return -1;
    }

    return 0;
}

void vk_shader_destroy(vk_context& context, vk_shader& shader)
{
    vkDestroyShaderModule(context.logical_device, shader.vertex, NULL);
    vkDestroyShaderModule(context.logical_device, shader.fragment, NULL);
}

int vk_pipeline_create(vk_context& context, vk_pipeline_config& config, vk_pipeline* pipeline)
{
    if(pipeline == NULL) return -1;

    VkPipelineShaderStageCreateInfo vertex_info{};
    vertex_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertex_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertex_info.module = config.shader.vertex;
    vertex_info.pName = "main";

    VkPipelineShaderStageCreateInfo fragment_info{};
    fragment_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragment_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragment_info.module = config.shader.fragment;
    fragment_info.pName = "main";

    VkPipelineShaderStageCreateInfo shader_stages[] = { vertex_info, fragment_info };


    // TODO: Come here when implementing vertex buffers
    VkPipelineVertexInputStateCreateInfo vertex_input_info{};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount = 0;
    vertex_input_info.pVertexBindingDescriptions = NULL;
    vertex_input_info.vertexAttributeDescriptionCount = 0;
    vertex_input_info.pVertexAttributeDescriptions = NULL;

    VkPipelineInputAssemblyStateCreateInfo input_assembly_info{};
    input_assembly_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly_info.primitiveRestartEnable = VK_FALSE;

    std::vector<VkDynamicState> dynamic_states = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamic_state_info{};
    dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state_info.dynamicStateCount = dynamic_states.size();
    dynamic_state_info.pDynamicStates = dynamic_states.data();

    VkPipelineViewportStateCreateInfo viewport_info{};
    viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_info.viewportCount = 1;
    viewport_info.scissorCount = 1;
    //viewport_info.pViewports = &viewports;
    //viewport_info.pScissors = &scissors;

    VkPipelineRasterizationStateCreateInfo rasterizer_info{};
    rasterizer_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer_info.depthClampEnable = VK_FALSE;
    rasterizer_info.rasterizerDiscardEnable = VK_FALSE;
    rasterizer_info.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer_info.lineWidth = 1.0f;
    rasterizer_info.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer_info.depthBiasEnable = VK_FALSE;
    
    VkPipelineMultisampleStateCreateInfo multisample_info{};
    multisample_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample_info.sampleShadingEnable = VK_FALSE;
    multisample_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState blend_attachment{};
    blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blend_attachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo blend_info{};
    blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend_info.logicOpEnable = VK_FALSE;
    blend_info.attachmentCount = 1;
    blend_info.pAttachments = &blend_attachment;


    //TODO: Move this code out of here and place it in its own layout struct so we can reuse layouts among many pipelines
    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    if(vkCreatePipelineLayout(context.logical_device, &pipeline_layout_info, NULL, &pipeline->layout) != VK_SUCCESS)
    {
        std::cerr << "Failed to create pipeline layout" << std::endl;
        return -1;
    }

    // Renderpass setup
    // TODO: Need to fix this because framebuffers need to be able to reference this render pass
    // Likely will need to take this code out of here and figure out a way to do renderpasses elsewhere (maybe pass it in with pipeline config)

    VkAttachmentDescription color_attachment_desc{};
    color_attachment_desc.format = context.swapchain.format.format;
    color_attachment_desc.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment_desc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment_desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment_desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment_desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment_desc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_ref{};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderpass_info{};
    renderpass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderpass_info.attachmentCount = 1;
    renderpass_info.pAttachments = &color_attachment_desc;
    renderpass_info.subpassCount = 1;
    renderpass_info.pSubpasses = &subpass;
    renderpass_info.dependencyCount = 1;
    renderpass_info.pDependencies = &dependency;

    if(vkCreateRenderPass(context.logical_device, &renderpass_info, NULL, &pipeline->renderpass) != VK_SUCCESS)
    {
        std::cerr << "Failed to create renderpass" << std::endl;
        return -1;
    }

    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_stages;
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly_info;
    pipeline_info.pViewportState = &viewport_info;
    pipeline_info.pRasterizationState = &rasterizer_info;
    pipeline_info.pMultisampleState = &multisample_info;
    pipeline_info.pDepthStencilState = NULL;
    pipeline_info.pColorBlendState = &blend_info;
    pipeline_info.pDynamicState = &dynamic_state_info;
    pipeline_info.layout = pipeline->layout;
    pipeline_info.renderPass = pipeline->renderpass;
    pipeline_info.subpass = 0;

    if(vkCreateGraphicsPipelines(context.logical_device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &pipeline->pipeline) != VK_SUCCESS)
    {
        std::cerr << "Failed to create graphics pipeline" << std::endl;
        return -1;
    }

    return 0;
}

int vk_pipeline_destroy(vk_context& context, vk_pipeline& pipeline)
{
    vkDestroyRenderPass(context.logical_device, pipeline.renderpass, NULL);
    vkDestroyPipelineLayout(context.logical_device, pipeline.layout, NULL);
    vkDestroyPipeline(context.logical_device, pipeline.pipeline, NULL);
    
    return 0;
}

int vk_dynamic_pipeline_create(vk_context& context, vk_pipeline_config& config, vk_dynamic_pipeline* pipeline)
{
    if(pipeline == NULL) return -1;

    VkPipelineShaderStageCreateInfo vertex_info{};
    vertex_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertex_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertex_info.module = config.shader.vertex;
    vertex_info.pName = "main";

    VkPipelineShaderStageCreateInfo fragment_info{};
    fragment_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragment_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragment_info.module = config.shader.fragment;
    fragment_info.pName = "main";

    VkPipelineShaderStageCreateInfo shader_stages[] = { vertex_info, fragment_info };


    // TODO: Come here when implementing vertex buffers
    VkPipelineVertexInputStateCreateInfo vertex_input_info{};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount = 0;
    vertex_input_info.pVertexBindingDescriptions = NULL;
    vertex_input_info.vertexAttributeDescriptionCount = 0;
    vertex_input_info.pVertexAttributeDescriptions = NULL;

    VkPipelineInputAssemblyStateCreateInfo input_assembly_info{};
    input_assembly_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly_info.primitiveRestartEnable = VK_FALSE;

    std::vector<VkDynamicState> dynamic_states = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamic_state_info{};
    dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state_info.dynamicStateCount = dynamic_states.size();
    dynamic_state_info.pDynamicStates = dynamic_states.data();

    VkPipelineViewportStateCreateInfo viewport_info{};
    viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_info.viewportCount = 1;
    viewport_info.scissorCount = 1;
    //viewport_info.pViewports = &viewports;
    //viewport_info.pScissors = &scissors;

    VkPipelineRasterizationStateCreateInfo rasterizer_info{};
    rasterizer_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer_info.depthClampEnable = VK_FALSE;
    rasterizer_info.rasterizerDiscardEnable = VK_FALSE;
    rasterizer_info.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer_info.lineWidth = 1.0f;
    rasterizer_info.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer_info.depthBiasEnable = VK_FALSE;
    
    VkPipelineMultisampleStateCreateInfo multisample_info{};
    multisample_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample_info.sampleShadingEnable = VK_FALSE;
    multisample_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState blend_attachment{};
    blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blend_attachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo blend_info{};
    blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend_info.logicOpEnable = VK_FALSE;
    blend_info.attachmentCount = 1;
    blend_info.pAttachments = &blend_attachment;


    //TODO: Move this code out of here and place it in its own layout struct so we can reuse layouts among many pipelines
    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    if(vkCreatePipelineLayout(context.logical_device, &pipeline_layout_info, NULL, &pipeline->layout) != VK_SUCCESS)
    {
        std::cerr << "Failed to create pipeline layout" << std::endl;
        return -1;
    }

    VkPipelineRenderingCreateInfoKHR pipeline_rendering_info{};
    pipeline_rendering_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    pipeline_rendering_info.colorAttachmentCount = 1;
    pipeline_rendering_info.pColorAttachmentFormats = &context.swapchain.format.format;

    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_stages;
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly_info;
    pipeline_info.pViewportState = &viewport_info;
    pipeline_info.pRasterizationState = &rasterizer_info;
    pipeline_info.pMultisampleState = &multisample_info;
    pipeline_info.pDepthStencilState = NULL;
    pipeline_info.pColorBlendState = &blend_info;
    pipeline_info.pDynamicState = &dynamic_state_info;
    pipeline_info.layout = pipeline->layout;
    pipeline_info.renderPass = VK_NULL_HANDLE;
    pipeline_info.pNext = &pipeline_rendering_info;

    if(vkCreateGraphicsPipelines(context.logical_device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &pipeline->pipeline) != VK_SUCCESS)
    {
        std::cerr << "Failed to create graphics pipeline" << std::endl;
        return -1;
    }

    return 0;
}

int vk_dynamic_pipeline_destroy(vk_context& context, vk_dynamic_pipeline& pipeline)
{
    vkDestroyPipelineLayout(context.logical_device, pipeline.layout, NULL);
    vkDestroyPipeline(context.logical_device, pipeline.pipeline, NULL);
    return 0;
}

int vk_command_pool_create(vk_context& context, vk_command_pool* pool, uint32_t queue_index)
{
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = queue_index;
    
    if(vkCreateCommandPool(context.logical_device, &pool_info, NULL, &pool->command_pool) != VK_SUCCESS)
    {
        std::cerr << "Failed to create command pool" << std::endl;
        return -1;
    }
    
    return 0;
}

int vk_command_pool_add_buffers(vk_context& context, vk_command_pool& pool, uint32_t n)
{

    uint32_t prev_size = pool.buffers.size();
    pool.buffers.resize(prev_size + n);

    VkCommandBufferAllocateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    buffer_info.commandPool = pool.command_pool;
    buffer_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    buffer_info.commandBufferCount = n;

    if(vkAllocateCommandBuffers(context.logical_device, &buffer_info, &pool.buffers[prev_size]) != VK_SUCCESS)
    {
        std::cerr << "Failed to add command buffers" << std::endl;
        return -1;
    }

    return 0;
}

int vk_command_pool_destroy(vk_context& context, vk_command_pool& pool)
{
    vkDestroyCommandPool(context.logical_device, pool.command_pool, NULL);
    return 0;
}

static std::vector<char> load_file_bytes(const std::string& path)
{
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if(!file.is_open())
    {
        std::cout << "Failed to open file" << std::endl;
        return std::vector<char>(0);
    }
    size_t size = file.tellg();
    std::vector<char> buffer(size);
    file.seekg(0);
    file.read(buffer.data(), size);
    file.close();

    return buffer;
}

static int vk_create_swapchain(vk_context* context, GLFWwindow* window)
{
    VkSurfaceCapabilitiesKHR surface_capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context->physical_device, context->surface, &surface_capabilities);

    uint32_t format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(context->physical_device, context->surface, &format_count, NULL);
    std::vector<VkSurfaceFormatKHR> surface_formats(format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(context->physical_device, context->surface, &format_count, surface_formats.data());

    uint32_t present_mode_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(context->physical_device, context->surface, &present_mode_count, NULL);
    std::vector<VkPresentModeKHR> present_modes(present_mode_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(context->physical_device, context->surface, &present_mode_count, present_modes.data());

    context->swapchain.format = surface_formats[0];
    for(const VkSurfaceFormatKHR& format : surface_formats)
    {
        if(format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            context->swapchain.format = format;
            break;
        }
    }

    context->swapchain.present_mode = VK_PRESENT_MODE_FIFO_KHR;
    for(const VkPresentModeKHR& mode : present_modes)
    {
        if(mode == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            context->swapchain.present_mode = mode;
            break;
        }
    }

    if(surface_capabilities.currentExtent.width != UINT32_MAX)
    {
        context->swapchain.extent = surface_capabilities.currentExtent;
    }
    else
    {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        VkExtent2D actual_extent =
        {
            (uint32_t)width,
            (uint32_t)height
        };

        actual_extent.width = std::clamp(actual_extent.width, surface_capabilities.minImageExtent.width, surface_capabilities.maxImageExtent.width);
        actual_extent.height = std::clamp(actual_extent.height, surface_capabilities.minImageExtent.height, surface_capabilities.maxImageExtent.height);

        context->swapchain.extent = actual_extent;
    }

    uint32_t image_count = surface_capabilities.minImageCount + 1;
    if(surface_capabilities.maxImageCount > 0 && image_count > surface_capabilities.maxImageCount)
    {
        image_count = surface_capabilities.maxImageCount;
    }

    context->swapchain.image_count = image_count;

    VkSwapchainCreateInfoKHR swapchain_info{};
    swapchain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_info.surface = context->surface;
    swapchain_info.minImageCount = context->swapchain.image_count;
    swapchain_info.imageFormat = context->swapchain.format.format;
    swapchain_info.imageColorSpace = context->swapchain.format.colorSpace;
    swapchain_info.imageExtent = context->swapchain.extent;
    swapchain_info.imageArrayLayers = 1;
    swapchain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    queue_families queues = vk_get_device_queues(context->physical_device, *context);
    uint32_t queue_indices[] = { queues.graphics, queues.present };
    if(queues.graphics != queues.present)
    {
        swapchain_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchain_info.queueFamilyIndexCount = 2;
        swapchain_info.pQueueFamilyIndices = queue_indices;
    }
    else
    {
        swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    swapchain_info.preTransform = surface_capabilities.currentTransform;
    swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_info.presentMode = context->swapchain.present_mode;
    swapchain_info.clipped = VK_TRUE;
    swapchain_info.oldSwapchain = VK_NULL_HANDLE;

    if(vkCreateSwapchainKHR(context->logical_device, &swapchain_info, NULL, &context->swapchain.swapchain) != VK_SUCCESS)
    {   
        return -1;
    }

    vkGetSwapchainImagesKHR(context->logical_device, context->swapchain.swapchain, &context->swapchain.image_count, NULL);
    context->swapchain.images.resize(context->swapchain.image_count);
    vkGetSwapchainImagesKHR(context->logical_device, context->swapchain.swapchain, &context->swapchain.image_count, context->swapchain.images.data());
    context->swapchain.image_views.resize(context->swapchain.image_count);

    {
        VkImageViewCreateInfo view_info{};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = context->swapchain.format.format;
        view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        for(int i = 0; i < context->swapchain.images.size(); i++)
        {
            view_info.image = context->swapchain.images[i];
            if(vkCreateImageView(context->logical_device, &view_info, NULL, &context->swapchain.image_views[i]) != VK_SUCCESS)
            {
                return -1;
            }
        }
    }

    return 0;
}


queue_families vk_get_device_queues(const VkPhysicalDevice& physical_device, vk_context& context)
{
    queue_families queues;
    uint32_t queue_family_count = 0;


    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, NULL);
    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families.data());

    for(int i = 0; i < queue_families.size(); i++)
    {
        if(queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            queues.graphics = i;
            queues.has_graphics = 1;
        }

        VkBool32 present_support = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, context.surface, &present_support);
        if(present_support)
        {
            queues.present = i;
            queues.has_present = 1;
        }

        if(queues.has_graphics && queues.has_present)
        {
            break;
        }
    }

    return queues;
}

int vk_terminate(vk_context* context)
{

    for(VkImageView& view : context->swapchain.image_views)
    {
        vkDestroyImageView(context->logical_device, view, NULL);
    }

    vkDestroySwapchainKHR(context->logical_device, context->swapchain.swapchain, NULL);
    vkDestroyDevice(context->logical_device, NULL);
    vkDestroySurfaceKHR(context->instance, context->surface, NULL);
    DestroyDebugUtilsMessengerEXT(context->instance, context->debug_messenger, NULL);
    vkDestroyInstance(context->instance, NULL);

    return 0;
}

static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) 
{

    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

    return VK_FALSE;
}