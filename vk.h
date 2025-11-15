#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <vector>

#define MAX_FRAMES_IN_FLIGHT 2

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
    std::vector<VkImage> swapchain_images;
    std::vector<VkImageView> swapchain_image_views;
    VkCommandPool command_pool;
    std::vector<VkCommandBuffer> command_buffers;
    std::vector<VkFence> fences;
    std::vector<VkSemaphore> image_acquired_semaphores;
    std::vector<VkSemaphore> render_complete_semaphores;
    VkRenderPass render_pass;
    std::vector<VkFramebuffer> framebuffers;
    std::vector<VkDescriptorPool> descriptor_pools;
    VkDescriptorSetLayout descriptor_set_layout;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
    std::vector<VkDescriptorSet> descriptor_sets;
    uint32_t frame_index;
};

void init_vk(VkContext *context, GLFWwindow *window, uint32_t width, uint32_t height);

void cleanup_vk(VkContext *context);

// wait for the previous frame's graphics commands to complete on the gpu
void wait_for_previous_frame(VkContext *context);

void acquire_next_image(VkContext *context, uint32_t *image_index);

void submit(VkContext *context);

void present(VkContext *context, uint32_t image_index);

void begin_render_pass(VkContext *context, VkCommandBuffer command_buffer, VkRenderPass render_pass,
                       VkFramebuffer framebuffer, uint32_t width, uint32_t height, VkClearValue *clear_value);

void end_render_pass(VkContext *context, VkCommandBuffer command_buffer);

void begin_command_buffer(VkContext *context, VkCommandBuffer command_buffer);

void end_command_buffer(VkContext *context, VkCommandBuffer command_buffer);

void create_buffer(VkContext *context, VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer *buffer);
