cmake_minimum_required(VERSION 3.28)

project(BilliardsSaloon
    VERSION 0.1.0
    DESCRIPTION "A 3D billiards game built from scratch in C++ and OpenGL"
    LANGUAGES C CXX
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
)
FetchContent_MakeAvailable(glm)

find_package(OpenGL REQUIRED)

add_executable(BilliardsSaloon
    src/app/main.cpp
    src/app/application.cpp
    src/platform/window.cpp
    src/platform/timer.cpp
    src/render/shader.cpp
    src/render/mesh.cpp
    src/render/camera.cpp
    external/glad/src/gl.c
)

target_include_directories(BilliardsSaloon PRIVATE
    src
    external/glad/include
)

target_link_libraries(BilliardsSaloon PRIVATE
    glfw
    OpenGL::GL
    glm::glm
)

if(MSVC)
    target_compile_options(BilliardsSaloon PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(BilliardsSaloon PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

target_compile_definitions(BilliardsSaloon PRIVATE
    GLFW_INCLUDE_NONE
    $<$<CONFIG:Debug>:BS_DEBUG=1>
)