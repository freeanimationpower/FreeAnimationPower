# Report: Efecto Acetato + Timeline NLE Compacto

**Fecha**: 2026-07-03 (final)

---

## 1. Renderizado Multi-Sequence (Efecto Acetato)

### Archivo: `src/ui_v2/canvas_widget_v2.cpp`

Tres ubicaciones modificadas (buildBackgroundCache full, partial, render):

```cpp
// Premiere-style: track 0 = visual top, track N = background
for (size_t si = doc.sequenceCount(); si > 0; --si) {
    size_t idx = si - 1;
    if (!doc.sequenceAt(idx).visible()) continue;
    float seqOpacity = doc.sequenceAt(idx).opacity();
    const auto& root = doc.sequenceAt(idx).rootLayerForFrame(currentFrame_);
    for (size_t li = root.layerCount(); li > 0; --li) {
        const Layer* layer = root.layerAt(li - 1);
        if (!layer || !layer->visible() || layer->type() != LayerType::Raster) continue;
        p.setOpacity(layer->opacity() * seqOpacity);
        p.drawImage(..., SourceOver);
    }
}
```

#### Orden de Composicion
- Secuencia [0] = capa visual superior (top track en timeline)
- Secuencia [N] = fondo (bottom track)
- Layers dentro de cada secuencia: layer[0] = arriba, layer[N] = abajo (coincide con panel de capas)
- Conexion `documentChanged` → `invalidateBackgroundCache()` añadida para opacidad en vivo

---

## 2. Visibilidad y Opacidad de Secuencia

### Archivo: `src/core/sequence.hpp` / `.cpp`

```cpp
bool visible_ = true;
float opacity_ = 1.0f;  // 0.0-1.0 clamped

bool visible() const;
void setVisible(bool v);
float opacity() const;
void setOpacity(float o);
```

Ambos preservados en `clone()`.

---

## 3. Reordenamiento de Secuencias

### Archivos: `src/core/document.hpp/cpp`, `src/core/app_state.hpp/cpp`

```cpp
// Document
void Document::moveSequence(size_t from, size_t to) {
    // Mueve secuencia en el vector + ajusta active_sequence_
    // si se movio la activa o si el indice activo necesita recalcularse
}

// AppState
void AppState::moveSequence(int from, int to) {
    document_->moveSequence(from, to);
    emit documentChanged();      // → invalida cache del canvas
    emit activeSequenceChanged();
}
```

---

## 4. Timeline Panel V2 — Diseño Final

### Constantes

| Parametro | Valor |
|---|---|
| kHeaderWidth | 120px |
| kTrackHeight | 64px |
| kCellWidth | 32px |
| kCellSpacing | 1px |
| kRulerHeight | 18px |

### Transport Bar

```
[<] [>] [||] [■]    FPS [56px/12pt]    Frame: 1/24 [11pt]    [+ Track 80x24]
                     ← 16px spacers →
```

### SequenceTrackWidget Header (120px × 64px)

```
┌── 120px ────────────────────────────────────┐
│ █ Name (QLineEdit 11px)     ▲ ▼ ⧉ ✕       │ ← Row 1 (22px)
│ Opacity: ═══════●═══════════════            │ ← Row 2 (18px)
└──────────────────────────────────────────────┘
```

**Elementos:**
- Barra acento activa: 2px solid #FF6B4A (borde izquierdo)
- QLineEdit: 11px Inter, transparente, edicion directa (sin double-click)
- Botones: iconos reales de recursos (move_up.png, move_down.png, duplicate.png, delete.png)
- Botones: siempre visibles (no hover-only)
- QSlider: 0-100 con CSS #FF6B4A, `blockSignals(true)` en init
- QLabel "Opacity:": 9px #8B8FA3

### Colores Dark Theme

```
kTrackBg        = #121418   (fondo track inactivo)
kTrackActiveBg  = #191D26   (fondo track activo)
kHeaderBg       = #0D1014   (fondo header)
kCellFilled     = #2E333D   (celda con contenido)
kCellEmpty      = #161920   (celda vacia)
kBorderColor    = #1E2128   (lineas divisorias)
kAccentColor    = #FF6B4A   (acento naranja)
```

