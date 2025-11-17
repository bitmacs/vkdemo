#include "vk.h"
#include "file.h"
#include <cassert>
#include <iostream>

#define LOAD_INSTANCE_PROC_ADDR(instance, name) (PFN_ ## name) vkGetInstanceProcAddr(instance, #name);
#define LOAD_DEVICE_PROC_ADDR(device, name) (PFN_ ## name) vkGetDeviceProcAddr(device, #name);

VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
                                              VkDebugUtilsMessageTypeFlagsEXT message_type,
                                              const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
                                              void *user_data) {
    const char *severity_str;
    const char *type_str;

    if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        severity_str = "error";
    } else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        severity_str = "warning";
    } else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
        severity_str = "info";
    } else {
        severity_str = "debug";
    }

    if (message_type & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) {
        type_str = "general";
    } else if (message_type & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
        type_str = "validation";
    } else if (message_type & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) {
        type_str = "performance";
    } else {
        type_str = "unknown";
    }

    std::cerr << "validation layer: " << severity_str << ": " << type_str << ": " << callback_data->pMessage <<
            std::endl;
    return VK_FALSE;
}

static void create_instance(VkContext *context) {
    std::vector<const char *> instance_extensions;
    std::vector<const char *> instance_layers;

    {
        uint32_t count;
        const char **extensions = glfwGetRequiredInstanceExtensions(&count);
        instance_extensions.insert(instance_extensions.end(), extensions, extensions + count);
    }
    bool has_VK_KHR_portability_enumeration = false;
    {
        // enumerate instance extensions
        uint32_t count = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
        std::vector<VkExtensionProperties> extensions(count);
        vkEnumerateInstanceExtensionProperties(nullptr, &count, extensions.data());
        for (const auto &extension: extensions) {
            if (strcmp(extension.extensionName, "VK_KHR_portability_enumeration") == 0) {
                instance_extensions.push_back("VK_KHR_portability_enumeration");
                has_VK_KHR_portability_enumeration = true;
            }
        }
    }
    instance_extensions.push_back("VK_EXT_debug_utils");

    instance_layers.push_back("VK_LAYER_KHRONOS_validation");

    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "VkDemo";
    app_info.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
    app_info.pEngineName = "VkDemo";
    app_info.engineVersion = VK_MAKE_VERSION(0, 0, 1);
    app_info.apiVersion = VK_API_VERSION_1_3;

    VkDebugUtilsMessengerCreateInfoEXT debug_utils_messenger_create_info = {};
    debug_utils_messenger_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debug_utils_messenger_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                                        VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                                                        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                                        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debug_utils_messenger_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                                    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                                    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debug_utils_messenger_create_info.pfnUserCallback = debug_callback;

    VkInstanceCreateInfo instance_create_info = {};
    instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_create_info.pApplicationInfo = &app_info;
    instance_create_info.enabledExtensionCount = instance_extensions.size();
    instance_create_info.ppEnabledExtensionNames = instance_extensions.data();
    instance_create_info.enabledLayerCount = instance_layers.size();
    instance_create_info.ppEnabledLayerNames = instance_layers.data();
    instance_create_info.pNext = &debug_utils_messenger_create_info;
    instance_create_info.flags |= has_VK_KHR_portability_enumeration ? VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR : 0;

    VkResult result = vkCreateInstance(&instance_create_info, nullptr, &context->instance);
    assert(result == VK_SUCCESS);

    auto vkCreateDebugUtilsMessengerEXT = LOAD_INSTANCE_PROC_ADDR(context->instance, vkCreateDebugUtilsMessengerEXT);
    result = vkCreateDebugUtilsMessengerEXT(context->instance, &debug_utils_messenger_create_info, nullptr,
                                            &context->debug_utils_messenger);
    assert(result == VK_SUCCESS);
}

static void create_surface(VkContext *context, GLFWwindow *window) {
    VkResult result = glfwCreateWindowSurface(context->instance, window, nullptr, &context->surface);
    assert(result == VK_SUCCESS);
}

