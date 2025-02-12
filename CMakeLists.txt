cmake_minimum_required(VERSION 3.13)
project(vulkan-renderer)
set(CMAKE_CXX_STANDARD 17)
find_package(Vulkan REQUIRED)

set(SOURCE_DIR "${CMAKE_SOURCE_DIR}/src")
set(DEPS_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/glfw/include")
include_directories(${Vulkan_INCLUDE_DIRS} ${DEPS_INCLUDE_DIRS})

set(GLFW_BUILD_DOCS OFF CACHE BOOL "GLFW lib only")
set(GLFW_INSTALL OFF CACHE BOOL "GLFW lib only")

add_subdirectory(glfw)

file(GLOB_RECURSE SRC_CXX_FILES "${SOURCE_DIR}/*.cpp")
file(GLOB_RECURSE SRC_C_FILES "${SOURCE_DIR}/*.c")

add_executable(ren ${SRC_CXX_FILES} ${SRC_C_FILES})
target_link_libraries(ren ${Vulkan_LIBRARIES} glfw)