cmake_minimum_required(VERSION 3.20)
project(FlyTracer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

include(CheckIPOSupported)
check_ipo_supported(RESULT IPO_SUPPORTED)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ${IPO_SUPPORTED})

find_package(Vulkan 1.4 REQUIRED)

# =============================================================================
# External dependencies
# =============================================================================
include(FetchContent)

FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-3.4.18
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)
FetchContent_Declare(
    tinyobjloader
    GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
    GIT_TAG        v2.0.0rc13
    GIT_SHALLOW    TRUE
)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.5
    GIT_SHALLOW    TRUE
)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
    GIT_SHALLOW    TRUE
)
FetchContent_Declare(
    FlyFish
    URL https://raw.githubusercontent.com/fdlombae/FlyFish/853468cff6f63ae06643bf0ec57aa9514e1f68b5/src/FlyFish.h
    DOWNLOAD_NO_EXTRACT TRUE
)

set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(SDL3 tinyobjloader imgui googletest FlyFish)

# =============================================================================
# Header-only FlyFish (PGA) and ImGui with SDL3 + Vulkan backends
# =============================================================================
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)
target_include_directories(imgui SYSTEM PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui PUBLIC SDL3::SDL3 Vulkan::Vulkan)

add_library(FlyFish INTERFACE)
target_include_directories(FlyFish SYSTEM INTERFACE ${flyfish_SOURCE_DIR})

set(NATIVE_RELEASE_OPTIONS $<$<AND:$<CXX_COMPILER_ID:GNU,Clang,AppleClang>,$<CONFIG:Release>>:-march=native>)

# =============================================================================
# FlyTracerCore - static library: the CPU side (scenes, meshes, BVHs, GPU encoding), no Vulkan
# =============================================================================
add_library(FlyTracerCore STATIC
    engine/src/BVH.cpp
    engine/src/GameScene.cpp
    engine/src/GpuScene.cpp
    engine/src/Mesh.cpp
    engine/src/MeshLines.cpp
    engine/src/SceneEncoding.cpp
)
target_include_directories(FlyTracerCore PUBLIC engine/include)
target_link_libraries(FlyTracerCore PUBLIC FlyFish PRIVATE tinyobjloader)
target_compile_options(FlyTracerCore PRIVATE ${NATIVE_RELEASE_OPTIONS})
set_target_properties(FlyTracerCore PROPERTIES OUTPUT_NAME flytracer_core)

# =============================================================================
# FlyTracerEngine - static library: the Vulkan renderer, window and application on top of the core
# =============================================================================
add_library(FlyTracerEngine STATIC
    engine/src/Application.cpp
    engine/src/Benchmark.cpp
    engine/src/GpuTimer.cpp
    engine/src/ImGuiLayer.cpp
    engine/src/RaytracingPass.cpp
    engine/src/Swapchain.cpp
    engine/src/VulkanContext.cpp
    engine/src/VulkanRenderer.cpp
)
target_link_libraries(FlyTracerEngine PUBLIC FlyTracerCore SDL3::SDL3 Vulkan::Vulkan imgui)
target_compile_options(FlyTracerEngine PRIVATE ${NATIVE_RELEASE_OPTIONS})
set_target_properties(FlyTracerEngine PROPERTIES OUTPUT_NAME flytracer_engine)

