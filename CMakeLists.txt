cmake_minimum_required(VERSION 4.0)
project(vkdemo)

set(CMAKE_CXX_STANDARD 20)

find_package(Vulkan REQUIRED)

add_subdirectory(external/glfw)
add_subdirectory(external/glm)

add_executable(vkdemo main.cpp vk.cpp)
target_link_libraries(vkdemo PRIVATE Vulkan::Vulkan glfw glm)
target_compile_definitions(vkdemo PRIVATE GLFW_INCLUDE_NONE)
