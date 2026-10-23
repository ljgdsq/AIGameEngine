cmake_minimum_required(VERSION 3.24)

project(AsterEngine VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

set(ASTER_EXTERNAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external" CACHE PATH "Local third-party source directory")

FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
    GIT_SHALLOW TRUE)

FetchContent_Declare(yaml_cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG 0.8.0
    GIT_SHALLOW TRUE)

FetchContent_Declare(cgltf
    GIT_REPOSITORY https://github.com/jkuhlmann/cgltf.git
    GIT_TAG v1.15)

FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG 003501f6d7a8668ce8b6d875e34dd97ad0d5d5b5)

if(EXISTS "${ASTER_EXTERNAL_DIR}/glm/CMakeLists.txt")
    set(FETCHCONTENT_SOURCE_DIR_GLM "${ASTER_EXTERNAL_DIR}/glm" CACHE PATH "" FORCE)
endif()
if(EXISTS "${ASTER_EXTERNAL_DIR}/yaml-cpp/CMakeLists.txt")
    set(FETCHCONTENT_SOURCE_DIR_YAML_CPP "${ASTER_EXTERNAL_DIR}/yaml-cpp" CACHE PATH "" FORCE)
endif()
if(EXISTS "${ASTER_EXTERNAL_DIR}/cgltf/cgltf.h")
    set(FETCHCONTENT_SOURCE_DIR_CGLTF "${ASTER_EXTERNAL_DIR}/cgltf" CACHE PATH "" FORCE)
endif()
if(EXISTS "${ASTER_EXTERNAL_DIR}/stb/stb_image.h")
    set(FETCHCONTENT_SOURCE_DIR_STB "${ASTER_EXTERNAL_DIR}/stb" CACHE PATH "" FORCE)
endif()

set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(glm yaml_cpp cgltf stb)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(${ASTER_EXTERNAL_DIR}/glfw external/glfw EXCLUDE_FROM_ALL)

