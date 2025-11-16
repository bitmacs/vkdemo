#version 440 core

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 color;

layout (set = 0, binding = 0, std140) uniform CameraUBO {
    mat4 view;
    mat4 projection;
} camera_ubo;

layout (location = 0) out VS_OUT {
    vec3 color;
} vs_out;

void main() {
    gl_Position = camera_ubo.projection * camera_ubo.view * vec4(position, 1.0);
    vs_out.color = color;
}
