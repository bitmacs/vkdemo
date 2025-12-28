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

// 圆盘结构 - 一个平面上的实心圆盘
struct Disk {
    glm::vec3 center; // 圆盘中心
    glm::vec3 normal; // 圆盘法向量（已归一化，定义圆盘所在的平面）
    float radius;     // 圆盘半径
};

// 圆柱曲面（Cylinder）结构 - 不包含顶部与底部的圆柱侧面
struct Cylinder {
    glm::vec3 base_center; // 底部中心点
    glm::vec3 axis;        // 轴向（已归一化，从底部指向顶部）
    float radius;          // 圆柱半径
    float height;          // 圆柱高度
};

struct RayCylinderHit {
    float t;             // 射线参数 t，使得 hit_point = ray.origin + t * ray.direction
    glm::vec3 hit_point; // 碰撞点
    glm::vec3 normal;    // 碰撞点处的法向量（从圆柱轴指向碰撞点）
};

std::optional<float> ray_ring_intersection_distance(const Ray& ray, const Ring& ring);

std::optional<RayCylinderHit> ray_cylinder_side_intersection(const Ray& ray, const Cylinder& cylinder);

// 检测射线与圆盘的交点，返回交点的 t 值（如果存在）
std::optional<float> ray_disk_intersection(const Ray& ray, const Disk& disk);
