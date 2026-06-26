# Technical Report: Free Animation Power — Qt Bilinear Filter Artifact

## 1. Software Overview

**Free Animation Power** — 2D animation software, hybrid vector+raster engine.
- **Language**: C++20, **UI framework**: Qt 6.5+
- **Canvas**: `CanvasWidget` (QWidget subclass) with QPainter-based rendering
- **Layer model**: `RasterLayer` stores pixels in `std::vector<uint32_t>` (ARGB32 Premultiplied)
- **Brush engine**: strokes rendered as thick antialiased QPainterPath lines (RoundCap+RoundJoin)
- **Rendering**: Zero-copy `QImage` wrapping the `std::vector<uint32_t>` pixel buffer

## 2. Symptom

When drawing with the brush tool (black brush on white canvas), after releasing the stroke, a faint **rectangular/squared ghost outline** (bounding box contour) appears around the brush stroke area. The artifact is most visible at zoom levels != 1.0.

### Visual Description
- A faint gray/white outline tracing the exact mathematical bounding box of the brush stroke
- Not a tile-seam grid pattern — a single rectangle contour per stroke
- Persists after the stroke is committed to the layer buffer
- Visible on subsequent repaints (not just during live drawing)

## 3. Rendering Pipeline (Current State)

### 3.1 Layer Compositing (paintEvent)
```cpp
// canvas_widget.cpp:160-239
void CanvasWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);   // GLOBAL AA ON
    p.fillRect(rect(), QColor(100,100,100));    // widget bg: gray
    p.fillRect(screenRect, Qt::white);          // canvas bg: white
    
    p.save();
    p.translate(ox, oy);       // pan offset (rounded to integer)
    p.scale(zoom_, zoom_);     // zoom
    
    // For each visible raster layer:
    //   1. setOpacity + setCompositionMode per layer
    //   2. wrapRasterLayer() → zero-copy QImage from PixelBuffer
    //   3. IF zoom != 1.0:
    //      a. SNAP world transform dx/dy to integer device pixels
    //      b. DISABLE Antialiasing
    //      c. ENABLE SmoothPixmapTransform
    //      d. drawImage(targetRect, layerImage, sourceRect)
    //      e. Restore transform + hints
    //   4. ELSE: drawImage without SmoothPixmapTransform
}
```

### 3.2 Stroke Commitment (commitStroke)
```cpp
// canvas_widget.cpp:1207-1266
void CanvasWidget::commitStroke() {
    // 1. Compute dirtyRect = stroke bounds + sz*1.5+1.0 padding
    // 2. Take before-snapshot of dirtyRect pixels (for undo)
    // 3. ensureContains() for each stroke point → buffer expansion with kGuardBand=2
    // 4. Create SMALL QImage stamp = dirtyRect + kStampPad=2 padding
    //    - img.fill(Qt::transparent) → 0x00000000
    //    - QPainter draws vector stroke onto stamp
    // 5. blendStrokeToLayerAt(r, img, stampRect.x(), stampRect.y())
    //    → manual premultiplied alpha blending (src OVER dst)
    //    → skips pixels with sa < 4 (alpha threshold filter)
    // 6. Take after-snapshot (for undo)
    // 7. Push PaintCommand to undo stack
}
```

### 3.3 Buffer Guard Band (ensureContains)
```cpp
// layer.cpp:173-179
void RasterLayer::ensureContains(int x, int y, int w, int h) {
    x -= kGuardBand;  // = 2
    y -= kGuardBand;
    w += kGuardBand * 2;
    h += kGuardBand * 2;
    // ... expand buffer with kGrowthPad=512 if needed
    // New areas initialized to 0x00000000
}
```

### 3.4 Live Stroke Preview (paintLiveStroke)
```cpp
// canvas_widget.cpp:452-532
void CanvasWidget::paintLiveStroke(QPainter& p) {
    // SINGLE CLICK: renders filled circle on padded QImage,
    //              then blits to widget with SmoothPixmapTransform
    // DRAG (2+ points): renders offscreen QImage with QPainterPath,
    //                   then blits to widget with SmoothPixmapTransform
    // ERASER (tool==1): white circles directly on widget
}
```

### 3.5 Manual Blend Function (blendStrokeToLayerAt)
```cpp
// canvas_widget.cpp:720-758
void blendStrokeToLayerAt(RasterLayer* raster, const QImage& stroke,
                          int canvasX, int canvasY) {
    // For each pixel in the stamp:
    //   sa = src alpha
    //   if (sa < 4) continue;        // skip near-transparent
    //   if (sa == 255) dst = src;     // opaque overwrite
    //   else: out = src + dst * (1 - srcA/255)  // premultiplied OVER
}
```

