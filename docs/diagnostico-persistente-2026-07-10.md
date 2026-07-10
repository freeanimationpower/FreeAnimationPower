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
| 7 | **Secuencia fantasma al reabrir** | **16-38-23** | `open_seq_count_2` consistente (no más 3) |
| 8 | Tracer sin `session_end` | 16-38-23 | `session_end` presente en ambos traces |

---

## 2. BUGS PENDIENTES

### 2.1 Frame 0 Vacío en Secuencia Duplicada
**Severidad**: ALTO
**Estado**: NO REPRODUCIDO en última sesión
**Archivos involucrados**: `core/sequence.cpp:clone()`, `ui_v2/timeline_panel_v2.cpp:frameHasContent`

**Evidencia en traces**:
- 16-38-23: `clone_frame` × 11, `cloned` presente. `frame_has_content` reporta frames 0,1,9,10.
- La secuencia clonada tiene 11 frames clonados. Falta confirmar si frame 0 está vacío.

**Hipótesis**: 
1. El clon copia `hasContent_` correctamente
2. El `SequenceTrackWidget` para la secuencia clonada puede tener `seqIndex_` incorrecto
3. O las capas clonadas tienen `visible_=false` o `locked_=true` que impiden la detección

**Puntos de diagnóstico**:
- `frameHasContent()` ya traza `frame_has_content` cuando encuentra contenido
- `Sequence::clone()` ya traza `clone_frame` por cada frame clonado
- `onDupTrack()` ya traza `duplicate` + `rebuild_after_dup`

### 2.2 Canvas Bloqueado — No Dibuja
**Severidad**: ALTO
**Estado**: NO REPRODUCIDO en últimas sesiones
**Archivos involucrados**: `canvas_widget_v2.cpp:mousePressEvent`

**Evidencia en traces**:
- 15-44-25 seq 165-176: mouse press/move sin `stroke begin/end`
- 15-56-09: sin eventos `stroke_blocked`
- 16-38-23: sin eventos `stroke_blocked`

**Hipótesis**: 
1. `activeRasterLayer()` retorna nullptr después de operaciones de secuencia
2. El `activeSequenceIndex` o `activeLayerIndex` quedan desincronizados
3. El canvas no recibe notificación de cambio de secuencia activa

**Puntos de diagnóstico**:
- `stroke_blocked_no_layer` y `stroke_blocked_locked` ya implementados
- Buscar estos eventos en futuros traces

### 2.3 Undo (Ctrl+Z) No Funciona Después de Fill
**Severidad**: ALTO
**Estado**: PARCIALMENTE CORREGIDO
**Archivos involucrados**: `canvas_widget_v2.cpp:PaintCommandV2`

**Fix implementado**: `PaintCommandV2::resolveLayer()` ahora busca la capa por `layerUid_` en todas las secuencias antes de caer en `layerAccessor_()`.

**Pendiente**: Probar con fill en capa diferente a la activa, luego undo.

### 2.4 Undo No Funciona Después de Eliminar Frames
**Severidad**: MEDIO
**Estado**: NO IMPLEMENTADO
**Archivos involucrados**: `timeline_panel_v2.cpp:deleteFrame`

**Causa**: `deleteFrame()` no crea `UndoCommand`. La eliminación de frames es irreversible.

### 2.5 Layer Rename Crash (Doble Delete QLineEdit)
**Severidad**: ALTO
**Estado**: NO CORREGIDO
**Archivos involucrados**: `layer_panel_v2.cpp:startRename`

**Causa**: `returnPressed` + `editingFinished` ambos conectados a `commitRename`. Qt dispara ambos al presionar Enter, resultando en doble `deleteLater()` sobre el mismo QLineEdit.

### 2.6 Video Export Totalmente Roto
**Severidad**: CRÍTICO
**Estado**: NO CORREGIDO
**Archivos involucrados**: `video_export.cpp:renderFrame`

**Causa**: `renderFrame()` ignora `frameIndex` y lee pixel data desde paths de archivo legacy. Produce frames en blanco.

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
