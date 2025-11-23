#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <glm/vec3.hpp>
#include <vector>

struct VkContext {
    VkInstance instance;
    VkDebugUtilsMessengerEXT debug_utils_messenger;
    VkSurfaceKHR surface;
    VkPhysicalDevice physical_device;
    uint32_t queue_family_index;
    VkDevice device;
    VkQueue queue;
    VkSwapchainKHR swapchain;
    VkFormat surface_format;
    VkColorSpaceKHR surface_color_space;
    uint32_t swapchain_image_count;
    std::vector<VkImage> swapchain_images;
    std::vector<VkImageView> swapchain_image_views;
    VkCommandPool command_pool;
    VkRenderPass render_pass;
    std::vector<VkFramebuffer> framebuffers;
    VkDescriptorSetLayout descriptor_set_layout;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline_solid;
    VkPipeline pipeline_wireframe;
};

void init_vk(VkContext *context, GLFWwindow *window, uint32_t width, uint32_t height);

void cleanup_vk(VkContext *context);

void acquire_next_image(VkContext *context, VkSemaphore image_acquired_semaphore, uint32_t *image_index);

void submit(VkContext *context, VkCommandBuffer command_buffer, VkSemaphore wait_semaphore,
            VkSemaphore signal_semaphore, VkFence fence);

void present(VkContext *context, VkSemaphore wait_semaphore, uint32_t image_index);

void begin_render_pass(VkContext *context, VkCommandBuffer command_buffer, VkRenderPass render_pass,
                       VkFramebuffer framebuffer, uint32_t width, uint32_t height, VkClearValue *clear_value);

void end_render_pass(VkContext *context, VkCommandBuffer command_buffer);

void begin_command_buffer(VkContext *context, VkCommandBuffer command_buffer);

void end_command_buffer(VkContext *context, VkCommandBuffer command_buffer);

void create_buffer(VkContext *context, VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer *buffer);

void get_memory_type_index(VkContext *context, const VkMemoryRequirements &memory_requirements,
                           VkMemoryPropertyFlags memory_property_flags, uint32_t *memory_type_index);

void allocate_memory(VkContext *context, VkDeviceSize size, uint32_t memory_type_index, VkDeviceMemory *memory);

void set_viewport(VkCommandBuffer command_buffer, uint32_t x, uint32_t y, uint32_t width, uint32_t height);

void set_scissor(VkCommandBuffer command_buffer, uint32_t x, uint32_t y, uint32_t width, uint32_t height);