### Sincronizacion de Scroll
- QScrollBar padre → sharedScrollOffset_ → update() en RulerWidget + todos los SequenceTrackWidgets
- Cada track calcula: `x = headerWidth + (frame * cellTotal) - sharedScrollOffset`

---

## 5. Bugs Corregidos

### Use-After-Free (3 niveles de defensa)
1. `MainWindowV2::sequenceChanged` no llama `rebuildTracks()` (actualizacion inline)
2. `onDupTrack`/`onDelTrack`/`onNewSequence`: `QTimer::singleShot(0, rebuildTracks)`
3. `rebuildTracks()`: `QApplication::focusWidget()->clearFocus()` antes de borrar

### LayerPanelV2: moveLayerDown sin invalidar cache
`moveLayerDown()` no emitia `layerDisplayPropertiesChanged()`. Añadido.

### CanvasWidgetV2: documentChanged sin conexion
`AppState::setSequenceOpacity` emitia `documentChanged()` pero el canvas no lo escuchaba. Añadida conexion → `invalidateBackgroundCache()`.

### frameHasContent usaba secuencia incorrecta
Cambiado de `activeSequence()` a `sequenceAt(seqIndex_)`.

### Pre-existing canvas bugs (3)
- `caretX` undeclared
- `middlePanning_` → `panning_`
- `TextEntry` sin `undoImage`/`undoRect`

---

## 6. Tests

154/154 tests pass, incluyendo:
- 4 opacity tests (OpacityDefaults, SetOpacity, ClonePreservesOpacity, SetSequenceOpacity)

---

## 7. Sequence Lock Shield

### Core
- `Sequence::locked_` (bool default false) + `locked()`/`setLocked()` + `clone()` preservation
- `AppState::setSequenceLocked(index, locked)` → `emit documentChanged()`

### Canvas Shield (3 mouse events)
```cpp
if (isSequenceLocked()) {
    auto tool = appState_->toolState().activeTool();
    if (tool != ToolType::Hand && tool != ToolType::ColorPicker) {
        setCursor(Qt::ForbiddenCursor);
        event->ignore();
        return;
    }
}
```
Blocks: Brush, Eraser, Fill, Text, Line, Rect, Ellipse, Move, Select.
Allows: Hand (pan), ColorPicker (eyedropper).

### TimelinePanelV2
- lockBtn_ (28x28) in header Row1: 🔒/🔓 icons from resources
- QLineEdit red `#FF4A4A` when locked (via `updateNameStyle()`)
- `kHeaderWidth`: 240 → 280px (eliminated delBtn_ overflow past header bg)

---

## 8. Audio Track Support

### AudioTrackWidget
- NEW: `src/ui_v2/audio_track_widget.hpp` (62 lines) + `.cpp` (254 lines)
- QMediaPlayer + QAudioOutput for real playback
- QAudioDecoder (async) with forced `QAudioFormat::Int16` mono 44100Hz
- Waveform drawn in cyan `#00D4AA` progressively (every 5 buffers)
- Error handling via `QAudioDecoder::error` signal → qWarning fallback
- Volume QSlider 0-100 → `audioOutput_->setVolume(val/100.0f)`
- Mute toggle (🔊/🔇) → `audioOutput_->setMuted()`
- Safe destructor: `player_->stop()` before deletion

### TimelinePanelV2 Integration
- `[+ Track ▾]` QMenu: "Add Animation Sequence" + "Add Audio Track"
- `onImportAudio()`: QFileDialog (mp3/wav/ogg/flac) → creates AudioTrackWidget
- `removeAudioTrack(AudioTrackWidget*)`: pointer-based via `std::find`
- Audio sync: onPlayPause/onStop/setCurrentFrame/onTrackFrameClicked
  - Anti-stutter: `setPosition()` once on play, never during active playback
  - Scrubbing: `setPosition()` each frame only when NOT playing

### Waveform Fix
- `decoder->setAudioFormat(format)` with `QAudioFormat::Int16` mono 44100Hz
- Fixes silent waveform on MP3 (native Float32 codec) and FLAC
- Buffer validation: `isValid()` + `sampleCount() > 0`
