# Session Report: Multi-Sequence Architecture — FAP v2.1

**Fecha**: 2026-07-02 (ampliado 2026-07-03)

---

## 1. Arquitectura de Secuencias

### Problema
El sistema original tenia un unico Timeline acoplado al Document, con un solo UndoManager.
No permitia multiples lineas de tiempo independientes.

### Solucion
Creacion de la clase `Sequence` que encapsula:
- Frame data (`map<int, unique_ptr<GroupLayer>>`)
- UndoManager aislado por secuencia (128 entries max)
- Estado de playback (currentFrame, totalFrames, fps, playing, looping)
- Keyframes matrix
- Visibilidad (bool visible_)

`Document` ahora orquesta un `vector<unique_ptr<Sequence>>` con indice activo.
Todos los metodos legacy de Document delegan a `activeSequence()`.

### Archivos clave
| Archivo | Rol |
|---|---|
| `src/core/sequence.hpp/cpp` | Clase Sequence (timeline + frames + undo + visibilidad) |
| `src/core/document.hpp/cpp` | Orquestador de secuencias con delegados backward-compat |
| `src/core/app_state.hpp/cpp` | Sin Timeline, conectado a activeSequence via wireSequenceSignals |
| `src/engine/animation/timeline_engine.cpp` | Opera sobre `Sequence&` externo (sin estado duplicado) |

### Undo Aislado
Cada `Sequence` tiene su propio `UndoManager`. `Document::undoManager()` delega a `activeSequence().undoManager()`. Al cambiar de secuencia, el historial cambia automaticamente.

### Deep Copy (Clone)
`Sequence::clone()` usa `GroupLayer::clone()` recursivo que ya regenera UIDs via `Layer()` constructor. `visible_` se preserva en la copia.

---

## 2. Pipeline de Renderizado Multi-Sequence (Efecto Acetato)

### Problema
El `buildBackgroundCache` solo componia capas de la secuencia activa. Las demas secuencias eran invisibles en el canvas.

### Solucion
Los 3 bucles de composicion en `canvas_widget_v2.cpp` ahora iteran TODAS las secuencias:

```cpp
for (size_t si = 0; si < doc.sequenceCount(); ++si) {
    if (!doc.sequenceAt(si).visible()) continue;
    const auto& root = doc.sequenceAt(si).rootLayerForFrame(currentFrame_);
    for (size_t li = 0; li < root.layerCount(); ++li) { /* compositar */ }
}
```

Secuencia 0 = fondo, secuencia N = capa superior (SourceOver).

### Ubicaciones modificadas en `canvas_widget_v2.cpp`
- `buildBackgroundCache` full rebuild
- `buildBackgroundCache` partial rebuild
- `render()` export

### Rendimiento
Sin impacto. `strokeBuffer_` sigue componiendose solo contra la capa activa.
El multi-sequence overlay se cocina en `backgroundCache_` (cache valida hasta frame/sequence/layer change).

### Visibilidad
`Sequence::visible_` (bool, default true) permite ocultar secuencias del render.
Añadido a `sequence.hpp` (+3 lineas) y verificado en cada bucle de canvas.

---

## 3. UI Multi-Pista (TimelinePanelV2)

### Evolucion
1. **V1**: QComboBox selector de secuencias (desechado)
2. **V2**: Multi-pista con SequenceTrackWidgets apilados en QScrollArea
3. **V3**: Rediseno compacto NLE con QLineEdit permanente

### Diseno V3 (Actual)

```
┌──────────────────────────────────────────────────────────────┐
│ [<] [>] [||] [■]  FPS [24]  1/24                 [+ Track]  │ Top bar
├──────┬───────────────────────────────────────────────────────┤
│Ruler │ 1    5    10   15   20   24                           │ RulerWidget (18px)
│      │ ▼                                                     │
├──────┼───────────────────────────────────────────────────────┤
│ █S1  │ ▓▓ ▓▓ ░░ ▓▓ ▓▓ ░░ ░░ ▓▓ ░░ ░░ ░░ ░░      [+]       │ Track 28px (activa)
├──────┼───────────────────────────────────────────────────────┤
│  S2  │ ░░ ░░ ▓▓ ▓▓ ░░ ░░ ░░ ░░ ▓▓ ░░ ░░ ░░ ░░  [+]        │ Track 28px
└──────┴───────────────────────────────────────────────────────┘
        ◄══════════════▓████══════════════════════►  scrollbar
```

