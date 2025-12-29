#include "vk.h"
#include "file.h"
#include "meshes.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <glm/mat4x4.hpp>
#include <vulkan/vulkan_core.h>

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

    // Configure validation features first
    std::vector<VkValidationFeatureEnableEXT> validation_feature_enables = {};
    validation_feature_enables.push_back(VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT);
    // validation_feature_enables.push_back(VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT);
    // validation_feature_enables.push_back(VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT);

    VkValidationFeaturesEXT validation_features = {};
    validation_features.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
    validation_features.enabledValidationFeatureCount = validation_feature_enables.size();
    validation_features.pEnabledValidationFeatures = validation_feature_enables.data();
    validation_features.pNext = nullptr; // End of chain

    // Configure debug messenger to capture validation messages during instance creation
    // This should be first in the pNext chain to catch messages from instance creation
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
    debug_utils_messenger_create_info.pNext = &validation_features; // Link validation features

    VkInstanceCreateInfo instance_create_info = {};
    instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_create_info.pApplicationInfo = &app_info;
    instance_create_info.enabledExtensionCount = instance_extensions.size();
    instance_create_info.ppEnabledExtensionNames = instance_extensions.data();
    instance_create_info.enabledLayerCount = instance_layers.size();
    instance_create_info.ppEnabledLayerNames = instance_layers.data();
    instance_create_info.pNext = &debug_utils_messenger_create_info; // Start with debug messenger
    instance_create_info.flags |= has_VK_KHR_portability_enumeration
                                      ? VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR
                                      : 0;

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

        // if (device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            context->physical_device = device;
            context->queue_family_index = queue_family_index;

            printf("using physical device: %s\n", device_properties.deviceName);

            return;
        // }
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
            } else if (strcmp(extension.extensionName, "VK_KHR_shader_non_semantic_info") == 0) {
                device_extensions.push_back("VK_KHR_shader_non_semantic_info");
            }
        }
    }
    device_extensions.push_back("VK_KHR_swapchain");
    // device_extensions.push_back("VK_KHR_deferred_host_operations");
    // device_extensions.push_back("VK_KHR_acceleration_structure");
    // device_extensions.push_back("VK_KHR_ray_query");
    // device_extensions.push_back("VK_KHR_pipeline_library");
    // device_extensions.push_back("VK_KHR_ray_tracing_pipeline");

    device_layers.push_back("VK_LAYER_KHRONOS_validation");

    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_create_info = {};
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = context->queue_family_index;
    queue_create_info.queueCount = 1;
    queue_create_info.pQueuePriorities = &queue_priority;

    VkPhysicalDeviceFeatures device_features = {};
    device_features.fillModeNonSolid = VK_TRUE;

    VkDeviceCreateInfo device_create_info = {};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.queueCreateInfoCount = 1;
    device_create_info.pQueueCreateInfos = &queue_create_info;
    device_create_info.enabledExtensionCount = device_extensions.size();
    device_create_info.ppEnabledExtensionNames = device_extensions.data();
    device_create_info.enabledLayerCount = device_layers.size();
    device_create_info.ppEnabledLayerNames = device_layers.data();
    device_create_info.pEnabledFeatures = &device_features;

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

    VkSurfaceCapabilitiesKHR surface_capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context->physical_device, context->surface, &surface_capabilities);

    uint32_t min_image_count = 2;
    if (surface_capabilities.minImageCount > min_image_count) {
        min_image_count = surface_capabilities.minImageCount;
    }
    if (surface_capabilities.maxImageCount > 0 && surface_capabilities.maxImageCount < min_image_count) {
        min_image_count = surface_capabilities.maxImageCount;
    }

    // 检查 surface 是否支持 TRANSFER_DST_BIT（用于 blit 到 swapchain）
    VkImageUsageFlags image_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    assert(surface_capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT && "surface does not support TRANSFER_DST_BIT");
    image_usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    VkSwapchainCreateInfoKHR swapchain_create_info = {};
    swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_create_info.surface = context->surface;
    swapchain_create_info.minImageCount = min_image_count;
    swapchain_create_info.imageFormat = surface_format;
    swapchain_create_info.imageColorSpace = surface_color_space;
    swapchain_create_info.imageExtent = {width, height};
    swapchain_create_info.imageArrayLayers = 1;
    swapchain_create_info.imageUsage = image_usage;
    swapchain_create_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_create_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapchain_create_info.clipped = VK_TRUE;
    swapchain_create_info.oldSwapchain = VK_NULL_HANDLE;

    VkResult result = vkCreateSwapchainKHR(context->device, &swapchain_create_info, nullptr, &context->swapchain);
    assert(result == VK_SUCCESS);

    context->surface_format = surface_format;
    context->surface_color_space = surface_color_space;
    context->swapchain_image_count = min_image_count;

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

