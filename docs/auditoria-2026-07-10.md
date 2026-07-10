# Auditoría Completa — Free Animation Power

**Fecha**: 10 de Julio, 2026
**Versión**: v2.5.1
**Estado**: 154/154 tests pasan, software funcional con bugs de detalle

---

## Resumen Ejecutivo

Se realizó una auditoría exhaustiva de las 4 áreas principales del código (core, engine, UI, IO). Se identificaron **8 bugs críticos**, **10+ bugs altos**, y **20+ bugs medios/bajos**. El software es funcional en su flujo principal, pero tiene problemas de integridad de datos en exportación, undo, y persistencia.

### Distribución por severidad

| Severidad | Cantidad | Impacto |
|-----------|----------|---------|
| Crítico | 8 | Data loss, crashes, funcionalidad rota |
| Alto | 12 | Comportamiento incorrecto, memory leaks |
| Medio | 15 | Edge cases, UX degradada |
| Bajo | 12 | Code quality, hygiene |

---

## 1. BUGS CRÍTICOS

### 1.1 Video Export: `renderFrame()` completamente roto
**Archivo**: `src/io/video_export.cpp:18-48`
**Impacto**: Toda exportación de video (MP4, GIF, PNG sequence) produce frames en blanco.

- `renderFrame()` ignora el parámetro `frameIndex` y siempre usa `doc.rootLayer()` (frame 0)
- Construye paths de archivo legacy (`frames/L{li}_{frame}.png`) en vez de leer `RasterLayer::pixelData()` de memoria
- Las secuencias son completamente ignoradas

**Fix**: Reescribir `renderFrame` para componer los `pixelData()` reales para el frame dado.

### 1.2 Pixel Dedup: Capas compartidas cargan vacías
**Archivo**: `src/io/document_manager.cpp:636-688`
**Impacto**: Al guardar con capas que comparten pixel buffer (clonadas), al reabrir el .fap las capas clonadas aparecen vacías.

- El save path detecta buffers compartidos por `shared_ptr::use_count()` y escribe `"shared_pixels": true`
- El load path NUNCA lee el flag `shared_pixels` — siempre intenta cargar PNG
- Como el PNG no existe en el ZIP (fue omitido en save), `mz_zip_reader_extract_file_to_heap` retorna null
- La capa se crea con buffer default (todo transparente)

**Fix**: `readLayerData` debe trackear buffers ya cargados y copiarlos cuando `shared_pixels == true`.

### 1.3 Undo aplica a capa equivocada
**Archivo**: `src/ui_v2/canvas_widget_v2.cpp:2140-2142`
**Impacto**: Ctrl+Z puede modificar la capa incorrecta si el usuario cambió de capa activa entre el stroke y el undo.

- `PaintCommandV2` almacena `layerUid_`, `frame_`, y `layerIndex_` pero NUNCA los usa
- `applySnapshot()` llama a `layerAccessor_()` que retorna `activeRasterLayer()` — la capa activa actual
- Si el usuario dibujó en Layer 1, cambió a Layer 2, y presiona Ctrl+Z, el undo se aplica a Layer 2

**Fix**: Usar `layerUid_` para buscar la capa correcta vía `GroupLayer::layerByUid()`.

### 1.4 Use-after-free en lambda de undo
**Archivo**: `src/ui_v2/canvas_widget_v2.cpp:2140`
**Impacto**: Crash si el canvas se destruye antes que el UndoManager.

- La lambda del undo captura `[this]` raw de `CanvasWidgetV2`
- `UndoManager` vive en `Document`, que vive en `AppState` (shared_ptr)
- Si `AppState` sobrevive al `CanvasWidgetV2`, llamar undo/redo invoca `activeRasterLayer()` en objeto destruido

**Fix**: Usar `std::weak_ptr<AppState>` o limpiar el UndoManager en el destructor del canvas.

### 1.5 Layer Rename: Doble delete del QLineEdit
**Archivo**: `src/ui_v2/layer_panel_v2.cpp:437-451`
**Impacto**: Crash al presionar Enter después de renombrar una capa.

