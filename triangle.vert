#version 430 core

// layout (location = 0) in vec3 position;

void main() {
    const vec4 vertices[3] = vec4[3](
        vec4(-0.5, -0.5, 0.0, 1.0),
        vec4( 0.5, -0.5, 0.0, 1.0),
        vec4( 0.0,  0.5, 0.0, 1.0)
    );
    gl_Position = vertices[gl_VertexIndex];
}
