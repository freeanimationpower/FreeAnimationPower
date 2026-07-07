# Session Report: Timeline Panel v2.4 — Real FPS, Work Area & Duration Control

**Fecha**: 2026-07-04

---

## 1. Objetivo Original

Refactorizar la logica del Timeline para separar conceptualmente "Velocidad de Reproduccion (FPS)", "Area de Trabajo (Work Area)" y "Duracion Total de la Secuencia", emulando los estandares de la industria (Adobe Premiere).

---

## 2. Arquitectura Final

### 2.1 Extensiones del Modelo de Datos (`src/core/sequence.hpp`)

```cpp
int workAreaStart_ = 0;      // In point (frame index)
int workAreaEnd_ = 0;        // Out point (0 = usar totalFrames_ como fin)
int durationFrames_;         // Ancho total del timeline scrollable
```

Metodos clave:
- `effectiveWorkAreaEnd()` — `workAreaEnd_ > 0 ? min(workAreaEnd_, totalFrames_) : totalFrames_`
- `advanceFrame()` — avanza 1 frame dentro de los limites WA, wrappea a `workAreaStart_` al final. Qt-free, engine-level.
- `clone()` preserva los 3 nuevos campos.
- Constructor inicializa `durationFrames_ = totalFrames_`.

### 2.2 Pipeline de Estado Centralizado (`AppState`)

Todas las mutaciones pasan por bridges que emiten `documentChanged()`:

| Bridge | Accion |
|--------|--------|
| `setWorkAreaStart(frame)` | → `activeSequence().setWorkAreaStart(frame)` |
| `setWorkAreaEnd(frame)` | → `activeSequence().setWorkAreaEnd(frame)` |
| `setDurationFrames(count)` | → `activeSequence().setDurationFrames(count)` |

La UI nunca muta `Sequence` directamente — siempre a traves de `AppState`.

### 2.3 RulerWidget Interactividad

```cpp
enum class DragTarget { None, Playhead, WorkAreaIn, WorkAreaOut };
DragTarget currentDrag_ = DragTarget::None;
```

| Interaccion | Comportamiento |
|------------|----------------|
| Hover ±5px de borde WA | Cursor `SizeHorCursor` |
| Click ±5px de In/Out edge | Inicia drag del respectivo borde WA |
| Click en otra parte | Click de playhead + drag = scrubbing |
| Drag WA In edge | Redimension en tiempo real, clamp `[0, waEnd-1]` |
| Drag WA Out edge | Redimension en tiempo real, clamp `[waStart+1, totalFrames]` |
| Click durante playback | Auto-pausa playback + audio antes de mover playhead |

Barra de Work Area: fill 4px `QColor(255, 107, 74, 80)` con lineas delimitadoras 1px `QColor(255, 107, 74, 180)`.

### 2.4 Control de Duracion

- `SequenceTrackWidget::paintEvent()`: celdas renderizan hasta `durationFrames_` (vacios mas alla de `totalFrames_`)
- `updateScrollBarRange()`: scroll horizontal usa `durationFrames_` en vez de `totalFrames_ + 1`
- `PropertyEditorV2`: grupo SEQUENCE con spinbox DURATION (1-10000 fr), `refreshSequenceFields()`
- Boton `+` auto-expande `durationFrames_` cuando alcanza el limite

### 2.5 Sincronizacion de Audio y Transporte

| Disparador | Accion |
|-----------|--------|
| Play | `setPlaybackRate(fps/24.0)` + `syncToFrame()` + `play()` |
| Pause (||) | `playbackTimer_->stop()` + `player()->pause()` en todos los tracks |
| Stop (■) | `player()->stop()` + reset de posicion |
| Cambio de FPS | `setPlaybackRate(fps/24.0)` en todos los tracks, intervalo `1000/fps` |
| WA loop (looping=true) | `syncToFrame(waStart)` — re-sincroniza audio |
| WA end (looping=false) | Auto-stop: timer frena + `player()->pause()` + playhead en ultimo frame WA |
| Switch de secuencia | `setPlaybackRate(newFps/24.0)` en todos los tracks |

Guard: flag `updatingFps_` previene bucle de senales en `fpsSpin_` ↔ `onFPSChanged`.

### 2.6 Atajos de Teclado

| Tecla | Accion |
|-------|--------|
| `Shift+I` | Set Work Area In point al frame actual |
| `Shift+O` | Set Work Area Out point al frame actual + 1 |

La `I` sin Shift mantiene su funcion original de ColorPicker (estandar Photoshop/Krita).

---

## 3. Correccion de Regresiones

### 3.1 FPS Widget Trabe

**Sintoma**: QSpinBox de FPS solo permitia disminuir el valor.

**Diagnostico**: Posible bucle de senales `fpsChanged` ↔ `document().setFPS()` interfiriendo con la interaccion del spinbox.

**Solucion**: Flag `updatingFps_` como guardia anti-reentrada, clamp explicito `[1, 120]`.

### 3.2 Waveform "No waveform data"

**Sintoma**: La forma de onda dejo de dibujarse tras los cambios de layout.

**Diagnostico**: `refreshTimelineLayout()` llamaba `at->update()` sin forzar `tracksLayout_->update()` sincrono. Siguiendo la invariante v2.3: *"`tracksLayout_->update()` called before `widget->update()` — forces synchronous geometry before waveform repaint"*.

