# Session Report: Timeline Panel v2.3 — Non-Destructive Rebuild & Free Audio Track Movement

**Fecha**: 2026-07-03

---

## 1. Bug Original: Use-After-Free + Audio Tracks Huérfanas

### Síntomas
- Al duplicar/crear/eliminar secuencias, las pistas de audio desaparecían visualmente
- Crash aleatorio al interactuar con el Timeline tras operaciones con secuencias
- El scroll vertical era invisible/insuficiente

### Causa raíz
`rebuildTracks()` aplicaba `delete item->widget()` sobre **todos** los widgets del `tracksLayout_`, incluyendo `AudioTrackWidget*`. Los punteros en `audioTrackWidgets_` quedaban colgados (dangling), y cualquier iteración posterior (`setCurrentFrame`, `onPlayPause`, `onStop`) causaba use-after-free.

```cpp
// CÓDIGO BUG (v2.2)
while ((item = tracksLayout_->takeAt(0)) != nullptr) {
    delete item->widget();  // ← BORRA AudioTrackWidgets!
    delete item;
}
```

---

## 2. Arquitectura Final (v2.3)

### Timeline Layout — estructura de widgets

```
┌──────────────────────────────────────────────────┐
│  [▶][⏹][◀][▶]  FPS [24]  1/24     [+ Track ▾]  │  ← Top bar (FIJO)
├──────────────────────────────────────────────────┤
│  RulerWidget (regla de frames)                   │  ← FIJO
├──────────────────────────────────────────────────┤
│ ┌ QScrollArea (ScrollBarAlwaysOn vertical) ────┐│
│ │ [SequenceTrackWidget 0]                       ││
│ │ [SequenceTrackWidget 1]                       ││
│ │ [AudioTrackWidget 0]   ← movido con ▲/▼      ││
│ │ [SequenceTrackWidget 2]                       ││
│ │ [stretch infinito]                            ││
│ └───────────────────────────────────────────────┘│
├──────────────────────────────────────────────────┤
│  ═══════ QScrollBar Horizontal ═══════════════  │  ← FIJO
└──────────────────────────────────────────────────┘
```

### Separación de dominios

| Dominio | Tipo | Gestión |
|---------|------|---------|
| Secuencias | `Sequence` en `Document` (Core) | `AppState::moveSequence` + `QTimer::singleShot(0, rebuildTracks)` |
| Audio | `AudioTrackWidget` en UI | `onMoveAudioTrack` con `takeAt/insertItem` directo (sin Core) |

### rebuildTracks() — flujo no-destructivo

```
1. clearFocus() — protege QLineEdit activos
2. tracksLayout_->removeWidget(at) — extrae audio tracks SIN borrar
3. tw->deleteLater() — destruye SOLO SequenceTrackWidget
4. trackWidgets_.clear()
5. Limpia QSpacerItem del layout
6. Crea nuevos SequenceTrackWidget desde Document
7. Reinserta AudioTrackWidget preservados
8. addStretch() — espaciador infinito hacia abajo
```

### onMoveAudioTrack() — movimiento libre

```
1. Encuentra posición actual del widget en tracksLayout_
2. Calcula newPos = oldPos + delta
3. trackLayout_->takeAt(oldPos)
4. Null-check: if (item) insertItem(newPos, item)
5. trackLayout_->update() — fuerza recálculo de geometrías
6. widget->positionHeader() + widget->update() — repintado correcto
```

### Waveform fix

El movimiento con `takeAt/insertItem` dejaba el widget con `width()=0` antes del primer repintado, causando que `drawWaveform` no renderizara. La solución: `tracksLayout_->update()` síncrono antes de `widget->update()`.

---

## 3. Defensas de Memoria (3 niveles preservados)

| Nivel | Mecanismo | Archivo |
|-------|-----------|---------|
| 1 | `MainWindowV2::sequenceChanged` actualiza inline (no llama rebuild) | `main_window_v2.cpp:399` |
| 2 | `QTimer::singleShot(0, rebuildTracks)` difiere ejecución | `timeline_panel_v2.cpp:930` |
| 3 | `QApplication::focusWidget()->clearFocus()` antes de remover widgets | `timeline_panel_v2.cpp:625` |
| 4 | `if (item)` guard en takeAt/insertItem | `timeline_panel_v2.cpp:976` |
| 5 | `std::max(0, count()-1)` en insertWidget | `timeline_panel_v2.cpp:943` |

---

## 4. Archivos Modificados

| Archivo | Cambios | Líneas |
|---------|---------|--------|
| `audio_track_widget.hpp` | +Q_OBJECT, +signals, +upBtn_/downBtn_, +setTrackIndex | +8 |
| `audio_track_widget.cpp` | +button creation, +positionHeader 4-buttons, +QIcon | +29 |
| `timeline_panel_v2.hpp` | +onMoveAudioTrack, -separator_, -updateSeparator, -kSeparatorHeight | ±3 |
| `timeline_panel_v2.cpp` | rebuildTracks rewrite, onMoveAudioTrack, onImportAudio signals, removeAudioTrack reindex, audio repaints, scrollbar AlwaysOn | +71/-9 |

**Total**: +111, -9 líneas. 154/154 tests pass.

---

## 5. Commits

```
844f1c2 refactor: non-destructive timeline rebuild + free audio track movement (v2.3)
```

### Estado final: Estable, listo para producción.