static void pick_physical_device(VkContext *context) {
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(context->instance, &device_count, nullptr);
    assert(device_count > 0);

    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(context->instance, &device_count, devices.data());

    for (const auto &device: devices) {
        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);

        std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

        uint32_t queue_family_index = UINT32_MAX;

        for (uint32_t i = 0; i < queue_family_count; ++i) {
            if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                VkBool32 presentation_support = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(device, i, context->surface, &presentation_support);
                if (presentation_support) {
                    queue_family_index = i;
                    break;
                }
            }
        }

        if (queue_family_index == UINT32_MAX) {
            continue; // no queue family support graphics and presentation
        }

        VkPhysicalDeviceProperties device_properties;
        vkGetPhysicalDeviceProperties(device, &device_properties);

        if (device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            context->physical_device = device;
            context->queue_family_index = queue_family_index;

            printf("using physical device: %s\n", device_properties.deviceName);

            return;
        }
    }
    assert(false && "no suitable physical device found");
}

static void create_device(VkContext *context) {
    std::vector<const char *> device_extensions;
    std::vector<const char *> device_layers;

    {
        // enumerate device extensions
        uint32_t count = 0;
        vkEnumerateDeviceExtensionProperties(context->physical_device, nullptr, &count, nullptr);
        std::vector<VkExtensionProperties> extensions(count);
        vkEnumerateDeviceExtensionProperties(context->physical_device, nullptr, &count, extensions.data());
        for (const auto &extension: extensions) {
            if (strcmp(extension.extensionName, "VK_KHR_portability_subset") == 0) {
                device_extensions.push_back("VK_KHR_portability_subset");
            }
        }
    }
    device_extensions.push_back("VK_KHR_swapchain");
    device_extensions.push_back("VK_KHR_deferred_host_operations");
    device_extensions.push_back("VK_KHR_acceleration_structure");
    device_extensions.push_back("VK_KHR_ray_query");
    device_extensions.push_back("VK_KHR_pipeline_library");
    device_extensions.push_back("VK_KHR_ray_tracing_pipeline");

    device_layers.push_back("VK_LAYER_KHRONOS_validation");

    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_create_info = {};
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = context->queue_family_index;
    queue_create_info.queueCount = 1;
    queue_create_info.pQueuePriorities = &queue_priority;

    VkDeviceCreateInfo device_create_info = {};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.queueCreateInfoCount = 1;
    device_create_info.pQueueCreateInfos = &queue_create_info;
    device_create_info.enabledExtensionCount = device_extensions.size();
    device_create_info.ppEnabledExtensionNames = device_extensions.data();
    device_create_info.enabledLayerCount = device_layers.size();
    device_create_info.ppEnabledLayerNames = device_layers.data();

    VkResult result = vkCreateDevice(context->physical_device, &device_create_info, nullptr, &context->device);
    assert(result == VK_SUCCESS);

    vkGetDeviceQueue(context->device, context->queue_family_index, 0, &context->queue);
}

