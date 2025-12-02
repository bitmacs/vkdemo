#pragma once

#include "meshes.h"

struct Mesh {
    MeshBuffersHandle mesh_buffers_handle;
};

struct Transform {
    glm::vec3 position;
    glm::quat orientation;
    glm::vec3 scale;
};

struct Material {
    glm::vec3 color;
};
