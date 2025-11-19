#include "mesh.h"
#include <cmath>
#include <glm/glm.hpp>
#include <glm/ext/scalar_constants.hpp>

Mesh generate_triangle_mesh() {
    Mesh mesh;
    mesh.vertices = {
        {glm::vec3(-0.5f, -0.5f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f)},
        {glm::vec3( 0.5f, -0.5f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)},
        {glm::vec3( 0.0f,  0.5f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)},
    };
    mesh.indices = {0, 1, 2};
    return mesh;
}
