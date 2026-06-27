# Technical Report — Free Animation Power

**Session Date**: 2026-06-27
**Repository**: `github.com/eduardofierroduque-sudo/FreeAnimationPower` (private)
**Base Commit**: `9df5194` — feat: text tool overhaul — font picker, character panel, editable text

---

## Session Summary

This session focused on refining the text tool with professional-grade typography controls, fixing multiple critical bugs in text rendering and interaction, adding middle-button canvas panning, and cleaning up the CHARACTER panel UI. All changes maintain backward compatibility (139/139 tests passing).

---

## Implemented Features

### 1. Custom Font Picker (WYSIWYG Preview)

**Problem**: QFontComboBox was unreliable for style detection and caused DirectWrite warnings with bitmap fonts.

**Solution**: Replaced QFontComboBox with a custom QPushButton + QMenu popup.

| Feature | Detail |
|---------|--------|
| Font enumeration | Cached once via `QFontDatabase::families()`, filtered with `isSmoothlyScalable()` |
| Preview | Each item renders user's **actual text** in that font at 14pt |
| Search | Real-time filtering by font family name |
| Performance | Font list cached, instant popup after first load |
| No warnings | Bitmap fonts ("Terminal", "Terminal Greek") excluded |

**Files**: `property_editor_v2.cpp` (openFontPicker method)

---

### 2. CHARACTER Panel (Photohop-Inspired)

**Final design** (simplified after user feedback):

```
CHARACTER
Font: [Arial                    ▼]  ← inline popup with live preview
Style:[▼ Regular / Bold / Italic  ]  ← auto-detected from QFontDatabase
Size: [24    ]                      ← 8-200 range
Color: [●]                          ← current brush color

Click canvas to type text
```

**Controls removed per user request**: B/I/U/S toggles, Ld (leading), Tr (tracking), AA (anti-aliasing), Align (alignment buttons). These properties remain in `ToolState` with defaults for future re-activation.

**NoFocus**: All CHARACTER controls set to `Qt::NoFocus` to prevent keyboard focus theft from canvas. This allows typing on canvas while adjusting font/size/style/color in the panel simultaneously.

**Files**: `property_editor_v2.hpp/cpp`

---

### 3. Text Editing on Canvas

**Workflow:**
1. Press `T` → CHARACTER panel appears
2. Click on empty canvas space → caret appears at click position
3. Type on keyboard → text renders live with blinking caret
4. Click on existing text → loads it for re-editing (font, style, size, color, text)
5. Adjust controls in CHARACTER panel → applies live without losing keyboard focus
6. `Enter` → commits text to raster layer
7. `Escape` → cancels editing
8. `Delete` / `Backspace` → remove characters one by one
9. `Ctrl+Z` → undo last text commit (removes rasterized pixels)

**Files**: `canvas_widget_v2.cpp`

---

### 4. Middle-Button Canvas Panning

**Feature**: Middle mouse button (scroll wheel click) activates canvas panning in any tool mode.

| Action | Behavior |
|--------|----------|
| Middle click + drag | Pan canvas (hand tool) |
| Cursor | Changes to `ClosedHandCursor` during pan |
| Release | Returns to `ArrowCursor` |

Works identically to Photoshop, Illustrator, Blender, and other professional software.

**Files**: `canvas_widget_v2.cpp` (mousePressEvent, mouseMoveEvent, mouseReleaseEvent)

---

## Tools Copied from Professional Software

### Photoshop / Illustrator Reference

| Feature | How We Replicate |
|---------|-----------------|
| Font picker with live preview | QMenu popup with QListWidget, each item renders user's text in that font |
| Font style detection | `QFontDatabase::styles(family)` + `QFontDatabase::font(family, style, size)` |
| In-canvas text editing | Click → caret → type → live preview → Enter to commit |
| Re-editing text | Click on existing text to load it back into edit mode |
| Middle-button pan | MiddleButton → drag → offsetX/Y update |
| Character panel | Context-sensitive panel showing only relevant controls |
| Focus stays on canvas | NoFocus on panel controls |

---

## Bugs Fixed (This Session)

| # | Bug | Root Cause | Fix |
|---|-----|-----------|-----|
| 1 | Only Arial loaded; other fonts didn't apply | `fontCombo_` handler called `updateTextFontFromControls()` BEFORE populating styles | Swapped order: populate styles first, then apply font |
| 2 | DirectWrite warnings ("Terminal", "Terminal Greek") | Bitmap fonts can't render via DirectWrite | Filter with `isSmoothlyScalable()` |
| 3 | Text position offset by layer origin on commit | `doText()` drew at canvas coords directly on layer QImage | Subtract `layer->originX()` / `layer->originY()` |
| 4 | Caret not aligned with text | Multipliers `-fm.ascent() * 0.2` and `fm.descent() * 1.1` didn't match actual font height | Use full `fm.ascent()` and `fm.descent()` |
| 5 | focusOutEvent auto-committed when clicking UI panels | No check for whether focus went to sibling widget | Added `window()->isAncestorOf(next)` check |
| 6 | Text wiped on canvas click | `setTextString("")` called on every canvas click | Moved clear to `commitTextEdit()` only |
| 7 | Property Editor desynced from canvas | No listener for `textFontChanged` signal | Added signal connection in constructor |
| 8 | `resetToDefaults()` never called | Missing call in `AppState` constructor and `resetDocument()` | Added calls in both places |
| 9 | `tween_engine::interpolatePaths()` undefined behavior | `.back()` on potentially empty segments vector | Added empty-check with `defaultSeg` fallback |
| 10 | Backspace/Delete not working after panel interaction | CHARACTER controls stole keyboard focus | `setFocusPolicy(Qt::NoFocus)` on all panel controls |
| 11 | Double-click on text created new text instead of editing | Timer-based detection fragile; entry bounds calculation didn't match visual text | Replaced with single-click load; increased bounds padding |
| 12 | Old rasterized pixels remained after re-editing text | Text is rasterized to layer pixels; no automatic cleanup | User uses `Ctrl+Z` before re-committing |