static void create_swapchain(VkContext *context, uint32_t width, uint32_t height) {
    // get supported formats
    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(context->physical_device, context->surface, &format_count, nullptr);
    assert(format_count > 0);

    std::vector<VkSurfaceFormatKHR> formats(format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(context->physical_device, context->surface, &format_count, formats.data());

    VkFormat surface_format = formats[0].format;
    VkColorSpaceKHR surface_color_space = formats[0].colorSpace;

    VkSwapchainCreateInfoKHR swapchain_create_info = {};
    swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_create_info.surface = context->surface;
    swapchain_create_info.minImageCount = MAX_FRAMES_IN_FLIGHT;
    swapchain_create_info.imageFormat = surface_format;
    swapchain_create_info.imageColorSpace = surface_color_space;
    swapchain_create_info.imageExtent = {width, height};
    swapchain_create_info.imageArrayLayers = 1;
    swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchain_create_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_create_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapchain_create_info.clipped = VK_TRUE;
    swapchain_create_info.oldSwapchain = VK_NULL_HANDLE;

    VkResult result = vkCreateSwapchainKHR(context->device, &swapchain_create_info, nullptr, &context->swapchain);
    assert(result == VK_SUCCESS);

    context->surface_format = surface_format;
    context->surface_color_space = surface_color_space;

    // get swapchain images
    uint32_t image_count = 0;
    vkGetSwapchainImagesKHR(context->device, context->swapchain, &image_count, nullptr);
    assert(image_count > 0);

    context->swapchain_images.resize(image_count);
    vkGetSwapchainImagesKHR(context->device, context->swapchain, &image_count, context->swapchain_images.data());

    // create swapchain image views
    context->swapchain_image_views.resize(image_count);
    for (size_t i = 0; i < image_count; i++) {
        VkImageViewCreateInfo image_view_create_info = {};
        image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        image_view_create_info.image = context->swapchain_images[i];
        image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        image_view_create_info.format = surface_format;
        image_view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_view_create_info.subresourceRange.baseMipLevel = 0;
        image_view_create_info.subresourceRange.levelCount = 1;
        image_view_create_info.subresourceRange.baseArrayLayer = 0;
        image_view_create_info.subresourceRange.layerCount = 1;
        VkResult result = vkCreateImageView(context->device, &image_view_create_info, nullptr,
                                            &context->swapchain_image_views[i]);
        assert(result == VK_SUCCESS);
    }
}

static void create_command_pool(VkContext *context) {
    VkCommandPoolCreateInfo command_pool_create_info = {};
    command_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    command_pool_create_info.queueFamilyIndex = context->queue_family_index;
    VkResult result = vkCreateCommandPool(context->device, &command_pool_create_info, nullptr, &context->command_pool);
    assert(result == VK_SUCCESS);
}

static void create_command_buffers(VkContext *context) {
    context->command_buffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo command_buffer_allocate_info = {};
    command_buffer_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_buffer_allocate_info.commandPool = context->command_pool;
    command_buffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command_buffer_allocate_info.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
    VkResult result = vkAllocateCommandBuffers(context->device, &command_buffer_allocate_info,
                                               context->command_buffers.data());
    assert(result == VK_SUCCESS);
}

static void create_fences(VkContext *context) {
    context->fences.resize(MAX_FRAMES_IN_FLIGHT);

    VkFenceCreateInfo fence_create_info = {};
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkResult result = vkCreateFence(context->device, &fence_create_info, nullptr, &context->fences[i]);
        assert(result == VK_SUCCESS);
    }
}

static void create_semaphores(VkContext *context) {
    context->image_acquired_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
    context->render_complete_semaphores.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphore_create_info = {};
    semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphore_create_info.flags = VK_SEMAPHORE_TYPE_BINARY;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkResult result = vkCreateSemaphore(context->device, &semaphore_create_info, nullptr,
                                            &context->image_acquired_semaphores[i]);
        assert(result == VK_SUCCESS);
        result = vkCreateSemaphore(context->device, &semaphore_create_info, nullptr,
                                   &context->render_complete_semaphores[i]);
        assert(result == VK_SUCCESS);
    }
}

static void create_render_pass(VkContext *context) {
    VkAttachmentDescription color_attachment = {};
    color_attachment.format = context->surface_format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_ref = {};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;

    VkRenderPassCreateInfo render_pass_create_info = {};
    render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_create_info.attachmentCount = 1;
    render_pass_create_info.pAttachments = &color_attachment;
    render_pass_create_info.subpassCount = 1;
    render_pass_create_info.pSubpasses = &subpass;
    VkResult result = vkCreateRenderPass(context->device, &render_pass_create_info, nullptr, &context->render_pass);
    assert(result == VK_SUCCESS);
}

