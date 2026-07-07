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

---

## 9. Timeline Panel v2.3 — Non-Destructive Rebuild & Free Audio Movement

### Bug: Use-After-Free en Audio Tracks

**Sintoma**: Al duplicar/crear/eliminar secuencias, las pistas de audio desaparecian visualmente y la app crasheaba.

**Causa**: `rebuildTracks()` usaba `delete item->widget()` sobre TODOS los widgets del `tracksLayout_`, incluyendo `AudioTrackWidget*`. Los punteros preservados en `audioTrackWidgets_` quedaban dangling.

### Solucion Arquitectonica

**Dominios separados con manejo diferenciado:**

| Componente | Dominio | Comportamiento en rebuild |
|------------|--------|--------------------------|
| `SequenceTrackWidget` | Core (`Document::sequences()`) | Destruido y recreado desde `AppState` |
| `AudioTrackWidget` | UI-only (`audioTrackWidgets_`) | Extraido del layout (sin delete), preservado, reinsertado |

### rebuildTracks() — Flujo No-Destructivo

```
1. clearFocus()                              → protege QLineEdit
2. removeWidget(at) × N                      → extrae audio (sin borrar)
3. tw->deleteLater() × N                     → borra SOLO secuencias
4. trackWidgets_.clear()
5. Limpia QSpacerItem del layout
6. Crea SequenceTrackWidget[] desde Document  → inserta al tope
7. addWidget(at) × N                         → reinserta audio preservado
8. addStretch()                              → scroll infinito
```

### onMoveAudioTrack() — Movimiento Libre

Sin restriccion de zona. El audio puede intercalarse entre secuencias:

```cpp
void onMoveAudioTrack(int index, int delta) {
    auto* widget = audioTrackWidgets_[index];
    int oldPos = tracksLayout_->indexOf(widget);
    int newPos = oldPos + delta;
    // bounds: 0 a count()-2 (excluye stretch)
    if (newPos < 0 || newPos > tracksLayout_->count() - 2) return;

    auto* item = tracksLayout_->takeAt(oldPos);
    if (item) {
        tracksLayout_->insertItem(newPos, item);
        tracksLayout_->update();       // fuerza geometria sync
        widget->positionHeader();      // recalcula header
        widget->update();              // repinta waveform
    }
}
```

### Waveform Fix

- `tracksLayout_->update()` síncrono ANTES de `widget->update()`
- Sin esto, `drawWaveform` recibe `width()=0` tras el `takeAt`/`insertItem`
- La onda se renderiza correctamente tras cada movimiento

### Layout Final (scroll vertical + libre)

```
┌──────────────────────────────────────────────────┐
│ Transport Bar [▶][⏹][◀][▶] FPS[24]  [+Track▾]   │ ← FIJO
├──────────────────────────────────────────────────┤
│ RulerWidget (regla de frames)                    │ ← FIJO
├──────────────────────────────────────────────────┤
│ ┌ QScrollArea (ScrollBarAlwaysOn vertical) ────┐│
│ │ [SequenceTrackWidget 0]                       ││
│ │ [AudioTrackWidget 0]        ▲ ▼ 🔊 ✕         ││
│ │ [SequenceTrackWidget 1]                       ││
│ │ [SequenceTrackWidget 2]                       ││
│ │ [= stretch =]                                 ││
│ └───────────────────────────────────────────────┘│
├──────────────────────────────────────────────────┤
│ ═══════ QScrollBar Horizontal ═════════════════ │ ← FIJO
└──────────────────────────────────────────────────┘
```

### AudioTrackWidget — Botones de Movimiento

- ▲/▼ botones (28x28) con iconos `move_up.png` / `move_down.png`
- Signals `moveUpRequested()` / `moveDownRequested()` via Q_OBJECT
- Lambdas dinamicas en `onImportAudio()`: buscan indice en vector via `std::find`
- `setTrackIndex(int)` para reindexacion al eliminar tracks
- `positionHeader()`: 4 botones horizontales (up, down, mute, del)