- `returnPressed` y `editingFinished` están ambos conectados a `commitRename`
- En Qt, `returnPressed` dispara internamente `editingFinished`
- Resultado: dos invocaciones de `commitRename`
- Primera: `editor->deleteLater()` + `removeWidget(editor)`
- Segunda: opera sobre widget ya marcado para delete → use-after-free

**Fix**: Conectar solo a `returnPressed` o usar flag `bool committed_` de guardia.

### 1.6 `saveDocument()` usa formato legacy en vez de ZIP
**Archivo**: `src/io/document_io.cpp:98`
**Impacto**: Si algún código llama a `saveDocument()` en vez de `DocumentManager::save()`, el archivo se guarda en formato de directorios legacy (incompatible con el .fap actual).

**Fix**: Redirigir `saveDocument()` a `DocumentManager::save()` o eliminar la función legacy.

### 1.7 Doble emisión de `frameChanged`
**Archivo**: `src/ui_v2/timeline_panel_v2.cpp:948-979`
**Impacto**: Cada cambio de frame dispara dos ciclos de repaint (canvas + layer panel + status bar).

- `setCurrentFrame()` emite `frameChanged`
- `onPrevFrame()`, `onNextFrame()`, `onPlaybackTick()` también emiten `frameChanged` después de llamar a `setCurrentFrame()`
- Slots conectados se ejecutan 2x por cada frame

**Fix**: Eliminar las emisiones redundantes en los callers.

### 1.8 Audio decode bloquea UI thread
**Archivo**: `src/ui_v2/audio_track_widget.cpp:131`
**Impacto**: Al importar audio, el UI se congela completamente durante la decodificación.

- `decodeAudio()` es síncrono y se llama en el constructor de `AudioTrackWidget`
- Para archivos de audio grandes, el event loop queda bloqueado cientos de ms
- El mensaje "Decoding audio..." nunca se renderiza porque el loop está bloqueado

**Fix**: Mover la decodificación a `QTimer::singleShot(0, ...)` o `QtConcurrent::run`.

---

## 2. BUGS ALTOS

### 2.1 Deformation Mesh: Error de interpolación bilineal
**Archivo**: `src/engine/deformation/deformation_mesh.cpp:89`
```cpp
+ p01.y * (1 - fx) * fy + p11.y * fy * fy;  // BUG: fy*fy debe ser fx*fy
```
La coordenada Y usa `fy^2` en vez de `fx*fy` para la esquina p11, distorsionando la deformación.

### 2.2 NodeGraph::evaluate ignora el parámetro target
**Archivo**: `src/engine/compositor/node_graph.cpp:376-383`
Los resultados del grafo de nodos se computan pero nunca se escriben al `target`. El parámetro es completamente ignorado.

### 2.3 `hasContent_` no se restaura en undo
**Archivo**: `src/ui_v2/canvas_widget_v2.cpp:71-100`
`applySnapshot()` copia datos de píxeles pero nunca actualiza `layer->hasContent_`. Tras undo que borra todo el contenido, la celda del timeline sigue mostrando que tiene contenido.

### 2.4 `extractAudio()` siempre retorna true
**Archivo**: `src/io/document_manager.cpp:726`
Aunque todos los archivos de audio fallen al extraerse, la función reporta éxito. Archivos corruptos o faltantes en el ZIP se aceptan silenciosamente.

### 2.5 Audio temp dir nunca se limpia
**Archivo**: `src/io/document_manager.cpp:695-697`
Cada apertura de proyecto crea `%TEMP%/fap_audio_<PID>/`. Nunca se elimina. Acumulación indefinida en disco.

### 2.6 Colisión de nombres de audio
**Archivo**: `src/io/document_manager.cpp:711-718`
Dos pistas con el mismo `displayName` (ej. "song.mp3") se sobreescriben — ambas apuntan al mismo archivo.

### 2.7 Floating selection: undo commands no-op
**Archivo**: `src/ui_v2/canvas_widget_v2.cpp:383-398`
Cortar/borrar floating selection crea un undo command donde before == after (el pixel buffer no cambia, solo se limpia `floatingSelection_` en memoria del canvas).

### 2.8 Undo fragmentado en edición de texto
**Archivo**: `src/ui_v2/canvas_widget_v2.cpp:2811-2834`
Editar texto existente produce 2 entradas de undo separadas (clearTextRaster + doText). Ctrl+Z parcial deja estado inconsistente.

