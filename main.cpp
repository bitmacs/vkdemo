#include "camera.h"
#include "ecs.h"
#include "events.h"
#include "frame_context.h"
#include "inputs.h"
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
#include <iostream>

#define MAX_FRAMES_IN_FLIGHT 2

struct VkDemo {
};

Inputs inputs = {};
Events events = {};
TaskSystem task_system = {};
VkContext vk_context = {};
MeshBuffersRegistry mesh_buffers_registry = {};
Camera camera = {};
VkPolygonMode polygon_mode = VK_POLYGON_MODE_FILL;
VkCullModeFlags cull_mode = VK_CULL_MODE_BACK_BIT;
entt::registry registry;
entt::entity gizmo_y_ring_entity = entt::null;

static void glfw_error_callback(int error, const char *description) {
    assert(false);
}

static void glfw_key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        press_key(&inputs, key);
    } else if (action == GLFW_RELEASE) {
        release_key(&inputs, key);
    }
    if (action == GLFW_RELEASE) {
        if (key == GLFW_KEY_ESCAPE) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        } else if (key == GLFW_KEY_P) {
            polygon_mode = polygon_mode == VK_POLYGON_MODE_FILL ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
        } else if (key == GLFW_KEY_C) {
            cull_mode = cull_mode == VK_CULL_MODE_NONE ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE;
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

static void glfw_cursor_pos_callback(GLFWwindow *window, double xpos, double ypos) {
    float x = (float) xpos;
    float y = (float) ypos;
    move_mouse(&inputs, x, y);
    dispatch_event(&events, EVENT_CODE_MOUSE_MOVE, EventData{.f32 = {x, y}});
}

static void glfw_mouse_button_callback(GLFWwindow *window, int button, int action, int mods) {
    if (action == GLFW_PRESS) {
        press_mouse_button(&inputs, button);
    } else if (action == GLFW_RELEASE) {
        release_mouse_button(&inputs, button);
    }
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

        auto &material = registry.emplace<Material>(entity);
        material.color = glm::vec3(1.0f, 1.0f, 1.0f);

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
    }
}

struct CameraData {
    glm::mat4 view;
    glm::mat4 projection;
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
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2}, // camera buffer array (2 cameras)
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
    };

    VkDescriptorPoolCreateInfo descriptor_pool_create_info = {};
    descriptor_pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptor_pool_create_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    descriptor_pool_create_info.maxSets = 1; // unified descriptor set for all cameras
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
    glm::vec3 color;
};

