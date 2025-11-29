#pragma once

#include <glm/glm.hpp>
#include <glm/detail/type_quat.hpp>

struct Camera {
    glm::vec3 position; // 位置
    glm::quat orientation; // 朝向
    float fov_y; // in radians
    float aspect_ratio; // width / height
    float z_near;
    float z_far;
};

glm::mat4 compute_view_matrix(const Camera &camera);
glm::mat4 compute_projection_matrix(const Camera &camera);

std::pair<glm::vec3, glm::vec3> compute_ray_from_screen(const Camera &camera, float x, float y, float width,
                                                        float height);
glm::vec3 compute_ray_far_plane_intersection(const Camera &camera, const glm::vec3 &ray_origin,
                                             const glm::vec3 &ray_dir);