### Defensas de Memoria (5 niveles)

| Nivel | Mecanismo |
|-------|-----------|
| 1 | `MainWindowV2::sequenceChanged` inline (no llama rebuild) |
| 2 | `QTimer::singleShot(0, ...)` difiere ejecucion |
| 3 | `clearFocus()` antes de remover widgets del layout |
| 4 | `if (item)` null-check tras `takeAt()` |
| 5 | `std::max(0, count()-1)` bounds en `insertWidget()` |

### Archivos

| Archivo | Cambios | Lineas |
|---------|---------|--------|
| `audio_track_widget.hpp` | +Q_OBJECT, +signals, +botones, +setTrackIndex | +8 |
| `audio_track_widget.cpp` | +creacion de botones, +positionHeader 4-btn | +29 |
| `timeline_panel_v2.hpp` | +onMoveAudioTrack, -separator | +1/-3 |
| `timeline_panel_v2.cpp` | rebuildTracks rewrite, onMoveAudioTrack, onImportAudio signals, removeAudioTrack reindex, audio repaints, scrollbar | +71/-9 |

### Tests

154/154 tests pass (100%). Build: 0 errors, 0 warnings.

---

## 10. Waveform Decoding Odyssey — Silent Killers in QAudioDecoder

### El bug: "No waveform data" infinito

La forma de onda nunca se renderizaba. El `paintEvent` mostraba "No waveform data" o "Decoder error".

### Causas (3 silent killers)

| # | Killer | Mecanismo | Fix |
|---|--------|-----------|-----|
| 1 | `decoder->setAudioFormat(Int16)` | WMF rechazaba conversion. `bufferReady` nunca se emitia. | Eliminar `setAudioFormat`. Usar formato nativo. |
| 2 | `buffer.constData<qint16>()` en Float32 | Reinterpretacion de 32-bit float como 16-bit int → peaks ~0 | 5-branch handler: Float, Int32, Int16, UInt8, fallback |
| 3 | `else { return; }` en bufferReady | Formatos desconocidos (Int32, UInt8) abortaban silenciosamente | Fallback `peak = 0.2f` visible en el `else` |

### Linea de tiempo de fixes (5 commits)

```
0c88917 fix: if (w <= hdrW) return — guard against layout recalculation
792c7a6 fix: Float/Int16 dynamic detection + paintEvent "No waveform data"
dbafa35 fix: all 4 Qt6 formats (Float/Int32/Int16/UInt8) + 0.2f fallback
719baaa fix: remove decoder->setAudioFormat(Int16) — WMF delivers Float natively
614ff00 debug: qDebug instrumentation + decodeError_ flag
```

### Arquitectura final del bufferReady

```cpp
QObject::connect(decoder, &QAudioDecoder::bufferReady, this, [this, decoder]() {
    auto buffer = decoder->read();
    if (!buffer.isValid() || buffer.sampleCount() == 0) return;

    float peak = 0.0f;
    QAudioFormat::SampleFormat fmt = buffer.format().sampleFormat();

    if (fmt == Float)       { peak = max(abs(float samples)); }
    else if (fmt == Int32)  { peak = max(abs(qint32) / 2^31); }
    else if (fmt == Int16)  { peak = max(abs(qint16) / 2^15); }
    else if (fmt == UInt8)  { peak = max(abs(quint8 - 128) / 128); }
    else                    { peak = 0.2f; }  // visible fallback

    waveformPicks_.push_back(peak);
    if (waveformPicks_.size() % 5 == 0) update();
});
```

### Leccion aprendida

`QAudioDecoder::setAudioFormat()` no es un hint — es un contrato que el backend puede rechazar silenciosamente. En Windows/WMF, forzar Int16 bloquea la decodificacion de MP3. La estrategia correcta es dejar que el decoder use el formato nativo del backend y adaptar el handler de lectura a cualquier `sampleFormat`.

---

