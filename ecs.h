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

enum RenderQueueType {
    RENDER_QUEUE_TYPE_SCENE,
    RENDER_QUEUE_TYPE_GIZMO,
    RENDER_QUEUE_TYPE_UI,
};

struct Material {
    RenderQueueType render_queue_type;
    glm::vec3 color;
    bool depth_test_enabled;
};

struct Transform2D {
    glm::vec3 position; // x，y 代表在 ui 空间的位置，坐标原点为屏幕左下角，单位为像素。z 代表 z-order，值越小，越靠前。
    glm::vec2 scale;
};

struct Rect {
    glm::vec2 min; // 物体本地坐标系中相对于中心点的偏移量
    glm::vec2 max; // 物体本地坐标系中相对于中心点的偏移量
};