### 2.9 Legacy load omite `setHasContent(true)`
**Archivo**: `src/io/file_format.cpp:232-284`
Cargar archivos legacy (v1/v2/v3) copia píxeles PNG pero nunca llama a `setHasContent(true)`. Todas las capas reportan `hasContent() == false`.

### 2.10 Pixel format inconsistency: ARGB vs BGRA
| Fuente | Formato |
|--------|---------|
| `blend_utils.hpp` | ARGB (R en byte 0) |
| `raster_stroke.cpp` | BGRA (B en byte 0) |
| `onion_skin.cpp` | `uint8_t[4]` RGBA |
| Qt `QImage::Format_ARGB32` | BGRA (little-endian) |

Riesgo de swap R↔B en bordes entre subsistemas.

### 2.11 Sin advertencia al cerrar con cambios sin guardar
**Archivo**: `src/ui_v2/main_window_v2.cpp`
No hay `closeEvent()` override. Cerrar con Ctrl+W o X del título pierde cambios sin confirmación.

### 2.12 FFmpeg `waitForFinished(-1)` — hang infinito
**Archivo**: `src/io/video_export.cpp:64`
Timeout infinito. Si FFmpeg se cuelga, la aplicación se congela para siempre.

---

## 3. BUGS MEDIOS

### 3.1 Atomic rename no es atómico en Windows
**Archivo**: `src/io/document_manager.cpp:211-227`
Delete + Rename tiene una ventana donde el archivo no existe. Crash en ese momento = datos perdidos.

### 3.2 `busy_` flag sin exception safety
**Archivo**: `src/io/document_manager.cpp:229-293`
Si una excepción ocurre durante save, `busy_` queda en true para siempre. Sin RAII guard.

### 3.3 ViewState data race durante save
**Archivo**: `src/io/document_manager.cpp:117-121, 237`
El mutex se libera antes de leer `viewState_` para serializar. Carrera de datos con setters desde UI thread.

### 3.4 Layer path padding a solo 2 dígitos
**Archivo**: `src/io/document_manager.cpp:188`
100+ capas por frame causan colisión de paths (`layer_00` vs `layer_100`).

### 3.5 Legacy v1: layer index hardcodeado a L0
**Archivo**: `src/io/file_format.cpp:314-315`
Archivos v1 con múltiples capas cargan todas con los mismos píxeles.

### 3.6 Version field ignorado en load
**Archivo**: `src/io/document_manager.cpp:147-148`
Archivos .fap v3 con nuevos campos se interpretan como v2 sin warning.

### 3.7 Archivos de audio faltantes: silencio en save
**Archivo**: `src/io/document_manager.cpp:444-461`
Si el archivo de audio fue borrado entre import y save, el save procede sin error y el audio se pierde del archive.

### 3.8 `.fap` files mal ruteados a loader legacy
**Archivo**: `src/io/document_io.cpp:33-35`
Un `.fap` se detecta como `FAP`, se pasa a `loadFAP()` que espera un directorio, y falla con mensaje confuso.

### 3.9 `commitRename` captura punteros dangling
**Archivo**: `src/ui_v2/layer_panel_v2.cpp:437`
Lambda captura raw pointers a editor/nameLabel/hlay. Si la fila se destruye entre signal y delivery, todos danglean.

### 3.10 Ctrl+L shortcut ambiguo en LayerLockButton
**Archivo**: `src/ui_v2/layer_lock_button.cpp:36`
Cada instancia de `LayerLockButton` registra Ctrl+L. Qt resuelve al primero en tab-order, no a la capa seleccionada.

### 3.11 `setColor()` emite aunque color no cambió
**Archivo**: `src/ui_v2/toolbox_panel_v2.cpp:403-406`
Sin guardia, causa cadena de repaints innecesarios.

### 3.12 Onion skin slider sin debounce
**Archivo**: `src/ui_v2/toolbox_panel_v2.cpp`
Cada tick del slider dispara `invalidateBackgroundCache()` + `update()` en el canvas.

### 3.13 Font picker bloquea con popup->exec()
**Archivo**: `src/ui_v2/property_editor_v2.cpp:1124`
`QMenu::exec()` anida event loop. QLineEdit embebido puede no recibir foco correctamente.

