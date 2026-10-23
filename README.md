# Aster Engine

Aster is a small, production-oriented 3D game engine and editor. The current implementation targets
**Windows with Direct3D 12** through [NVRHI](https://github.com/NVIDIA-RTX/nvrhi), [GLFW](https://www.glfw.org/),
[Dear ImGui](https://github.com/ocornut/imgui), glm, Lua, yaml-cpp, cgltf, stb, and spdlog. Vulkan,
macOS, and Linux are not built or validated yet.

## Features

- **Renderer**: HDR RGBA16F offscreen pipeline with ACES tonemapping, forward PBR (metallic/roughness),
  directional plus point and spot lights, a PCF directional shadow map, image-based lighting from a
  procedural HDR studio environment (prefiltered cubemap, SH9 irradiance, BRDF LUT), screen-space
  ambient occlusion (SSAO), a skybox pass, a single-pass bloom approximation, sRGB albedo textures,
  emissive materials, sub-rect viewport rendering, OBJ and glTF mesh loading.
- **Editor**: project hub, create/open/save projects and scenes, hierarchy, inspector, viewport picking,
  transform gizmo, camera navigation, green collider debug draw, Scene/Game view switching, Lua/physics/audio
  play mode, spdlog-backed console, game export, and AI/scripted scene authoring through `--exec`. While
  playing, the Game view captures WASD/Space/R for the Marble Run demo and scene editing is disabled.
- **Runtime**: standalone `AsterPlayer` that runs an exported project from its primary scene camera with
  an ImGui HUD showing the project name, FPS, entity count, and Marble Run score/time when active.
- **Audio**: WAV decoding through XAudio2 with spatial sources and listener updates.
- **Networking**: synchronous, timeout-bounded HTTP GET/POST through libcurl (Schannel TLS) and TCP
  sockets through standalone Asio, exposed to Lua as `Network.HttpGet`, `Network.HttpPost`, and
  `Network.TcpConnect`.
- **ECS and data**: generational entity handles, YAML scene/prefab serialization, project-relative assets.
- **Scripting**: Lua 5.4 lifecycle callbacks, entity/scene APIs, component authoring (`AddMesh`, `SetColor`,
  `AddCamera`, `AddLight`, `AddRigidBody`, colliders, `AddScript`, `AddAudio`), and the HTTP/TCP network API.
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

The easiest launcher is `run_engine.bat` at the repository root. Double-click it, or drag a project
folder onto it; it finds `build-aster`/`build` Debug or Release automatically and passes through
flags such as `--play`, `--frames`, and `--screenshot`.

For AI/scripted authoring, run a Lua scene script and save the result:

```powershell
build\bin\Debug\AsterEditor.exe --project .\Samples\MarbleRun --exec .\build_scene.lua
```

In the editor, press **F5** or use the **Play** menu to enter play mode. Play mode switches to the Game
view and snapshots the scene, builds physics bodies, starts enabled Lua scripts, and restores the scene when
stopped. The Marble Run sample uses:

```powershell
build\bin\Debug\AsterEditor.exe --project .\Samples\MarbleRun --play
```

Use **WASD** to roll, **Space** to jump, and **R** to restart. Play mode blocks viewport picking, gizmos, and
inspector edits; press **Stop** to return to editing. Use the **Console** panel for runtime and editor logs.

## Logging

Logs go through spdlog and the editor console. Use `ASTER_LOG_TRACE`, `ASTER_LOG_DEBUG`, `ASTER_LOG_INFO`,
`ASTER_LOG_WARN`, and `ASTER_LOG_ERROR` with a category and printf-style format string:

```cpp
ASTER_LOG_DEBUG("Editor", "Viewport focus applied (focused=%d)", focused ? 1 : 0);
```

The runtime level is controlled by `Aster::Log::SetLevel`. Trace/Debug statements are compile-time controlled
as well: configure with `-DASTER_ENABLE_DEBUG_LOGS=OFF` to remove them from release builds (the default is ON
for development).

## Networking

HTTP requests and TCP connections are synchronous and always have a timeout, so scripts cannot block the
frame forever. Lua examples:

```lua
local response = Network.HttpGet("https://example.com/data.json", 5000)
if response.ok then
    print(response.status, response.body)
else
    print("HTTP error: " .. response.error)
end

local socket, error = Network.TcpConnect("127.0.0.1", 9000, 3000)
if socket then
    assert(socket:Send("ping\n"))
    local data = socket:Receive(4096, 3000)
    socket:Close()
end
```

HTTP uses libcurl with the Windows Schannel backend. Sockets use standalone Asio and the Win32
`ws2_32`/`mswsock` libraries. Both dependencies are vendored under `external/`.


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
loading, render math, Marble Run gameplay, Lua scripting, HTTP/TCP networking, audio, and game export
success/failure behavior. `AsterSandbox` is a small headless integration check that steps a representative
physics scene:

```powershell
build\bin\Debug\AsterSandbox.exe
```

## Project Layout

- `include/Aster`, `src/Aster`: engine, renderer, scene, assets, physics, scripting, project, runtime.
- `src/Aster/Editor`: editor application and ImGui UI.
- `src/Aster/Runtime`: standalone player.
- `assets`: shaders plus shared sample textures.
- `Samples/MarbleRun`: playable representative project (scene, assets, prefabs).
- `tests`: automated tests and the lightweight test framework.
- `external`: vendored dependency sources used by CMake.

## Platform Status

Only the Windows/D3D12 path has been built and exercised locally. Do not assume macOS/Linux support or a
Vulkan backend. FXAA, skeletal animation, general convex collision, and 2D UI authoring are not implemented
yet. The shadow path uses a single directional 2048x2048 map with 3x3 PCF, bloom is a single-pass
screen-space approximation rather than a multi-pass bloom chain, and networking is synchronous rather than
an asynchronous multiplayer layer.
