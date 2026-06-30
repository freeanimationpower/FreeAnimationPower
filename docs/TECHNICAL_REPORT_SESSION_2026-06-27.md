# Technical Report — Free Animation Power

**Session Date**: 2026-06-27
**Repository**: `github.com/eduardofierroduque-sudo/FreeAnimationPower` (private)
**Base Commit**: `9df5194` — feat: text tool overhaul — font picker, character panel, editable text

---

## Session Summary

This session focused on refining the text tool with professional-grade typography controls, fixing multiple critical bugs in text rendering and interaction, adding middle-button canvas panning, and cleaning up the CHARACTER panel UI. The second half of the session addressed the critical "ghost text" bug during text re-editing, added cursor positioning and text selection, and eliminated Qt QPainter composition issues by switching to direct pixel copy for layer restoration. All changes maintain backward compatibility (154/154 tests passing).

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

### 7. Ghost Text Elimination + Cursor Positioning + Text Selection

**Problem**: After the initial text tool improvements, three critical issues remained:

1. **Ghost text on re-edit**: When clicking on existing rasterized text, deleting all overlay characters, and committing, the old rasterized pixels remained visible on the layer. The paintEvent renders layer pixels first, then overlay text — so when the overlay was cleared, the layer text shone through as a "phantom" that couldn't be erased.

2. **No cursor positioning**: Delete and Backspace both always removed only the last character (`chop(1)`). There was no cursor concept — typing always appended to the end, and arrow keys did nothing.

3. **No text selection**: Shift+arrow, Ctrl+A, and selection highlighting were entirely absent. Users couldn't select text for bulk deletion.

**Root Cause Analysis**:

The ghost text had a dual nature:
- **Visual layer**: During editing, the `paintEvent` rendered both the rasterized layer (old pixels) and the overlay (editable text). When the overlay was cleared, the layer pixels remained visible.
- **Persistence layer**: On commit, `doText()` drew new text on top of existing layer pixels using `QPainter::drawText`. When re-editing committed text, the new (possibly shorter/empty) text was drawn over old pixels — old text remained on the layer.

**Solution**:

**Visual: White overlay background rect** (`paintEvent`, lines 692+):
- Computes text bounding box from `QFontMetrics` and line positions
- When re-editing (`editingEntryIndex_ >= 0`), expands rect to cover at least the original `TextEntry`'s text area (computed from stored entry's font metrics)
- Fills rect with `#FFFFFF` before drawing overlay text — masks rasterized text underneath
- 10px padding on all sides, minimum 20px width

**Persistence: UndoImage pixel restoration** (`doText` + `commitTextEdit`):
- `doText()`: Captures clean layer region via `QImage::copy(textUndoRect)` BEFORE drawing text. Stores as `undoImage`/`undoRect` in `TextEntry`
- `commitTextEdit()` on re-edit: restores `undoImage` pixels to layer using direct `std::copy` from scanlines (byte-for-byte, avoiding `QPainter::drawImage` composition issues with `Format_ARGB32_Premultiplied`)
- If text empty after re-edit: only erases old pixels and removes `TextEntry` (no `doText` call)
- If text non-empty: erases old pixels, then `doText()` draws new text on clean layer

**Cursor positioning** (`textCursorPos_` in `CanvasWidgetV2`):
- ← →: move cursor one character
- Home/End: jump to text start/end
- Backspace: delete char before cursor, decrement cursor
- Delete: delete char at cursor
- Typing: insert at cursor position via `QString::insert()`
- Caret rendered at correct x using `fm.horizontalAdvance(beforeCaret)` with alignment-aware positioning

**Text selection** (`textSelectionAnchor_` in `CanvasWidgetV2`):
- Shift + ← → Home/End: extends selection from anchor point
- Ctrl+A: selects all (`anchor=0`, `cursor=len`)
- With active selection: Backspace/Delete removes entire selected range via `QString::remove()`
- Blue semi-transparent highlight `QColor(42, 130, 218, 100)` drawn behind selected text via `fillRect()`
- Multi-line selection: iterates lines, computes per-line highlight rects
- Typing any character clears selection first

**Escape behavior**:
- Re-edit: Escape cancels editing without touching layer (old rasterized text stays intact, white rect disappears)
- New text: Escape clears overlay text and exits editing

**Key Architecture Decision**: Instead of using `QPainter::drawImage` for pixel restoration (which applies SourceOver composition on premultiplied alpha, potentially preserving old pixels), the restoration now uses direct `std::copy` from the undoImage's scanlines to the layer's pixel buffer. This guarantees exact byte-for-byte pixel restoration — the undoImage completely replaces the layer region.

**TextEntry struct additions**:
```cpp
QImage undoImage;   // Layer pixels before text was rasterized
QRect undoRect;     // Region coordinates in layer-local space
```

**New CanvasWidgetV2 members**:
```cpp
int textCursorPos_ = 0;
int textSelectionAnchor_ = -1;  // -1 = no selection
```

**Files**: `canvas_widget_v2.hpp`, `canvas_widget_v2.cpp`

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
- **Test count**: 154 tests (139 from prior work + 15 memory stress tests)
- **Pass rate**: 100% (0 failures)
- **Build target**: `free-animation-2d-style` (Release, MSVC 2022)

---

## Author

**Eduardo Fierro Duque**
- Web: [www.fierroduque.com](https://www.fierroduque.com)
- LinkedIn: [linkedin.com/in/eduardofierroduque](https://www.linkedin.com/in/eduardofierroduque/)

## License

GNU General Public License v3.0 (GPLv3). See [LICENSE](../LICENSE).
