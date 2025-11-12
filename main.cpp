#include "vk.h"
#include <cassert>

struct VkDemo {
};

static void glfw_error_callback(int error, const char *description) {
    assert(false);
}

static void glfw_key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    glfwSetWindowShouldClose(window, GLFW_TRUE);
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
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        wait_for_previous_frame(&vk_context);

        uint32_t image_index;
        acquire_next_image(&vk_context, &image_index);

        VkCommandBuffer command_buffer = vk_context.command_buffers[vk_context.frame_index];
        begin_command_buffer(&vk_context, command_buffer);

        {
            VkClearValue clear_value = {};
            clear_value.color = {.float32 = {0.2f, 0.6f, 0.4f, 1.0f}};

            begin_render_pass(&vk_context, command_buffer, vk_context.render_pass,
                              vk_context.framebuffers[vk_context.frame_index], width, height, &clear_value);

            // // draw
            // vkCmdBindPipeline(vk_context.command_buffers[vk_context.frame_index], VK_PIPELINE_BIND_POINT_GRAPHICS,
            //                   vk_context.graphics_pipeline);
            // vkCmdDraw(vk_context.command_buffers[vk_context.frame_index], 3, 1, 0, 0);

            end_render_pass(&vk_context, command_buffer);
        }

        end_command_buffer(&vk_context, command_buffer);

        submit(&vk_context);

        present(&vk_context, image_index);

        vk_context.frame_index = (vk_context.frame_index + 1) % MAX_FRAMES_IN_FLIGHT;
    }
    vkDeviceWaitIdle(vk_context.device);
    cleanup_vk(&vk_context);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