### 3.14 `RulerWidget::mouseReleaseEvent` usa `QCursor::pos()`
**Archivo**: `src/ui_v2/timeline_panel_v2.cpp:243`
Query global de cursor en vez de posición del evento. Inexacto si el mouse salió del widget.

### 3.15 `FrameViewerData::is_keyframe` nunca se setea
**Archivo**: `src/engine/animation/frame_thumbnail.hpp`
El campo existe en `FrameInfo` pero siempre es false.

---

## 4. BUGS BAJOS

1. **`beforeSnapshot_` capturado pero nunca usado** — `canvas_widget_v2.cpp:1253`, dead code
2. **Indentación engañosa en `refreshFields`** — `property_editor_v2.cpp:623`, 4 hide()s visualmente anidados
3. **`#include <limits>` duplicado** — `canvas_widget_v2.cpp:16,21`
4. **3 archivos .cpp que son headers** — `timeline_engine.cpp`, `playback.cpp`, `onion_skin.cpp` empiezan con `#pragma once`
5. **`paper_texture.cpp` sin header** — clase inaccesible desde otras TUs
6. **Funciones de serialización duplicadas** — `file_format.cpp` y `serialization_common.cpp`
7. **`AudioEngine` no usa `start_frame`** — `src/engine/animation/audio_engine.cpp:96-103`
8. **`BreakdownPoseEngine::bias_` nunca leído** — dead member
9. **`connect` con raw pointer de shared_ptr** — `main_window_v2.cpp`, frágil
10. **SVG export: base64 PNG inline puede ser enorme** — sin límite de tamaño
11. **SVG close-path detection con epsilon frágil** — `svg_export.cpp:61`
12. **`ffmpegAvailable()` imprime versión a consola** — `video_export.cpp:52`

---

## 5. PROBLEMAS DE DISEÑO

### 5.1 Sin undo para operaciones estructurales
No hay soporte de undo para:
- `addLayer()`, `deleteLayer()`, `duplicateLayer()`, `moveLayer()`
- `addFrame()`, `deleteFrame()`, `duplicateFrame()`
- Cambios de visibilidad, lock, blend mode, opacidad, renombre de capa

`deleteLayer()` y `deleteFrame()` son **destrucción irreversible de datos**.

### 5.2 Sin tests de UI
0 tests para `CanvasWidgetV2`, `MainWindowV2`, `TimelinePanelV2`, `LayerPanelV2`, `ToolboxPanelV2`, `ColorPanelV2`, `PropertyEditorV2`, `AudioTrackWidget`.

### 5.3 Sin tests de IO
0 tests para `DocumentManager::save/load` (formato .fap binario), `video_export`, `svg_export`.

### 5.4 Sin tests de AppState/ToolState
El pipeline central de estado (`AppState` → señales → widgets) no tiene cobertura.

### 5.5 Sin soporte de tablet
`CanvasWidgetV2` solo maneja `QMouseEvent`, no `QTabletEvent` ni `QTouchEvent`.

### 5.6 Canvas class huérfana
`Canvas` (viewport math) está en `core/` pero ninguna clase core lo usa. Pertenece a UI.

---

## 6. COBERTURA DE TESTS

### Estado actual
- **154 tests compilados y ejecutados**: 154/154 PASS (100%)
- **19 tests muertos** en 2 archivos excluidos de CMake
  - `test_timeline.cpp` (5 tests): referencia header inexistente `core/timeline.hpp`
  - `test_memory_stress.cpp` (14 tests): mismo header + APIs Windows-only

### Distribución por módulo

| Módulo | Tests | Cobertura |
|--------|-------|-----------|
| `core/` (Document, Layer, Sequence) | 36 | Buena |
| `engine/brush/` | 3 | Básica |
| `engine/vector/` | 5 | Básica |
| `engine/compositor/` | 21 | Buena |
| `engine/animation/` | 29 | Media |
| `engine/deformation/` | 10 | Buena |
| `io/` (solo legacy) | 1 | **Insuficiente** |
| `core/` (undo) | 7 | Solo mocks |
| `ui_v2/` | 0 | **Nula** |
| `platform/` | 0 | **Nula** |

---

## 7. RECOMENDACIONES PRIORIZADAS

