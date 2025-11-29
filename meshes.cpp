#include "meshes.h"
#include <cmath>
#include <cstring>
#include <glm/ext/scalar_constants.hpp>

MeshData generate_triangle_mesh_data() {
    MeshData mesh_data;
    mesh_data.vertices = {
        {glm::vec3(-0.5f, -0.5f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f)},
        {glm::vec3( 0.5f, -0.5f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)},
        {glm::vec3( 0.0f,  0.5f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)},
    };
    mesh_data.indices = {0, 1, 2};
    return mesh_data;
}

MeshData generate_plane_mesh_data(float size, uint32_t segments) {
    MeshData mesh;

    float step = size / static_cast<float>(segments);
    float half_size = size * 0.5f;

    // 生成顶点
    for (uint32_t y = 0; y <= segments; ++y) {
        for (uint32_t x = 0; x <= segments; ++x) {
            float x_pos = -half_size + static_cast<float>(x) * step;
            float z_pos = -half_size + static_cast<float>(y) * step;

            // 使用简单的颜色渐变
            float r = static_cast<float>(x) / static_cast<float>(segments);
            float g = static_cast<float>(y) / static_cast<float>(segments);

            mesh.vertices.push_back({
                glm::vec3(x_pos, 0.0f, z_pos),
                glm::vec3(r, g, 0.5f)
            });
        }
    }

    // 生成索引
    for (uint32_t y = 0; y < segments; ++y) {
        for (uint32_t x = 0; x < segments; ++x) {
            uint32_t top_left = y * (segments + 1) + x;
            uint32_t top_right = top_left + 1;
            uint32_t bottom_left = (y + 1) * (segments + 1) + x;
            uint32_t bottom_right = bottom_left + 1;

            // 第一个三角形
            mesh.indices.push_back(top_left);
            mesh.indices.push_back(bottom_left);
            mesh.indices.push_back(top_right);

            // 第二个三角形
            mesh.indices.push_back(top_right);
            mesh.indices.push_back(bottom_left);
            mesh.indices.push_back(bottom_right);
        }
    }

    return mesh;
}

bool increment_mesh_buffers_ref_count(MeshBuffersRegistry *mesh_buffers_registry,
                                      MeshBuffersHandle mesh_buffers_handle) {
    std::lock_guard<std::mutex> lock(mesh_buffers_registry->mutex);
    MeshBuffersEntry &entry = mesh_buffers_registry->entries[mesh_buffers_handle];
    if (entry.ref_count == 0) {
        // empty entry
        return false;
    }
    ++entry.ref_count;
    return true;
}

void decrement_mesh_buffers_ref_count(MeshBuffersRegistry *mesh_buffers_registry, TaskSystem *task_system,
                                      VkContext *context, MeshBuffersHandle mesh_buffers_handle) {
    std::lock_guard<std::mutex> lock(mesh_buffers_registry->mutex);
    MeshBuffersEntry &entry = mesh_buffers_registry->entries[mesh_buffers_handle];
    if ((--entry.ref_count) == 0) {
        MeshBuffers mesh_buffers = entry.mesh_buffers;
        memset(&entry, 0, sizeof(MeshBuffersEntry)); // zero out the entry
        // raise a task to destroy the mesh buffers
        push_task(task_system, [context, mesh_buffers]() {
            if (mesh_buffers.has_indices) {
                vkDestroyBuffer(context->device, mesh_buffers.index_buffer, nullptr);
                vkFreeMemory(context->device, mesh_buffers.index_buffer_memory, nullptr);
            }
            vkDestroyBuffer(context->device, mesh_buffers.vertex_buffer, nullptr);
            vkFreeMemory(context->device, mesh_buffers.vertex_buffer_memory, nullptr);
        });
    }
}