## 11. Timeline Panel v2.4 — Real FPS, Work Area & Duration (Jul 2026)

### Separacion conceptual

Tres conceptos que antes estaban colapsados en `totalFrames_`:

| Concepto | Variable | Uso |
|----------|----------|-----|
| Velocidad de reproduccion | `fps_` | Intervalo del QTimer `1000/fps` + `setPlaybackRate(fps/24.0)` |
| Area de trabajo | `workAreaStart_/End_` | Limites de loop/playback. Interactivo via RulerWidget click+drag |
| Duracion total | `durationFrames_` | Ancho del scrollbar horizontal. Celdas visibles en el timeline |

### RulerWidget Interactividad

```cpp
enum class DragTarget { None, Playhead, WorkAreaIn, WorkAreaOut };
```

- **Hover**: cursor `SizeHorCursor` a ±5px de los bordes WA
- **Drag In edge**: clamp `[0, waEnd-1]`
- **Drag Out edge**: clamp `[waStart+1, totalFrames]`
- **Playhead scrubbing**: click + drag continuo con `hitFrame(mx)`
- **Auto-pause**: click en regla durante playback → `togglePlayback()` antes de mover playhead
- **Barra visual**: fill 4px `#FF6B4A` (alpha 80) + lineas delimitadoras 1px (alpha 180)

### Control de Duracion

- `SequenceTrackWidget` renderiza celdas hasta `durationFrames_` (vacios mas alla de `totalFrames_`)
- `updateScrollBarRange()` usa `durationFrames_` para max scroll horizontal
- `PropertyEditorV2`: grupo SEQUENCE con spinbox DURATION (1-10000 fr)
- Boton `+` auto-expande `durationFrames_`

### Sincronizacion de Audio y Transporte

| Disparador | Accion |
|-----------|--------|
| Play | `setPlaybackRate(fps/24.0)` + `syncToFrame()` |
| Pause (||) | Timer stop + `player()->pause()` en todos los tracks |
| Cambio FPS | `setPlaybackRate(fps/24.0)` en todos los tracks |
| WA loop | `syncToFrame(waStart)` — re-sync audio |
| WA end (!loop) | Auto-stop: timer + audio + playhead en ultimo frame |
| Switch secuencia | `setPlaybackRate(newFps/24.0)` |

Guards: `updatingFps_` flag (anti signal-loop), `tracksLayout_->update()` sincrono (waveform geometry fix), `isPlaying()` check (ruler click auto-pause).

### Atajos

| Tecla | Accion |
|-------|--------|
| `Shift+I` | Work Area In = currentFrame |
| `Shift+O` | Work Area Out = currentFrame + 1 |

### AppState Bridges (pipeline centralizado)

```cpp
void setWorkAreaStart(int frame);  // → activeSequence() → emit documentChanged()
void setWorkAreaEnd(int frame);
void setDurationFrames(int count);
```

### Commits

```
7319298 feat: Timeline v2.4 — Real FPS, Work Area In/Out, Duration Control, Transport Sync
b7c6410 docs: comprehensive v2.4 architecture docs
089850c fix: O(1) frameHasContent + waveform layout sync + hasContent_ lifecycle
```

### Archivos

10 archivos codigo, 4 archivos docs. 154/154 tests pass. Build: 0 errors, 0 warnings.

---

## 12. Frame Content Detection — O(1) hasContent_ Flag (Jul 2026)

### Problema

Las celdas vacias se pintaban como `kCellFilled` por el `frameHasContent` original que solo verificaba `root->layerCount() > 0` sin escanear si habia pixeles reales.

### Solucion

Flag `hasContent_` en `RasterLayer`, activado en:
- `commitStroke()` / `doText()` / `commitMove()` / `commitFloatingSelection()` en `CanvasWidgetV2`
- `setPixel()` (alpha > 0) y `blendPixel()` (outA > 0) en `layer.cpp`
- Reseteado en `clear()`, preservado en `clone()`