static bool is_gizmo_y_ring_hovered(const glm::vec3 &origin, const glm::vec3 &dir) {
    float margin = 0.1f;
    std::optional<float> distance = ray_ring_intersection_distance(Ray{origin, dir}, Ring{glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 1.0f});
    if (distance && glm::abs(*distance - 1.0f) < margin) {
        return true;
    }
    std::optional<RayCylinderHit> hit = ray_cylinder_side_intersection(Ray{origin, dir}, Cylinder{glm::vec3(0.0f, -margin, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 1.0f, 2 * margin});
    return hit ? true : false;
}

static glm::mat4 compute_transform_matrix(const Transform &transform) {
    glm::mat4 translation = glm::translate(glm::mat4(1.0f), transform.position);
    glm::mat4 rotation = glm::mat4_cast(transform.orientation);
    glm::mat4 scale = glm::scale(glm::mat4(1.0f), transform.scale);
    return translation * rotation * scale;
}

static glm::mat4 compute_transform_matrix(const Transform2D &transform) {
    return glm::translate(glm::mat4(1.0f), transform.position)
           * glm::scale(glm::mat4(1.0f), glm::vec3(transform.scale, 1.0f));
}

static PipelineKey get_pipeline_key(VkPrimitiveTopology primitive_topology, VkPolygonMode polygon_mode) {
    if (primitive_topology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST || primitive_topology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP) {
        polygon_mode = VK_POLYGON_MODE_LINE; // polygon mode must be line for line list or line strip topology
    }
    return PipelineKey(primitive_topology, polygon_mode);
}

static void update_camera(float delta_time) {
    float move_speed = 2.5f; // 移动速度（单位/秒）
    float rotate_speed = glm::radians(60.0f); // 旋转速度（弧度/秒）

    {
        glm::mat3 rot_mat = glm::mat3_cast(camera.orientation);
        glm::vec3 right = rot_mat[0];
        glm::vec3 forward = -rot_mat[2];
        glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

        glm::vec3 move_dir(0.0f);

        if (is_key_pressed(&inputs, GLFW_KEY_W)) { move_dir += forward; }
        if (is_key_pressed(&inputs, GLFW_KEY_S)) { move_dir -= forward; }
        if (is_key_pressed(&inputs, GLFW_KEY_A)) { move_dir -= right; }
        if (is_key_pressed(&inputs, GLFW_KEY_D)) { move_dir += right; }
        if (is_key_pressed(&inputs, GLFW_KEY_Q)) { move_dir += up; }
        if (is_key_pressed(&inputs, GLFW_KEY_E)) { move_dir -= up; }

        if (glm::length(move_dir) > 0.0f) {
            camera.position += glm::normalize(move_dir) * move_speed * delta_time;
        }
    }
    {
        glm::vec2 rot_delta(0.0f);

        if (is_key_pressed(&inputs, GLFW_KEY_UP)) { rot_delta.x += rotate_speed * delta_time; }
        if (is_key_pressed(&inputs, GLFW_KEY_DOWN)) { rot_delta.x -= rotate_speed * delta_time; }
        if (is_key_pressed(&inputs, GLFW_KEY_LEFT)) { rot_delta.y += rotate_speed * delta_time; }
        if (is_key_pressed(&inputs, GLFW_KEY_RIGHT)) { rot_delta.y -= rotate_speed * delta_time; }

        if (rot_delta.x != 0.0f || rot_delta.y != 0.0f) {
            glm::quat yaw_quat = glm::angleAxis(rot_delta.y, glm::vec3(0.0f, 1.0f, 0.0f));
            glm::quat pitch_quat = glm::angleAxis(rot_delta.x, glm::vec3(1.0f, 0.0f, 0.0f));
            camera.orientation = glm::normalize(yaw_quat * camera.orientation * pitch_quat);
        }
    }
}

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
    glfwSetCursorPosCallback(window, glfw_cursor_pos_callback);

    init_inputs(&inputs);
    start(&task_system);
    init_vk(&vk_context, window, width, height);

    register_event_handler(&events, EVENT_CODE_MOUSE_MOVE, [width, height](const EventData &event_data)-> bool {
        float x = event_data.f32[0];
        float y = event_data.f32[1];
        auto [origin, dir] = compute_ray_from_screen(camera, x, y, width, height);

        // 检测是否在 gizmo 范围内
        if (is_gizmo_y_ring_hovered(origin, dir)) {
            registry.get<Material>(gizmo_y_ring_entity).color = glm::vec3(0.8f, 0.0f, 0.0f); // set ring color to red
        } else {
            registry.get<Material>(gizmo_y_ring_entity).color = glm::vec3(1.0f, 1.0f, 1.0f); // set ring color to white
        }
        return true;
    });

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

    CameraData camera_data[2] = {}; // [0] = 3D scene camera, [1] = UI camera

    double last_frame_time = glfwGetTime();

    std::vector<VkBuffer> camera_buffers = {};
    std::vector<VkDeviceMemory> camera_buffer_memories = {};
    camera_buffers.resize(MAX_FRAMES_IN_FLIGHT);
    camera_buffer_memories.resize(MAX_FRAMES_IN_FLIGHT);
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        create_buffer(&vk_context, sizeof(CameraData) * 2, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &camera_buffers[i]);

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

        auto &material = registry.emplace<Material>(entity);
        material.color = glm::vec3(1.0f, 1.0f, 1.0f);
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

        auto &material = registry.emplace<Material>(entity);
        material.color = glm::vec3(0.7f, 0.65f, 0.6f); // 浅棕色地面颜色
    }
    {
        auto entity = registry.create();

        auto &[mesh_buffers_handle] = registry.emplace<Mesh>(entity);
        MeshData mesh_data = generate_ring_mesh_data(1.0f, 32);
        mesh_buffers_handle = request_mesh_buffers(&mesh_buffers_registry, &task_system, &vk_context, std::move(mesh_data));

        auto &[position, orientation, scale] = registry.emplace<Transform>(entity);
        position = glm::vec3(0.0f, 0.0f, 0.0f);
        orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        scale = glm::vec3(1.0f, 1.0f, 1.0f);

        auto &material = registry.emplace<Material>(entity);
        material.color = glm::vec3(1.0f, 1.0f, 1.0f);

        gizmo_y_ring_entity = entity;
    }
    {
        auto entity = registry.create();

        auto &[mesh_buffers_handle] = registry.emplace<Mesh>(entity);
        float quad_width = 200.0f; // in pixels
        float quad_height = height / (float) width * quad_width;
        MeshData mesh_data = generate_quad_mesh_data(quad_width, quad_height);
        mesh_buffers_handle = request_mesh_buffers(&mesh_buffers_registry, &task_system, &vk_context, std::move(mesh_data));

        auto &[position, scale] = registry.emplace<Transform2D>(entity);
        position = glm::vec3(quad_width * 0.5f + 20.0f, quad_height * 0.5f + 20.0f, 0.0f);
        scale = glm::vec2(1.0f, 1.0f);

        auto &material = registry.emplace<Material>(entity);
        material.color = glm::vec3(1.0f, 1.0f, 1.0f);
    }

    while (!glfwWindowShouldClose(window)) {
        double current_time = glfwGetTime();
        float delta_time = (float) (current_time - last_frame_time);
        last_frame_time = current_time;

        begin_inputs_frame(&inputs);
        glfwPollEvents();

        update_camera(delta_time);

        wait_for_frame(&vk_context, frame_index);

        on_gpu_complete(&frame_contexts[frame_index], &mesh_buffers_registry, &task_system, &vk_context);

        uint32_t image_index;
        VkSemaphore image_acquired_semaphore = semaphore_pool.acquire_semaphore(&vk_context);
        acquire_next_image(&vk_context, image_acquired_semaphore, &image_index);
        if (image_acquired_semaphores[image_index] != VK_NULL_HANDLE) {
            semaphore_pool.release_semaphore(image_acquired_semaphores[image_index]);
        }
        image_acquired_semaphores[image_index] = image_acquired_semaphore;

        // Update 3D scene camera (index 0)
        const glm::mat4 view = compute_view_matrix(camera);
        const glm::mat4 projection = compute_projection_matrix(camera);

        // vulkan clip space has inverted y and half z
        glm::mat4 clip = glm::mat4(
            1.0f,  0.0f, 0.0f, 0.0f, // 1st column
            0.0f, -1.0f, 0.0f, 0.0f,
            0.0f,  0.0f, 0.5f, 0.0f,
            0.0f,  0.0f, 0.5f, 1.0f
        );

        camera_data[0].view = view;
        camera_data[0].projection = clip * projection;

        // Update UI camera (index 1)
        glm::mat4 ui_view = glm::mat4(1.0f);
        glm::mat4 ui_projection = glm::ortho(0.0f, (float) width, 0.0f, (float) height, -1.0f, 1.0f);
        camera_data[1].view = ui_view;
        camera_data[1].projection = clip * ui_projection;

        // Update unified camera buffer
        void *ptr = nullptr;
        vkMapMemory(vk_context.device, camera_buffer_memories[frame_index], 0, sizeof(CameraData) * 2, 0, &ptr);
        memcpy(ptr, camera_data, sizeof(CameraData) * 2);
        vkUnmapMemory(vk_context.device, camera_buffer_memories[frame_index]);

        // Update descriptor set with camera array
        VkDescriptorBufferInfo descriptor_buffer_infos[2] = {};
        descriptor_buffer_infos[0].buffer = camera_buffers[frame_index];
        descriptor_buffer_infos[0].offset = 0;
        descriptor_buffer_infos[0].range = sizeof(CameraData);
        descriptor_buffer_infos[1].buffer = camera_buffers[frame_index];
        descriptor_buffer_infos[1].offset = sizeof(CameraData);
        descriptor_buffer_infos[1].range = sizeof(CameraData);

        VkWriteDescriptorSet write_descriptor_set = {};
        write_descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write_descriptor_set.dstSet = descriptor_sets[frame_index];
        write_descriptor_set.dstBinding = 0;
        write_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write_descriptor_set.descriptorCount = 2;
        write_descriptor_set.pBufferInfo = descriptor_buffer_infos;

        vkUpdateDescriptorSets(vk_context.device, 1, &write_descriptor_set, 0, nullptr);

        std::unordered_map<PipelineKey, std::vector<Renderable>, PipelineKeyHash> scene_pipeline_renderables;
        for (auto view = registry.view<Mesh, Transform, Material>(); auto entity: view) {
            Mesh &mesh = view.get<Mesh>(entity);
            Transform &transform = view.get<Transform>(entity);

            std::lock_guard lock(mesh_buffers_registry.mutex);
            MeshBuffersEntry &entry = mesh_buffers_registry.entries[mesh.mesh_buffers_handle];
            if (!entry.uploaded) { continue; }
            add_ref(&frame_contexts[frame_index], mesh.mesh_buffers_handle);
            ++entry.ref_count;
            PipelineKey pipeline_key = get_pipeline_key(entry.mesh_buffers.primitive_topology, polygon_mode);
            scene_pipeline_renderables[pipeline_key].push_back({
                .mesh_buffers_handle = mesh.mesh_buffers_handle,
                .model_matrix = compute_transform_matrix(transform),
                .color = registry.get<Material>(entity).color,
            });
        }
        std::vector<PipelineKey> pipeline_render_order = {
            {VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_POLYGON_MODE_FILL},
            {VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_POLYGON_MODE_LINE},
            {VK_PRIMITIVE_TOPOLOGY_LINE_STRIP, VK_POLYGON_MODE_LINE},
            {VK_PRIMITIVE_TOPOLOGY_LINE_LIST, VK_POLYGON_MODE_LINE},
        };
        std::vector<std::pair<PipelineKey, std::vector<Renderable>>> sorted_scene_pipeline_renderables;
        for (const auto &pipeline_key: pipeline_render_order) {
            if (const auto it = scene_pipeline_renderables.find(pipeline_key); it != scene_pipeline_renderables.end()) {
                sorted_scene_pipeline_renderables.emplace_back(pipeline_key, std::move(it->second));
            }
        }

        std::unordered_map<PipelineKey, std::vector<Renderable>, PipelineKeyHash> ui_pipeline_renderables;
        for (auto view = registry.view<Mesh, Transform2D, Material>(); auto entity: view) {
            Mesh &mesh = view.get<Mesh>(entity);
            Transform2D &transform = view.get<Transform2D>(entity);

            std::lock_guard lock(mesh_buffers_registry.mutex);
            MeshBuffersEntry &entry = mesh_buffers_registry.entries[mesh.mesh_buffers_handle];
            if (!entry.uploaded) { continue; }
            add_ref(&frame_contexts[frame_index], mesh.mesh_buffers_handle);
            ++entry.ref_count;
            PipelineKey pipeline_key = get_pipeline_key(entry.mesh_buffers.primitive_topology, polygon_mode);
            ui_pipeline_renderables[pipeline_key].push_back({
                .mesh_buffers_handle = mesh.mesh_buffers_handle,
                .model_matrix = compute_transform_matrix(transform),
                .color = registry.get<Material>(entity).color,
            });
        }
        std::vector<std::pair<PipelineKey, std::vector<Renderable>>> sorted_ui_pipeline_renderables;
        for (const auto &pipeline_key: pipeline_render_order) {
            if (const auto it = ui_pipeline_renderables.find(pipeline_key); it != ui_pipeline_renderables.end()) {
                sorted_ui_pipeline_renderables.emplace_back(pipeline_key, std::move(it->second));
            }
        }

        VkCommandBuffer command_buffer = command_buffers[frame_index];
        begin_command_buffer(&vk_context, command_buffer);

        {
            VkClearValue clear_values[2] = {};
            clear_values[0].color = {.float32 = {0.2f, 0.6f, 0.4f, 1.0f}};
            clear_values[1].depthStencil = {.depth = 1.0f, .stencil = 0};

            begin_render_pass(&vk_context, command_buffer, vk_context.render_pass,
                              vk_context.framebuffers[image_index], width, height, clear_values, std::size(clear_values));

            for (const auto &[pipeline_key, renderables]: sorted_scene_pipeline_renderables) {
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
                    instance.color = renderable.color;
                    instance.camera_index = 0; // 3D scene camera
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

            // Render UI with camera_index = 1
            for (const auto &[pipeline_key, renderables]: sorted_ui_pipeline_renderables) {
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
                    instance.color = renderable.color;
                    instance.camera_index = 1; // UI camera
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
    shutdown_inputs(&inputs);
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
