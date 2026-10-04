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

option(BS_BUILD_TESTS "Build the unit test executable" ON)
option(BS_WARNINGS_AS_ERRORS "Treat compiler warnings in project code as errors" OFF)

include(FetchContent)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

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

function(bs_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /EHsc)
        if(BS_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
        if(BS_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

# Headless game library: ECS, gameplay rules and physics. No OpenGL or GLFW,
# so it can be linked by unit tests, tools and (later) the AI.
add_library(bs_game STATIC
    src/core/asset_paths.cpp
    src/gameplay/shot_state.cpp
    src/gameplay/game_variant.cpp
    src/gameplay/match_state.cpp
    src/gameplay/shot_result.cpp
    src/gameplay/turn_rules.cpp
    src/physics/billiards_physics.cpp
)

target_include_directories(bs_game PUBLIC src)
target_link_libraries(bs_game PUBLIC glm::glm)
target_compile_definitions(bs_game PRIVATE
    BS_SOURCE_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets"
)
bs_set_warnings(bs_game)

add_executable(BilliardsSaloon
    src/app/main.cpp
    src/app/application.cpp
    src/platform/window.cpp
    src/platform/timer.cpp
    src/render/shader.cpp
    src/render/mesh.cpp
    src/render/camera.cpp
    src/render/camera_rig.cpp
    src/render/ui_overlay.cpp
    external/glad/src/gl.c
)

target_include_directories(BilliardsSaloon PRIVATE
    src
    external/glad/include
)

target_link_libraries(BilliardsSaloon PRIVATE
    bs_game
    glfw
    OpenGL::GL
)

bs_set_warnings(BilliardsSaloon)

target_compile_definitions(BilliardsSaloon PRIVATE
    GLFW_INCLUDE_NONE
    $<$<CONFIG:Debug>:BS_DEBUG=1>
)

# Ship assets next to the executable so it runs from any working directory.
add_custom_command(TARGET BilliardsSaloon POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:BilliardsSaloon>/assets
    COMMENT "Copying assets next to BilliardsSaloon"
)

if(BS_BUILD_TESTS)
    FetchContent_Declare(
        doctest
        GIT_REPOSITORY https://github.com/doctest/doctest.git
        GIT_TAG v2.4.12
    )
    FetchContent_MakeAvailable(doctest)

    enable_testing()

    add_executable(bs_tests
        tests/test_main.cpp
        tests/test_registry.cpp
        tests/test_turn_rules.cpp
        tests/test_physics.cpp
        tests/test_asset_paths.cpp
    )

    target_link_libraries(bs_tests PRIVATE bs_game doctest::doctest)
    bs_set_warnings(bs_tests)

    add_test(NAME bs_tests COMMAND bs_tests)
endif()
