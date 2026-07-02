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

## Known Architectural Issues (Jul 2026)

**GPU Rendering Trade-off**: Using `QPainter` on `QOpenGLWidget` with premultiplied alpha images causes white-line artifacts at dab edges. Baking everything into a single opaque image fixes display but corrupts frame data (onion skin + composite layers get baked into active layer). Attempts to separate display from data with dual-image pipelines failed — each fix for one side breaks the other. The root cause is QPainter+QOpenGLWidget premultiplied alpha compositing bugs. Future fix should either: (a) use CPU backbuffer with single QImage blit, (b) raw OpenGL/Vulkan rendering, or (c) extract dabs mathematically from baked composite.

**Session report**: `docs/session-report-2026-07-02.md` — full post-mortem of the 2-day debugging session and rollback to commit `1f5f4dc`.

## Testing

Use `tdd` skill for new features. All tests in `tests/` directory.
Test files use GoogleTest framework. Run with:
```bash
cmake --build build --target fap-tests
./build/fap-tests
```
