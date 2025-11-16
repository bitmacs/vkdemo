#pragma once

#include <glm/glm.hpp>
#include <glm/detail/type_quat.hpp>

struct Camera {
    glm::vec3 position;
    glm::quat rotation;
    float fov_y;
    float aspect_ratio;
    float z_near;
    float z_far;
};

glm::mat4 compute_view_matrix(const Camera &camera);
glm::mat4 compute_projection_matrix(const Camera &camera);
