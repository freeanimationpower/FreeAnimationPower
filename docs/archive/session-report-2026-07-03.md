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

## 5. Waveform Decoding Odyssey (Jul 2026)

### Bug: "No waveform data" persistente

La forma de onda nunca se dibujaba al importar archivos MP3/WAV/FLAC. El texto "No waveform data" aparecía indefinidamente.

### Root cause chain

1. **Forced format rejection** — `decoder->setAudioFormat(Int16, 44100, mono)` en `decodeAudio()` forzaba al backend WMF de Windows a convertir MP3 a Int16. WMF rechazaba la conversión → el decoder nunca emitía `bufferReady` → `waveformPicks_` vacío permanente.

2. **Native Float32 reinterpretation** — El codec MP3 nativo entrega Float32. El código original interpretaba los bytes como `qint16` via `buffer.constData<qint16>()`. Los 16 bits bajos de un float de audio típico (magnitud < 0.01) son ~0 → peaks de 0.0 → drawWaveform invisible.

3. **Silent format rejection** — El `else { return; }` en el `bufferReady` lambda abortaba silenciosamente cualquier formato no reconocido (Int32 de WAV 24-bit, UInt8 de WMF). El `finished()` se emitía igual → `decoded_ = true` → "No waveform data".

### Solucion final (5 iteraciones)

**Iteracion 1**: `w <= hdrW` guard en `drawWaveform` — protege division por cero durante `takeAt/insertItem`.

**Iteracion 2**: 5-branch format handler (Float, Int32, Int16, UInt8, fallback 0.2f) reemplaza reinterpretacion ciega de Int16.

**Iteracion 3**: `paintEvent` 3-ramas (`!decoded_` / `waveformPicks_.empty()` / `drawWaveform`) con mensaje "No waveform data" visual.

**Iteracion 4**: Eliminado `decoder->setAudioFormat(format)` — el decoder usa formato nativo del backend. WMF entrega Float32 para MP3, FFmpeg entrega Float32, DirectSound entrega Int16.

**Iteracion 5**: Instrumentacion con `qDebug` en `bufferReady`, `finished`, `error` + flag `decodeError_` + mensaje "Decoder error" en `paintEvent`.

### Arquitectura final de decodificacion

```
decodeAudio()
  → QAudioDecoder (sin formato forzado)
  → bufferReady lambda (5-branch handler)
     ├─ Float   → std::abs(samples[i])
     ├─ Int32   → abs / 2^31
     ├─ Int16   → abs / 2^15
     ├─ UInt8   → (sample - 128) / 128
     └─ else    → peak = 0.2f (visible fallback)
  → waveformPicks_.push_back(peak)
  → update() cada 5 buffers
  → finished / error → decoded_ = true
```

### Commits del waveform fix

```
0c88917 fix: guard drawWaveform against width <= headerWidth
792c7a6 fix: decode audio in native buffer format (Float for MP3)
dbafa35 fix: bufferReady handles all 4 Qt6 native formats
719baaa fix: remove forced setAudioFormat(Int16) — let decoder use native format
614ff00 debug: instrument bufferReady/finished/error with qDebug + decodeError_
```

### Estado final

Build: 0 errors. Tests: 154/154 pass. Waveform: visible en todos los formatos (MP3, WAV, FLAC, OGG) en Windows + WMF backend.

---

## 6. Commits

```
844f1c2 refactor: non-destructive timeline rebuild + free audio track movement (v2.3)
```

### Estado final: Estable, listo para producción.
