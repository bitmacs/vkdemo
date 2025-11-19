cmake_minimum_required(VERSION 4.0)
project(vkdemo)

set(CMAKE_CXX_STANDARD 20)

find_package(Vulkan REQUIRED)

get_target_property(VULKAN_LIB_PATH Vulkan::Vulkan LOCATION)
get_filename_component(VULKAN_LIB_DIR "${VULKAN_LIB_PATH}" DIRECTORY)
list(APPEND CMAKE_BUILD_RPATH "${VULKAN_LIB_DIR}")

add_subdirectory(external/glfw)
add_subdirectory(external/glm)

add_executable(vkdemo main.cpp file.cpp vk.cpp camera.cpp mesh.cpp)
target_link_libraries(vkdemo PRIVATE Vulkan::Vulkan glfw glm)
target_compile_definitions(vkdemo PRIVATE GLFW_INCLUDE_NONE)
