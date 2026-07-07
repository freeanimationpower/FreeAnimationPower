# Free Animation Power — Architecture Document (v2.0.0)

## Overview

Free Animation Power is a hybrid 2D animation application combining raster and vector drawing engines. Targets traditional frame-by-frame animation and digital painting. Built with C++20, Qt 6, and CMake.

The V2 UI (`src/ui_v2/`) is the primary interface with unified dockable panels connected to a central `AppState` via Qt signals/slots.

---

## Layer Architecture

```
+------------------------------------------------------------------+
|                          UI Layer (Qt 6)                         |
|  MainWindowV2 | CanvasWidgetV2 | TimelinePanelV2 | ToolboxPanel  |
|  ColorPanelV2 | LayerPanelV2   | PropertyEditorV2                |
+------------------------------------------------------------------+
                                |
                                v
+------------------------------------------------------------------+
|                       AppState (Central Hub)                      |
|  ToolState | Document | Timeline | FrameThumbnailCache            |
+------------------------------------------------------------------+
                                |
                                v
+------------------------------------------------------------------+
|                       Engine Layer                                |
|  brush/ (BrushEngine, BrushPreset)                                |
|  raster/ (RasterEngine, RasterStroke)                             |
|  vector/ (VectorEngine, BezierPath, VectorStroke)                 |
|  compositor/ (Compositor, NodeGraph)                              |
|  animation/ (TimelineEngine, OnionSkin, Playback, FrameThumbnail) |
|  deformation/ (DeformationEngine, DeformationMesh)                |
|  common/ (blend_utils)                                            |
+------------------------------------------------------------------+
                                |
                                v
+------------------------------------------------------------------+
|                      Core Data Model                             |
|  Document | Layer (Raster/Vector/Group) | UndoManager | Timeline  |
|  ToolState | Types (Vec2, Color, Rect, BlendMode, etc.)           |
+------------------------------------------------------------------+
                                |
                                v
+------------------------------------------------------------------+
|                        Platform Layer                             |
|  IO (file_format, document_io, video_export)                     |
|  Platform Input (tablet_handler, input_manager)                  |
+------------------------------------------------------------------+
```

---

## V2 UI Panels

| Panel | File | Purpose |
|-------|------|---------|
| MainWindowV2 | `src/ui_v2/main_window_v2.cpp` | Main window with dockable panels, dark theme, signal wiring |
| CanvasWidgetV2 | `src/ui_v2/canvas_widget_v2.cpp` | Central drawing canvas; brush stamping, layer display, onion skin, zoom |
| ToolboxPanelV2 | `src/ui_v2/toolbox_panel_v2.cpp` | Tool palette, color swatch, onion skin settings, canvas resize |
| LayerPanelV2 | `src/ui_v2/layer_panel_v2.cpp` | Layer list (visibility/lock), blend mode dropdown, add/delete/duplicate |
| ColorPanelV2 | `src/ui_v2/color_panel_v2.cpp` | Color picker |
| PropertyEditorV2 | `src/ui_v2/property_editor_v2.cpp` | Brush properties: size, opacity, hardness, shape, stabilizer, pressure |
| TimelinePanelV2 | `src/ui_v2/timeline_panel_v2.cpp` | Frame thumbnails, playback, FPS, scrubbing, add/dup/delete frames |

---

## Data Flow

```
Input (Mouse/Tablet)
  → CanvasWidgetV2::mousePressEvent
    → drawBrushStamp(cx, cy)
      → RasterLayer::setPixel(lx, ly, color)
  → mouseMoveEvent
    → drawLineStamps(from, to)
  → mouseReleaseEvent
    → commitStroke()
      → captureRect (after snapshot)
      → PaintCommandV2 (before + after + dirty rect)
      → UndoManager::pushCommand()
      → canvasUpdated signal
        → TimelinePanelV2::update (refresh thumbnails)
  → paintEvent()
    → composited layers → QPainter → screen
```

### Per-Frame Data Model

```
Document
├── width_, height_ (canvas dimensions)
├── fps_ (frames per second)
├── total_frames_
└── frames_: std::map<int, unique_ptr<GroupLayer>>
    ├── frame 0 → GroupLayer
    │   ├── RasterLayer "Layer 1"
    │   └── RasterLayer "Layer 2"
    ├── frame 1 → GroupLayer
    │   └── RasterLayer "Layer 1"
    └── ...
```

Each frame has its own independent layer stack, enabling exposure sheet workflows.

---

## Key Design Decisions

1. **CPU raster rendering**: Brush strokes rendered on CPU pixel buffers (like TVPaint), no GPU required
2. **Per-frame layer stacks**: `Document::frames_` map enables independent layer data per frame
3. **ToolState as QObject**: Central observable state with properties and signals
4. **Zero-copy QImage**: `CanvasWidgetV2::render()` creates QImage wrappers directly on layer pixel buffers
5. **Command pattern undo**: `PaintCommandV2` snapshots before/after pixel regions
6. **Blend modes at QPainter level**: `QPainter::setCompositionMode()` with full 12-mode support

---

## File Format (.fap v2)

```
project.fap/
├── document.json         # Project metadata and layer tree
└── frames/
    ├── F0_L0.png         # Frame 0, Layer 0
    ├── F0_L1.png         # Frame 0, Layer 1
    ├── F1_L0.png         # Frame 1, Layer 0
    └── ...
```

### document.json Schema

```json
{
  "version": 2,
  "canvas_w": 1920,
  "canvas_h": 1080,
  "fps": 24,
  "total_frames": 60,
  "frames": [
    {
      "layers": [
        {
          "name": "Background",
          "type": "raster",
          "visible": true,
          "opacity": 1.0,
          "blend_mode": "normal",
          "locked": false,
          "width": 1920,
          "height": 1080
        }
      ]
    }
  ]
}
```

**Layer types**: `raster`, `vector`, `group`, `audio`, `camera`
**Blend modes**: `normal`, `multiply`, `screen`, `overlay`, `add`, `subtract`, `darken`, `lighten`, `color_burn`, `color_dodge`, `soft_light`, `hard_light`

