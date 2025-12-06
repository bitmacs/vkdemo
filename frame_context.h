#pragma once

#include "meshes.h"
#include <unordered_set>

struct FrameContext {
    std::unordered_set<MeshBuffersHandle> mesh_buffers_handles;
};

void add_ref(FrameContext *frame_context, MeshBuffersHandle mesh_buffers_handle);
void on_gpu_complete(FrameContext *frame_context, MeshBuffersRegistry *mesh_buffers_registry, TaskSystem *task_system,
                     VkContext *vk_context);