set(DXHEADERS_BUILD_TEST OFF CACHE BOOL "" FORCE)
set(DXHEADERS_BUILD_GOOGLE_TEST OFF CACHE BOOL "" FORCE)
set(DXHEADERS_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(${ASTER_EXTERNAL_DIR}/directx-headers external/directx-headers EXCLUDE_FROM_ALL)

set(NVRHI_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(NVRHI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(NVRHI_INSTALL OFF CACHE BOOL "" FORCE)
set(NVRHI_WITH_VALIDATION OFF CACHE BOOL "" FORCE)
set(NVRHI_WITH_VULKAN OFF CACHE BOOL "" FORCE)
set(NVRHI_WITH_DX11 OFF CACHE BOOL "" FORCE)
set(NVRHI_WITH_DX12 ON CACHE BOOL "" FORCE)
set(NVRHI_WITH_NVAPI OFF CACHE BOOL "" FORCE)
set(NVRHI_WITH_RTXMU OFF CACHE BOOL "" FORCE)
set(NVRHI_FETCH_DIRECTX_HEADERS OFF CACHE BOOL "" FORCE)
add_subdirectory(${ASTER_EXTERNAL_DIR}/nvrhi external/nvrhi EXCLUDE_FROM_ALL)

set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "" FORCE)
set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)
set(SPDLOG_FMT_EXTERNAL OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
add_subdirectory(${ASTER_EXTERNAL_DIR}/spdlog external/spdlog EXCLUDE_FROM_ALL)

# Static libcurl with the Windows Schannel TLS backend.
set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(CURL_ENABLE_SSL ON CACHE BOOL "" FORCE)
set(CURL_USE_SCHANNEL ON CACHE BOOL "" FORCE)
set(CURL_USE_OPENSSL OFF CACHE BOOL "" FORCE)
set(CURL_ZLIB OFF CACHE BOOL "" FORCE)
set(CURL_BROTLI OFF CACHE BOOL "" FORCE)
set(CURL_ZSTD OFF CACHE BOOL "" FORCE)
set(CURL_USE_LIBPSL OFF CACHE BOOL "" FORCE)
set(CURL_USE_LIBSSH2 OFF CACHE BOOL "" FORCE)
set(CURL_USE_GSSAPI OFF CACHE BOOL "" FORCE)
set(USE_LIBIDN2 OFF CACHE BOOL "" FORCE)
set(USE_NGHTTP2 OFF CACHE BOOL "" FORCE)
set(CURL_DISABLE_LDAP ON CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory(${ASTER_EXTERNAL_DIR}/curl external/curl EXCLUDE_FROM_ALL)

add_library(AsterAsio INTERFACE)
target_include_directories(AsterAsio INTERFACE ${ASTER_EXTERNAL_DIR}/asio/include)
target_compile_definitions(AsterAsio INTERFACE ASIO_STANDALONE WIN32_LEAN_AND_MEAN NOMINMAX)
if(WIN32)
    target_link_libraries(AsterAsio INTERFACE ws2_32 mswsock)
endif()

add_library(AsterImGui STATIC
    ${ASTER_EXTERNAL_DIR}/imgui/imgui.cpp
    ${ASTER_EXTERNAL_DIR}/imgui/imgui_draw.cpp
    ${ASTER_EXTERNAL_DIR}/imgui/imgui_tables.cpp
    ${ASTER_EXTERNAL_DIR}/imgui/imgui_widgets.cpp
    ${ASTER_EXTERNAL_DIR}/imgui/imgui_demo.cpp
    ${ASTER_EXTERNAL_DIR}/imgui/misc/cpp/imgui_stdlib.cpp
    ${ASTER_EXTERNAL_DIR}/imgui/backends/imgui_impl_glfw.cpp
    ${ASTER_EXTERNAL_DIR}/imgui/backends/imgui_impl_dx12.cpp)
target_include_directories(AsterImGui PUBLIC
    ${ASTER_EXTERNAL_DIR}/imgui
    ${ASTER_EXTERNAL_DIR}/imgui/backends
    ${ASTER_EXTERNAL_DIR}/imgui/misc/cpp)
target_link_libraries(AsterImGui PUBLIC glfw d3d12)
if(MSVC)
    target_compile_options(AsterImGui PRIVATE /utf-8)
endif()

add_library(AsterCgltf INTERFACE)
target_include_directories(AsterCgltf INTERFACE ${cgltf_SOURCE_DIR})

add_library(AsterStb INTERFACE)
target_include_directories(AsterStb INTERFACE ${stb_SOURCE_DIR})

add_library(AsterLua STATIC ${ASTER_EXTERNAL_DIR}/lua/onelua.c)
target_include_directories(AsterLua PUBLIC ${ASTER_EXTERNAL_DIR}/lua)
target_compile_definitions(AsterLua PRIVATE MAKE_LIB _CRT_SECURE_NO_WARNINGS)
if(MSVC)
    target_compile_options(AsterLua PRIVATE /W3 /utf-8)
endif()

set(ASTER_SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders")
file(MAKE_DIRECTORY "${ASTER_SHADER_OUTPUT_DIR}")
find_program(ASTER_DXC_EXECUTABLE NAMES dxc
    HINTS "C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64"
    "C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64")
if(NOT ASTER_DXC_EXECUTABLE)
    message(FATAL_ERROR "Aster Engine requires the DirectX Shader Compiler (dxc.exe). Set ASTER_DXC_EXECUTABLE.")
endif()

set(ASTER_PBR_SHADER "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/Pbr.hlsl")
set(ASTER_PBR_VS "${ASTER_SHADER_OUTPUT_DIR}/Pbr.vs.dxil")
set(ASTER_PBR_PS "${ASTER_SHADER_OUTPUT_DIR}/Pbr.ps.dxil")
set(ASTER_SHADOW_VS "${ASTER_SHADER_OUTPUT_DIR}/Pbr.shadow.vs.dxil")
set(ASTER_SHADOW_PS "${ASTER_SHADER_OUTPUT_DIR}/Pbr.shadow.ps.dxil")
set(ASTER_POST_VS "${ASTER_SHADER_OUTPUT_DIR}/Pbr.post.vs.dxil")
set(ASTER_POST_PS "${ASTER_SHADER_OUTPUT_DIR}/Pbr.post.ps.dxil")
set(ASTER_SSAO_PS "${ASTER_SHADER_OUTPUT_DIR}/Pbr.ssao.ps.dxil")
set(ASTER_SKY_PS "${ASTER_SHADER_OUTPUT_DIR}/Pbr.sky.ps.dxil")
add_custom_command(
    OUTPUT "${ASTER_PBR_VS}" "${ASTER_PBR_PS}" "${ASTER_SHADOW_VS}" "${ASTER_SHADOW_PS}" "${ASTER_POST_VS}"
        "${ASTER_POST_PS}" "${ASTER_SSAO_PS}" "${ASTER_SKY_PS}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${ASTER_SHADER_OUTPUT_DIR}"
    COMMAND "${ASTER_DXC_EXECUTABLE}" -nologo -T vs_6_0 -E VSMain -Fo "${ASTER_PBR_VS}" "${ASTER_PBR_SHADER}"
    COMMAND "${ASTER_DXC_EXECUTABLE}" -nologo -T ps_6_0 -E PSMain -Fo "${ASTER_PBR_PS}" "${ASTER_PBR_SHADER}"
    COMMAND "${ASTER_DXC_EXECUTABLE}" -nologo -T vs_6_0 -E VSShadow -Fo "${ASTER_SHADOW_VS}" "${ASTER_PBR_SHADER}"
    COMMAND "${ASTER_DXC_EXECUTABLE}" -nologo -T ps_6_0 -E PSShadow -Fo "${ASTER_SHADOW_PS}" "${ASTER_PBR_SHADER}"
    COMMAND "${ASTER_DXC_EXECUTABLE}" -nologo -T vs_6_0 -E VSFullscreen -Fo "${ASTER_POST_VS}"
        "${ASTER_PBR_SHADER}"
    COMMAND "${ASTER_DXC_EXECUTABLE}" -nologo -T ps_6_0 -E PSTonemap -Fo "${ASTER_POST_PS}" "${ASTER_PBR_SHADER}"
    COMMAND "${ASTER_DXC_EXECUTABLE}" -nologo -T ps_6_0 -E PSSsao -Fo "${ASTER_SSAO_PS}" "${ASTER_PBR_SHADER}"
    COMMAND "${ASTER_DXC_EXECUTABLE}" -nologo -T ps_6_0 -E PSSky -Fo "${ASTER_SKY_PS}" "${ASTER_PBR_SHADER}"
    DEPENDS "${ASTER_PBR_SHADER}"
    COMMENT "Compiling Aster PBR, shadow, sky, SSAO, and post shaders with DXC"
    VERBATIM)
add_custom_target(AsterShaders DEPENDS "${ASTER_PBR_VS}" "${ASTER_PBR_PS}" "${ASTER_SHADOW_VS}"
    "${ASTER_SHADOW_PS}" "${ASTER_POST_VS}" "${ASTER_POST_PS}" "${ASTER_SSAO_PS}" "${ASTER_SKY_PS}")

set(ASTER_DEBUG_SHADER "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/DebugLine.hlsl")
set(ASTER_DEBUG_VS "${ASTER_SHADER_OUTPUT_DIR}/DebugLine.vs.dxil")
set(ASTER_DEBUG_PS "${ASTER_SHADER_OUTPUT_DIR}/DebugLine.ps.dxil")
add_custom_command(
    OUTPUT "${ASTER_DEBUG_VS}" "${ASTER_DEBUG_PS}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${ASTER_SHADER_OUTPUT_DIR}"
    COMMAND "${ASTER_DXC_EXECUTABLE}" -nologo -T vs_6_0 -E VSLine -Fo "${ASTER_DEBUG_VS}"
        "${ASTER_DEBUG_SHADER}"
    COMMAND "${ASTER_DXC_EXECUTABLE}" -nologo -T ps_6_0 -E PSLine -Fo "${ASTER_DEBUG_PS}"
        "${ASTER_DEBUG_SHADER}"
    DEPENDS "${ASTER_DEBUG_SHADER}"
    COMMENT "Compiling Aster debug line shaders with DXC"
    VERBATIM)
add_custom_target(AsterDebugShaders DEPENDS "${ASTER_DEBUG_VS}" "${ASTER_DEBUG_PS}")
add_dependencies(AsterShaders AsterDebugShaders)

add_library(AsterEngine STATIC
    src/Aster/Assets/Asset.cpp
    src/Aster/Assets/AssetManager.cpp
    src/Aster/Assets/CgltfImpl.cpp
    src/Aster/Assets/GltfLoader.cpp
    src/Aster/Assets/ImageLoader.cpp
    src/Aster/Assets/ObjMeshLoader.cpp
    src/Aster/Assets/Primitives.cpp
    src/Aster/Assets/StbImpl.cpp
    src/Aster/Audio/AudioEngine.cpp
    src/Aster/Audio/WavDecoder.cpp
    src/Aster/Core/FileSystem.cpp
    src/Aster/Core/Log.cpp
    src/Aster/Core/UUID.cpp
    src/Aster/ECS/Registry.cpp
    src/Aster/Project/GameExporter.cpp
    src/Aster/Project/Project.cpp
    src/Aster/Math/Math.cpp
    src/Aster/Network/Network.cpp
    src/Aster/Network/NetworkLua.cpp
    src/Aster/Physics/PhysicsWorld.cpp
    src/Aster/Platform/Window.cpp
    src/Aster/Rendering/Camera.cpp
    src/Aster/Rendering/D3D12Context.cpp
    src/Aster/Rendering/D3D12Renderer.cpp
    src/Aster/Rendering/DebugDraw.cpp
    src/Aster/Rendering/Ibl.cpp
    src/Aster/Rendering/ImGuiRenderer.cpp
    src/Aster/Rendering/Pbr.cpp
    src/Aster/Runtime/MarbleGame.cpp
    src/Aster/Runtime/RuntimeApp.cpp
    src/Aster/Scene/Components.cpp
    src/Aster/Scene/Prefab.cpp
    src/Aster/Scene/Scene.cpp
    src/Aster/Scene/SceneSerializer.cpp
    src/Aster/Scripting/ScriptEngine.cpp)

target_include_directories(AsterEngine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(AsterEngine
    PUBLIC glm::glm glfw nvrhi
    PRIVATE yaml-cpp::yaml-cpp AsterCgltf AsterStb AsterLua spdlog::spdlog nvrhi_d3d12 d3d12 dxgi AsterImGui
        CURL::libcurl AsterAsio)
target_compile_features(AsterEngine PUBLIC cxx_std_20)
target_compile_definitions(AsterEngine PUBLIC
    ASTER_HAS_YAML=1
    ASTER_ENABLE_DEBUG_LOGS=$<BOOL:${ASTER_ENABLE_DEBUG_LOGS}>
    GLM_ENABLE_EXPERIMENTAL
    ASTER_SHADER_DIR="${ASTER_SHADER_OUTPUT_DIR}"
    ASTER_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
if(WIN32)
    target_compile_definitions(AsterEngine PUBLIC ASTER_PLATFORM_WINDOWS=1)
    target_link_libraries(AsterEngine PRIVATE xaudio2)
endif()
if(MSVC)
    target_compile_options(AsterEngine PRIVATE /W4 /permissive- /utf-8 /Zc:preprocessor)
endif()
add_dependencies(AsterEngine AsterShaders)

option(ASTER_BUILD_TESTS "Build Aster Engine tests" ON)
option(ASTER_ENABLE_DEBUG_LOGS "Compile Trace/Debug log statements" ON)
if(ASTER_BUILD_TESTS)
    enable_testing()
    add_executable(AsterEngineTests
        tests/TestMain.cpp
        tests/AssetTests.cpp
        tests/CoreTests.cpp
        tests/EcsTests.cpp
        tests/SceneTests.cpp
        tests/PhysicsTests.cpp
        tests/ProjectTests.cpp
        tests/ExportTests.cpp
        tests/RenderMathTests.cpp
        tests/DebugDrawTests.cpp
        tests/AudioTests.cpp
        tests/MarbleGameTests.cpp
        tests/NetworkTests.cpp
        tests/ScriptingTests.cpp
        tests/SerializationTests.cpp)
    target_link_libraries(AsterEngineTests PRIVATE AsterEngine AsterAsio)
    target_include_directories(AsterEngineTests PRIVATE tests)
    target_compile_definitions(AsterEngineTests PRIVATE ASTER_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    if(MSVC)
        target_compile_options(AsterEngineTests PRIVATE /utf-8)
    endif()
    add_test(NAME AsterEngineTests COMMAND AsterEngineTests)
endif()

add_executable(AsterEditor
    src/Aster/Editor/EditorApp.cpp
    src/Aster/Editor/EditorMain.cpp)
target_link_libraries(AsterEditor PRIVATE AsterEngine AsterImGui)
if(MSVC)
    target_compile_options(AsterEditor PRIVATE /utf-8 /permissive- /Zc:preprocessor)
endif()

add_executable(AsterPlayer src/Aster/Runtime/PlayerMain.cpp)
target_link_libraries(AsterPlayer PRIVATE AsterEngine)
if(MSVC)
    target_compile_options(AsterPlayer PRIVATE /utf-8 /permissive- /Zc:preprocessor)
endif()

add_executable(AsterSandbox examples/Sandbox.cpp)
target_link_libraries(AsterSandbox PRIVATE AsterEngine)
if(MSVC)
    target_compile_options(AsterSandbox PRIVATE /utf-8 /permissive- /Zc:preprocessor)
endif()
