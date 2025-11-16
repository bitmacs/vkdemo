#pragma once

#include <glm/glm.hpp>
#include <glm/detail/type_quat.hpp>

struct Camera {
    glm::vec3 position;
    glm::quat rotation;
};

glm::mat4 compute_view_matrix(const Camera &camera);
