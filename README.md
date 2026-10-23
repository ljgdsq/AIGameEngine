# Aster Engine

Aster is a small, production-oriented 3D game engine and editor. The current implementation targets
**Windows with Direct3D 12** through [NVRHI](https://github.com/NVIDIA-RTX/nvrhi), [GLFW](https://www.glfw.org/),
[Dear ImGui](https://github.com/ocornut/imgui), glm, Lua, yaml-cpp, cgltf, stb, and spdlog. Vulkan,
macOS, and Linux are not built or validated yet.

## Features

- **Renderer**: forward PBR (metallic/roughness), directional plus point and spot lights, sRGB albedo
  textures, emissive materials, ACES tonemapping, sub-rect viewport rendering, OBJ and glTF mesh loading.
- **Editor**: project hub, create/open/save projects and scenes, hierarchy, inspector, viewport picking,
  transform gizmo, camera navigation, Lua/physics play mode, spdlog-backed console, and game export.
- **Runtime**: standalone `AsterPlayer` that runs an exported project from its primary scene camera.
- **ECS and data**: generational entity handles, YAML scene/prefab serialization, project-relative assets.
- **Scripting**: Lua 5.4 lifecycle callbacks and entity/scene APIs.
- **Physics**: AABB/sphere rigid-body simulation with fixed substeps for static, kinematic, and dynamic bodies.

See [GameEngineDoc.md](GameEngineDoc.md) for the design goals and the current validated status. Features that
are listed there as not implemented should not be treated as supported.

## Requirements

- Windows 10 or 11, x64.
- Visual Studio 2022 or newer with the MSVC C++20 toolset.
- Windows SDK 10.0.26100 and its `dxc.exe` shader compiler.
- CMake 3.24 or newer.
- A Direct3D 12-capable GPU and driver.

The required third-party sources are kept under `external/`, so configuring and building does not require a
network connection. `third_repo/` is a local download area and is ignored by Git.

## Build

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Debug --target AsterEditor AsterPlayer AsterEngineTests
```

If CMake rejects the old minimum-policy declaration in a dependency, add
`-DCMAKE_POLICY_VERSION_MINIMUM=3.5`. If DXC is not in the default Windows SDK location, pass
`-DASTER_DXC_EXECUTABLE="C:/path/to/dxc.exe"`.

## Run

```powershell
# Create a sample project and open it in the editor.
build\bin\Debug\AsterEditor.exe --create --project .\AsterProjects\Sample --name Sample

# Open an existing project.
build\bin\Debug\AsterEditor.exe --project .\AsterProjects\Sample

# Automated smoke run.
build\bin\Debug\AsterEditor.exe --project .\AsterProjects\Sample --frames 60 --screenshot .\shot.png

# Start in play mode.
build\bin\Debug\AsterEditor.exe --project .\AsterProjects\Sample --play
```

In the editor, press **F5** or use the **Play** menu to enter play mode. Play mode snapshots the scene, builds
physics bodies, starts enabled Lua scripts, and restores the scene when stopped. Use the **Console** panel for
runtime and editor logs.

## Export

Use **File > Export Game...** in the editor, or export from the command line:

```powershell
build\bin\Debug\AsterEditor.exe --project .\AsterProjects\Sample `
    --export .\Exports\Sample
```

The export directory must be empty and outside the project. It receives `Game.exe` (a copy of `AsterPlayer`),
the compiled PBR shaders, the project file, and the project's assets/scenes/prefabs. Run the result with:

```powershell
.\Exports\Sample\Game.exe
```

`--player` can point at a different player executable when exporting from a custom build layout.

## Tests

```powershell
ctest --test-dir build -C Debug --output-on-failure
# or
build\bin\Debug\AsterEngineTests.exe
```

The test suite covers core utilities, ECS, scene/component serialization, physics, project defaults, texture
loading, render math, Lua scripting, and game export success/failure behavior. `AsterSandbox` is a small
headless integration check that steps a representative physics scene:

```powershell
build\bin\Debug\AsterSandbox.exe
```

## Project Layout

- `include/Aster`, `src/Aster`: engine, renderer, scene, assets, physics, scripting, project, runtime.
- `src/Aster/Editor`: editor application and ImGui UI.
- `src/Aster/Runtime`: standalone player.
- `assets`: shaders plus shared sample textures.
- `tests`: automated tests and the lightweight test framework.
- `external`: vendored dependency sources used by CMake.

## Platform Status

Only the Windows/D3D12 path has been built and exercised locally. Do not assume macOS/Linux support or a
Vulkan backend. Rendering features such as shadow maps, SSAO, IBL/skybox, and HDR post-processing are not
implemented yet.