El motor de brochas escribe directamente sobre `pixelSpan()` via QPainter, por eso el flag se activa en commit time (no en el hot path de setPixel/blendPixel).

### Resultado

`SequenceTrackWidget::frameHasContent()` → `rl->hasContent()` en O(1). Sin escaneo de pixeles en `paintEvent`.

### Waveform layout fix

`rebuildTracks()` añade `tracksLayout_->update()` sincrono despues de `addStretch()`, asegurando geometria correcta para `AudioTrackWidget::paintEvent()`.

---

## 13. FPS Bridge Centralizado — Pipeline Unidireccional (Jul 2026)

### Problema

El control de FPS estaba acoplado al `QSpinBox` via `onFPSChanged()` sin un bridge en `AppState`, a diferencia de `setWorkAreaStart/End/DurationFrames`. Los botones ± no existian.

### Solucion

| Componente | Cambio |
|-----------|--------|
| `AppState::setFps(fps)` | Bridge con guard `if (fps() == fps) return`. `emit documentChanged()`. |
| `fpsMinusBtn_` (−) | 20x22, conexion lambda: `appState_->setFps(currentFps - 1)` clamp `> 1` |
| `fpsPlusBtn_` (+) | Idem: `appState_->setFps(currentFps + 1)` clamp `< 120` |
| `documentChanged` listener | `if (fps_ != seqFps)` guard → sync spinbox + timer + `setPlaybackRate(fps/24.0)` |
| `onFPSChanged` | Simplificado a llamar solo `appState_->setFps(clamped)` |

### Flujo

```
fpsMinusBtn_/fpsPlusBtn_ → lambda → appState_->setFps(±1)
fpsSpin_                  → onFPSChanged → appState_->setFps(valor)
                                        ↓
                              AppState::setFps() → emit documentChanged()
                                        ↓
                  connect(documentChanged) → sync fpsSpin_, timer, playbackRate
```

### Commits

```
e4f30e8 feat: centralized FPS bridge + +/- buttons — unidirectional pipeline
089850c fix: O(1) frameHasContent + waveform layout sync + hasContent_ lifecycle
7319298 feat: Timeline v2.4 — Real FPS, Work Area In/Out, Duration Control, Transport Sync
```

---

## 14. UI Cleanup — FPS SpinBox Ghost Buttons + PropertyEditor Deduplication (Jul 2026)

### Problema

El `fpsSpin_` (QSpinBox) mostraba flechas nativas duplicando los controles `fpsMinusBtn_`/`fpsPlusBtn_`. Los botones ± no invocaban `setText()` explícito. El grupo SEQUENCE + DURATION en PropertyEditorV2 duplicaba información que ya reside en el Timeline.

### Solución

#### Timeline Panel (`timeline_panel_v2.cpp`)

- `fpsSpin_->setButtonSymbols(QAbstractSpinBox::NoButtons)` — elimina flechas nativas
- `fpsMinusBtn_->setText("−")` + `fpsPlusBtn_->setText("+")` — texto explícito, visible
- Botones conservan `setFixedSize(20, 22)` y estilo dark theme

#### PropertyEditor (`property_editor_v2.cpp`, `property_editor_v2.hpp`)

- Bloque `timelineGroup_` (SEQUENCE label + DURATION spinbox + separator + signal connect) comentado con `/* */`
- `refreshSequenceFields()` reducido a no-op (cuerpo vacío) — requerido por `main_window_v2.cpp:401`
- `durationFramesChanged` signal conservado — requerido por `main_window_v2.cpp:404` para compilación
- Miembros `timelineGroup_` y `durationSpin_` comentados en header

### Pipeline intacto

Ninguna lambda, conexión de `appState_->setFps()`, `documentChanged`, o `updatingFps_` modificada.

### Commits

```
e124074 fix: FPS spinbox NoButtons + explicit button text + remove duplicate SEQUENCE/DURATION from PropertyEditor
```

154/154 tests pass. Build: 0 errors, 0 warnings.