**Solucion**: Añadir `tracksLayout_->update()` antes de iterar los widgets de audio en `refreshTimelineLayout()`.

### 3.3 Audio Freewheeling (No se detenia)

**Sintoma**: Al pausar playback (boton || o auto-stop), el audio seguia reproduciendose.

**Diagnostico**: `playbackTimer_->stop()` detenia el timer visual pero `QMediaPlayer` (sistema independiente) quedaba en `PlayingState`.

**Solucion**: Iterar `audioTrackWidgets_` y llamar `player()->pause()` en:
- `onPlayPause()` rama de pausa
- `onPlaybackTick()` auto-stop cuando `!looping` y WA wrap
- RulerWidget `mousePressEvent` al hacer click en playhead durante playback (via `togglePlayback()`)

---

## 4. Archivos Modificados

| Archivo | Cambios | Lineas |
|---------|---------|--------|
| `src/core/sequence.hpp` | +3 miembros, +8 metodos + advanceFrame() | +13 |
| `src/core/sequence.cpp` | Implementacion metodos, update constructor/clone | +28 |
| `src/core/app_state.hpp` | +3 bridges (setWorkAreaStart/End, setDurationFrames) | +3 |
| `src/core/app_state.cpp` | Implementacion bridges | +15 |
| `src/ui_v2/timeline_panel_v2.hpp` | +appState(), +refreshTimelineLayout(), +isPlaying(), +updatingFps_ | +4 |
| `src/ui_v2/timeline_panel_v2.cpp` | RulerWidget WA drag + scrub, onPlaybackTick, updateScrollBarRange, SequenceTrackWidget cells, audio transport sync, waveform layout fix | +214/-17 |
| `src/ui_v2/canvas_widget_v2.cpp` | Shift+I/O shortcuts | +15 |
| `src/ui_v2/property_editor_v2.hpp` | SEQUENCE group + durationFramesChanged signal | +7 |
| `src/ui_v2/property_editor_v2.cpp` | Timeline group UI + refreshSequenceFields | +51 |
| `src/ui_v2/main_window_v2.cpp` | Signal wiring: durationFramesChanged + sequenceChanged | +9 |

**Total**: 10 archivos, +342/-17 lineas. 154/154 tests pass.

---

## 5. Commits

```
7319298 feat: Timeline v2.4 — Real FPS, Work Area In/Out, Duration Control, Transport Sync
b7c6410 docs: comprehensive v2.4 architecture docs
089850c fix: O(1) frameHasContent + waveform layout sync + hasContent_ lifecycle
```

---

## 6. Optimizacion O(1) de frameHasContent (Jul 2026)

### Problema

Las celdas iniciales del timeline (frames 0, 1, 2) se pintaban en `kCellFilled` (#2E333D) aunque no tuvieran contenido real. El `frameHasContent` original solo verificaba `root->layerCount() > 0`, lo cual era true incluso con la capa default vacia (sin pixeles dibujados).

### Solucion: Flag hasContent_ en RasterLayer

| Operacion | hasContent_ |
|-----------|-------------|
| `setPixel()` con alpha > 0 | true |
| `blendPixel()` con outA > 0 | true |
| `clear()` | false |
| `clone()` | preservado |
| `commitStroke()` / `doText()` / `commitMove()` / `commitFloatingSelection()` | true en commit time |

**Key invariant**: El motor de brochas escribe directamente sobre `pixelSpan()` via QPainter, sin pasar por `setPixel`/`blendPixel`. Por eso `hasContent_` se activa tambien al final de cada metodo de commit en `CanvasWidgetV2`.

### Resultado

`SequenceTrackWidget::frameHasContent()` lee `rl->hasContent()` en O(1). No hay escaneo de pixeles en el `paintEvent`.

### Waveform layout fix

`rebuildTracks()` ahora llama `tracksLayout_->update()` sincrono despues de `addStretch()`, igual que `refreshTimelineLayout()`. Esto garantiza que `AudioTrackWidget::paintEvent()` reciba el `width()` correcto para renderizar la onda.

---

## 7. FPS Bridge Centralizado (Jul 2026)

### Problema

El control de FPS estaba acoplado a `onFPSChanged()` sin un bridge en `AppState` como `setWorkAreaStart/End/DurationFrames`. Los botones ±FPS no existian.

### Solucion: Bridge setFps() + Botones ±

| Componente | Cambio |
|-----------|--------|
| `AppState::setFps(fps)` | Bridge con guard `if (fps() == fps) return`. Emite `documentChanged()`. |
| `fpsMinusBtn_` / `fpsPlusBtn_` | Botones ASCII `−`/`+` (20x22) flanqueando `fpsSpin_` en el transport bar. |
| Lambda `fpsMinusBtn_` | `appState_->setFps(currentFps - 1)` clamp `> 1` |
| Lambda `fpsPlusBtn_` | `appState_->setFps(currentFps + 1)` clamp `< 120` |
| `documentChanged` listener | `if (fps_ != seqFps)` sync: spinbox `setValue()` + timer interval + `setPlaybackRate(fps/24.0)` |
| `onFPSChanged` | Simplificado a 5 lineas: solo llama `appState_->setFps(clamped)` |

### Flujo unidireccional

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
b7c6410 docs: comprehensive v2.4 architecture docs
7319298 feat: Timeline v2.4 — Real FPS, Work Area In/Out, Duration Control, Transport Sync
```

---

### Estado final: Estable, listo para produccion.