### Fase 1 — Data Safety (sprint actual)
1. Fix undo wrong-layer (bug 1.3)
2. Fix layer rename double delete (bug 1.5)
3. Fix pixel dedup load (bug 1.2)
4. Agregar `closeEvent` con confirmación (bug 2.11)

### Fase 2 — Funcionalidad rota
5. Reescribir `renderFrame` para video export (bug 1.1)
6. Fix doble `frameChanged` emission (bug 1.7)
7. Mover audio decode a async (bug 1.8)
8. Agregar undo para deleteLayer/deleteFrame

### Fase 3 — Estabilidad
9. Fix use-after-free en lambda de undo (bug 1.4)
10. Fix `busy_` RAII + atomic rename (bugs 3.1, 3.2)
11. Limpiar audio temp dir (bug 2.5)
12. Agregar tests de IO round-trip

### Fase 4 — Pulido
13. Unificar pixel format (bug 2.10)
14. Fix `hasContent_` en undo + legacy load (bugs 2.3, 2.9)
15. Fix deformation mesh interpolación (bug 2.1)
16. Resto de bugs medios/bajos

---

## 8. ARCHIVOS CON MÁS BUGS

| Archivo | Bugs | Severidad máx |
|---------|------|---------------|
| `canvas_widget_v2.cpp` | 5 | Crítico |
| `document_manager.cpp` | 7 | Crítico |
| `layer_panel_v2.cpp` | 3 | Crítico |
| `video_export.cpp` | 2 | Crítico |
| `timeline_panel_v2.cpp` | 2 | Crítico |
| `audio_track_widget.cpp` | 1 | Crítico |
| `file_format.cpp` | 2 | Alto |
| `deformation_mesh.cpp` | 1 | Alto |
| `node_graph.cpp` | 1 | Alto |

---

## 9. BUGS DESCUBIERTOS DURANTE TESTING (Jul 10, 2026)

### 9.1 Brush Stamp Performance Collapse
**Síntoma**: Pincel a tamaño máximo (size 200) colapsa — lag severo, especialmente con Pencil y Calligraphy.
**Causa**: `drawBrushStamp()` usaba un loop pixel-por-pixel (201×201 = 40,401 iteraciones por dab). A 60fps, esto implicaba procesar 2.4M de píxeles/segundo con trigonometría y blending.
**Fix**: 
- Sistema de stamp caching (`StampCache`) — el stamp se pre-renderiza 1 vez por cambio de parámetro
- Direct pixel copy con skip de transparentes — reemplaza QPainter overhead
- Calligraphy: cache key cuantizado a múltiplos de 5px
- Padding del dirty rect capeado a 16px
**Archivos**: `canvas_widget_v2.cpp`, `canvas_widget_v2.hpp`

### 9.2 Botón "+" de Timeline Trabado
**Síntoma**: Al agregar frames más allá del total inicial, el botón "+" queda fuera del área scrolleable.
**Causa**: `addFrame()/duplicateFrame()/deleteFrame()` actualizaban `totalFrames_` pero no `durationFrames_`. El scrollbar usa `durationFrames_` para calcular el rango.
**Fix**: Sincronizar `durationFrames_ = totalFrames_` en las 3 funciones.
**Archivo**: `timeline_panel_v2.cpp`

### 9.3 Crash con Fill Tool en Capa Grande
**Síntoma**: Al usar tarro de pintura en una capa de 1920×1080 con 34 entradas de undo, el programa crashea.
**Causa**: `pushFullLayerUndo()` crea un `afterSnap` de 8MB adicional. Con undo stack grande (~34 entradas a ~8-16MB cada una), memoria se agota.
**Fix**:
- Límite de píxeles en `doFill()`: rechaza capas > 2.5M px
- Traces `fill_begin` y commit después de undo para diagnóstico
- `setHasContent(true)` agregado después del fill
**Archivo**: `canvas_widget_v2.cpp`

### 9.4 FAP_TRACE_BUFFER_COMMIT sin frameIndex
**Síntoma**: Todos los commits de buffer en el trace reportaban `frame:0`, imposible saber en qué frame se pintó.
**Causa**: `traceBufferCommitEvent()` no aceptaba ni seteaba `frameIndex`.
**Fix**: Agregar `frameIndex` como parámetro y pasarlo desde `commitStroke()` y `doFill()`.
**Archivos**: `tracer_macros.hpp`, `canvas_widget_v2.cpp`

