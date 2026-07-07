# Free Animation Power — Domain Context

## Domain Language

- **Canvas**: The drawing surface where animation frames are created
- **Frame**: A single image in the animation sequence, with its own independent layer stack
- **Layer**: An independent drawing level within a frame, composited bottom-to-top
- **Stroke**: A continuous mark made by a brush tool, defined by a series of points
- **Brush Preset**: A saved configuration of brush parameters (size, shape, dynamics)
- **Paper Texture**: A grayscale image that modulates brush opacity to simulate traditional media
- **Onion Skin**: Semi-transparent display of previous/next frames for animation reference
- **Timeline**: The temporal organization of frames with playback controls
- **Exposure**: How many frames a single drawing is displayed (on ones, twos, threes)
- **Keyframe**: A frame where a significant change occurs (new drawing or transform)
- **Inbetween**: A frame generated between two keyframes
- **Raster/Bitmap**: Pixel-based graphics representation
- **Vector**: Path-based graphics using mathematical curves (Bezier)
- **Blend Mode**: Algorithm determining how overlapping pixels combine (12 modes)
- **Stabilizer**: Stroke smoothing to reduce hand jitter (0-50 level)
- **ToolState**: Central observable state for active tool and brush settings
- **AppState**: Aggregates Document, ToolState, Timeline, and ThumbnailCache

## Core Entities

- **Document**: Top-level container (canvas size, FPS, per-frame layer stacks via `frames_` map)
- **Layer**: Base class; types: RasterLayer (pixel buffer), VectorLayer (bezier strokes), GroupLayer (children)
- **Timeline**: Manages frame sequencing, playback state, keyframe tracking
- **ToolState**: QObject with properties (activeTool, brushSize, brushOpacity, brushHardness, brushShape, pressureSize, pressureOpacity, stabilizerLevel, canvasWidth/Height, onion settings)
- **AppState**: Aggregates Document, ToolState, Timeline, ThumbnailCache; provides `activeLayer()` and `currentFrame()`
- **BrushEngine**: Processes input events into rendered strokes with preset dynamics
- **CanvasWidgetV2**: Central drawing widget, renders layers, handles input events, brush stamping
- **PaintCommandV2**: Undo command that snapshots before/after pixel regions for undo/redo
- **FrameThumbnailCache**: Cached 58px thumbnails of each frame for timeline display

## UI Panels (V2)

| Panel | Dock | Purpose |
|-------|------|---------|
| ToolboxPanelV2 | Left | Tool palette (Brush, Eraser, ColorPicker, Fill, Text, Line, Rectangle, Ellipse, Move, Select, Hand), color swatch, onion skin settings, canvas resize |
| LayerPanelV2 | Right | Layer list with visibility/lock toggles, blend mode combo, add/dup/move/delete buttons |
| ColorPanelV2 | Right | Color picker with hue wheel |
| PropertyEditorV2 | Right | Context-sensitive: brush size, opacity, hardness, shape, stabilizer, pressure checkboxes |
| TimelinePanelV2 | Bottom | Frame thumbnails with white preview, playback controls (play/stop/prev/next), FPS spinbox, scrolling |

## Architecture Decisions

1. **Hybrid Vector+Raster**: Both representations coexist; layers choose their type
2. **CPU Rendering for Brush**: Brush strokes rendered on CPU pixel buffers (like TVPaint), no GPU required for drawing
3. **Per-frame Layer Stacks**: `Document::frames_` is `std::map<int, unique_ptr<GroupLayer>>` — each frame has independent layers, enabling exposure sheet animation
4. **Qt 6 for UI**: Cross-platform native widgets with dark theme (#2d2d30 background, #d4782a accent)
5. **V2 UI as Primary**: `MainWindowV2` with unified dockable panels is the main interface
6. **ToolState as Observable**: Changes emit Qt signals received by all panels, keeping UI in sync
7. **Command Pattern Undo**: `UndoCommand` base with `undo()`/`redo()`; `PaintCommandV2` snapshots pixel regions
8. **Zero-copy Rendering**: `wrapRasterLayer()` creates QImage directly from layer pixel buffer
9. **FFmpeg for Export**: External process invocation for MP4/GIF encoding
