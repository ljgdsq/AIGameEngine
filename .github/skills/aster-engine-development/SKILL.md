---
name: aster-engine-development
description: Implement and validate features in the Aster Engine C++20 and CMake codebase.
---

# Aster Engine Development

- Follow the repository's `AGENTS.md` conventions and keep work within this repository.
- Before implementation, identify the owning module and a nearby behavior test.
- Keep the core engine independent from editor code and isolate third-party APIs behind engine-owned interfaces where practical.
- Keep the play-mode and player update order identical: step physics, sync transforms, run Lua `OnUpdate`, then render. Reuse the same `PhysicsWorld` and `ScriptEngine` paths.
- Keep `Project` in `Aster/Project` and editor-only code in `Aster/Editor`; the runtime must not depend on editor application state.
- Add focused automated tests for normal behavior, invalid input, and lifecycle boundaries.
- Update the representative test scene when adding a component or scripting capability.
- Validate by configuring, building, and running CTest; report commands that could not be run.
- Do not mark renderer, physics, scripting, editor, or platform support complete without implementation and validation evidence.
- Do not stop work when finishing a small step; continue until all tasks are complete.
- Do not spend excessive time on trivial tests; focus on engine core functionality.
