cmake_minimum_required(VERSION 3.28)

project(BilliardsSaloon
    VERSION 0.8.0
    DESCRIPTION "A 3D billiards game built from scratch in C++ and OpenGL"
    LANGUAGES C CXX
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BS_BUILD_TESTS "Build the unit test executable" ON)
option(BS_BUILD_PYTHON "Build the billiards_sim Python module (needs Python 3.12 dev)" OFF)
option(BS_WITH_ONNXRUNTIME "Learned AI evaluation with ONNX Runtime (downloads the prebuilt library)" OFF)
option(BS_WARNINGS_AS_ERRORS "Treat compiler warnings in project code as errors" OFF)

include(FetchContent)

# Every dependency is linked statically into the game.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

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

# Audio: miniaudio (public domain / MIT-0), header only. SOURCE_SUBDIR points
# at a folder without a CMakeLists so only the sources are fetched.
FetchContent_Declare(
    miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG 0.11.22
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR header-only
)
FetchContent_MakeAvailable(miniaudio)

FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
)
FetchContent_MakeAvailable(nlohmann_json)

# UI: FreeType for font rendering, RmlUi for HTML/CSS-style menus and HUD.
set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
FetchContent_Declare(
    freetype
    GIT_REPOSITORY https://github.com/freetype/freetype.git
    GIT_TAG VER-2-13-3
    GIT_SHALLOW TRUE
    SYSTEM
)
FetchContent_MakeAvailable(freetype)
if(NOT TARGET Freetype::Freetype)
    add_library(Freetype::Freetype ALIAS freetype)
endif()

# Use the FreeType built above, never a system copy, so builds are reproducible.
set(CMAKE_DISABLE_FIND_PACKAGE_Freetype TRUE)
set(RMLUI_SAMPLES OFF CACHE BOOL "" FORCE)
set(RMLUI_FONT_ENGINE "freetype" CACHE STRING "" FORCE)
set(RMLUI_LUA_BINDINGS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    rmlui
    GIT_REPOSITORY https://github.com/mikke89/RmlUi.git
    GIT_TAG 6.3
    GIT_SHALLOW TRUE
    SYSTEM
)
FetchContent_MakeAvailable(rmlui)

# RmlUi ships its GLFW platform and OpenGL 3 renderer as sources; build them
# against our GLAD loader instead of RmlUi's bundled one.
add_library(bs_rmlui_backend STATIC
    ${rmlui_SOURCE_DIR}/Backends/RmlUi_Platform_GLFW.cpp
    ${rmlui_SOURCE_DIR}/Backends/RmlUi_Renderer_GL3.cpp
)
target_include_directories(bs_rmlui_backend SYSTEM
    PUBLIC ${rmlui_SOURCE_DIR}/Backends
    PRIVATE external/glad/include
)
target_compile_definitions(bs_rmlui_backend PRIVATE
    GLFW_INCLUDE_NONE
    "RMLUI_GL3_CUSTOM_LOADER=<glad/gl.h>"
)
target_link_libraries(bs_rmlui_backend PUBLIC RmlUi::RmlUi glfw)

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

# Event-based billiards simulator: double precision, no graphics, no game
# state. Linked by the game, the tests and (later) the Python bindings.
add_library(bs_sim STATIC
    src/sim/events.cpp
    src/sim/motion.cpp
    src/sim/resolve.cpp
    src/sim/roots.cpp
    src/sim/simulate.cpp
    src/sim/table.cpp
)
target_include_directories(bs_sim PUBLIC src)
target_link_libraries(bs_sim PUBLIC glm::glm)
bs_set_warnings(bs_sim)

# Headless game library: ECS, gameplay rules and physics. No OpenGL or GLFW,
# so it can be linked by unit tests, tools and (later) the AI.
add_library(bs_game STATIC
    src/core/asset_paths.cpp
    src/core/gamepad_math.cpp
    src/core/launch_options.cpp
    src/core/settings.cpp
    src/gameplay/shot_state.cpp
    src/gameplay/director.cpp
    src/gameplay/equipment.cpp
    src/gameplay/game_variant.cpp
    src/gameplay/match_session.cpp
    src/ai/ai_profile.cpp
    src/ai/environment.cpp
    src/ai/features.cpp
    src/ai/planner.cpp
    src/audio/shot_sounds.cpp
    src/audio/sound_synth.cpp
    src/rules/racking.cpp
    src/scene/hall_geometry.cpp
    src/scene/table_geometry.cpp
    src/rules/referee.cpp
    src/rules/shot_record.cpp
)

