#include "raycast.h"
#include <algorithm>
#include <cmath>
#include <glm/ext/scalar_constants.hpp>

// 计算射线与圆环平面的交点，返回交点到圆心的距离
std::optional<float> ray_ring_intersection_distance(const Ray &ray, const Ring &ring) {
    // 计算射线与圆环平面的交点
    float denom = glm::dot(ring.normal, ray.direction);
    if (std::abs(denom) < glm::epsilon<float>()) { // 射线与平面平行，无交点
        return std::nullopt;
    }
    float t = glm::dot(ring.center - ray.origin, ring.normal) / denom;
    if (t < 0.0f) { // 交点在射线起点的反方向
        return std::nullopt;
    }
    glm::vec3 hit_point = ray.origin + t * ray.direction;
    float distance = glm::length(hit_point - ring.center);
    return distance;
}
