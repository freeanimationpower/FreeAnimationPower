# AGENTS.md - AI Agent Instructions

## Project: Free Animation Power

2D animation software with hybrid vector + raster engine. Built with C++20 and Qt 6.

## Build Commands

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build
```

## Project Structure

- `src/core/` — Data model (Document, Layer, Timeline, Types)
- `src/engine/raster/` — Bitmap pixel engine
- `src/engine/vector/` — Bezier path vector engine
- `src/engine/brush/` — Brush engine with paper texture
- `src/engine/animation/` — Timeline playback, onion skinning
- `src/ui/` — Qt widgets (Canvas, panels, MainWindow)
- `src/io/` — File format (.fap), video export
- `src/platform/` — Input handling, tablet support
- `src_py/` — Python prototype (reference implementation)
- `tests/` — GoogleTest test files
- `docs/` — Architecture and build documentation
- `resources/` — Brush tips, palettes, paper textures

## Code Conventions

- Namespace: `fap`
- C++20, CMake 3.20+, Qt 6.5+
- Headers use `#pragma once`
- Engine code is Qt-independent (no QObject/Qt types in engine/)
- UI code uses Qt 6 Widgets with dark theme (#2d2d30 background, #d4782a accent)
- No external dependencies beyond Qt, FFmpeg, and GoogleTest

## Skills Available

37 agent skills in `.agents/skills/`. Key ones:
- `tdd` — Test-driven development workflow
- `improve-codebase-architecture` — Architecture analysis
- `design-an-interface` — API design
- `ui-ux-pro-max` — UI/UX design intelligence
- `diagnose` — Bug diagnosis
- `review` — Code review
- `setup-pre-commit` — Pre-commit hooks

## Testing

Use `tdd` skill for new features. All tests in `tests/` directory.
Test files use GoogleTest framework. Run with:
```bash
cmake --build build --target fap-tests
./build/fap-tests
```
