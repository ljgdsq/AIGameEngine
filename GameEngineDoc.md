# AI Game Engine

We're building a production-grade, simple and straight-forward 3D Game Engine for Windows, macOS and Linux (Ubuntu 24+). This is professional-grade software that needs to be stable, fully-tested, and real-world deployable. No hacks, no shortcuts - solid as a rock.

## Dev Workflow
- Ask questions only when absolutely necessary - work autonomously
- IMRORTANT! DO NOT look outside of working directory or use other code on my computer as reference
- Start by asking question to determine engine name. Suggest a few
- Testing is of the utmost importance - use lots of unit tests (any framework, simple is good), run automated testing
- Setup a scene used for testing, which tests every single feature in the engine - all components, and entire scripting API
- Do things properly, this is production-grade and not a hack project
- Create AGENTS.md and relevant skills to support development, and contain concrete development guidelines
- Code style to match [Hazel](https://docs.hazelengine.com/HazelForEngineers/DeveloperGuide#naming), full repo [here](https://github.com/TheCherno/Hazel)
- Create git repository, commit and push to [GitHub repo](https://github.com/ljgdsq/AIGameEngine). IMPORTANT: do a code review before committing, and make sure all changes comply with code style, production-grade quality standards, and have been properly tested/have unit tests that have passed where necessary

## Tech stack
- C++ and CMake for core engine
- [GLFW](https://github.com/glfw/glfw) and [nvrhi](https://github.com/NVIDIA-RTX/NVRHI), using Vulkan primarily on all platforms
- [glm](https://github.com/g-truc/glm) for math
- [miniaudio](https://github.com/mackron/miniaudio) for audio
- Lua for scripting, though this can be up for discussion

## Basic Architecture
- Static library for core engine, executable for editor and runtime (depending on chosen design)
- Need to have editor to build games - this can be embedded into runtime executable (and stripped from distribution builds), or can be a standalone executable
- Simple ECS perhaps, to author scene with entities and components
- Ability to "export" game - executable that runs game without editing ability that we can distribute
- Editor needs to be fully controllable by AI agents - I should be able to ask you to build me a game like Tetris, and you should have all the tools available to do so without my intervention

## 3D Renderer
- Import gltf meshes with materials and textures
- Editor has gizmo to position them in the world
- PBR material workflow
- IBL with HDRIs from https://polyhaven.com/hdris
- Soft shadow maps
- Good SSAO
- HDR pipeline with tonemapping

### Implementation Status (2026-10-23)
- Windows/D3D12 vertical slice is implemented and locally validated on an NVIDIA RTX 3050. The renderer uses NVRHI's D3D12 backend, GLFW, and a DXGI flip-discard swap chain owned by `D3D12Context`; ImGui 1.91.9b renders through its native DX12 backend. Vulkan is intentionally not built or validated yet.
- Rendering: forward physically based shading with directional, point (up to four), and spot (up to four) lights; sRGB albedo textures; metallic/roughness, emissive, opacity, ACES tonemapping; viewport/scissor sub-rects; OBJ and glTF mesh loading; per-material submeshes.
- Editor (`AsterEditor`): project hub, create/open/save projects and scenes, scene hierarchy, inspector for transforms/cameras/lights/materials/physics/scripts, viewport picking, translate/rotate/scale gizmo, camera navigation, play/stop mode, spdlog-backed console, and game export. Command-line automation supports `--project`, `--create`, `--name`, `--play`, `--frames`, `--screenshot`, `--export`, and `--player`.
- Runtime (`AsterPlayer`): standalone D3D12 player that opens an exported project, runs Lua and physics, renders from the primary scene camera, and supports the same frame/screenshot automation.
- Scripting: Lua 5.4.8 lifecycle callbacks (`OnStart`, `OnUpdate`, `OnDestroy`), entity transform/name/destroy APIs, and a scene API for creating, finding, counting, and destroying entities. Script paths are project-relative and serialized with the scene.
- Physics: deterministic fixed-substep AABB/sphere collision and response for static, kinematic, and dynamic rigid bodies. Angular motion and general convex collision are not implemented.
- Assets and data: YAML project/scene/prefab serialization, OBJ/glTF mesh import, PNG/JPG/TGA/BMP/HDR image loading, embedded default primitive/checker assets, and project-relative asset resolution.
- Export: `GameExporter` copies a project, compiled shaders, and the player into an empty output directory as `Game.exe`; the exported player has been launched and screenshotted locally.
- Tests: 63 automated tests cover core utilities, ECS, scene/component serialization, project defaults (including the checker/cargo textures, lights, Lua script, and physics bodies), export success/failure paths, physics, render math/IBL reference math, and Lua scripting.
- Not implemented or not validated: IBL/skybox, shadow maps, SSAO, HDR bloom/FXAA post-processing, audio, skeletal animation, full convex physics, Vulkan, and macOS/Linux builds. Only the Windows/D3D12 path above has been built and exercised.

## Build And Validation
- Requirements: Windows 10/11, Visual Studio 2022+ (MSVC with C++20), Windows SDK 10.0.26100, CMake 3.24+, and `dxc.exe` from the Windows SDK. The repository keeps the required third-party sources under `external/` so no network fetch is needed.
- Configure: `cmake -S . -B build -G "Visual Studio 18 2026" -A x64` (older CMake may also need `-DCMAKE_POLICY_VERSION_MINIMUM=3.5`).
- Build: `cmake --build build --config Debug --target AsterEditor AsterPlayer AsterEngineTests`.
- Test: `ctest --test-dir build -C Debug --output-on-failure` or run `build/bin/Debug/AsterEngineTests.exe`.


## 3D Physics
- Use simple 3D physics library of your choosing
- Controllable via scripting, authored via components in editor

## Behavior
- Scripting with Lua (or other chosen language)
- Control entities/components and run in update loop, and entity destruction/creation
- Spawn new entities/prefabs


## Serization Behavior
- use json or yaml

## Test
- simple test
- do not cost much time
- focus on engine core function impl


## Important!!!
- do not stop work when finish a small step. keep working! just stop when finish all task!