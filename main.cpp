#include "camera.h"
#include "mesh.h"
#include "vk.h"
#include <cassert>
#include <cstring>
#include <unordered_set>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#define MAX_FRAMES_IN_FLIGHT 1

// MeshBuffers: 包含GPU资源和绘制所需的元数据
struct MeshBuffers {
    // GPU资源
    VkBuffer vertex_buffer;
    VkBuffer index_buffer;
    VkDeviceMemory vertex_buffer_memory;
    VkDeviceMemory index_buffer_memory;

    // 绘制元数据（从Mesh创建时保存，用于绘制命令）
    uint32_t index_count;      // 索引数量，用于vkCmdDrawIndexed
    uint32_t vertex_count;     // 顶点数量，用于vkCmdDraw（非索引绘制）
    VkIndexType index_type;    // 索引类型，默认为VK_INDEX_TYPE_UINT32
};

struct VkDemo {
};

Camera camera = {};
VkPolygonMode polygon_mode = VK_POLYGON_MODE_FILL;
VkCullModeFlags cull_mode = VK_CULL_MODE_BACK_BIT;
std::vector<MeshBuffers> mesh_buffers_registry;

static void glfw_error_callback(int error, const char *description) {
    assert(false);
}

static void glfw_key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    if (action == GLFW_RELEASE) {
        if (key == GLFW_KEY_ESCAPE) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        } else if (key == GLFW_KEY_P) {
            polygon_mode = polygon_mode == VK_POLYGON_MODE_FILL ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
        } else if (key == GLFW_KEY_C) {
            cull_mode = cull_mode == VK_CULL_MODE_NONE ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE;
        }
        {
            glm::mat3 rot_mat = glm::mat3_cast(camera.orientation);
            glm::vec3 right = rot_mat[0];
            glm::vec3 forward = -rot_mat[2];
            glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

            float move_delta = 1.0f;
            glm::vec3 move_dir(0.0f);

            if (key == GLFW_KEY_W) { move_dir += forward; }
            if (key == GLFW_KEY_S) { move_dir -= forward; }
            if (key == GLFW_KEY_A) { move_dir -= right; }
            if (key == GLFW_KEY_D) { move_dir += right; }
            if (key == GLFW_KEY_Q) { move_dir += up; }
            if (key == GLFW_KEY_E) { move_dir -= up; }
            if (glm::length(move_dir) > 0.0f) {
                camera.position += glm::normalize(move_dir) * move_delta;
            }
        }
        {
            float angle_delta = glm::radians(5.0f);
            glm::vec2 rot_delta(0.0f);

            if (key == GLFW_KEY_UP) { rot_delta.x += angle_delta; }
            if (key == GLFW_KEY_DOWN) { rot_delta.x -= angle_delta; }
            if (key == GLFW_KEY_LEFT) { rot_delta.y += angle_delta; }
            if (key == GLFW_KEY_RIGHT) { rot_delta.y -= angle_delta; }

            if (rot_delta.x != 0.0f || rot_delta.y != 0.0f) {
                glm::quat yaw_quat = glm::angleAxis(rot_delta.y, glm::vec3(0.0f, 1.0f, 0.0f));
                glm::quat pitch_quat = glm::angleAxis(rot_delta.x, glm::vec3(1.0f, 0.0f, 0.0f));
                camera.orientation = glm::normalize(yaw_quat * camera.orientation * pitch_quat);
            }
        }
    }
}

struct CameraData {
    glm::mat4 view;
    glm::mat4 projection;
};

struct InstanceConstants {
    glm::mat4 model;
};