target_include_directories(bs_game PUBLIC src)
target_link_libraries(bs_game
    PUBLIC glm::glm bs_sim
    PRIVATE nlohmann_json::nlohmann_json
)
target_compile_definitions(bs_game PRIVATE
    BS_SOURCE_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets"
)
bs_set_warnings(bs_game)

add_executable(BilliardsSaloon
    src/app/main.cpp
    src/app/application.cpp
    src/app/audio_engine.cpp
    src/app/miniaudio_impl.cpp
    src/app/hud_screen.cpp
    src/app/locker_screen.cpp
    src/app/match_setup_screen.cpp
    src/app/saloon_scene.cpp
    src/app/settings_screen.cpp
    src/app/shell_menus.cpp
    src/platform/input.cpp
    src/platform/window.cpp
    src/platform/timer.cpp
    src/render/scene_renderer.cpp
    src/render/render_targets.cpp
    src/render/number_atlas.cpp
    src/render/screenshot.cpp
    src/render/shader.cpp
    src/render/mesh.cpp
    src/render/camera.cpp
    src/render/camera_rig.cpp
    src/ui/ui_system.cpp
    external/glad/src/gl.c
)

target_include_directories(BilliardsSaloon PRIVATE
    src
    external/glad/include
)
target_include_directories(BilliardsSaloon SYSTEM PRIVATE external/stb)

target_link_libraries(BilliardsSaloon PRIVATE
    bs_game
    bs_rmlui_backend
    glfw
    OpenGL::GL
)
# Audio: miniaudio's headers, its implementation in one file without our
# warnings, and the system audio frameworks it talks to.
target_include_directories(BilliardsSaloon SYSTEM PRIVATE ${miniaudio_SOURCE_DIR})
set_source_files_properties(src/app/miniaudio_impl.cpp PROPERTIES COMPILE_OPTIONS "$<IF:$<CXX_COMPILER_ID:MSVC>,/w,-w>")
if(APPLE)
    target_link_libraries(BilliardsSaloon PRIVATE "-framework CoreFoundation" "-framework CoreAudio" "-framework AudioToolbox")
elseif(UNIX)
    target_link_libraries(BilliardsSaloon PRIVATE pthread m dl)
endif()

# Ball numbers are rasterised with FreeType, which RmlUi already links.
target_include_directories(BilliardsSaloon SYSTEM PRIVATE $<TARGET_PROPERTY:freetype,INTERFACE_INCLUDE_DIRECTORIES>)

bs_set_warnings(BilliardsSaloon)

target_compile_definitions(BilliardsSaloon PRIVATE
    BS_VERSION="${PROJECT_VERSION}"
    GLFW_INCLUDE_NONE
    $<$<CONFIG:Debug>:BS_DEBUG=1>
)

# Ship assets next to the executable so it runs from any working directory.
# A separate target runs on every build, so edited UI documents, shaders and
# data reach the build folder even when no code changed.
add_custom_target(bs_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:BilliardsSaloon>/assets
    COMMENT "Copying assets next to BilliardsSaloon"
)
add_dependencies(BilliardsSaloon bs_assets)

