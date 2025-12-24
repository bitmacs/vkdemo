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
    bool depth_test_enabled;
};

enum RenderLayerType {
    RENDER_LAYER_TYPE_SCENE,
    RENDER_LAYER_TYPE_GIZMO,
    RENDER_LAYER_TYPE_UI,
};

struct RenderLayer {
    RenderLayerType render_layer_type;
};

struct Rect {
    glm::vec2 min; // 物体本地坐标系中相对于中心点的偏移量
    glm::vec2 max; // 物体本地坐标系中相对于中心点的偏移量
};