// 从Mesh创建MeshBuffers，同时保存绘制所需的元数据
void create_mesh_buffers(VkContext *context, const Mesh &mesh, MeshBuffers *mesh_buffers) {
    // 创建GPU缓冲区
    create_buffer(context, sizeof(Vertex) * mesh.vertices.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &mesh_buffers->vertex_buffer);
    create_buffer(context, sizeof(uint32_t) * mesh.indices.size(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, &mesh_buffers->index_buffer);

    VkMemoryRequirements vertex_buffer_memory_requirements;
    VkMemoryRequirements index_buffer_memory_requirements;
    vkGetBufferMemoryRequirements(context->device, mesh_buffers->vertex_buffer, &vertex_buffer_memory_requirements);
    vkGetBufferMemoryRequirements(context->device, mesh_buffers->index_buffer, &index_buffer_memory_requirements);
    uint32_t vertex_buffer_memory_type_index = UINT32_MAX;
    uint32_t index_buffer_memory_type_index = UINT32_MAX;
    get_memory_type_index(context, vertex_buffer_memory_requirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &vertex_buffer_memory_type_index);
    get_memory_type_index(context, index_buffer_memory_requirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &index_buffer_memory_type_index);

    allocate_memory(context, vertex_buffer_memory_requirements.size, vertex_buffer_memory_type_index, &mesh_buffers->vertex_buffer_memory);
    allocate_memory(context, index_buffer_memory_requirements.size, index_buffer_memory_type_index, &mesh_buffers->index_buffer_memory);
    void *vertex_buffer_data = nullptr;
    void *index_buffer_data = nullptr;
    vkMapMemory(context->device, mesh_buffers->vertex_buffer_memory, 0, sizeof(Vertex) * mesh.vertices.size(), 0, &vertex_buffer_data);
    vkMapMemory(context->device, mesh_buffers->index_buffer_memory, 0, sizeof(uint32_t) * mesh.indices.size(), 0, &index_buffer_data);
    memcpy(vertex_buffer_data, mesh.vertices.data(), sizeof(Vertex) * mesh.vertices.size());
    memcpy(index_buffer_data, mesh.indices.data(), sizeof(uint32_t) * mesh.indices.size());
    vkUnmapMemory(context->device, mesh_buffers->vertex_buffer_memory);
    vkUnmapMemory(context->device, mesh_buffers->index_buffer_memory);
    vkBindBufferMemory(context->device, mesh_buffers->vertex_buffer, mesh_buffers->vertex_buffer_memory, 0);
    vkBindBufferMemory(context->device, mesh_buffers->index_buffer, mesh_buffers->index_buffer_memory, 0);

    // 保存绘制元数据
    mesh_buffers->index_count = static_cast<uint32_t>(mesh.indices.size());
    mesh_buffers->vertex_count = static_cast<uint32_t>(mesh.vertices.size());
    mesh_buffers->index_type = VK_INDEX_TYPE_UINT32; // 默认使用uint32索引
}

void destroy_mesh_buffers(VkContext *context, MeshBuffers *mesh_buffers) {
    vkDestroyBuffer(context->device, mesh_buffers->index_buffer, nullptr);
    vkDestroyBuffer(context->device, mesh_buffers->vertex_buffer, nullptr);
    vkFreeMemory(context->device, mesh_buffers->index_buffer_memory, nullptr);
    vkFreeMemory(context->device, mesh_buffers->vertex_buffer_memory, nullptr);
}

struct FrameContext {
};

struct SemaphorePool {
    std::unordered_set<VkSemaphore> available_semaphores;
    std::unordered_set<VkSemaphore> used_semaphores;

    VkSemaphore acquire_semaphore(VkContext *context) {
        if (available_semaphores.empty()) {
            VkSemaphoreCreateInfo semaphore_create_info = {};
            semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            semaphore_create_info.flags = VK_SEMAPHORE_TYPE_BINARY;
            VkSemaphore semaphore;
            vkCreateSemaphore(context->device, &semaphore_create_info, nullptr, &semaphore);
            available_semaphores.insert(semaphore);
        }
        VkSemaphore semaphore = *available_semaphores.begin();
        available_semaphores.erase(semaphore);
        used_semaphores.insert(semaphore);
        return semaphore;
    }

    void release_semaphore(VkSemaphore semaphore) {
        used_semaphores.erase(semaphore);
        available_semaphores.insert(semaphore);
    }

    void cleanup(VkContext *context) {
        for (auto &semaphore: available_semaphores) {
            vkDestroySemaphore(context->device, semaphore, nullptr);
        }
        available_semaphores.clear();
        for (auto &semaphore: used_semaphores) {
            vkDestroySemaphore(context->device, semaphore, nullptr);
        }
        used_semaphores.clear();
    }
};

std::vector<VkFence> fences = {}; // each frame has a fence
std::vector<VkSemaphore> image_acquired_semaphores = {}; // each swapchain image has a image acquired semaphore
std::vector<VkSemaphore> render_complete_semaphores = {}; // each swapchain image has a render complete semaphore
SemaphorePool semaphore_pool = {}; // currently used for image acquired semaphores and render complete semaphores
std::vector<VkCommandBuffer> command_buffers = {}; // each frame has a command buffer
std::vector<VkDescriptorPool> descriptor_pools = {}; // each frame has a descriptor pool
std::vector<VkDescriptorSet> descriptor_sets = {}; // each frame has a descriptor set

static void create_descriptor_pools(VkContext *context) {
    descriptor_pools.resize(MAX_FRAMES_IN_FLIGHT);

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
                                                 &descriptor_pools[i]);
        assert(result == VK_SUCCESS);
    }
}