### 9.5 Falta de Traces en UI (Onion Skin, Layers, Frames, Fill)
**Síntoma**: El tracer no capturaba cambios de onion skin, operaciones de capa, add/delete/duplicate de frames, ni operaciones de fill.
**Fix**: Se agregaron traces en:
- `ToolState`: onion skin (enabled, opacity, prev/next frames)
- `LayerPanelV2`: add, delete, duplicate, move, visibility, lock, blend mode
- `TimelinePanelV2`: addFrame, duplicateFrame, deleteFrame
- `CanvasWidgetV2::doFill()`: fill_begin + buffer commit
**Archivos**: `tool_state.cpp`, `layer_panel_v2.cpp`, `timeline_panel_v2.cpp`, `canvas_widget_v2.cpp`

---

## 10. DIAGNOSTIC TRACER SYSTEM (Jul 10, 2026)

### Arquitectura
Sistema de captura de sesiones en `src/core/diagnostic/`:

| Archivo | Rol |
|---------|-----|
| `tracer_types.hpp` | Tipos de eventos (15 categorías) y serialización JSON |
| `tracer_macros.hpp` | Inline functions + macros `FAP_TRACE_*` (compilan a void en Release sin el flag) |
| `tracer.hpp` | Singleton `Tracer` con event filter global de Qt |
| `tracer.cpp` | Hilo escritor asíncrono, batch writes (256 líneas), flush cada 500ms, heartbeat cada 1000 eventos, límite de cola 10k |

### Activación
```powershell
$env:FAP_TRACE = "1"
.\build\Release\free-animation-power.exe
```
Los traces se guardan en `./fap_traces/trace_YYYY-MM-DD_HH-MM-SS_sN.jsonl`. Cada sesión crea un archivo nuevo.

### Cobertura actual
| Categoría | Eventos capturados |
|-----------|-------------------|
| Mouse | press, move (throttled 50ms), release, wheel |
| Keyboard | key_press con modifiers |
| Tool | change (tool, size, opacity, hardness, shape, color) |
| Stroke | begin, end, fill_begin |
| Buffer | commit (layerUid, frame, dirty rect), resize, clear, ensureContains |
| Undo | push_applied, undo, redo con stack sizes |
| Layer | add, delete, duplicate, move_up/down, visible_on/off, lock_on/off, blend_mode |
| Frame | change, add, duplicate, delete |
| Onion skin | enabled, disabled, opacity, prev_frames, next_frames |
| Playback | play, pause, stop con frame+fps |
| IO | save_begin/end, load_begin/end |
| Paint | paint (duración) |
| App | session_start, session_end, heartbeat, reset_to_defaults |

### Protecciones
- Límite de cola: 10,000 eventos (droppea los más viejos si se excede)
- Heartbeat cada 1,000 eventos con timestamp + dropped count
- Flush forzado si cola > 500 eventos
- Flush periódico cada 500ms
- Try-catch en hilo escritor
- Los traces antiguos nunca se borran

---

## 11. ESTADO ACTUAL (Jul 10, 2026)

### Bugs corregidos en esta sesión
| # | Bug | Estado |
|---|-----|--------|
| 1 | Brush stamp performance (size 200) | Corregido |
| 2 | Botón "+" timeline trabado | Corregido |
| 3 | Crash fill tool en capa grande | Protegido |
| 4 | FAP_TRACE_BUFFER_COMMIT sin frameIndex | Corregido |
| 5 | Falta de traces en UI | Corregido |
| 6 | Brush padding cap (16px) | Corregido |
| 7 | Pixel dedup: capas compartidas cargan vacías (1.2) | Corregido |
| 8 | Save largo bloquea UI + crash por re-entrada | Corregido |
| 9 | Skip capas vacías en save (hasContent_) | Corregido |
| 10 | .tmp preservado en fallo de rename | Corregido |
| 11 | processEvents(ExcludeUserInputEvents) durante save | Corregido |
| 12 | Guardia saving_ en newProject/openProject | Corregido |
| 13 | UI: botones de layers/sequence/audio track fondo naranja | Corregido |