### Constantes
| Parametro | Valor |
|---|---|
| Header width | 120px |
| Track height | 28px |
| Cell width | 32px |
| Cell spacing | 1px |
| Ruler height | 18px |

### SequenceTrackWidget V3
- Header 120px: QLineEdit permanente + botones dup/del (hover)
  - QLineEdit: transparente, 8px Inter, editingFinished + returnPressed
  - Botones: Unicode ⚌ (dup) y ✕ (del), visibles solo en hover
- Body: celdas con color solido (▓▓ contenido, ░░ vacia). Sin thumbnails
  - Colores: kCellFilled #2E333D, kCellEmpty #161920, borde #1E2128
- Playhead: linea vertical naranja 1px en track activa
- frameHasContent corregido: usa `sequenceAt(seqIndex_)` en vez de `activeSequence()`

### Sincronizacion de Scroll
`QScrollBar` padre → `sharedScrollOffset_` → `update()` en ruler + todos los tracks.

### Colores del tema
```
kTrackBg = #121418, kTrackActiveBg = #191D26, kTrackHoverBg = #161A22
kHeaderBg = #0D1014, kHeaderActiveBg = #11131B
kBorderColor = #1E2128, kAccent = #FF6B4A
```

---

## 4. Bugs Corregidos

### Use-After-Free (critico)
**Sintoma**: App se cerraba al hacer click en pista para activarla.
**Causa**: `onActivateTrack` → `sequenceChanged` → `MainWindowV2::rebuildTracks()` borraba el SequenceTrackWidget mientras ejecutaba `mousePressEvent`.
**Fix** (3 niveles):
- `MainWindowV2::sequenceChanged` ya no llama `rebuildTracks()`. Los tracks se actualizan inline via `setActive()`
- `onDupTrack`/`onDelTrack`/`onNewSequence` usan `QTimer::singleShot(0, ...)` para diferir el rebuild
- `rebuildTracks()` llama `QApplication::focusWidget()->clearFocus()` antes de borrar widgets

### frameHasContent usaba secuencia incorrecta
**Fix**: Cambiado de `activeSequence()` a `sequenceAt(seqIndex_)`.

### Pre-existing canvas bugs (3)
- `caretX` undeclared (duplicate code block eliminado en paintEvent)
- `middlePanning_` → `panning_`
- `TextEntry` sin `undoImage`/`undoRect`

---

## 5. Tests

150/150 tests pasan. Nuevos tests para:
- `DocumentTest`: 10 tests (secuencias, delegates, activeSequence)
- `SequenceTest`: 11 tests (estado, frames, keyframes, clone, undo aislado, shiftFrameData, isolated undo)

---

## 6. Archivos Modificados (Resumen)

| Archivo | Accion |
|---|---|
| `src/core/sequence.hpp` | NUEVO — clase Sequence |
| `src/core/sequence.cpp` | NUEVO — impl + clone + UID regen |
| `src/core/layer.hpp` | +regenerateUid() |
| `src/core/document.hpp` | REESCRITO — secuencias + delegados |
| `src/core/document.cpp` | REESCRITO — gestion de secuencias |
| `src/core/timeline.hpp` | ELIMINADO |
| `src/core/timeline.cpp` | ELIMINADO |
| `src/core/app_state.hpp` | REESCRITO — sin Timeline |
| `src/core/app_state.cpp` | REESCRITO — delega a Sequence |
| `src/engine/animation/timeline_engine.cpp` | REESCRITO — Sequence& |
| `src/ui_v2/canvas_widget_v2.cpp` | 3 bucles multi-seq + 3 bugfixes |
| `src/ui_v2/canvas_widget_v2.hpp` | +TextEntry campos |
| `src/ui_v2/timeline_panel_v2.hpp` | Diseño multi-pista compacto |
| `src/ui_v2/timeline_panel_v2.cpp` | SequenceTrackWidget + RulerWidget + UAF fixes |
| `src/ui_v2/main_window_v2.cpp` | Sin maxHeight, sequenceChanged simplificado |
| `tests/test_document.cpp` | REESCRITO — 20 tests |
| `CMakeLists.txt` | -timeline +sequence |