MeshBuffersHandle request_mesh_buffers(MeshBuffersRegistry *mesh_buffers_registry, TaskSystem *task_system,
                                       VkContext *context, MeshData &&mesh_data) {
    std::lock_guard<std::mutex> lock(mesh_buffers_registry->mutex);
    for (uint32_t i = 0; i < MAX_MESH_BUFFERS; ++i) {
        if (mesh_buffers_registry->entries[i].ref_count == 0) {
            mesh_buffers_registry->entries[i].ref_count = 1;
            mesh_buffers_registry->entries[i].uploaded = false;
            // raise a task to create the mesh buffers
            std::function task_body = [context, mesh_data = std::move(mesh_data)]() mutable -> MeshBuffers {
                MeshBuffers mesh_buffers = {};
                // 创建GPU缓冲区
                create_buffer(context, sizeof(Vertex) * mesh_data.vertices.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &mesh_buffers.vertex_buffer);

                VkMemoryRequirements vertex_buffer_memory_requirements;
                vkGetBufferMemoryRequirements(context->device, mesh_buffers.vertex_buffer, &vertex_buffer_memory_requirements);
                uint32_t vertex_buffer_memory_type_index = UINT32_MAX;
                get_memory_type_index(context, vertex_buffer_memory_requirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &vertex_buffer_memory_type_index);
                allocate_memory(context, vertex_buffer_memory_requirements.size, vertex_buffer_memory_type_index, &mesh_buffers.vertex_buffer_memory);
                void *vertex_buffer_data = nullptr;
                vkMapMemory(context->device, mesh_buffers.vertex_buffer_memory, 0, sizeof(Vertex) * mesh_data.vertices.size(), 0, &vertex_buffer_data);
                memcpy(vertex_buffer_data, mesh_data.vertices.data(), sizeof(Vertex) * mesh_data.vertices.size());
                vkUnmapMemory(context->device, mesh_buffers.vertex_buffer_memory);
                vkBindBufferMemory(context->device, mesh_buffers.vertex_buffer, mesh_buffers.vertex_buffer_memory, 0);

                mesh_buffers.vertex_count = static_cast<uint32_t>(mesh_data.vertices.size()); // 保存绘制元数据

                if (mesh_buffers.has_indices = !mesh_data.indices.empty(); mesh_buffers.has_indices) {
                    create_buffer(context, sizeof(uint32_t) * mesh_data.indices.size(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, &mesh_buffers.index_buffer);

                    VkMemoryRequirements index_buffer_memory_requirements;
                    vkGetBufferMemoryRequirements(context->device, mesh_buffers.index_buffer, &index_buffer_memory_requirements);
                    uint32_t index_buffer_memory_type_index = UINT32_MAX;
                    get_memory_type_index(context, index_buffer_memory_requirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &index_buffer_memory_type_index);
                    allocate_memory(context, index_buffer_memory_requirements.size, index_buffer_memory_type_index, &mesh_buffers.index_buffer_memory);
                    void *index_buffer_data = nullptr;
                    vkMapMemory(context->device, mesh_buffers.index_buffer_memory, 0, sizeof(uint32_t) * mesh_data.indices.size(), 0, &index_buffer_data);
                    memcpy(index_buffer_data, mesh_data.indices.data(), sizeof(uint32_t) * mesh_data.indices.size());
                    vkUnmapMemory(context->device, mesh_buffers.index_buffer_memory);
                    vkBindBufferMemory(context->device, mesh_buffers.index_buffer, mesh_buffers.index_buffer_memory, 0);

                    mesh_buffers.index_count = static_cast<uint32_t>(mesh_data.indices.size());
                    mesh_buffers.index_type = VK_INDEX_TYPE_UINT32; // 默认使用uint32索引
                }
                return mesh_buffers;
            };
            std::function task_callback = [mesh_buffers_registry, i](const MeshBuffers &mesh_buffers) mutable {
                std::lock_guard<std::mutex> lock(mesh_buffers_registry->mutex);
                mesh_buffers_registry->entries[i].mesh_buffers = mesh_buffers;
                mesh_buffers_registry->entries[i].uploaded = true;
            };
            std::function task = [task_body = std::move(task_body), task_callback = std::move(task_callback)]() {
                MeshBuffers mesh_buffers = task_body();
                task_callback(mesh_buffers);
            };
            push_task(task_system, std::move(task));
            return i;
        }
    }
    assert(false);
}

void release_mesh_buffers(MeshBuffersRegistry *mesh_buffers_registry, TaskSystem *task_system, VkContext *context,
                          MeshBuffersHandle mesh_buffers_handle) {
    decrement_mesh_buffers_ref_count(mesh_buffers_registry, task_system, context, mesh_buffers_handle);
}
