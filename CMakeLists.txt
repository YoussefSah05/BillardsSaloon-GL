cmake_minimum_required(VERSION 3.20)
project(BilliardsSaloon VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# External dependency: GLFW
find_package(glfw3 REQUIRED)

# FetchContent for GLM and nlohmann/json
include(FetchContent)

FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
    GIT_SHALLOW    TRUE
)

FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(glm json)

# GLAD (bundled in third_party)
add_library(glad STATIC third_party/glad/src/glad.c)
target_include_directories(glad PUBLIC third_party/glad/include)

# Collect all C++ sources (with automatic re‑scan on file changes)
file(GLOB_RECURSE BS_SOURCES CONFIGURE_DEPENDS 
src/*.cpp
src/**/*.cpp
)

add_executable(BilliardsSaloon ${BS_SOURCES})

target_include_directories(BilliardsSaloon PRIVATE
    src
    third_party/glad/include
)

target_link_libraries(BilliardsSaloon PRIVATE
    glad
    glfw
    glm::glm
    nlohmann_json::nlohmann_json
)

# macOS specific OpenGL and deprecation silence
if(APPLE)
    find_package(OpenGL REQUIRED)
    target_link_libraries(BilliardsSaloon PRIVATE OpenGL::GL)
    target_compile_definitions(BilliardsSaloon PRIVATE GL_SILENCE_DEPRECATION)
endif()

# Symlink assets into build directory (post‑build, ensures it exists at runtime)
add_custom_command(TARGET BilliardsSaloon POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink
        ${CMAKE_SOURCE_DIR}/assets
        ${CMAKE_BINARY_DIR}/assets
    COMMENT "Symlinking assets/ into build directory"
)

# Symlink compile_commands.json to project root (configure‑time, for IDE support)
# Remove any existing symlink/file first to avoid errors
file(REMOVE ${CMAKE_SOURCE_DIR}/compile_commands.json)
file(CREATE_LINK ${CMAKE_BINARY_DIR}/compile_commands.json ${CMAKE_SOURCE_DIR}/compile_commands.json SYMBOLIC)