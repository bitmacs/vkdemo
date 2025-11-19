#pragma once

#include <glm/vec3.hpp>
#include <vector>

struct Vertex {
    glm::vec3 position;
    glm::vec3 color;
};

// Mesh数据容器，包含顶点和索引
struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

Mesh generate_triangle_mesh();
