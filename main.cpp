#include "vk.h"
#include <cassert>
#include <cstring>
#include <glm/glm.hpp>

struct VkDemo {
};

static void glfw_error_callback(int error, const char *description) {
    assert(false);
}

static void glfw_key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    glfwSetWindowShouldClose(window, GLFW_TRUE);
}

struct CameraData {
    glm::mat4 view;
    glm::mat4 projection;
};

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

    CameraData camera_data = {};

    std::vector<VkBuffer> camera_buffers = {};
    std::vector<VkDeviceMemory> camera_buffer_memories = {};
    camera_buffers.resize(MAX_FRAMES_IN_FLIGHT);
    camera_buffer_memories.resize(MAX_FRAMES_IN_FLIGHT);
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        create_buffer(&vk_context, sizeof(CameraData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &camera_buffers[i]);

        VkMemoryPropertyFlags memory_property_flags =
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

        VkMemoryRequirements memory_requirements;
        vkGetBufferMemoryRequirements(vk_context.device, camera_buffers[i], &memory_requirements);

        VkPhysicalDeviceMemoryProperties memory_properties;
        vkGetPhysicalDeviceMemoryProperties(vk_context.physical_device, &memory_properties);

        uint32_t memory_type_index = UINT32_MAX;
        for (uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i) {
            if ((memory_requirements.memoryTypeBits & (1 << i)) &&
                memory_properties.memoryTypes[i].propertyFlags & memory_property_flags) {
                memory_type_index = i;
                break;
            }
        }
        assert(memory_type_index != UINT32_MAX);

        VkMemoryAllocateInfo memory_allocate_info = {};
        memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memory_allocate_info.allocationSize = memory_requirements.size;
        memory_allocate_info.memoryTypeIndex = memory_type_index;

        VkResult result = vkAllocateMemory(vk_context.device, &memory_allocate_info, nullptr,
                                           &camera_buffer_memories[i]);
        assert(result == VK_SUCCESS);

        {
            VkResult result = vkBindBufferMemory(vk_context.device, camera_buffers[i], camera_buffer_memories[i],
                                                 0);
            assert(result == VK_SUCCESS);
        }
    }


    Vertex vertices[] = {
        {glm::vec3(-0.5f, -0.5f, 0.0f), glm::vec3(1.0, 0.0, 0.0)},
        {glm::vec3( 0.5f, -0.5f, 0.0f), glm::vec3(0.0, 1.0, 0.0)},
        {glm::vec3( 0.0f,  0.5f, 0.0f), glm::vec3(0.0, 0.0, 1.0)},
    };
    uint32_t indices[] = {0, 1, 2};

    VkBuffer vertex_buffer;
    VkBuffer index_buffer;
    create_buffer(&vk_context, sizeof(Vertex) * 3, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &vertex_buffer);
    create_buffer(&vk_context, sizeof(uint32_t) * 3, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, &index_buffer);
    
    VkMemoryRequirements vertex_buffer_memory_requirements;
    VkMemoryRequirements index_buffer_memory_requirements;
    vkGetBufferMemoryRequirements(vk_context.device, vertex_buffer, &vertex_buffer_memory_requirements);
    vkGetBufferMemoryRequirements(vk_context.device, index_buffer, &index_buffer_memory_requirements);
    uint32_t vertex_buffer_memory_type_index = UINT32_MAX;
    uint32_t index_buffer_memory_type_index = UINT32_MAX;
    get_memory_type_index(&vk_context, vertex_buffer_memory_requirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &vertex_buffer_memory_type_index);
    get_memory_type_index(&vk_context, index_buffer_memory_requirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &index_buffer_memory_type_index);

    VkDeviceMemory vertex_buffer_memory;
    VkDeviceMemory index_buffer_memory;
    allocate_memory(&vk_context, vertex_buffer_memory_requirements.size, vertex_buffer_memory_type_index, &vertex_buffer_memory);
    allocate_memory(&vk_context, index_buffer_memory_requirements.size, index_buffer_memory_type_index, &index_buffer_memory);
    void *vertex_buffer_data = nullptr;
    void *index_buffer_data = nullptr;
    vkMapMemory(vk_context.device, vertex_buffer_memory, 0, sizeof(Vertex) * 3, 0, &vertex_buffer_data);
    vkMapMemory(vk_context.device, index_buffer_memory, 0, sizeof(uint32_t) * 3, 0, &index_buffer_data);
    memcpy(vertex_buffer_data, &vertices, sizeof(Vertex) * 3);
    memcpy(index_buffer_data, &indices, sizeof(uint32_t) * 3);
    vkUnmapMemory(vk_context.device, vertex_buffer_memory);
    vkUnmapMemory(vk_context.device, index_buffer_memory);
    vkBindBufferMemory(vk_context.device, vertex_buffer, vertex_buffer_memory, 0);
    vkBindBufferMemory(vk_context.device, index_buffer, index_buffer_memory, 0);
    
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

        // vulkan clip space has inverted y and half z
        glm::mat4 clip = glm::mat4(
            1.0f,  0.0f, 0.0f, 0.0f, // 1st column
            0.0f, -1.0f, 0.0f, 0.0f,
            0.0f,  0.0f, 0.5f, 0.0f,
            0.0f,  0.0f, 0.5f, 1.0f
        );

        camera_data.view = glm::mat4(1.0f);
        camera_data.projection = clip * glm::mat4(1.0f);

        void *ptr = nullptr;
        vkMapMemory(vk_context.device, camera_buffer_memories[vk_context.frame_index], 0, sizeof(CameraData), 0, &ptr);
        memcpy(ptr, &camera_data, sizeof(CameraData));
        vkUnmapMemory(vk_context.device, camera_buffer_memories[vk_context.frame_index]);

        {
            VkClearValue clear_value = {};
            clear_value.color = {.float32 = {0.2f, 0.6f, 0.4f, 1.0f}};

            begin_render_pass(&vk_context, command_buffer, vk_context.render_pass,
                              vk_context.framebuffers[vk_context.frame_index], width, height, &clear_value);

            // draw a triangle
            vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_context.pipeline);
            vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_context.pipeline_layout, 0, 1,
                                    &vk_context.descriptor_sets[vk_context.frame_index], 0, nullptr);
            VkViewport viewport = {
                .x = 0.0f,
                .y = 0.0f,
                .width = (float) width,
                .height = (float) height,
                .minDepth = 0.0f,
                .maxDepth = 1.0f,
            };
            VkRect2D scissor = {.offset = {0, 0}, .extent = {(uint32_t) width, (uint32_t) height}};
            vkCmdSetViewport(command_buffer, 0, 1, &viewport);
            vkCmdSetScissor(command_buffer, 0, 1, &scissor);
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(command_buffer, 0, 1, &vertex_buffer, offsets);
            vkCmdBindIndexBuffer(command_buffer, index_buffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(command_buffer, 3, 1, 0, 0, 0);

            end_render_pass(&vk_context, command_buffer);
        }

        end_command_buffer(&vk_context, command_buffer);

        submit(&vk_context);

        present(&vk_context, image_index);

        vk_context.frame_index = (vk_context.frame_index + 1) % MAX_FRAMES_IN_FLIGHT;
    }
    vkDeviceWaitIdle(vk_context.device);
    vkDestroyBuffer(vk_context.device, vertex_buffer, nullptr);
    vkDestroyBuffer(vk_context.device, index_buffer, nullptr);
    vkFreeMemory(vk_context.device, vertex_buffer_memory, nullptr);
    vkFreeMemory(vk_context.device, index_buffer_memory, nullptr);
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        vkDestroyBuffer(vk_context.device, camera_buffers[i], nullptr);
        vkFreeMemory(vk_context.device, camera_buffer_memories[i], nullptr);
    }
    cleanup_vk(&vk_context);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