static void create_render_pass(VkContext *context) {
    VkAttachmentDescription color_attachment = {};
    color_attachment.format = context->surface_format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL; // 用于 blit

    VkAttachmentDescription depth_attachment = {};
    depth_attachment.format = context->depth_image_format;
    depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription attachments[2] = {color_attachment, depth_attachment};

    VkAttachmentReference color_attachment_ref = {};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_attachment_ref = {};
    depth_attachment_ref.attachment = 1;
    depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;
    subpass.pDepthStencilAttachment = &depth_attachment_ref;

    VkRenderPassCreateInfo render_pass_create_info = {};
    render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_create_info.attachmentCount = std::size(attachments);
    render_pass_create_info.pAttachments = attachments;
    render_pass_create_info.subpassCount = 1;
    render_pass_create_info.pSubpasses = &subpass;
    VkResult result = vkCreateRenderPass(context->device, &render_pass_create_info, nullptr, &context->render_pass);
    assert(result == VK_SUCCESS);
}

static void create_offscreen_resources(VkContext *context, uint32_t width, uint32_t height, uint32_t frame_count) {
    // 为每个 in-flight 帧创建离屏资源
    context->color_images.resize(frame_count);
    context->color_image_memories.resize(frame_count);
    context->color_image_views.resize(frame_count);
    context->depth_images.resize(frame_count);
    context->depth_image_memories.resize(frame_count);
    context->depth_image_views.resize(frame_count);
    context->framebuffers.resize(frame_count);

    for (uint32_t i = 0; i < frame_count; ++i) {
        {
            VkImageCreateInfo image_create_info = {};
            image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            image_create_info.imageType = VK_IMAGE_TYPE_2D;
            image_create_info.format = context->surface_format;
            image_create_info.extent = {width, height, 1};
            image_create_info.mipLevels = 1;
            image_create_info.arrayLayers = 1;
            image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
            image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
            image_create_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
            image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            VkResult result = vkCreateImage(context->device, &image_create_info, nullptr, &context->color_images[i]);
            assert(result == VK_SUCCESS);

            VkMemoryRequirements memory_requirements;
            vkGetImageMemoryRequirements(context->device, context->color_images[i], &memory_requirements);
            uint32_t memory_type_index = UINT32_MAX;
            get_memory_type_index(context, memory_requirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memory_type_index);
            allocate_memory(context, memory_requirements.size, memory_type_index, &context->color_image_memories[i]);
            vkBindImageMemory(context->device, context->color_images[i], context->color_image_memories[i], 0);

            VkImageViewCreateInfo image_view_create_info = {};
            image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            image_view_create_info.image = context->color_images[i];
            image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            image_view_create_info.format = context->surface_format;
            image_view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            image_view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            image_view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            image_view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            image_view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            image_view_create_info.subresourceRange.baseMipLevel = 0;
            image_view_create_info.subresourceRange.levelCount = 1;
            image_view_create_info.subresourceRange.baseArrayLayer = 0;
            image_view_create_info.subresourceRange.layerCount = 1;
            result = vkCreateImageView(context->device, &image_view_create_info, nullptr, &context->color_image_views[i]);
            assert(result == VK_SUCCESS);
        }
        {
            VkImageCreateInfo image_create_info = {};
            image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            image_create_info.imageType = VK_IMAGE_TYPE_2D;
            image_create_info.format = context->depth_image_format;
            image_create_info.extent = {width, height, 1};
            image_create_info.mipLevels = 1;
            image_create_info.arrayLayers = 1;
            image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
            image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
            image_create_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            VkResult result = vkCreateImage(context->device, &image_create_info, nullptr, &context->depth_images[i]);
            assert(result == VK_SUCCESS);

            VkMemoryRequirements memory_requirements;
            vkGetImageMemoryRequirements(context->device, context->depth_images[i], &memory_requirements);
            uint32_t memory_type_index = UINT32_MAX;
            get_memory_type_index(context, memory_requirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memory_type_index);
            allocate_memory(context, memory_requirements.size, memory_type_index, &context->depth_image_memories[i]);
            vkBindImageMemory(context->device, context->depth_images[i], context->depth_image_memories[i], 0);

            VkImageViewCreateInfo image_view_create_info = {};
            image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            image_view_create_info.image = context->depth_images[i];
            image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            image_view_create_info.format = context->depth_image_format;
            image_view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            image_view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            image_view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            image_view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            image_view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            image_view_create_info.subresourceRange.baseMipLevel = 0;
            image_view_create_info.subresourceRange.levelCount = 1;
            image_view_create_info.subresourceRange.baseArrayLayer = 0;
            image_view_create_info.subresourceRange.layerCount = 1;
            result = vkCreateImageView(context->device, &image_view_create_info, nullptr, &context->depth_image_views[i]);
            assert(result == VK_SUCCESS);
        }
        {
            VkImageView attachments[2] = {context->color_image_views[i], context->depth_image_views[i]};

            VkFramebufferCreateInfo framebuffer_create_info = {};
            framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebuffer_create_info.renderPass = context->render_pass;
            framebuffer_create_info.attachmentCount = std::size(attachments);
            framebuffer_create_info.pAttachments = attachments;
            framebuffer_create_info.width = width;
            framebuffer_create_info.height = height;
            framebuffer_create_info.layers = 1;

            VkResult result = vkCreateFramebuffer(context->device, &framebuffer_create_info, nullptr, &context->framebuffers[i]);
            assert(result == VK_SUCCESS);
        }
    }
}