static void create_framebuffers(VkContext *context, uint32_t width, uint32_t height) {
    context->framebuffers.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < context->swapchain_image_views.size(); ++i) {
        VkFramebufferCreateInfo framebuffer_create_info = {};
        framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_create_info.renderPass = context->render_pass;
        framebuffer_create_info.attachmentCount = 1;
        framebuffer_create_info.pAttachments = &context->swapchain_image_views[i];
        framebuffer_create_info.width = width;
        framebuffer_create_info.height = height;
        framebuffer_create_info.layers = 1;

        VkResult result = vkCreateFramebuffer(context->device, &framebuffer_create_info, nullptr,
                                              &context->framebuffers[i]);
        assert(result == VK_SUCCESS);
    }
}

static void create_descriptor_pools(VkContext *context) {
    context->descriptor_pools.resize(MAX_FRAMES_IN_FLIGHT);

    VkDescriptorPoolSize descriptor_pool_sizes[] = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
    };

    VkDescriptorPoolCreateInfo descriptor_pool_create_info = {};
    descriptor_pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptor_pool_create_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    descriptor_pool_create_info.maxSets = 1;
    descriptor_pool_create_info.poolSizeCount = std::size(descriptor_pool_sizes);
    descriptor_pool_create_info.pPoolSizes = descriptor_pool_sizes;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkResult result = vkCreateDescriptorPool(context->device, &descriptor_pool_create_info, nullptr,
                                                 &context->descriptor_pools[i]);
        assert(result == VK_SUCCESS);
    }
}

static void create_descriptor_set_layout(VkContext *context) {
    VkDescriptorSetLayoutBinding descriptor_set_layout_bindings[] = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
    };

    VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info = {};
    descriptor_set_layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptor_set_layout_create_info.bindingCount = std::size(descriptor_set_layout_bindings);
    descriptor_set_layout_create_info.pBindings = descriptor_set_layout_bindings;
    VkResult result = vkCreateDescriptorSetLayout(context->device, &descriptor_set_layout_create_info, nullptr,
                                                  &context->descriptor_set_layout);
    assert(result == VK_SUCCESS);
}

static void create_pipeline_layout(VkContext *context) {
    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {};
    pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_create_info.setLayoutCount = 1;
    pipeline_layout_create_info.pSetLayouts = &context->descriptor_set_layout;
    VkResult result = vkCreatePipelineLayout(context->device, &pipeline_layout_create_info, nullptr,
                                             &context->pipeline_layout);
    assert(result == VK_SUCCESS);
}

static void create_shader_module(VkContext *context, const char *filepath, VkShaderModule *shader_module) {
    const auto code = read_binary_file(filepath);

    VkShaderModuleCreateInfo shader_module_create_info = {};
    shader_module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_module_create_info.codeSize = code.size();
    shader_module_create_info.pCode = (const uint32_t *) code.data();
    VkResult result = vkCreateShaderModule(context->device, &shader_module_create_info, nullptr, shader_module);
    assert(result == VK_SUCCESS);
}

