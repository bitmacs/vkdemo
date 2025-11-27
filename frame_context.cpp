#include "frame_context.h"

bool add_ref(FrameContext *frame_context, MeshBuffersRegistry *mesh_buffers_registry,
             MeshBuffersHandle mesh_buffers_handle) {
    if (!increment_mesh_buffers_ref_count(mesh_buffers_registry, mesh_buffers_handle)) {
        return false;
    }
    frame_context->mesh_buffers_handles.insert(mesh_buffers_handle);
    return true;
}

void on_gpu_complete(FrameContext *frame_context, MeshBuffersRegistry *mesh_buffers_registry, TaskSystem *task_system,
                     VkContext *vk_context) {
    for (auto &mesh_buffers_handle: frame_context->mesh_buffers_handles) {
        decrement_mesh_buffers_ref_count(mesh_buffers_registry, task_system, vk_context, mesh_buffers_handle);
    }
    frame_context->mesh_buffers_handles.clear();
}
