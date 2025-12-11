#include "meshes.h"
#include <cmath>
#include <cstring>
#include <glm/ext/scalar_constants.hpp>

MeshData generate_triangle_mesh_data() {
    MeshData mesh_data;
    mesh_data.vertices = {
        {glm::vec3(-0.5f, -0.5f, 0.0f)},
        {glm::vec3( 0.5f, -0.5f, 0.0f)},
        {glm::vec3( 0.0f,  0.5f, 0.0f)},
    };
    mesh_data.indices = {0, 1, 2};
    mesh_data.primitive_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
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

            mesh.vertices.push_back({
                glm::vec3(x_pos, 0.0f, z_pos)
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

    mesh.primitive_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    return mesh;
}

MeshData generate_line_mesh_data(const glm::vec3 &start, const glm::vec3 &end) {
    MeshData mesh;
    mesh.vertices = {{start}, {end}};
    mesh.primitive_topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    return mesh;
}

MeshData generate_ring_mesh_data(float radius, uint32_t segments) {
    MeshData mesh;

    float angle_step = 2.0f * glm::pi<float>() / static_cast<float>(segments);

    // 绘制圆环
    for (uint32_t i = 0; i <= segments; ++i) {
        float angle = (float) i * angle_step;
        float cos_a = std::cos(angle);
        float sin_a = std::sin(angle);
        mesh.vertices.push_back({glm::vec3(radius * cos_a, 0.0f, radius * sin_a)});
    }

    mesh.primitive_topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
    return mesh;
}

MeshData generate_quad_mesh_data(float width, float height) {
    MeshData mesh;

    float half_width = width * 0.5f;
    float half_height = height * 0.5f;

    // 定义4个顶点（在XY平面上）
    mesh.vertices = {
        {glm::vec3(-half_width, -half_height, 0.0f)}, // 左下
        {glm::vec3( half_width, -half_height, 0.0f)}, // 右下
        {glm::vec3( half_width,  half_height, 0.0f)}, // 右上
        {glm::vec3(-half_width,  half_height, 0.0f)}, // 左上
    };

    // 定义两个三角形
    mesh.indices = {
        0, 1, 2,  // 第一个三角形
        2, 3, 0,  // 第二个三角形
    };

    mesh.primitive_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
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
            if (mesh_buffers.index_count > 0) {
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

                if (!mesh_data.indices.empty()) {
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
                mesh_buffers.primitive_topology = mesh_data.primitive_topology;
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