static void create_pipeline(VkContext *context) {
    VkVertexInputBindingDescription vertex_input_binding_description = {};
    vertex_input_binding_description.binding = 0;
    vertex_input_binding_description.stride = sizeof(Vertex);
    vertex_input_binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::vector<VkVertexInputAttributeDescription> vertex_input_attribute_descriptions = {};
    {
        VkVertexInputAttributeDescription vertex_input_attribute_description = {};
        vertex_input_attribute_description.binding = 0;
        vertex_input_attribute_description.location = 0;
        vertex_input_attribute_description.format = VK_FORMAT_R32G32B32_SFLOAT;
        vertex_input_attribute_description.offset = offsetof(Vertex, position);

        vertex_input_attribute_descriptions.push_back(vertex_input_attribute_description);
    }
    {
        VkVertexInputAttributeDescription vertex_input_attribute_description = {};
        vertex_input_attribute_description.binding = 0;
        vertex_input_attribute_description.location = 1;
        vertex_input_attribute_description.format = VK_FORMAT_R32G32B32_SFLOAT;
        vertex_input_attribute_description.offset = offsetof(Vertex, color);

        vertex_input_attribute_descriptions.push_back(vertex_input_attribute_description);
    }

    VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info = {};
    vertex_input_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_state_create_info.vertexBindingDescriptionCount = 1;
    vertex_input_state_create_info.pVertexBindingDescriptions = &vertex_input_binding_description;
    vertex_input_state_create_info.vertexAttributeDescriptionCount = vertex_input_attribute_descriptions.size();
    vertex_input_state_create_info.pVertexAttributeDescriptions = vertex_input_attribute_descriptions.data();

    VkPipelineInputAssemblyStateCreateInfo input_assembly_state_create_info = {};
    input_assembly_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_state_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineRasterizationStateCreateInfo rasterization_state_create_info = {};
    rasterization_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization_state_create_info.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization_state_create_info.lineWidth = 1.0f;
    rasterization_state_create_info.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization_state_create_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineColorBlendAttachmentState color_blend_attachment_state = {};
    color_blend_attachment_state.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment_state.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo color_blend_state_create_info = {};
    color_blend_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend_state_create_info.attachmentCount = 1;
    color_blend_state_create_info.pAttachments = &color_blend_attachment_state;

    VkPipelineDepthStencilStateCreateInfo depth_stencil_state_create_info = {};
    depth_stencil_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil_state_create_info.depthTestEnable = VK_TRUE;
    depth_stencil_state_create_info.depthWriteEnable = VK_TRUE;
    depth_stencil_state_create_info.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineViewportStateCreateInfo viewport_state_create_info = {};
    viewport_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state_create_info.viewportCount = 1;
    viewport_state_create_info.scissorCount = 1;

    VkPipelineMultisampleStateCreateInfo multisample_state_create_info = {};
    multisample_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample_state_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkShaderModule vertex_shader_module = VK_NULL_HANDLE;
    VkShaderModule fragment_shader_module = VK_NULL_HANDLE;
    create_shader_module(context, "triangle.vert.spv", &vertex_shader_module);
    create_shader_module(context, "triangle.frag.spv", &fragment_shader_module);

    VkPipelineShaderStageCreateInfo shader_stage_create_infos[] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertex_shader_module,
            .pName = "main",
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragment_shader_module,
            .pName = "main",
        },
    };

    std::vector<VkDynamicState> dynamic_states = {};
    dynamic_states.emplace_back(VK_DYNAMIC_STATE_VIEWPORT);
    dynamic_states.emplace_back(VK_DYNAMIC_STATE_SCISSOR);

    VkPipelineDynamicStateCreateInfo dynamic_state_create_info = {};
    dynamic_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state_create_info.dynamicStateCount = dynamic_states.size();
    dynamic_state_create_info.pDynamicStates = dynamic_states.data();

    VkGraphicsPipelineCreateInfo pipeline_create_info = {};
    pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_create_info.stageCount = std::size(shader_stage_create_infos);
    pipeline_create_info.pStages = shader_stage_create_infos;
    pipeline_create_info.pVertexInputState = &vertex_input_state_create_info;
    pipeline_create_info.pInputAssemblyState = &input_assembly_state_create_info;
    pipeline_create_info.pViewportState = &viewport_state_create_info;
    pipeline_create_info.pRasterizationState = &rasterization_state_create_info;
    pipeline_create_info.pMultisampleState = &multisample_state_create_info;
    pipeline_create_info.pDepthStencilState = &depth_stencil_state_create_info;
    pipeline_create_info.pColorBlendState = &color_blend_state_create_info;
    pipeline_create_info.pDynamicState = &dynamic_state_create_info;
    pipeline_create_info.layout = context->pipeline_layout;
    pipeline_create_info.renderPass = context->render_pass;
    VkResult result = vkCreateGraphicsPipelines(context->device, nullptr, 1, &pipeline_create_info, nullptr,
                                                &context->pipeline);
    assert(result == VK_SUCCESS);

    vkDestroyShaderModule(context->device, vertex_shader_module, nullptr);
    vkDestroyShaderModule(context->device, fragment_shader_module, nullptr);
}

