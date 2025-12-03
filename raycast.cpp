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

std::optional<RayCylinderHit> ray_cylinder_side_intersection(const Ray &ray, const Cylinder &cylinder) {
    // 构建局部坐标系：以圆柱轴为 z 轴
    glm::vec3 z_axis = cylinder.axis;

    // 使用最小分量法选择参考向量，确保数值稳定性
    // 找到 z_axis 绝对值最小的分量，使用对应的单位向量作为参考
    glm::vec3 abs_z_axis = glm::abs(z_axis);
    glm::vec3 ref_vec(0.0f);
    if (abs_z_axis.x <= abs_z_axis.y && abs_z_axis.x <= abs_z_axis.z) {
        ref_vec.x = 1.0f; // x 分量最小，使用 (1,0,0) 作为参考
    } else if (abs_z_axis.y <= abs_z_axis.z) {
        ref_vec.y = 1.0f; // y 分量最小，使用 (0,1,0) 作为参考
    } else {
        ref_vec.z = 1.0f; // z 分量最小，使用 (0,0,1) 作为参考
    }

    // 通过叉乘得到与 z_axis 垂直的 x_axis
    glm::vec3 x_axis = glm::normalize(glm::cross(z_axis, ref_vec));
    // 通过叉乘得到 y_axis，完成右手坐标系
    glm::vec3 y_axis = glm::cross(z_axis, x_axis);

    // 将射线转换到局部坐标系
    glm::vec3 local_origin = ray.origin - cylinder.base_center;
    float local_origin_x = glm::dot(local_origin, x_axis); // local_origin 在 x_axis 方向上的投影长度
    float local_origin_y = glm::dot(local_origin, y_axis); // local_origin 在 y_axis 方向上的投影长度
    float local_origin_z = glm::dot(local_origin, z_axis); // local_origin 在 z_axis 方向上的投影长度

    // 方向向量不需要平移，只需投影到新基向量上
    float local_dir_x = glm::dot(ray.direction, x_axis);
    float local_dir_y = glm::dot(ray.direction, y_axis);
    float local_dir_z = glm::dot(ray.direction, z_axis);

    // 在局部坐标系中，圆柱方程为：x² + y² = r²，z ∈ [0, height]
    // 射线方程：x = local_origin_x + t * local_dir_x
    //          y = local_origin_y + t * local_dir_y
    //          z = local_origin_z + t * local_dir_z
    // 代入圆柱方程：(local_origin_x + t * local_dir_x)² + (local_origin_y + t * local_dir_y)² = r²
    // 展开：a*t² + b*t + c = 0
    float a = local_dir_x * local_dir_x + local_dir_y * local_dir_y;
    float b = 2.0f * (local_origin_x * local_dir_x + local_origin_y * local_dir_y);
    float c = local_origin_x * local_origin_x + local_origin_y * local_origin_y - cylinder.radius * cylinder.radius;

    // 如果 a 接近 0，射线与圆柱轴平行
    if (std::abs(a) < glm::epsilon<float>()) {
        return std::nullopt;
    }

    // 求解二次方程
    float discriminant = b * b - 4.0f * a * c;
    if (discriminant < 0.0f) {
        return std::nullopt; // 无实数解
    }

    float sqrt_discriminant = std::sqrt(discriminant);
    float t1 = (-b - sqrt_discriminant) / (2.0f * a);
    float t2 = (-b + sqrt_discriminant) / (2.0f * a);

    // 检查两个交点是否在圆柱高度范围内，并选择最近的交点
    float t = -1.0f;
    bool found_valid = false;

    // 检查 t1
    if (t1 >= 0.0f) {
        float z1 = local_origin_z + t1 * local_dir_z;
        if (z1 >= 0.0f && z1 <= cylinder.height) {
            t = t1;
            found_valid = true;
        }
    }

    // 检查 t2（如果 t1 无效或 t2 更近）
    if (t2 >= 0.0f) {
        float z2 = local_origin_z + t2 * local_dir_z;
        if (z2 >= 0.0f && z2 <= cylinder.height) {
            if (!found_valid || t2 < t) {
                t = t2;
                found_valid = true;
            }
        }
    }

    if (!found_valid) {
        return std::nullopt;
    }

    // 计算碰撞点和法向量
    glm::vec3 hit_point = ray.origin + t * ray.direction;

    // 计算碰撞点在局部坐标系中的位置
    glm::vec3 local_hit = hit_point - cylinder.base_center;
    float local_hit_x = glm::dot(local_hit, x_axis);
    float local_hit_y = glm::dot(local_hit, y_axis);

    // 法向量是从圆柱轴指向碰撞点的方向（在 xy 平面内）
    glm::vec3 local_normal = glm::normalize(glm::vec3(local_hit_x, local_hit_y, 0.0f));
    glm::vec3 normal = local_normal.x * x_axis + local_normal.y * y_axis;

    RayCylinderHit hit = {};
    hit.t = t;
    hit.hit_point = hit_point;
    hit.normal = normal;

    return hit;
}