## 4. Complete List of Applied Fixes

| # | File | Fix | Purpose |
|---|------|-----|---------|
| 1 | `layer.hpp:87` | `kGuardBand = 2` | 2px transparent padding in buffer |
| 2 | `layer.cpp:176` | `ensureContains` adds ±2px to request | Buffer always has transparent border |
| 3 | `canvas_widget.cpp:214-222` | Matrix dx/dy snapped to integer device pixels | Align QImage tile grid to pixel grid |
| 4 | `canvas_widget.cpp:223` | `Antialiasing` OFF before `drawImage` | Prevent Qt from antialiasing internal tile boundaries |
| 5 | `canvas_widget.cpp:452-532` | `paintLiveStroke` renders to offscreen transparent QImage | Live stroke edges blend with transparent instead of widget bg |
| 6 | `canvas_widget.cpp:464-488` | Single-click draws filled circle with 2px padding | Handle single-point brush stamps |
| 7 | `canvas_widget.cpp:1255-1266` | `commitStroke` uses small QImage stamp (not full buffer) | Isolate stroke rendering from buffer edges |
| 8 | `canvas_widget.cpp:720-758` | `blendStrokeToLayerAt` — manual premultiplied blend | Avoid QPainter SourceOver on premultiplied surface |
| 9 | `canvas_widget.cpp:1248-1250` | `writeLayerPixels` sanitizes alpha=0 → force 0x00000000 | Eliminate poisoned transparent pixels |
| 10 | `canvas_widget.cpp:699,739` | `sa < 4` threshold in blend functions | Filter QPainter rasterizer noise (alpha 1-3) |
| 11 | `canvas_widget.cpp:527-530` | SmoothPixmapTransform ON for stamp blit | Proper bilinear filtering on live stroke stamp |

## 5. Problem Persistence

After all fixes applied, the artifact PERSISTS: a faint ghost outline tracing the brush stroke's bounding box appears after the stroke is committed to the layer buffer.

### Hypotheses Eliminated

| Hypothesis | Test | Result |
|-----------|------|--------|
| Qt tile seam bug (grid pattern) | Antialiasing OFF + matrix snap + guard band | NOT the issue — artifacts are single rectangles, not a grid |
| Zero-copy QImage causing incorrect tiling | Deep `.copy()` before drawImage | No effect — ruled out |
| QPainter SourceOver bug on premultiplied | Manual blend replaces QPainter compositing | Ghost outline still appears |
| Poisoned pixels (alpha=0, RGB≠0) | Post-write sanitization | Ruled out |
| QPainter rasterizer noise (alpha 1-3) | `sa < 4` threshold in blend | Still persists |
| Buffer edge touching by QPainter | 2px kGuardBand in buffer + 2px kStampPad in stamp | Still persists |

## 6. Working Hypothesis

After exhausting the Qt-level fixes above, the most likely remaining cause is:

**QPainter's internal rasterizer produces an antialiased stroke whose bounding-box-edge pixels retain non-zero alpha values even after the stroke is drawn on a transparent surface.** The `sa < 4` threshold should have filtered these, but the threshold might need to be higher (the noise could be alpha ≥ 4), OR the issue is not in the pixel data but in Qt's `drawImage` compositing path during `paintEvent` layer rendering (SmoothPixmapTransform bilinear filter interacting with the composited layer pixels).

### Key Observation
The ghost outline appears as a **rectangle** (the exact bounding box of the brush stroke), not a soft halo. This suggests it's the edge between the drawn pixels and the guard-band transparent area. If even a single pixel at that boundary has alpha > 0, it creates a visible contour.

## 7. Potential Next Steps

1. **Increase alpha threshold**: try `sa < 8` or `sa < 16` in blend functions
2. **Post-blend sanitize the exact dirty rect boundaries**: after blending, zero out the outer 2px perimeter of the dirty rect in the raster
3. **Use OpenGL canvas**: bypass Qt's software rasterizer entirely
4. **Implement custom bilinear scaling**: draw the layer at 1:1, then manually scale via CPU
5. **Draw layers with point sampling at all zoom levels**: pre-scale QImage via `QImage::scaled(Qt::SmoothTransformation)` and draw with SmoothPixmapTransform OFF