static void allocate_descriptor_sets(VkContext *context) {
    context->descriptor_sets.resize(MAX_FRAMES_IN_FLIGHT);

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {};
        descriptor_set_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descriptor_set_allocate_info.descriptorPool = context->descriptor_pools[i];
        descriptor_set_allocate_info.descriptorSetCount = 1;
        descriptor_set_allocate_info.pSetLayouts = &context->descriptor_set_layout;

        VkResult result = vkAllocateDescriptorSets(context->device, &descriptor_set_allocate_info,
                                                   &context->descriptor_sets[i]);
        assert(result == VK_SUCCESS);
    }
}

void init_vk(VkContext *context, GLFWwindow *window, uint32_t width, uint32_t height) {
    create_instance(context);
    create_surface(context, window);
    pick_physical_device(context);
    create_device(context);
    create_swapchain(context, width, height);
    create_command_pool(context);
    create_command_buffers(context);
    create_fences(context);
    create_semaphores(context);
    create_render_pass(context);
    create_framebuffers(context, width, height);
    create_descriptor_pools(context);
    create_descriptor_set_layout(context);
    create_pipeline_layout(context);
    create_pipeline(context);
    allocate_descriptor_sets(context);
    context->frame_index = 0;
}

void cleanup_vk(VkContext *context) {
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        vkFreeDescriptorSets(context->device, context->descriptor_pools[i], 1, &context->descriptor_sets[i]);
    }
    context->descriptor_sets.clear();
    vkDestroyPipeline(context->device, context->pipeline, nullptr);
    vkDestroyPipelineLayout(context->device, context->pipeline_layout, nullptr);
    vkDestroyDescriptorSetLayout(context->device, context->descriptor_set_layout, nullptr);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        vkDestroyDescriptorPool(context->device, context->descriptor_pools[i], nullptr);
    }
    context->descriptor_pools.clear();
    for (size_t i = 0; i < context->framebuffers.size(); ++i) {
        vkDestroyFramebuffer(context->device, context->framebuffers[i], nullptr);
    }
    context->framebuffers.clear();
    vkDestroyRenderPass(context->device, context->render_pass, nullptr);
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        vkDestroySemaphore(context->device, context->render_complete_semaphores[i], nullptr);
        vkDestroySemaphore(context->device, context->image_acquired_semaphores[i], nullptr);
    }
    context->render_complete_semaphores.clear();
    context->image_acquired_semaphores.clear();
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        vkDestroyFence(context->device, context->fences[i], nullptr);
    }
    context->fences.clear();
    vkFreeCommandBuffers(context->device, context->command_pool, MAX_FRAMES_IN_FLIGHT, context->command_buffers.data());
    context->command_buffers.clear();
    vkDestroyCommandPool(context->device, context->command_pool, nullptr);
    for (uint32_t i = 0; i < context->swapchain_image_views.size(); ++i) {
        vkDestroyImageView(context->device, context->swapchain_image_views[i], nullptr);
    }
    context->swapchain_image_views.clear();
    context->swapchain_images.clear();
    vkDestroySwapchainKHR(context->device, context->swapchain, nullptr);
    vkDestroyDevice(context->device, nullptr);
    vkDestroySurfaceKHR(context->instance, context->surface, nullptr);
    auto vkDestroyDebugUtilsMessengerEXT = LOAD_INSTANCE_PROC_ADDR(context->instance, vkDestroyDebugUtilsMessengerEXT);
    vkDestroyDebugUtilsMessengerEXT(context->instance, context->debug_utils_messenger, nullptr);
    vkDestroyInstance(context->instance, nullptr);
}

void wait_for_previous_frame(VkContext *context) {
    VkResult result = vkWaitForFences(context->device, 1, &context->fences[context->frame_index], VK_TRUE,UINT64_MAX);
    assert(result == VK_SUCCESS);
    result = vkResetFences(context->device, 1, &context->fences[context->frame_index]);
    assert(result == VK_SUCCESS);
}

