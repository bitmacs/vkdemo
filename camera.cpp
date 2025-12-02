#include "camera.h"
#include <glm/gtc/quaternion.hpp>

glm::mat4 compute_view_matrix(const Camera &camera) {
    return glm::transpose(glm::mat4_cast(camera.orientation)) * glm::translate(glm::mat4(1.0f), -camera.position);
}

glm::mat4 compute_projection_matrix(const Camera &camera) {
    return glm::perspective(camera.fov_y, camera.aspect_ratio, camera.z_near, camera.z_far);
}

std::pair<glm::vec3, glm::vec3> compute_ray_from_screen(const Camera &camera, float x, float y, int width, int height) {
    // 1 将屏幕坐标转换为 NDC
    // Vulkan NDC: x, y 范围是 [-1, 1]，原点在中心，y 轴向上
    // 屏幕坐标: 原点在左上角，y 轴向下
    float ndc_x = (2.0f * x / (float) width) - 1.0f;
    float ndc_y = 1.0f - (2.0f * y / (float) height); // 翻转 y 轴

    // 2 创建近平面和远平面的 NDC 点
    glm::vec4 near_point_ndc(ndc_x, ndc_y, 0.0f, 1.0f); // z = 0 (近平面)
    glm::vec4 far_point_ndc(ndc_x, ndc_y, 1.0f, 1.0f); // z = 1 (远平面)

    // 3 计算视图和投影矩阵
    glm::mat4 view = compute_view_matrix(camera);
    glm::mat4 projection = compute_projection_matrix(camera);

    // 4 计算视图投影矩阵的逆矩阵
    glm::mat4 view_proj = projection * view;
    glm::mat4 inv_view_proj = glm::inverse(view_proj);

    // 5 将 NDC 坐标转换为世界空间坐标
    glm::vec4 near_point_world = inv_view_proj * near_point_ndc;
    glm::vec4 far_point_world = inv_view_proj * far_point_ndc;

    // 齐次坐标除法
    near_point_world /= near_point_world.w;
    far_point_world /= far_point_world.w;

    // 6 计算射线起点和方向
    glm::vec3 ray_origin = glm::vec3(near_point_world);
    glm::vec3 ray_direction = glm::normalize(glm::vec3(far_point_world) - ray_origin);

    return std::make_pair(ray_origin, ray_direction);
}

glm::vec3 compute_ray_far_plane_intersection(const Camera &camera, const glm::vec3 &ray_origin,
                                             const glm::vec3 &ray_dir) {
    glm::mat4 view = compute_view_matrix(camera);

    // 将射线起点和方向转换到相机空间
    glm::vec4 ray_origin_camera = view * glm::vec4(ray_origin, 1.0f);
    glm::vec4 ray_dir_camera = view * glm::vec4(ray_dir, 0.0f);

    // 在相机空间中，远平面位于 z = -z_far
    // 射线参数方程: P(t) = origin + t * direction
    // 在相机空间中，z = -z_far
    // origin_camera.z + t * dir_camera.z = -z_far
    // t = (-z_far - origin_camera.z) / dir_camera.z

    float t = (-camera.z_far - ray_origin_camera.z) / ray_dir_camera.z;

    // 计算相机空间中的交点
    glm::vec3 intersection_camera = glm::vec3(ray_origin_camera) + t * glm::vec3(ray_dir_camera);

    // 转换回世界空间
    glm::vec4 intersection_world = glm::inverse(view) * glm::vec4(intersection_camera, 1.0f);

    return glm::vec3(intersection_world);
}