### Bugs pendientes (del informe original)
Prioridad: undo wrong-layer (1.3), layer rename crash (1.5), video export broken (1.1).

---

## 12. SAVE DATA LOSS & CRASH (Jul 10, 2026)

### 12.1 Pixel Dedup → Capas Vacías al Reabrir
**Síntoma**: Guardar proyecto con capas duplicadas/clonadas. Al reabrir, solo una fracción de las capas tiene contenido.
**Causa**: `writeLayerData()` usa `unordered_set<void*>` para detectar buffers compartidos. Escribe `shared_pixels: true` en vez del PNG. `readLayerData()` NUNCA lee ese flag — intenta cargar PNG inexistente y la capa queda vacía.
**Fix**: 
- `writeLayerData()`: cambia `set<void*>` a `map<void*, LayerUid>`, escribe `share_source_uid`
- `readTimeline()`: lee `shared_pixels` + `share_source_uid`, guarda en `sharePixelMap_`
- `readLayerData()`: `loadedPixelBuffers` map resuelve capas compartidas copiando el buffer fuente
- Skip capas con `hasContent_ == false` en save (reduce tiempo de save)
**Archivos**: `document_manager.cpp`, `document_manager.hpp`

### 12.2 Save Bloquea UI → Crash por Re-entrada
**Síntoma**: Save de 40 segundos congela el UI. Si el usuario hace click en "New Project" durante el save, `processEvents()` despacha `resetDocument()` que destruye el documento que se está guardando → crash.
**Causa**: `processEvents()` permitía entrada del usuario durante el save.
**Fix**:
- `processEvents(QEventLoop::ExcludeUserInputEvents)` — refresca UI pero bloquea clicks
- Guardia `saving_` en `newProject()` y `openProject()`: "Save in progress, wait..."
- WaitCursor + status "Saving..." durante save
**Archivos**: `document_manager.cpp`, `main_window_v2.cpp`, `main_window_v2.hpp`

### 12.3 .tmp Preservado en Fallo de Rename
**Síntoma**: Si `commitAtomic` falla, el `.tmp` se borraba perdiendo los datos del ZIP.
**Fix**: `.tmp` se preserva. `lastError_` incluye la ruta para recuperación manual.
**Archivo**: `document_manager.cpp`

### 12.4 Traces de Error en Save
**Fix**: `FAP_TRACE_IO("save_error*")` en paths: zip_init, write, rename.
**Archivo**: `document_manager.cpp`

---

## 13. UI STYLING — BOTONES CON FONDO NARANJA (Jul 10, 2026)

**Objetivo**: Mejorar visibilidad de botones de acción cambiando el fondo gris/transparente por el naranja acento `#FF4800`.

### 13.1 Panel de Layers (5 botones)
**Archivo**: `layer_panel_v2.cpp`
- Constantes `kBtnBg` → `#FF4800`, `kBtnHover` → `#FF6A30`
- Afecta: + nueva capa, duplicar, subir, bajar, eliminar
- Delete mantiene hover rojo `#E05050`

### 13.2 Panel de Secuencias (5 botones)
**Archivo**: `timeline_panel_v2.cpp`
- `lockBtn_`, `dupBtn_`, `upBtn_`, `downBtn_`: `background:transparent` → `background:#FF4800`
- `delBtn_`: fondo naranja + hover rojo `#E05050`

### 13.3 Audio Track (4 botones)
**Archivo**: `audio_track_widget.cpp`
- `muteBtn_` (🔊), `upBtn_` (↑), `downBtn_` (↓): `background:transparent` → `background:#FF4800`
- `delBtn_` (✕): fondo naranja + hover rojo `#FF4A4A`

### Bugs pendientes (del informe original)
Ver secciones 1-4 para lista completa. Prioridad: undo wrong-layer (1.3), layer rename crash (1.5).

### Tests
- 154/154 pasan (100%)
- 0 tests de UI, 0 tests de IO (DocumentManager), 0 tests de exportación

---

## 14. SEQUENCE CLONE & DUPLICATION BUGS (Jul 10, 2026)

