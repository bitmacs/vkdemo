#include "camera.h"
#include <glm/gtc/quaternion.hpp>

glm::mat4 compute_view_matrix(const Camera &camera) {
    return glm::transpose(glm::mat4_cast(camera.orientation)) * glm::translate(glm::mat4(1.0f), -camera.position);
}

glm::mat4 compute_projection_matrix(const Camera &camera) {
    return glm::perspective(camera.fov_y, camera.aspect_ratio, camera.z_near, camera.z_far);
}