static void create_descriptor_set_layout(VkContext *context) {
    VkDescriptorSetLayoutBinding descriptor_set_layout_bindings[] = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2, VK_SHADER_STAGE_VERTEX_BIT, nullptr}, // camera array with 2 cameras
    };

    VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info = {};
    descriptor_set_layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptor_set_layout_create_info.bindingCount = std::size(descriptor_set_layout_bindings);
    descriptor_set_layout_create_info.pBindings = descriptor_set_layout_bindings;
    VkResult result = vkCreateDescriptorSetLayout(context->device, &descriptor_set_layout_create_info, nullptr,
                                                  &context->descriptor_set_layout);
    assert(result == VK_SUCCESS);
}

static void create_pipeline_layout(VkContext *context, size_t push_constant_size) {
    VkPushConstantRange push_constant_range = {};
    push_constant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    push_constant_range.offset = 0;
    push_constant_range.size = push_constant_size;

    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {};
    pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_create_info.setLayoutCount = 1;
    pipeline_layout_create_info.pSetLayouts = &context->descriptor_set_layout;
    pipeline_layout_create_info.pushConstantRangeCount = 1;
    pipeline_layout_create_info.pPushConstantRanges = &push_constant_range;
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

static void create_pipeline(VkContext *context, VkPrimitiveTopology primitive_topology, VkPolygonMode polygon_mode, bool depth_test_enabled) {
    if (primitive_topology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST ||
        primitive_topology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP) {
        assert(polygon_mode == VK_POLYGON_MODE_LINE); // polygon mode must be line for line list or line strip topology
    }

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

    VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info = {};
    vertex_input_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_state_create_info.vertexBindingDescriptionCount = 1;
    vertex_input_state_create_info.pVertexBindingDescriptions = &vertex_input_binding_description;
    vertex_input_state_create_info.vertexAttributeDescriptionCount = vertex_input_attribute_descriptions.size();
    vertex_input_state_create_info.pVertexAttributeDescriptions = vertex_input_attribute_descriptions.data();

    VkPipelineInputAssemblyStateCreateInfo input_assembly_state_create_info = {};
    input_assembly_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_state_create_info.topology = primitive_topology;
    if (primitive_topology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP) {
        input_assembly_state_create_info.primitiveRestartEnable = VK_TRUE;
    }

    VkPipelineRasterizationStateCreateInfo rasterization_state_create_info = {};
    rasterization_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization_state_create_info.polygonMode = polygon_mode;
    rasterization_state_create_info.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization_state_create_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization_state_create_info.lineWidth = 1.0f;

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
    depth_stencil_state_create_info.depthTestEnable = depth_test_enabled ? VK_TRUE : VK_FALSE;
    depth_stencil_state_create_info.depthWriteEnable = depth_test_enabled ? VK_TRUE : VK_FALSE;
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
    if (primitive_topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST) {
        dynamic_states.emplace_back(VK_DYNAMIC_STATE_CULL_MODE);
    }

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

    VkPipeline pipeline;
    VkResult result = vkCreateGraphicsPipelines(context->device, nullptr, 1, &pipeline_create_info, nullptr, &pipeline);
    assert(result == VK_SUCCESS);

    PipelineKey pipeline_key(primitive_topology, polygon_mode, depth_test_enabled);
    context->pipelines[pipeline_key] = pipeline;

    vkDestroyShaderModule(context->device, vertex_shader_module, nullptr);
    vkDestroyShaderModule(context->device, fragment_shader_module, nullptr);
}

void init_vk(VkContext *context, GLFWwindow *window, uint32_t width, uint32_t height, uint32_t max_frames_in_flight) {
    create_instance(context);
    create_surface(context, window);
    pick_physical_device(context);
    create_device(context);
    create_swapchain(context, width, height);
    context->depth_image_format = VK_FORMAT_D16_UNORM;
    create_render_pass(context);
    create_offscreen_resources(context, width, height, max_frames_in_flight);
    create_command_pool(context);
    create_descriptor_set_layout(context);
    create_pipeline_layout(context, sizeof(InstanceConstants));
    create_pipeline(context, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_POLYGON_MODE_FILL, true);
    create_pipeline(context, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_POLYGON_MODE_FILL, false);
    create_pipeline(context, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_POLYGON_MODE_LINE, true);
    create_pipeline(context, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_POLYGON_MODE_LINE, false);
    create_pipeline(context, VK_PRIMITIVE_TOPOLOGY_LINE_LIST, VK_POLYGON_MODE_LINE, true);
    create_pipeline(context, VK_PRIMITIVE_TOPOLOGY_LINE_STRIP, VK_POLYGON_MODE_LINE, true);

    // typically used for gizmos
    create_pipeline(context, VK_PRIMITIVE_TOPOLOGY_LINE_LIST, VK_POLYGON_MODE_LINE, false);
    create_pipeline(context, VK_PRIMITIVE_TOPOLOGY_LINE_STRIP, VK_POLYGON_MODE_LINE, false);
}

void cleanup_vk(VkContext *context) {
    for (auto &[_, pipeline]: context->pipelines) {
        vkDestroyPipeline(context->device, pipeline, nullptr);
    }
    context->pipelines.clear();
    vkDestroyPipelineLayout(context->device, context->pipeline_layout, nullptr);
    vkDestroyDescriptorSetLayout(context->device, context->descriptor_set_layout, nullptr);
    vkDestroyCommandPool(context->device, context->command_pool, nullptr);
    // 清理离屏资源
    for (size_t i = 0; i < context->framebuffers.size(); ++i) {
        vkDestroyFramebuffer(context->device, context->framebuffers[i], nullptr);
    }
    context->framebuffers.clear();
    for (size_t i = 0; i < context->color_images.size(); ++i) {
        vkDestroyImageView(context->device, context->color_image_views[i], nullptr);
        vkFreeMemory(context->device, context->color_image_memories[i], nullptr);
        vkDestroyImage(context->device, context->color_images[i], nullptr);
    }
    context->color_image_views.clear();
    context->color_image_memories.clear();
    context->color_images.clear();
    for (size_t i = 0; i < context->depth_image_views.size(); ++i) {
        vkDestroyImageView(context->device, context->depth_image_views[i], nullptr);
        vkFreeMemory(context->device, context->depth_image_memories[i], nullptr);
        vkDestroyImage(context->device, context->depth_images[i], nullptr);
    }
    context->depth_image_views.clear();
    context->depth_image_memories.clear();
    context->depth_images.clear();
    vkDestroyRenderPass(context->device, context->render_pass, nullptr);
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

void acquire_next_image(VkContext *context, VkSemaphore image_acquired_semaphore, uint32_t *image_index) {
    VkResult result = vkAcquireNextImageKHR(context->device, context->swapchain, UINT64_MAX, image_acquired_semaphore,
                                            VK_NULL_HANDLE, image_index);
    // assert(result == VK_SUCCESS);
}

void submit(VkContext *context, VkCommandBuffer command_buffer, VkSemaphore wait_semaphore, VkPipelineStageFlags wait_dst_stage,
            VkSemaphore signal_semaphore, VkFence fence) {
    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &wait_semaphore;
    submit_info.pWaitDstStageMask = &wait_dst_stage;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &signal_semaphore;

    VkResult result = vkQueueSubmit(context->queue, 1, &submit_info, fence);
    assert(result == VK_SUCCESS);
}

void present(VkContext *context, VkSemaphore wait_semaphore, uint32_t image_index) {
    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &wait_semaphore;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &context->swapchain;
    present_info.pImageIndices = &image_index;

    VkResult result = vkQueuePresentKHR(context->queue, &present_info);
    // assert(result == VK_SUCCESS);
}

void begin_render_pass(VkContext *context, VkCommandBuffer command_buffer, VkRenderPass render_pass,
                       VkFramebuffer framebuffer, uint32_t width, uint32_t height, VkClearValue *clear_values, size_t clear_value_count) {
    VkRenderPassBeginInfo render_pass_begin_info = {};
    render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_begin_info.renderPass = render_pass;
    render_pass_begin_info.framebuffer = framebuffer;
    render_pass_begin_info.renderArea.extent.width = width;
    render_pass_begin_info.renderArea.extent.height = height;
    render_pass_begin_info.clearValueCount = clear_value_count;
    render_pass_begin_info.pClearValues = clear_values;
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
    VkBufferCreateInfo buffer_create_info = {};
    buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create_info.size = size;
    buffer_create_info.usage = usage;
    buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    buffer_create_info.queueFamilyIndexCount = 1; // only one queue family will use this buffer
    buffer_create_info.pQueueFamilyIndices = &context->queue_family_index;

    VkResult result = vkCreateBuffer(context->device, &buffer_create_info, nullptr, buffer);
    assert(result == VK_SUCCESS);
}

void get_memory_type_index(VkContext *context, const VkMemoryRequirements &memory_requirements,
                           VkMemoryPropertyFlags memory_property_flags, uint32_t *memory_type_index) {
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

void set_viewport(VkCommandBuffer command_buffer, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    VkViewport viewport = {};
    viewport.x = (float) x;
    viewport.y = (float) y;
    viewport.width = (float) width;
    viewport.height = (float) height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);
}

void set_scissor(VkCommandBuffer command_buffer, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    VkRect2D scissor = {};
    scissor.offset.x = x;
    scissor.offset.y = y;
    scissor.extent.width = width;
    scissor.extent.height = height;
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);
}

void allocate_descriptor_set(VkContext *context, VkDescriptorPool descriptor_pool, VkDescriptorSet *descriptor_set) {
    VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {};
    descriptor_set_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptor_set_allocate_info.descriptorPool = descriptor_pool;
    descriptor_set_allocate_info.descriptorSetCount = 1;
    descriptor_set_allocate_info.pSetLayouts = &context->descriptor_set_layout;

    VkResult result = vkAllocateDescriptorSets(context->device, &descriptor_set_allocate_info, descriptor_set);
    assert(result == VK_SUCCESS);
}

VkPipeline get_pipeline(VkContext *context, PipelineKey pipeline_key) {
    if (const auto it = context->pipelines.find(pipeline_key); it != context->pipelines.end()) { return it->second; }
    assert(false);
}

void apply_pipeline_dynamic_states(VkContext *context, VkCommandBuffer command_buffer, PipelineKey pipeline_key,
                                   VkCullModeFlags cull_mode) {
    if (pipeline_key.primitive_topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST) {
        vkCmdSetCullMode(command_buffer, cull_mode);
    }
}

void blit_image(VkCommandBuffer command_buffer, VkImage src_image, VkImageLayout src_image_layout, VkImage dst_image, VkImageLayout dst_image_layout, uint32_t width, uint32_t height) {
    VkImageBlit blit_region = {};
    blit_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit_region.srcSubresource.mipLevel = 0;
    blit_region.srcSubresource.baseArrayLayer = 0;
    blit_region.srcSubresource.layerCount = 1;
    blit_region.srcOffsets[0] = {0, 0, 0};
    blit_region.srcOffsets[1] = {(int32_t) width, (int32_t) height, 1};
    blit_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit_region.dstSubresource.mipLevel = 0;
    blit_region.dstSubresource.baseArrayLayer = 0;
    blit_region.dstSubresource.layerCount = 1;
    blit_region.dstOffsets[0] = {0, 0, 0};
    blit_region.dstOffsets[1] = {(int32_t) width, (int32_t) height, 1};
    vkCmdBlitImage(command_buffer, src_image, src_image_layout, dst_image, dst_image_layout, 1, &blit_region, VK_FILTER_LINEAR);
}
