#pragma once

#include "tasks.h"
#include "vk.h"
#include <glm/glm.hpp>
#include <vector>
#include <vulkan/vulkan_core.h>

struct Vertex {
    glm::vec3 position;
    glm::vec3 color;
};

// Mesh数据容器，包含顶点和索引
struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

MeshData generate_triangle_mesh_data();

#define MAX_MESH_BUFFERS 1

struct MeshBuffers {
    // GPU资源
    VkBuffer vertex_buffer;
    VkBuffer index_buffer;
    VkDeviceMemory vertex_buffer_memory;
    VkDeviceMemory index_buffer_memory;

    // 绘制元数据（从Mesh创建时保存，用于绘制命令）
    uint32_t index_count; // 索引数量，用于vkCmdDrawIndexed
    uint32_t vertex_count; // 顶点数量，用于vkCmdDraw（非索引绘制）
    VkIndexType index_type; // 索引类型，默认为VK_INDEX_TYPE_UINT32
};

typedef uint32_t MeshBuffersHandle;

struct MeshBuffersEntry {
    MeshBuffers mesh_buffers;
    uint32_t ref_count;
    bool uploaded;
};

struct MeshBuffersRegistry {
    MeshBuffersEntry entries[MAX_MESH_BUFFERS];
    std::mutex mutex;
};

bool increment_mesh_buffers_ref_count(MeshBuffersRegistry *mesh_buffers_registry,
                                      MeshBuffersHandle mesh_buffers_handle);
void decrement_mesh_buffers_ref_count(MeshBuffersRegistry *mesh_buffers_registry, TaskSystem *task_system,
                                      VkContext *context, MeshBuffersHandle mesh_buffers_handle);
bool is_mesh_buffers_uploaded(MeshBuffersRegistry *mesh_buffers_registry, MeshBuffersHandle mesh_buffers_handle);
MeshBuffersHandle request_mesh_buffers(MeshBuffersRegistry *mesh_buffers_registry, TaskSystem *task_system,
                                       VkContext *context, MeshData &&mesh_data);
void release_mesh_buffers(MeshBuffersRegistry *mesh_buffers_registry, TaskSystem *task_system, VkContext *context,
                          MeshBuffersHandle mesh_buffers_handle);