static void allocate_descriptor_sets(VkContext *context) {
    descriptor_sets.resize(MAX_FRAMES_IN_FLIGHT);

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {};
        descriptor_set_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descriptor_set_allocate_info.descriptorPool = descriptor_pools[i];
        descriptor_set_allocate_info.descriptorSetCount = 1;
        descriptor_set_allocate_info.pSetLayouts = &context->descriptor_set_layout;

        VkResult result = vkAllocateDescriptorSets(context->device, &descriptor_set_allocate_info, &descriptor_sets[i]);
        assert(result == VK_SUCCESS);
    }
}

static void wait_for_frame(VkContext *context, uint32_t frame_index) {
    VkResult result = vkWaitForFences(context->device, 1, &fences[frame_index], VK_TRUE,UINT64_MAX);
    assert(result == VK_SUCCESS);
    result = vkResetFences(context->device, 1, &fences[frame_index]);
    assert(result == VK_SUCCESS);
}

int main() {
    glfwSetErrorCallback(glfw_error_callback);
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    int width = 800;
    int height = 600;
    GLFWwindow *window = glfwCreateWindow(width, height, "VkDemo", nullptr, nullptr);
    glfwSetKeyCallback(window, glfw_key_callback);
    VkContext vk_context;
    init_vk(&vk_context, window, width, height);

    create_descriptor_pools(&vk_context);
    allocate_descriptor_sets(&vk_context);
    {
        command_buffers.resize(MAX_FRAMES_IN_FLIGHT);

        VkCommandBufferAllocateInfo command_buffer_allocate_info = {};
        command_buffer_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        command_buffer_allocate_info.commandPool = vk_context.command_pool;
        command_buffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        command_buffer_allocate_info.commandBufferCount = command_buffers.size();
        VkResult result = vkAllocateCommandBuffers(vk_context.device, &command_buffer_allocate_info,
                                                   command_buffers.data());
        assert(result == VK_SUCCESS);
    }

    fences.resize(MAX_FRAMES_IN_FLIGHT);
    VkFenceCreateInfo fence_create_info = {};
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkResult result = vkCreateFence(vk_context.device, &fence_create_info, nullptr, &fences[i]);
        assert(result == VK_SUCCESS);
    }
    image_acquired_semaphores.resize(vk_context.swapchain_image_count, VK_NULL_HANDLE);
    render_complete_semaphores.resize(vk_context.swapchain_image_count, VK_NULL_HANDLE);
    std::vector<FrameContext> frame_contexts = {};
    frame_contexts.resize(MAX_FRAMES_IN_FLIGHT);
    uint32_t frame_index = 0;
    uint64_t frame_count = 0;

    camera.position = glm::vec3(0.0f, 0.0f, 2.0f);
    camera.orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    camera.fov_y = glm::radians(45.0f);
    camera.aspect_ratio = (float) width / (float) height;
    camera.z_near = 0.1f;
    camera.z_far = 100.0f;

    CameraData camera_data = {};

    std::vector<VkBuffer> camera_buffers = {};
    std::vector<VkDeviceMemory> camera_buffer_memories = {};
    camera_buffers.resize(MAX_FRAMES_IN_FLIGHT);
    camera_buffer_memories.resize(MAX_FRAMES_IN_FLIGHT);
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        create_buffer(&vk_context, sizeof(CameraData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &camera_buffers[i]);

        VkMemoryRequirements memory_requirements;
        vkGetBufferMemoryRequirements(vk_context.device, camera_buffers[i], &memory_requirements);
        uint32_t memory_type_index = UINT32_MAX;
        get_memory_type_index(&vk_context, memory_requirements,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                              &memory_type_index);
        allocate_memory(&vk_context, memory_requirements.size, memory_type_index, &camera_buffer_memories[i]);
        vkBindBufferMemory(vk_context.device, camera_buffers[i], camera_buffer_memories[i], 0);
    }

    // 使用mesh生成系统创建mesh
    {
        Mesh mesh = generate_triangle_mesh();
        MeshBuffers mesh_buffers = {};
        create_mesh_buffers(&vk_context, mesh, &mesh_buffers);
        mesh_buffers_registry.push_back(mesh_buffers);
    }

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        wait_for_frame(&vk_context, frame_index);

        uint32_t image_index;
        VkSemaphore image_acquired_semaphore = semaphore_pool.acquire_semaphore(&vk_context);
        acquire_next_image(&vk_context, image_acquired_semaphore, &image_index);
        if (image_acquired_semaphores[image_index] != VK_NULL_HANDLE) {
            semaphore_pool.release_semaphore(image_acquired_semaphores[image_index]);
        }
        image_acquired_semaphores[image_index] = image_acquired_semaphore;

        VkCommandBuffer command_buffer = command_buffers[frame_index];
        begin_command_buffer(&vk_context, command_buffer);

        VkDescriptorBufferInfo descriptor_buffer_info = {};
        descriptor_buffer_info.buffer = camera_buffers[frame_index];
        descriptor_buffer_info.offset = 0;
        descriptor_buffer_info.range = sizeof(CameraData);

        VkWriteDescriptorSet write_descriptor_set = {};
        write_descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write_descriptor_set.dstSet = descriptor_sets[frame_index];
        write_descriptor_set.dstBinding = 0;
        write_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write_descriptor_set.descriptorCount = 1;
        write_descriptor_set.pBufferInfo = &descriptor_buffer_info;

        vkUpdateDescriptorSets(vk_context.device, 1, &write_descriptor_set, 0, nullptr);

        const glm::mat4 view = compute_view_matrix(camera);
        const glm::mat4 projection = compute_projection_matrix(camera);

        // vulkan clip space has inverted y and half z
        glm::mat4 clip = glm::mat4(
            1.0f,  0.0f, 0.0f, 0.0f, // 1st column
            0.0f, -1.0f, 0.0f, 0.0f,
            0.0f,  0.0f, 0.5f, 0.0f,
            0.0f,  0.0f, 0.5f, 1.0f
        );

        camera_data.view = view;
        camera_data.projection = clip * projection;

        void *ptr = nullptr;
        vkMapMemory(vk_context.device, camera_buffer_memories[frame_index], 0, sizeof(CameraData), 0, &ptr);
        memcpy(ptr, &camera_data, sizeof(CameraData));
        vkUnmapMemory(vk_context.device, camera_buffer_memories[frame_index]);

        {
            VkClearValue clear_value = {};
            clear_value.color = {.float32 = {0.2f, 0.6f, 0.4f, 1.0f}};

            begin_render_pass(&vk_context, command_buffer, vk_context.render_pass,
                              vk_context.framebuffers[image_index], width, height, &clear_value);

            VkPipeline pipeline = polygon_mode == VK_POLYGON_MODE_FILL
                                      ? vk_context.pipeline_solid
                                      : vk_context.pipeline_wireframe;
            vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
            vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_context.pipeline_layout, 0, 1,
                                    &descriptor_sets[frame_index], 0, nullptr);
            set_viewport(command_buffer, 0, 0, width, height);
            set_scissor(command_buffer, 0, 0, width, height);
            vkCmdSetCullMode(command_buffer, cull_mode);

            for (auto &mesh_buffers: mesh_buffers_registry) {
                InstanceConstants instance = {};
                instance.model = glm::mat4(1.0f);
                vkCmdPushConstants(command_buffer, vk_context.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                                   sizeof(InstanceConstants), &instance);
                VkDeviceSize offsets[] = {0};
                vkCmdBindVertexBuffers(command_buffer, 0, 1, &mesh_buffers.vertex_buffer, offsets);
                vkCmdBindIndexBuffer(command_buffer, mesh_buffers.index_buffer, 0, mesh_buffers.index_type);
                vkCmdDrawIndexed(command_buffer, mesh_buffers.index_count, 1, 0, 0, 0);
            }

            end_render_pass(&vk_context, command_buffer);
        }

        end_command_buffer(&vk_context, command_buffer);

        VkSemaphore render_complete_semaphore = semaphore_pool.acquire_semaphore(&vk_context);
        submit(&vk_context, command_buffer, image_acquired_semaphore, render_complete_semaphore, fences[frame_index]);
        if (render_complete_semaphores[image_index] != VK_NULL_HANDLE) {
            semaphore_pool.release_semaphore(render_complete_semaphores[image_index]);
        }
        render_complete_semaphores[image_index] = render_complete_semaphore;

        present(&vk_context, render_complete_semaphore, image_index);

        frame_index = (frame_index + 1) % MAX_FRAMES_IN_FLIGHT;
    }
    vkDeviceWaitIdle(vk_context.device);
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        // todo cleanup frame context
    }
    frame_contexts.clear();
    for (uint32_t i = 0; i < vk_context.swapchain_image_count; ++i) {
        if (render_complete_semaphores[i] != VK_NULL_HANDLE) {
            semaphore_pool.release_semaphore(render_complete_semaphores[i]);
        }
    }
    render_complete_semaphores.clear();
    for (uint32_t i = 0; i < vk_context.swapchain_image_count; ++i) {
        if (image_acquired_semaphores[i] != VK_NULL_HANDLE) {
            semaphore_pool.release_semaphore(image_acquired_semaphores[i]);
        }
    }
    image_acquired_semaphores.clear();
    semaphore_pool.cleanup(&vk_context);
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        vkDestroyFence(vk_context.device, fences[i], nullptr);
    }
    fences.clear();
    for (auto &mesh_buffers: mesh_buffers_registry) {
        destroy_mesh_buffers(&vk_context, &mesh_buffers);
    }
    mesh_buffers_registry.clear();
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        vkDestroyBuffer(vk_context.device, camera_buffers[i], nullptr);
        vkFreeMemory(vk_context.device, camera_buffer_memories[i], nullptr);
    }
    vkFreeCommandBuffers(vk_context.device, vk_context.command_pool, MAX_FRAMES_IN_FLIGHT, command_buffers.data());
    command_buffers.clear();
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        vkFreeDescriptorSets(vk_context.device, descriptor_pools[i], 1, &descriptor_sets[i]);
    }
    descriptor_sets.clear();
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        vkDestroyDescriptorPool(vk_context.device, descriptor_pools[i], nullptr);
    }
    descriptor_pools.clear();
    cleanup_vk(&vk_context);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
