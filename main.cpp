#include "camera.h"
#include "ecs.h"
#include "frame_context.h"
#include "meshes.h"
#include "raycast.h"
#include "semaphores.h"
#include "tasks.h"
#include "vk.h"
#include <cassert>
#include <cstring>
#include <unordered_set>
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/TriangleShape.h>
#include <Jolt/RegisterTypes.h>

#define MAX_FRAMES_IN_FLIGHT 2

struct VkDemo {
};

TaskSystem task_system = {};
VkContext vk_context = {};
MeshBuffersRegistry mesh_buffers_registry = {};
Camera camera = {};
VkPolygonMode polygon_mode = VK_POLYGON_MODE_FILL;
VkCullModeFlags cull_mode = VK_CULL_MODE_BACK_BIT;
entt::registry registry;

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

static void glfw_scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
    float fov_y_delta = glm::radians(5.0f);
    float min_fov_y = glm::radians(10.0f);
    float max_fov_y = glm::radians(120.0f);

    camera.fov_y -= (float) yoffset * fov_y_delta;
    camera.fov_y = glm::clamp(camera.fov_y, min_fov_y, max_fov_y);
}

static void glfw_mouse_button_callback(GLFWwindow *window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
        int width, height;
        glfwGetWindowSize(window, &width, &height);

        double x, y;
        glfwGetCursorPos(window, &x, &y);

        auto [origin, dir] = compute_ray_from_screen(camera, (float) x, (float) y, (float) width, (float) height);

        glm::vec3 far_plane_intersection = compute_ray_far_plane_intersection(camera, origin, dir);

        auto entity = registry.create();

        Mesh &mesh = registry.emplace<Mesh>(entity);
        MeshData mesh_data = generate_line_mesh_data(origin, far_plane_intersection);
        mesh.mesh_buffers_handle = request_mesh_buffers(&mesh_buffers_registry, &task_system, &vk_context, std::move(mesh_data));

        Transform &transform = registry.emplace<Transform>(entity);
        transform.position = glm::vec3(0.0f, 0.0f, 0.0f);
        transform.orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        transform.scale = glm::vec3(1.0f, 1.0f, 1.0f);

        {
            // 创建一个三角形（三个顶点，逆时针顺序）
            JPH::Vec3 v1(-0.5f, -0.5f, 0.0f);
            JPH::Vec3 v2( 0.5f, -0.5f, 0.0f);
            JPH::Vec3 v3( 0.0f,  0.5f, 0.0f);
            JPH::RefConst triangle = new JPH::TriangleShape(v1, v2, v3);

            JPH::Vec3 ray_origin(origin.x, origin.y, origin.z);
            JPH::Vec3 ray_direction(dir.x, dir.y, dir.z);
            JPH::RayCast ray(ray_origin, ray_direction);

            // 执行 ray cast
            JPH::SubShapeIDCreator sub_shape_id_creator;
            JPH::RayCastResult hit;
            hit.mFraction = 100.0f; // 初始化为最大距离

            if (triangle->CastRay(ray, sub_shape_id_creator, hit)) {
                JPH::Vec3 hit_point = ray.GetPointOnRay(hit.mFraction);
                std::cout << "命中三角形" << std::endl;
                std::cout << "  命中点: (" << hit_point.GetX() << ", " << hit_point.GetY() << ", " << hit_point.GetZ() << ")" << std::endl;
                std::cout << "  距离分数: " << hit.mFraction << std::endl;
            } else {
                std::cout << "未命中三角形" << std::endl;
            }
        }

        {
            std::optional<float> distance = ray_ring_intersection_distance(Ray{origin, dir}, Ring{glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 1.0f});
            if (distance) {
                std::cout << "命中圆环" << std::endl;
                std::cout << "  距离: " << *distance << std::endl;
            } else {
                std::cout << "未命中圆环" << std::endl;
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

static void wait_for_frame(VkContext *context, uint32_t frame_index) {
    VkResult result = vkWaitForFences(context->device, 1, &fences[frame_index], VK_TRUE,UINT64_MAX);
    assert(result == VK_SUCCESS);
    result = vkResetFences(context->device, 1, &fences[frame_index]);
    assert(result == VK_SUCCESS);
}

struct Renderable {
    MeshBuffersHandle mesh_buffers_handle;
    glm::mat4 model_matrix;
};

int main() {
    JPH::RegisterDefaultAllocator();
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();
    glfwSetErrorCallback(glfw_error_callback);
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    int width = 800;
    int height = 600;
    GLFWwindow *window = glfwCreateWindow(width, height, "VkDemo", nullptr, nullptr);
    glfwSetKeyCallback(window, glfw_key_callback);
    glfwSetScrollCallback(window, glfw_scroll_callback);
    glfwSetMouseButtonCallback(window, glfw_mouse_button_callback);

    start(&task_system);
    init_vk(&vk_context, window, width, height);

    create_descriptor_pools(&vk_context);
    {
        descriptor_sets.resize(MAX_FRAMES_IN_FLIGHT);
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            allocate_descriptor_set(&vk_context, descriptor_pools[i], &descriptor_sets[i]);
        }
    }
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

    // registry.on_construct<Mesh>().connect<&MeshBuffers::create_mesh_buffers>();
    // registry.on_destroy<Mesh>().connect<&MeshBuffers::destroy_mesh_buffers>();
    {
        auto entity = registry.create();

        Mesh &mesh = registry.emplace<Mesh>(entity);
        MeshData mesh_data = generate_triangle_mesh_data();
        mesh.mesh_buffers_handle = request_mesh_buffers(&mesh_buffers_registry, &task_system, &vk_context, std::move(mesh_data));

        Transform &transform = registry.emplace<Transform>(entity);
        transform.position = glm::vec3(0.0f, 0.0f, 0.0f);
        transform.orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        transform.scale = glm::vec3(1.0f, 1.0f, 1.0f);
    }
    {
        auto entity = registry.create();

        Mesh &mesh = registry.emplace<Mesh>(entity);
        MeshData mesh_data = generate_plane_mesh_data(1.0f, 10);
        mesh.mesh_buffers_handle = request_mesh_buffers(&mesh_buffers_registry, &task_system, &vk_context, std::move(mesh_data));

        Transform &transform = registry.emplace<Transform>(entity);
        transform.position = glm::vec3(0.0f, 0.0f, 0.0f);
        transform.orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        transform.scale = glm::vec3(1.0f, 1.0f, 1.0f);
    }

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        wait_for_frame(&vk_context, frame_index);

        on_gpu_complete(&frame_contexts[frame_index], &mesh_buffers_registry, &task_system, &vk_context);

        uint32_t image_index;
        VkSemaphore image_acquired_semaphore = semaphore_pool.acquire_semaphore(&vk_context);
        acquire_next_image(&vk_context, image_acquired_semaphore, &image_index);
        if (image_acquired_semaphores[image_index] != VK_NULL_HANDLE) {
            semaphore_pool.release_semaphore(image_acquired_semaphores[image_index]);
        }
        image_acquired_semaphores[image_index] = image_acquired_semaphore;

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

        std::unordered_map<PipelineKey, std::vector<Renderable>> pipeline_renderables;
        for (auto view = registry.view<Mesh, Transform>(); auto entity: view) {
            Mesh &mesh = view.get<Mesh>(entity);
            Transform &transform = view.get<Transform>(entity);

            std::lock_guard lock(mesh_buffers_registry.mutex);
            MeshBuffersEntry &entry = mesh_buffers_registry.entries[mesh.mesh_buffers_handle];
            if (!entry.uploaded) { continue; }
            frame_contexts[frame_index].mesh_buffers_handles.insert(mesh.mesh_buffers_handle);
            ++entry.ref_count;
            VkPrimitiveTopology pipeline_primitive_topology = entry.mesh_buffers.primitive_topology;
            VkPolygonMode pipeline_polygon_mode = polygon_mode;
            if (pipeline_primitive_topology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST ||
                pipeline_primitive_topology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP) {
                pipeline_polygon_mode = VK_POLYGON_MODE_LINE; // polygon mode must be line for line list or line strip topology
            }
            PipelineKey pipeline_key(pipeline_primitive_topology, pipeline_polygon_mode);
            pipeline_renderables[pipeline_key].push_back({
                .mesh_buffers_handle = mesh.mesh_buffers_handle,
                .model_matrix = glm::mat4(1.0f),
            });
        }

        VkCommandBuffer command_buffer = command_buffers[frame_index];
        begin_command_buffer(&vk_context, command_buffer);

        {
            VkClearValue clear_values[2] = {};
            clear_values[0].color = {.float32 = {0.2f, 0.6f, 0.4f, 1.0f}};
            clear_values[1].depthStencil = {.depth = 1.0f, .stencil = 0};

            begin_render_pass(&vk_context, command_buffer, vk_context.render_pass,
                              vk_context.framebuffers[image_index], width, height, clear_values, std::size(clear_values));

            for (const auto &[pipeline_key, renderables]: pipeline_renderables) {
                VkPipeline pipeline = get_pipeline(&vk_context, pipeline_key);
                vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
                vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_context.pipeline_layout, 0, 1, &descriptor_sets[frame_index], 0, nullptr);
                set_viewport(command_buffer, 0, 0, width, height);
                set_scissor(command_buffer, 0, 0, width, height);
                apply_pipeline_dynamic_states(&vk_context, command_buffer, pipeline_key, cull_mode);
                VkDeviceSize offsets[] = {0};
                for (const auto &renderable: renderables) {
                    MeshBuffers &mesh_buffers = mesh_buffers_registry.entries[renderable.mesh_buffers_handle].mesh_buffers;
                    InstanceConstants instance = {};
                    instance.model = renderable.model_matrix;
                    vkCmdPushConstants(command_buffer, vk_context.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(InstanceConstants), &instance);
                    vkCmdBindVertexBuffers(command_buffer, 0, 1, &mesh_buffers.vertex_buffer, offsets);
                    if (mesh_buffers.index_count > 0) {
                        vkCmdBindIndexBuffer(command_buffer, mesh_buffers.index_buffer, 0, mesh_buffers.index_type);
                        vkCmdDrawIndexed(command_buffer, mesh_buffers.index_count, 1, 0, 0, 0);
                    } else {
                        vkCmdDraw(command_buffer, mesh_buffers.vertex_count, 1, 0, 0);
                    }
                }
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
        on_gpu_complete(&frame_contexts[i], &mesh_buffers_registry, &task_system, &vk_context);
    }
    frame_contexts.clear();
    for (auto view = registry.view<Mesh>(); auto entity: view) {
        Mesh &mesh = view.get<Mesh>(entity);
        release_mesh_buffers(&mesh_buffers_registry, &task_system, &vk_context, mesh.mesh_buffers_handle);
    }
    registry.clear();
    stop(&task_system);
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
    JPH::UnregisterTypes();
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;
    return 0;
}
