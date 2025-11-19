#include "camera.h"
#include "mesh.h"
#include "vk.h"
#include <cassert>
#include <cstring>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

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
        } else if (key == GLFW_KEY_W) {
            camera.position.z -= 1.0f;
        } else if (key == GLFW_KEY_S) {
            camera.position.z += 1.0f;
        } else if (key == GLFW_KEY_A) {
            camera.position.x -= 1.0f;
        } else if (key == GLFW_KEY_D) {
            camera.position.x += 1.0f;
        } else if (key == GLFW_KEY_Q) {
            camera.position.y += 1.0f;
        } else if (key == GLFW_KEY_E) {
            camera.position.y -= 1.0f;
        } else if (key == GLFW_KEY_P) {
            polygon_mode = polygon_mode == VK_POLYGON_MODE_FILL ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
        } else if (key == GLFW_KEY_C) {
            cull_mode = cull_mode == VK_CULL_MODE_NONE ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE;
        }
        {
            const float angle_delta = glm::radians(5.0f);
            glm::vec2 rot_delta(0.0f);

            if (key == GLFW_KEY_UP) {
                rot_delta.x = -angle_delta;
            } else if (key == GLFW_KEY_DOWN) {
                rot_delta.x = angle_delta;
            }

            if (key == GLFW_KEY_LEFT) {
                rot_delta.y = -angle_delta;
            } else if (key == GLFW_KEY_RIGHT) {
                rot_delta.y = angle_delta;
            }

            if (rot_delta.x != 0.0f || rot_delta.y != 0.0f) {
                glm::quat yaw_quat   = glm::angleAxis(rot_delta.y, glm::vec3(0.0f, 1.0f, 0.0f)); // global yaw

                glm::vec3 local_x = camera.rotation * glm::vec3(1.0f, 0.0f, 0.0f);
                glm::quat pitch_quat = glm::angleAxis(rot_delta.x, local_x); // local pitch

                camera.rotation = yaw_quat * camera.rotation * pitch_quat;
            }
        }
    }
}

struct CameraData {
    glm::mat4 view;
    glm::mat4 projection;
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

    camera.position = glm::vec3(0.0f, 0.0f, 2.0f);
    camera.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
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
        get_memory_type_index(&vk_context, memory_requirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &memory_type_index);
        allocate_memory(&vk_context, memory_requirements.size, memory_type_index, &camera_buffer_memories[i]);
        vkBindBufferMemory(vk_context.device, camera_buffers[i], camera_buffer_memories[i], 0);
    }

    // 使用mesh生成系统创建mesh
    {
        Mesh mesh = generate_plane_mesh(4, 4);
        MeshBuffers mesh_buffers = {};
        create_mesh_buffers(&vk_context, mesh, &mesh_buffers);
        mesh_buffers_registry.push_back(mesh_buffers);
    }


    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        wait_for_previous_frame(&vk_context);

        uint32_t image_index;
        acquire_next_image(&vk_context, &image_index);

        VkCommandBuffer command_buffer = vk_context.command_buffers[vk_context.frame_index];
        begin_command_buffer(&vk_context, command_buffer);

        VkDescriptorBufferInfo descriptor_buffer_info = {};
        descriptor_buffer_info.buffer = camera_buffers[vk_context.frame_index];
        descriptor_buffer_info.offset = 0;
        descriptor_buffer_info.range = sizeof(CameraData);

        VkWriteDescriptorSet write_descriptor_set = {};
        write_descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write_descriptor_set.dstSet = vk_context.descriptor_sets[vk_context.frame_index];
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
        vkMapMemory(vk_context.device, camera_buffer_memories[vk_context.frame_index], 0, sizeof(CameraData), 0, &ptr);
        memcpy(ptr, &camera_data, sizeof(CameraData));
        vkUnmapMemory(vk_context.device, camera_buffer_memories[vk_context.frame_index]);

        {
            VkClearValue clear_value = {};
            clear_value.color = {.float32 = {0.2f, 0.6f, 0.4f, 1.0f}};

            begin_render_pass(&vk_context, command_buffer, vk_context.render_pass,
                              vk_context.framebuffers[vk_context.frame_index], width, height, &clear_value);

            VkPipeline pipeline = polygon_mode == VK_POLYGON_MODE_FILL ? vk_context.pipeline_solid : vk_context.pipeline_wireframe;
            vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
            vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_context.pipeline_layout, 0, 1,
                                    &vk_context.descriptor_sets[vk_context.frame_index], 0, nullptr);
            set_viewport(command_buffer, 0, 0, width, height);
            set_scissor(command_buffer, 0, 0, width, height);
            vkCmdSetCullMode(command_buffer, cull_mode);

            for (auto &mesh_buffers : mesh_buffers_registry) {
                VkDeviceSize offsets[] = {0};
                vkCmdBindVertexBuffers(command_buffer, 0, 1, &mesh_buffers.vertex_buffer, offsets);
                vkCmdBindIndexBuffer(command_buffer, mesh_buffers.index_buffer, 0, mesh_buffers.index_type);
                vkCmdDrawIndexed(command_buffer, mesh_buffers.index_count, 1, 0, 0, 0);
            }

            end_render_pass(&vk_context, command_buffer);
        }

        end_command_buffer(&vk_context, command_buffer);

        submit(&vk_context);

        present(&vk_context, image_index);

        vk_context.frame_index = (vk_context.frame_index + 1) % MAX_FRAMES_IN_FLIGHT;
    }
    vkDeviceWaitIdle(vk_context.device);
    for (auto &mesh_buffers : mesh_buffers_registry) {
        destroy_mesh_buffers(&vk_context, &mesh_buffers);
    }
    mesh_buffers_registry.clear();
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        vkDestroyBuffer(vk_context.device, camera_buffers[i], nullptr);
        vkFreeMemory(vk_context.device, camera_buffer_memories[i], nullptr);
    }
    cleanup_vk(&vk_context);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