# Python bindings for training (ml/): the simulator, referee and classical AI.
if(BS_BUILD_PYTHON)
    find_package(Python 3.12 COMPONENTS Interpreter Development.Module REQUIRED)
    set(PYBIND11_FINDPYTHON ON)   # use the Python found above, not pybind11's own search
    FetchContent_Declare(
        pybind11
        GIT_REPOSITORY https://github.com/pybind/pybind11.git
        GIT_TAG v2.13.6
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(pybind11)
    pybind11_add_module(billiards_sim src/python/billiards_sim_module.cpp)
    target_link_libraries(billiards_sim PRIVATE bs_game)
    set_target_properties(billiards_sim PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/ml)
endif()

# Learned AI: ONNX Runtime's official prebuilt package for this platform.
if(BS_WITH_ONNXRUNTIME)
    set(BS_ORT_VERSION 1.20.1)
    if(APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
        set(BS_ORT_PACKAGE onnxruntime-osx-arm64-${BS_ORT_VERSION}.tgz)
        set(BS_ORT_LIBRARY libonnxruntime.${BS_ORT_VERSION}.dylib)
    elseif(APPLE)
        set(BS_ORT_PACKAGE onnxruntime-osx-x86_64-${BS_ORT_VERSION}.tgz)
        set(BS_ORT_LIBRARY libonnxruntime.${BS_ORT_VERSION}.dylib)
    elseif(WIN32)
        set(BS_ORT_PACKAGE onnxruntime-win-x64-${BS_ORT_VERSION}.zip)
        set(BS_ORT_LIBRARY onnxruntime.dll)
    else()
        set(BS_ORT_PACKAGE onnxruntime-linux-x64-${BS_ORT_VERSION}.tgz)
        set(BS_ORT_LIBRARY libonnxruntime.so.${BS_ORT_VERSION})
    endif()
    FetchContent_Declare(
        onnxruntime_prebuilt
        URL https://github.com/microsoft/onnxruntime/releases/download/v${BS_ORT_VERSION}/${BS_ORT_PACKAGE}
        SOURCE_SUBDIR no-cmake
    )
    FetchContent_MakeAvailable(onnxruntime_prebuilt)

    add_library(bs_onnxruntime SHARED IMPORTED)
    set_target_properties(bs_onnxruntime PROPERTIES
        IMPORTED_LOCATION ${onnxruntime_prebuilt_SOURCE_DIR}/lib/${BS_ORT_LIBRARY}
        INTERFACE_INCLUDE_DIRECTORIES ${onnxruntime_prebuilt_SOURCE_DIR}/include)
    if(WIN32)
        set_target_properties(bs_onnxruntime PROPERTIES IMPORTED_IMPLIB ${onnxruntime_prebuilt_SOURCE_DIR}/lib/onnxruntime.lib)
    endif()

    # value_model.cpp is compiled into each user (no extra static library,
    # so bs_game is linked once).
    add_executable(bs_ai_arena tools/ai_arena/main.cpp src/ai/value_model.cpp)
    target_link_libraries(bs_ai_arena PRIVATE bs_game bs_onnxruntime)
    bs_set_warnings(bs_ai_arena)

    target_sources(BilliardsSaloon PRIVATE src/ai/value_model.cpp)
    target_link_libraries(BilliardsSaloon PRIVATE bs_onnxruntime)
    target_compile_definitions(BilliardsSaloon PRIVATE BS_HAS_ONNXRUNTIME)
    # The runtime library travels next to the executables.
    foreach(target IN ITEMS BilliardsSaloon bs_ai_arena)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${onnxruntime_prebuilt_SOURCE_DIR}/lib/${BS_ORT_LIBRARY} $<TARGET_FILE_DIR:${target}>)
        if(APPLE)
            set_target_properties(${target} PROPERTIES BUILD_RPATH "@executable_path")
        elseif(UNIX)
            set_target_properties(${target} PROPERTIES BUILD_RPATH "$ORIGIN")
        endif()
    endforeach()
endif()

# Development tool: writes the synthesised sounds to WAV files.
add_executable(bs_sound_preview tools/sound_preview/main.cpp)
target_link_libraries(bs_sound_preview PRIVATE bs_game)
bs_set_warnings(bs_sound_preview)

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
        tests/test_rules_eight_ball.cpp
        tests/test_rules_rotation.cpp
        tests/test_shot_record.cpp
        tests/test_racking.cpp
        tests/test_match_session.cpp
        tests/test_match_rules.cpp
        tests/test_director.cpp
        tests/test_table_geometry.cpp
        tests/test_equipment.cpp
        tests/test_audio.cpp
        tests/test_ai.cpp
        tests/test_environment.cpp
        tests/test_game_variant.cpp
        tests/test_game_data.cpp
        tests/test_launch_options.cpp
        tests/test_settings.cpp
        tests/test_gamepad_math.cpp
        tests/test_sim_roots.cpp
        tests/test_sim_motion.cpp
        tests/test_sim_events.cpp
        tests/test_sim_shots.cpp
        tests/test_sim_golden.cpp
        tests/test_asset_paths.cpp
    )

    target_link_libraries(bs_tests PRIVATE bs_game doctest::doctest nlohmann_json::nlohmann_json)
    target_compile_definitions(bs_tests PRIVATE
        BS_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/test_scratch"
        BS_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data"
    )
    bs_set_warnings(bs_tests)

    add_test(NAME bs_tests COMMAND bs_tests)
endif()