void acquire_next_image(VkContext *context, uint32_t *image_index) {
    VkResult result = vkAcquireNextImageKHR(context->device, context->swapchain, UINT64_MAX,
                                            context->image_acquired_semaphores[context->frame_index],
                                            VK_NULL_HANDLE, image_index);
    assert(result == VK_SUCCESS);
}

void submit(VkContext *context) {
    VkPipelineStageFlags wait_dst_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &context->image_acquired_semaphores[context->frame_index];
    submit_info.pWaitDstStageMask = &wait_dst_stage;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &context->command_buffers[context->frame_index];
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &context->render_complete_semaphores[context->frame_index];

    VkResult result = vkQueueSubmit(context->queue, 1, &submit_info, context->fences[context->frame_index]);
    assert(result == VK_SUCCESS);
}

void present(VkContext *context, uint32_t image_index) {
    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &context->render_complete_semaphores[context->frame_index];
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &context->swapchain;
    present_info.pImageIndices = &image_index;

    VkResult result = vkQueuePresentKHR(context->queue, &present_info);
    assert(result == VK_SUCCESS);
}

void begin_render_pass(VkContext *context, VkCommandBuffer command_buffer, VkRenderPass render_pass,
                       VkFramebuffer framebuffer, uint32_t width, uint32_t height, VkClearValue *clear_value) {
    VkRenderPassBeginInfo render_pass_begin_info = {};
    render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_begin_info.renderPass = render_pass;
    render_pass_begin_info.framebuffer = framebuffer;
    render_pass_begin_info.renderArea.extent.width = width;
    render_pass_begin_info.renderArea.extent.height = height;
    render_pass_begin_info.clearValueCount = 1;
    render_pass_begin_info.pClearValues = clear_value;
    vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
}

void end_render_pass(VkContext *context, VkCommandBuffer command_buffer) {
    vkCmdEndRenderPass(command_buffer);
}

void begin_command_buffer(VkContext *context, VkCommandBuffer command_buffer) {
    VkCommandBufferBeginInfo command_buffer_begin_info = {};
    command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info);
}

void end_command_buffer(VkContext *context, VkCommandBuffer command_buffer) {
    vkEndCommandBuffer(command_buffer);
}

void create_buffer(VkContext *context, VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer *buffer) {
    VkBufferCreateInfo buffer_create_info{};
    buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create_info.size = size;
    buffer_create_info.usage = usage;
    buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    buffer_create_info.queueFamilyIndexCount = 1; // only one queue family will use this buffer
    buffer_create_info.pQueueFamilyIndices = &context->queue_family_index;

    VkResult result = vkCreateBuffer(context->device, &buffer_create_info, nullptr, buffer);
    assert(result == VK_SUCCESS);
}

void get_memory_type_index(VkContext *context, const VkMemoryRequirements &memory_requirements, VkMemoryPropertyFlags memory_property_flags, uint32_t *memory_type_index) {
    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(context->physical_device, &memory_properties);

    *memory_type_index = UINT32_MAX;
    for (uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i) {
        if ((memory_requirements.memoryTypeBits & (1 << i)) &&
            memory_properties.memoryTypes[i].propertyFlags & memory_property_flags) {
            *memory_type_index = i;
            break;
        }
    }
    assert(*memory_type_index != UINT32_MAX);
}

void allocate_memory(VkContext *context, VkDeviceSize size, uint32_t memory_type_index, VkDeviceMemory *memory) {
    VkMemoryAllocateInfo memory_allocate_info = {};
    memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory_allocate_info.allocationSize = size;
    memory_allocate_info.memoryTypeIndex = memory_type_index;

    VkResult result = vkAllocateMemory(context->device, &memory_allocate_info, nullptr, memory);
    assert(result == VK_SUCCESS);
}
