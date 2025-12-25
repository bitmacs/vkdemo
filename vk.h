#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <vector>

struct PipelineKey {
    // 位域布局（总共64位）：
    // [0-4]   primitive_topology (5 bits)
    // [5-6]   polygon_mode (2 bits)
    // [7]     depth_test_enabled (1 bit)
    union {
        struct {
            uint32_t primitive_topology: 5; // bits [0-4]: VkPrimitiveTopology (转换为uint32_t)
            uint32_t polygon_mode: 2; // bits [5-6]: VK_POLYGON_MODE_FILL, LINE
            uint32_t depth_test: 1; // bit [7]: 是否启用深度测试
        };
        uint32_t state_bits; // 低32位状态
    };

    uint32_t shader_hash; // 高32位：shader hash

    PipelineKey(VkPrimitiveTopology topology, VkPolygonMode mode, bool depth_test_enabled)
        : state_bits(0), shader_hash(0) {
        primitive_topology = static_cast<uint32_t>(topology);
        polygon_mode = static_cast<uint32_t>(mode);
        depth_test = depth_test_enabled ? 1 : 0;
    }

    bool operator==(const PipelineKey &other) const {
        return state_bits == other.state_bits && shader_hash == other.shader_hash;
    }
};

struct PipelineKeyHash {
    size_t operator()(const PipelineKey &key) const {
        uint64_t hash = 0; // 将状态打包成64位整数
        hash |= static_cast<uint64_t>(key.state_bits); // 低32位：状态位域
        hash |= static_cast<uint64_t>(key.shader_hash) << 32; // 高32位：shader hash
        return hash;
    }
};

struct InstanceConstants {
    glm::mat4 model;
    glm::vec3 color;
    uint32_t camera_index;
};

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
    VkRenderPass render_pass; // 离屏渲染的 render pass
    VkFormat depth_image_format;
    std::vector<VkImage> depth_images;
    std::vector<VkDeviceMemory> depth_image_memories;
    std::vector<VkImageView> depth_image_views;

    // 离屏渲染资源（每个 in-flight 帧一份）
    std::vector<VkImage> color_images;
    std::vector<VkDeviceMemory> color_image_memories;
    std::vector<VkImageView> color_image_views;
    std::vector<VkFramebuffer> framebuffers;

    VkDescriptorSetLayout descriptor_set_layout;
    VkPipelineLayout pipeline_layout;
    std::unordered_map<PipelineKey, VkPipeline, PipelineKeyHash> pipelines;
};

void init_vk(VkContext *context, GLFWwindow *window, uint32_t width, uint32_t height, uint32_t max_frames_in_flight);

void cleanup_vk(VkContext *context);

void acquire_next_image(VkContext *context, VkSemaphore image_acquired_semaphore, uint32_t *image_index);

void submit(VkContext *context, VkCommandBuffer command_buffer, VkSemaphore wait_semaphore, VkPipelineStageFlags wait_dst_stage,
            VkSemaphore signal_semaphore, VkFence fence);

void present(VkContext *context, VkSemaphore wait_semaphore, uint32_t image_index);

void begin_render_pass(VkContext *context, VkCommandBuffer command_buffer, VkRenderPass render_pass,
                       VkFramebuffer framebuffer, uint32_t width, uint32_t height, VkClearValue *clear_values, size_t clear_value_count);

void end_render_pass(VkContext *context, VkCommandBuffer command_buffer);

void begin_command_buffer(VkContext *context, VkCommandBuffer command_buffer);

void end_command_buffer(VkContext *context, VkCommandBuffer command_buffer);

void create_buffer(VkContext *context, VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer *buffer);

void get_memory_type_index(VkContext *context, const VkMemoryRequirements &memory_requirements,
                           VkMemoryPropertyFlags memory_property_flags, uint32_t *memory_type_index);

void allocate_memory(VkContext *context, VkDeviceSize size, uint32_t memory_type_index, VkDeviceMemory *memory);

void set_viewport(VkCommandBuffer command_buffer, uint32_t x, uint32_t y, uint32_t width, uint32_t height);

void set_scissor(VkCommandBuffer command_buffer, uint32_t x, uint32_t y, uint32_t width, uint32_t height);

void allocate_descriptor_set(VkContext *context, VkDescriptorPool descriptor_pool, VkDescriptorSet *descriptor_set);

VkPipeline get_pipeline(VkContext *context, PipelineKey pipeline_key);

void apply_pipeline_dynamic_states(VkContext *context, VkCommandBuffer command_buffer, PipelineKey pipeline_key,
                                   VkCullModeFlags cull_mode);