### 14.1 Frame 0 Vacío en Secuencia Duplicada
**Síntoma**: Al duplicar una secuencia con contenido, el frame 0 de la copia aparece vacío en la timeline.
**Causa**: `Sequence::clone()` creaba un frame 0 vacío con "Layer 1" por defecto si no existía en `frames_`, sobrescribiendo el frame 0 real clonado.
**Fix**: Cambiar la condición de `frames_.find(0) == end()` a `frames_.empty()` — solo crear frame 0 si la secuencia original está completamente vacía.
**Archivo**: `core/sequence.cpp`

### 14.2 Secuencia Fantasma al Reabrir Archivo
**Síntoma**: Al guardar un proyecto con secuencias duplicadas y reabrirlo, aparece una secuencia vacía extra entre la original y la duplicada.
**Causa**: `readTimeline()` llamaba `doc.addSequence("")` para secuencias si>0, creando secuencias con nombre vacío. `writeTimeline()` guardaba estos nombres vacíos. Al recargar, `addSequence("")` creaba otra secuencia vacía adicional.
**Fix**: `readTimeline()` renombra secuencias sin nombre a "Sequence N" (ej: "Sequence 2", "Sequence 3").
**Archivo**: `io/document_manager.cpp`

### 14.3 Audio Track Duplicación al Reabrir
**Síntoma**: Al abrir múltiples proyectos en la misma sesión, las pistas de audio se duplican (1 → 2 → 4...).
**Causa**: `openProject()` no llamaba `clearAudioTracks()` antes de restaurar pistas de audio desde el documento cargado. Los widgets de audio del proyecto anterior sobrevivían y se acumulaban.
**Fix**: Agregar `timeline_panel_->clearAudioTracks()` antes del loop de restauración en `openProject()`.
**Archivo**: `ui_v2/main_window_v2.cpp`

### 14.4 Crash al Crear Nuevo Proyecto (New Project)
**Síntoma**: Después de guardar un proyecto y hacer click en "New Project", el programa crashea.
**Causa**: `resetDocument()` destruía el documento viejo pero quedaban widgets pendientes de `deleteLater()` y eventos de paint en la cola de Qt referenciando capas destruidas.
**Fix**:
- `deleteLater()` → `delete` directo en `rebuildTracks()`, `clearAudioTracks()`, `removeAudioTrack()`
- `sendPostedEvents(DeferredDelete)` + `processEvents()` después de `resetDocument()` en `newProject()` y `openProject()`
- `canvas_->resetState()` limpia todos los buffers internos del canvas
**Archivos**: `ui_v2/main_window_v2.cpp`, `ui_v2/timeline_panel_v2.cpp`, `ui_v2/canvas_widget_v2.cpp/.hpp`

---
## 15. DIAGNOSTIC TRACE EXPANSIONS (Jul 10, 2026)

### 15.1 Sequence Traces
- `FAP_TRACE_SEQUENCE(evt, idx, name)`: nuevo macro para operaciones de secuencia
- Traces en: `onDupTrack`, `onDelTrack`, `Sequence::clone()`, `writeTimeline()`, `readTimeline()`
- `openProject()` traza `open_seq_count_N` post-load

### 15.2 Guardias Anti-Crash
- `CanvasWidgetV2::setCurrentFrame()`: `if (frame == currentFrame_) return`
- `TimelinePanelV2::setCurrentFrame()`: misma guardia
- `SequenceTrackWidget::frameHasContent()`: `seqIndex_ >= seqCount` → `return false`

### 15.3 Cobertura Actualizada del Tracer
| Nueva categoría | Eventos |
|-----------------|---------|
| Sequence | duplicate, delete, save, load, cloned |
| App | rebuild_tracks_begin/end, rebuild_after_dup, open_seq_count_N |
| Buffer | clone_frame |
| Frame | add, duplicate, delete, change, clone_frame |

### Bugs corregidos acumulados (14 en total)
| # | Bug | Estado |
|---|-----|--------|
| 1-6 | Brush performance, timeline, fill, traces, padding | Corregido |
| 7-12 | Pixel dedup, save crash, UI styling | Corregido |
| 13 | Audio track duplication on reopen | Corregido |
| 14 | Ghost sequence on reopen | Corregido |

### Bugs pendientes
Undo wrong-layer (1.3), layer rename crash (1.5), video export broken (1.1).
