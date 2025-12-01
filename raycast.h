#pragma once

#include <glm/glm.hpp>
#include <optional>

// 射线结构
struct Ray {
    glm::vec3 origin;    // 射线起点
    glm::vec3 direction; // 射线方向（已归一化）
};

// 圆环（Ring）结构 - 一个平面上的完整圆环
struct Ring {
    glm::vec3 center; // 圆环中心
    glm::vec3 normal; // 圆环法向量（已归一化，定义圆环所在的平面）
    float radius;     // 圆环半径
};


std::optional<float> ray_ring_intersection_distance(const Ray& ray, const Ring& ring);