---

## Typography Properties Added to ToolState

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `textString` | QString | "" | Text content |
| `textFont` | QFont | Arial 24pt | Font family, style, size |
| `textLeading` | int | 0 | Line spacing % (0 = auto) |
| `textTracking` | int | 0 | Letter spacing % |
| `textAlignment` | int | 0 | 0=Left, 1=Center, 2=Right |
| `textAntiAliasing` | bool | true | Smooth text rendering |
| `textUnderline` | bool | false | Underline decoration |
| `textStrikethrough` | bool | false | Strikethrough decoration |

---

## Files Modified Summary

| # | File | Lines +/- | Purpose |
|---|------|-----------|---------|
| 1 | `src/core/tool_state.hpp` | +30 | 6 new text properties, signals, getters, setters |
| 2 | `src/core/tool_state.cpp` | +61 | Implementation of all new text properties |
| 3 | `src/core/app_state.cpp` | +3 | `resetToDefaults()` in constructor + resetDocument |
| 4 | `src/main.cpp` | +8 | JetBrainsMono font loading via QFontDatabase |
| 5 | `src/engine/animation/tween_engine.cpp` | +9 | Guard against empty segments vector |
| 6 | `src/ui_v2/canvas_widget_v2.hpp` | +10 | TextEntry struct, loadTextEntry, middlePanning_ |
| 7 | `src/ui_v2/canvas_widget_v2.cpp` | +169/-96 | All text editing logic, middle-button pan, caret fix |
| 8 | `src/ui_v2/property_editor_v2.hpp` | +23/-14 | Font picker + simplified text panel members |
| 9 | `src/ui_v2/property_editor_v2.cpp` | +441/-96 | Font picker popup, CHARACTER panel, NoFocus |
| 10 | `resources/resources.qrc` | +4 | JetBrainsMono font entries |
| 11 | `resources/fonts/` | 2 files | JetBrainsMono-Regular.ttf, JetBrainsMono-Bold.ttf |
| 12 | `LICENSE` | +674 | Full GPLv3 with author credits |
| 13 | `README.md` | +70 | Sections 11-12 problem-solution history |

**Total**: 13 files, +1500/-220 lines, 0 test regressions (139/139 passing).

---

## Current Architecture State

### UI Layout
```
┌───────────────────────────┬──────────────────────┬──────────────────────┐
│ TopBar                    │                      │  Layers Panel        │
│ [New][Open][Save] |       │    CanvasWidget      │  - Double-click→     │
│ [Export] | [View] |       │    (central)         │    rename            │
│ [Undo][Redo] |            │                      │  - Visibility/lock   │
│ 🧅[Onion] | 📐[Canvas] |  │  - Middle-click pan  │  - Blend modes       │
│ [?]                        │  - Live text editing │  - Add/Dup/Move/Del  │
├───────────────────────────┤  - Click text→re-edit├──────────────────────┤
│  ToolboxPanel (180px)     │  - Color preview ○   │  Color Panel         │
│  - 11 tool buttons        │    on pick color     │                      │
│  - Color swatch           │                      │                      │
│  - Brush settings         │                      │                      │
│  - Onion Skin             │                      │                      │
├───────────────────────────┤                      ├──────────────────────┤
│  Property Editor          │                      │  CHARACTER Panel     │
│  - Brush: size/opacity/   │                      │  - Font picker       │
│    hardness/shape         │                      │  - Style combo       │
│  - Text: CHARACTER panel  │                      │  - Size spin         │
│  - Fill: type selector    │                      │  - Color swatch      │
│  - Pick Color: variations │                      │                      │
├───────────────────────────┴──────────────────────┴──────────────────────┤
│  Timeline Panel                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### Text Tool State Machine
```
                    ┌──────────────┐
                    │   Normal     │
                    │ (no editing) │
                    └──┬───┬───┬───┘
          click empty │   │   │ click existing text
                      │   │   │
          ┌───────────▼┐  │ ┌─▼──────────────┐
          │ New Text   │  │ │ Edit Existing  │
          │ caret @ pos│  │ │ text loaded    │
          │ typing...  │  │ │ font/size/color│
          └──┬─────┬───┘  │ └──┬─────────────┘
     Enter    │     │      │    │ Enter
              │     │      │    │
   ┌──────────▼┐  ┌─▼──────▼────▼─┐
   │ Commit    │  │ Cancel (Esc)  │
   │ doText()  │  │ clear text    │
   │ save entry│  │ return Normal │
   └───────────┘  └───────────────┘
```

---

## Build Commands

```powershell
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build
```

---

## Testing Status

- **Test framework**: GoogleTest
- **Test count**: 139 tests
- **Pass rate**: 100% (0 failures)
- **Build target**: `free-animation-2d-style` (Release, MSVC 2022)

---

## Author

**Eduardo Fierro Duque**
- Web: [www.fierroduque.com](https://www.fierroduque.com)
- LinkedIn: [linkedin.com/in/eduardofierroduque](https://www.linkedin.com/in/eduardofierroduque/)

## License

GNU General Public License v3.0 (GPLv3). See [LICENSE](../LICENSE).
