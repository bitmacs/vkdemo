#version 430 core

// layout (location = 0) in vec3 position;

layout (set = 0, binding = 0, std140) uniform CameraUBO {
    mat4 view;
    mat4 projection;
} camera_ubo;

void main() {
    const vec4 vertices[3] = vec4[3](
        vec4(-0.5, -0.5, 0.0, 1.0),
        vec4( 0.5, -0.5, 0.0, 1.0),
        vec4( 0.0,  0.5, 0.0, 1.0)
    );
    gl_Position = camera_ubo.projection * camera_ubo.view * vertices[gl_VertexIndex];
}
