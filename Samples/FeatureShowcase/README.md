# Feature Showcase

A single scene that exercises the validated Windows/D3D12 feature set of Aster Engine. It is intended
both as a visual demo and as an integration fixture: `tests/DemoTests.cpp` opens this project and asserts
that every feature below is present.

Run it:

```powershell
build-aster\bin\Debug\AsterEditor.exe --project .\Samples\FeatureShowcase
```

Press **F5** to play. The editor camera can be navigated with right-mouse drag and **WASD**; play mode uses
the scene's primary camera.

## What is in the scene

- **Directional light + shadows**: `Sun` casts a 2048x2048 PCF shadow map across the stage.
- **Spot light**: a warm spotlight over the stage demonstrates cone falloff.
- **Point lights**: red, green, and blue point lights light the chrome and matte spheres from different
  directions.
- **PBR materials**: a mirror-like chrome sphere (`metallic = 1`, `roughness = 0.04`), a brushed-gold
  rotating cube, a rough matte sphere, and a dark stage.
- **HDR / IBL / skybox**: the procedural studio HDR environment provides prefiltered specular IBL, SH9
  irradiance, and the visible skybox.
- **Bloom**: the emissive cyan orb and the bright fire particles exceed the bloom threshold and glow.
- **SSAO**: contact darkening around the stage and spheres.
- **Particles**: `Fire` is a CPU-simulated billboard emitter with per-particle lifetime, size and colour
  curves, gravity, and velocity spread; it feeds the HDR target so it also contributes to bloom.
- **Rigid-body physics**: the `Dynamic Crate` falls onto the static stage collider and comes to rest.
- **Lua scripting**: `Animation` runs `assets/scripts/showcase.lua`, which rotates the gold cube and moves
  the fire emitter every frame.
- **Audio**: a looping ambient WAV plays through the camera's audio listener during play mode.
- **Editor integration**: every component above is visible and editable in the Inspector, including the
  particle emitter's rate, lifetime, colours, size curve, velocity, gravity, and maximum particle count.

## Files

- `project.aster.yaml`: project definition and default scene.
- `scenes/showcase.aster.yaml`: all entities, lights, materials, physics, particles, and script wiring.
- `assets/scripts/showcase.lua`: per-frame Lua behaviour.
- `assets/models`: unit cube, sphere, and plane meshes.
- `assets/environments/studio.hdr`: procedural HDR studio environment.
- `assets/sounds/ambient.wav`: looping ambience.
