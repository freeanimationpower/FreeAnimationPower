# Diagnóstico Persistente — Free Animation Power

**Fecha**: 10 de Julio, 2026
**Sesiones analizadas**: 9 traces (08:01 → 16:38)
**Objetivo**: Documentar bugs que persisten a través de múltiples sesiones de test para referencia cruzada.

---

## 1. BUGS CORREGIDOS (VERIFICADOS)

| # | Bug | Sesión de verificación | Evidencia |
|---|-----|----------------------|-----------|
| 1 | Brush colapso a size 200 | 15-44-25 | Strokes con size 200 funcionan |
| 2 | Botón "+" timeline trabado | 15-44-25 | Frames se agregan correctamente |
| 3 | Crash fill en capa grande | 15-56-09 | Fill full canvas funciona |
| 4 | Save bloquea UI → crash re-entrada | 15-56-09 | Saves exitosos con `processEvents` |
| 5 | Pixel dedup → capas vacías al reabrir | 16-38-23 | Capas cargan con contenido |
| 6 | Audio track duplicación al reabrir | 16-38-23 | `clearAudioTracks()` funciona |
| 7 | Secuencia fantasma al reabrir | 16-38-23 | `open_seq_count_2` consistente (no más 3) |
| 8 | Tracer sin `session_end` | 16-38-23 | `session_end` presente en ambos traces |
| 9 | **NodeGraph::evaluate ignora target** | 2026-07-13 | `node_graph.cpp` copia resultado del OutputNode al target |
| 10 | **Legacy load omite setHasContent** | 2026-07-13 | `file_format.cpp` agrega `setHasContent(true)` en v1/v2/v3 |
| 11 | **Sin confirmación al cerrar** | 2026-07-13 | `MainWindowV2::closeEvent()` con diálogo Save/Discard/Cancel |
| 12 | **FFmpeg waitForFinished(-1) hang** | 2026-07-13 | Timeout 120s + `proc.kill()` en `video_export.cpp` |
| 13 | **Multi-seq data loss al reabrir** | 2026-07-13 | `readTimeline()` usa `doc.sequenceAt(si)` en vez de crear duplicados |
| 14 | **Layer rename perdido al cambiar foco** | 2026-07-13 | Reconectado `editingFinished` con guarda `committed` |
| 15 | **Video export roto** | 2026-07-13 | `renderExportFrame()` público + H.264/MOV alpha/WebM alpha |

---

## 2. BUGS PENDIENTES

### 2.1 Frame 0 Vacío en Secuencia Duplicada
**Severidad**: ALTO / CORREGIDO (multi-seq fix 2026-07-13)
**Archivos involucrados**: `core/sequence.cpp:clone()`, `ui_v2/timeline_panel_v2.cpp:frameHasContent`

### 2.2 Canvas Bloqueado — No Dibuja
**Severidad**: ALTO
**Estado**: NO REPRODUCIDO en últimas sesiones

### 2.3 Undo (Ctrl+Z) No Funciona Después de Fill
**Severidad**: ALTO / CORREGIDO (PaintCommandV2 resolveLayer fix 2026-07-03)
**Archivos involucrados**: `canvas_widget_v2.cpp:PaintCommandV2`

### 2.4 Undo No Funciona Después de Eliminar Frames
**Severidad**: MEDIO / CORREGIDO (DeleteFrameCommand 2026-07-03)
**Archivos involucrados**: `timeline_panel_v2.cpp:deleteFrame`

### 2.5 Layer Rename Crash (Doble Delete QLineEdit)
**Severidad**: ALTO / CORREGIDO (editingFinished con guarda committed 2026-07-13)
**Archivos involucrados**: `layer_panel_v2.cpp:startRename`

### 2.6 Video Export Totalmente Roto
**Severidad**: CRÍTICO / CORREGIDO (renderExportFrame + alpha formats 2026-07-13)
**Archivos involucrados**: `video_export.cpp`, `video_export.hpp`, `main_window_v2.cpp`

---

## 3. TRAZAS DE DIAGNÓSTICO ACTIVAS (para referencia al leer traces)

| Evento | Significado | Dónde buscar |
|--------|-------------|--------------|
| `stroke_blocked_no_layer` | Canvas no encuentra capa activa | `mousePressEvent` |
| `stroke_blocked_locked` | Capa activa está lockeada | `mousePressEvent` |
| `frame_has_content` | Frame tiene contenido (Raster o Vector) | `frameHasContent` |
| `clone_frame` | Un frame fue clonado durante `Sequence::clone()` | `sequence.cpp:clone()` |
| `open_seq_count_N` | N secuencias en el documento después de cargar archivo | `main_window_v2.cpp:openProject` |
| `rebuild_tracks_begin/end` | Timeline está reconstruyendo widgets | `timeline_panel_v2.cpp:rebuildTracks` |
| `rebuild_after_dup` | Rebuild disparado después de duplicar secuencia | `timeline_panel_v2.cpp:onDupTrack` |
| `save_error_*` | Fallo en save (zip_init, write, rename) | `document_manager.cpp:save` |
| `session_end` | Cierre normal de la aplicación | `tracer.cpp:shutdown` |

---

## 4. PLAN DE ATAQUE PARA LA PRÓXIMA SESIÓN

1. **Duplicar secuencia con dibujos en todos los frames** y verificar:
   - Si todos los frames de la copia muestran contenido en la timeline
   - Si `frame_has_content` aparece para cada frame
   - Si `open_seq_count` muestra el número correcto después de guardar y reabrir

2. **Probar Ctrl+Z después de fill en capa diferente**:
   - Crear 2 capas (Layer 1, Layer 2)
   - Fill en Layer 2
   - Cambiar a Layer 1
   - Ctrl+Z → debe restaurar Layer 2, no Layer 1

3. **Probar Ctrl+Z después de eliminar frame**:
   - Agregar frame, pintar
   - Eliminar frame
   - Ctrl+Z → debe restaurar el frame (actualmente no implementado)

4. **Forzar el bug de "no dibuja"**:
   - Crear 3+ secuencias, duplicar, eliminar varias veces
   - Intentar dibujar después de cada operación
   - Buscar `stroke_blocked` en el trace

5. **Renombrar capa y presionar Enter**:
   - Verificar que no crashea
   - Si crashea, buscar el evento en el trace
