# Session Report — 2026-07-08

## Pixel Offset Bug on .fap Load (v2.5.1)

### Symptom

Content drawn at the center of the canvas (e.g., 960,540 on 1920x1080) appeared at the top-left corner (0,0) after saving and reopening the `.fap` file.

### Investigation

#### Hypothesis 1: `setOrigin()` not called on load

**Verdict**: FALSE. Verified that both `DocumentManager::layerMetadataFromJson()` (line 71-72) and legacy `file_format.cpp::layerFromJson()` (line 121) correctly call `rl->setOrigin(origin_x, origin_y)` with values from the JSON metadata. The origin IS properly restored.

#### Hypothesis 2: Display pipeline ignores origin

**Verdict**: FALSE. Verified `buildBackgroundCache()` in `canvas_widget_v2.cpp` correctly uses:
- `QRect layerBounds(rl.originX(), rl.originY(), rl.width(), rl.height())` (lines 690, 748)
- `p.drawImage(QPoint(rl.originX(), rl.originY()), display)` (lines 596, 632, 662)
- `srcRect = inter.translated(-rl.originX(), -rl.originY())` (lines 694, 752)

#### Hypothesis 3: `ensureContains()` guard band on load (ROOT CAUSE)

**Verdict**: TRUE. In `readLayerData()` (line 617), `ensureContains()` was called with default `pad=true`, which triggers aggressive buffer expansion:

```
ensureContains(0, 0, 1920, 1080, true):
  x = 0 - 2 = -2           ← kGuardBand shifts into negative territory
  y = 0 - 2 = -2
  reqLeft = -2 < originX=0  ← triggers expansion

  newOriginX = -2 - 512 = -514   ← kGrowthPad
  newWidth = 1920 + 2 + 512 + 2 + 512 = ~2948

  relocatePixels() moves old zero data to correct canvas positions
  PNG copy writes at row 0 → canvas y = -514  ← WRONG!
  Should be at canvas y = 0 (original origin)
```

This happened even for layers with origin (0,0). The guard band of just 2px extended beyond the origin, triggering the full 512px growth pad.

### Solution

Single-character change in `src/io/document_manager.cpp:617`:

```diff
- rl->ensureContains(rl->originX(), rl->originY(),
-                        converted.width(), converted.height());
+ rl->ensureContains(rl->originX(), rl->originY(),
+                        converted.width(), converted.height(), false);
```

With `pad=false`:
- `ensureContains` is a no-op when the PNG fits within existing buffer dimensions
- No guard band, no growth pad, no origin shift
- PNG pixels copied at correct buffer positions
- Origin from JSON metadata (set by `layerMetadataFromJson`) remains unchanged

The guard band is only needed during active drawing (dab feathering), not during file loading.

### Verification

- 154/154 tests pass
- Manual verification: draw at canvas center → save → reopen → content at correct position

### Files Changed

| File | Change |
|------|--------|
| `src/io/document_manager.cpp` | Line 617: `ensureContains(..., true)` → `ensureContains(..., false)` |

---

## Tool State Desync on Load (v2.5.1)

### Symptom

After opening a .fap file, the brush tool behaved as an eraser on new layers. The toolbox UI showed Brush as selected, but drawing erased instead of painting. Toggling to Fill tool and back to Brush via UI clicks restored the correct behavior.

### Investigation

**Initial observation**: The canvas reads `appState_->toolState().activeTool()` directly in every mouse event handler (12 occurrences). No cached `currentTool_` variable in CanvasWidgetV2.

**Root cause trace**:

```
MainWindowV2::openProject()
  → appState_->resetDocument()
    → tool_state_->resetToDefaults()
      → setActiveTool(ToolType::Eraser)  // Line 167 of tool_state.cpp
        → emit activeToolChanged(Eraser)
          → NO ONE LISTENS to this signal in MainWindowV2
```

The `activeToolChanged` signal is emitted by `ToolState`, but `MainWindowV2::setupConnections()` never connects it. The toolbox's `QButtonGroup` is only synced when a user physically clicks a tool button (via `idClicked` lambda at line 217-220).

Meanwhile, `CanvasWidgetV2::setTool(int)` exists but is only called from:
1. `ToolboxPanelV2::toolChanged` signal → MainWindowV2 connection (line 352-353)
2. Keyboard shortcuts in canvas directly

Neither path is triggered during `resetDocument()`. The canvas reads tool state from `appState_->toolState()` directly on each event, so it immediately picks up the Eraser change.

### Solution

Single line at end of `MainWindowV2::openProject()` success branch:

```cpp
toolbox_panel_->setActiveTool(0);  // Restore Brush after load
```

`ToolboxPanelV2::setActiveTool(0)`:
1. Checks the Brush button (index 0) in `QButtonGroup` — syncs UI visually
2. Calls `appState_->toolState().setActiveToolByIndex(0)` → `setActiveTool(ToolType::Brush)` — syncs ToolState

### Verification

- 154/154 tests pass
- Manual verification: save project → reopen → brush paints normally (not erasing)

### Files Changed

| File | Change |
|------|--------|
| `src/ui_v2/main_window_v2.cpp` | Line 575: add `toolbox_panel_->setActiveTool(0);` after load