# =============================================================================
# FlyTracer - game executable
# =============================================================================
set(GAME_SCENE_SOURCES
    game/src/SceneRegistry.cpp
    game/src/TestBoxScene.cpp
    game/src/DebugDemoScene.cpp
    game/src/BiReflectionScene.cpp
    game/src/QuaternionScene.cpp
    game/src/PluckerScene.cpp
    game/src/HornetScene.cpp
)
add_executable(${PROJECT_NAME} game/src/main.cpp ${GAME_SCENE_SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE game/include)
target_link_libraries(${PROJECT_NAME} PRIVATE FlyTracerEngine)

# =============================================================================
# Shaders and resources, next to the executable (paths are resolved relative to it)
# =============================================================================
if(NOT Vulkan_GLSLC_EXECUTABLE)
    message(FATAL_ERROR "glslc not found - install the Vulkan SDK")
endif()

set(SHADER_DIR ${PROJECT_SOURCE_DIR}/engine/shaders)
set(SHADER_OUTPUT_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders)
file(GLOB SHADER_INCLUDES CONFIGURE_DEPENDS ${SHADER_DIR}/*.glsl)
list(APPEND SHADER_INCLUDES ${PROJECT_SOURCE_DIR}/engine/include/ShaderInterface.h)

# Compiles every .comp file in SOURCE_DIR to OUTPUT_DIR/<name>.comp.spv, built by the TARGET target
function(compile_shaders TARGET SOURCE_DIR OUTPUT_DIR)
    file(GLOB SOURCES CONFIGURE_DEPENDS ${SOURCE_DIR}/*.comp)
    file(MAKE_DIRECTORY ${OUTPUT_DIR})
    foreach(SHADER ${SOURCES})
        get_filename_component(SHADER_NAME ${SHADER} NAME)
        set(OUTPUT ${OUTPUT_DIR}/${SHADER_NAME}.spv)
        add_custom_command(
            OUTPUT ${OUTPUT}
            COMMAND ${Vulkan_GLSLC_EXECUTABLE} --target-env=vulkan1.3 -I${SHADER_DIR} -I${PROJECT_SOURCE_DIR}/engine/include ${SHADER} -o ${OUTPUT}
            DEPENDS ${SHADER} ${SHADER_INCLUDES}
            COMMENT "Compiling ${SHADER_NAME}"
            VERBATIM
        )
        list(APPEND OUTPUTS ${OUTPUT})
    endforeach()
    add_custom_target(${TARGET} ${ARGN} DEPENDS ${OUTPUTS})
endfunction()

compile_shaders(Shaders ${SHADER_DIR} ${SHADER_OUTPUT_DIR} ALL)
add_dependencies(${PROJECT_NAME} Shaders)

add_custom_target(CopyResources ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/game/resources ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/resources
    COMMENT "Copying game resources"
)
add_dependencies(${PROJECT_NAME} CopyResources)

# =============================================================================
# Tests
# =============================================================================
enable_testing()
include(GoogleTest)

add_executable(FlyTracer_Test
    tests/test_bvh.cpp
    tests/test_camera.cpp
    tests/test_game_scene.cpp
    tests/test_gpu_scene.cpp
    tests/test_gpu_timing.cpp
    tests/test_input.cpp
    tests/test_mesh.cpp
    tests/test_mesh_lines.cpp
    tests/test_mirror_solver.cpp
    tests/test_pga.cpp
    tests/test_scene_encoding.cpp
)
target_include_directories(FlyTracer_Test PRIVATE game/include)
target_link_libraries(FlyTracer_Test PRIVATE FlyTracerCore GTest::gtest_main)
gtest_discover_tests(FlyTracer_Test)

# Needs a Vulkan GPU; skips itself when there is none
add_executable(FlyTracer_RenderTest tests/test_render.cpp tests/test_image_file.cpp)
target_link_libraries(FlyTracer_RenderTest PRIVATE FlyTracerEngine GTest::gtest_main)
target_compile_definitions(FlyTracer_RenderTest PRIVATE
    FLYTRACER_TEST_REFERENCE_DIR="${PROJECT_SOURCE_DIR}/tests/reference")
add_dependencies(FlyTracer_RenderTest Shaders)
gtest_discover_tests(FlyTracer_RenderTest)

# Golden images of every game scene; needs a Vulkan GPU and skips itself when there is none
add_executable(FlyTracer_SceneTest tests/test_scene_images.cpp ${GAME_SCENE_SOURCES})
target_include_directories(FlyTracer_SceneTest PRIVATE game/include)
target_link_libraries(FlyTracer_SceneTest PRIVATE FlyTracerEngine GTest::gtest_main)
add_dependencies(FlyTracer_SceneTest Shaders CopyResources)
target_compile_definitions(FlyTracer_SceneTest PRIVATE FLYTRACER_TEST_REFERENCE_DIR="${PROJECT_SOURCE_DIR}/tests/reference")
gtest_discover_tests(FlyTracer_SceneTest DISCOVERY_TIMEOUT 30)

# Shaders that only the tests use
compile_shaders(TestShaders ${PROJECT_SOURCE_DIR}/tests/shaders ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_shaders)

# flyfish.glsl against FlyFish, and the GLSL structs and bindings against C++; needs a Vulkan GPU and skips
# itself when there is none
add_executable(FlyTracer_ShaderTest tests/test_flyfish_glsl.cpp tests/test_shader_interface.cpp)
target_link_libraries(FlyTracer_ShaderTest PRIVATE FlyTracerEngine GTest::gtest_main)
add_dependencies(FlyTracer_ShaderTest TestShaders)
gtest_discover_tests(FlyTracer_ShaderTest)

add_executable(FlyTracer_ConfigTest tests/test_config.cpp)
target_include_directories(FlyTracer_ConfigTest PRIVATE engine/include)
target_link_libraries(FlyTracer_ConfigTest PRIVATE GTest::gtest_main)
gtest_discover_tests(FlyTracer_ConfigTest)

# =============================================================================
# Install
# =============================================================================
install(TARGETS FlyTracerCore FlyTracerEngine ARCHIVE DESTINATION lib)
install(DIRECTORY engine/include/ DESTINATION include/flytracer FILES_MATCHING PATTERN "*.h")
install(DIRECTORY ${SHADER_OUTPUT_DIR}/ DESTINATION share/flytracer/shaders)
