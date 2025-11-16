#include "camera.h"

glm::mat4 compute_view_matrix(const Camera &camera) {
    // 视图矩阵的数学原理：
    // 如果相机变换是 T * R（先旋转后平移），则视图矩阵 = inverse(T * R) = R^T * T^-1
    // 其中 R^T 是旋转矩阵的转置（等于逆），T^-1 是负平移

    // 将四元数转换为旋转矩阵（3x3，比 4x4 更高效）
    const glm::mat3 rotation_matrix = glm::mat3_cast(camera.rotation);

    // 计算旋转后的负平移向量（在相机坐标系中）
    const glm::vec3 rotated_neg_position = rotation_matrix * (-camera.position);

    // 构建视图矩阵：旋转部分的转置 + 平移部分
    // 使用 glm::transpose 更简洁，编译器通常会优化
    glm::mat4 view_matrix = glm::mat4(glm::transpose(rotation_matrix));
    view_matrix[3] = glm::vec4(rotated_neg_position, 1.0f);

    return view_matrix;
}
