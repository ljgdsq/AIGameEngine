# Aster Engine Development Guidelines

## Scope and workflow
- Keep all project work inside this repository. Do not inspect unrelated machine files or use unrelated local code as a reference.
- Prefer small, reviewable changes with explicit ownership and behavior.
- Every engine feature must include automated tests for its public behavior and failure cases.
- Keep a representative test scene updated as components and scripting capabilities are added.
- Do not claim platform or feature support until it has been built and tested on that platform.

## C++ conventions
- Target C++20 and use four spaces for indentation.
- Use PascalCase for types and functions, `m_` plus PascalCase for private fields, and PascalCase for public members.
- Keep headers self-contained, use `#pragma once`, and include directly used standard/library headers.
- Prefer fixed-width integer types for serialized or externally visible identifiers.
- Make ownership and failure behavior explicit; avoid raw owning pointers and silent error recovery.

## Architecture
- Keep the core engine independent of editor-only code.
- Hide third-party implementation details behind engine-owned interfaces where practical.
- Fetch dependencies through documented, pinned CMake package versions; do not vendor opaque binaries.
- Treat asset loading, scripting, physics, rendering, and editor capabilities as separate tested modules.

## Validation
- Configure with `cmake -S . -B build -DASTER_BUILD_TESTS=ON`.
- Build with `cmake --build build --config Debug`.
- Run tests with `ctest --test-dir build -C Debug --output-on-failure`.
- Run the sandbox executable after a successful build.
- Review the final diff and report any platform or feature not validated locally.