# Changelog — Free Animation Power

## [v2.7] — 2026-07-15

### Non-destructive Frame Hiding — +/- controlan visibilidad, no borran
- Boton `+`: revela frame oculto si existe; crea nuevo solo si no hay ocultos
- Boton `-`: reduce `durationFrames_` (oculta ultimo frame visible), NO borra datos
- Frames ocultos: celdas y ticks atenuados en el timeline, bloqueados de navegacion
- Eliminacion real: click derecho → Delete Frame (unica forma de borrar datos)
- `durationFrames_` desacoplado de `totalFrames_`: navegacion, playback, onion skin usan `durationFrames_`
- **Archivos**: `sequence.cpp`, `timeline_panel_v2.{hpp,cpp}`, `canvas_widget_v2.{hpp,cpp}`, `main_window_v2.cpp`

### Timeline Markers (estilo After Effects)
- Boton `◆ Marker` en barra del timeline, o tecla `*` (numpad)
- Triangulos de color en el ruler con banda de duracion y titulo visible
- 9 colores: None, Red, Orange, Yellow, Green, Cyan, Blue, Purple, Magenta
- Dialogo Marker Settings: Title, Frame (1-based), Color Label, Detail (multilinea)
- Detail = notas internas, solo visibles en el dialogo (no en el ruler)
- Duracion se crea arrastrando borde derecho del marker
- Interacciones: drag para mover, doble click para editar, Ctrl+click para eliminar
- Navegacion: Ctrl+Shift+← / Ctrl+Shift+→
- Persistencia en manifest.json (retrocompatible con archivos viejos)
- Ruler height: 18px → 22px para mejor visibilidad
- Titulo: 7px bold blanco sobre pill oscuro para maxima legibilidad
- **Archivos**: `sequence.hpp`, `sequence.cpp`, `document_manager.cpp`, `timeline_panel_v2.{hpp,cpp}`, `canvas_widget_v2.cpp`

### Marker dialog — dark theme stylesheet
- QSpinBox, QLineEdit, QComboBox, QPlainTextEdit, QPushButton con estilos de tema oscuro
- Texto #E8ECF0 sobre fondo #13161D, borde focus #FF4800
- Flechas del QSpinBox visibles y funcionales

### updateMarker — restriccion de unicidad por frame
- `updateMarker()` ahora reemplaza marker existente en el frame destino (misma logica que `addMarker`)
- Repaint sincrono tras cerrar el dialogo modal

### Marker Frame — enumeracion 1-based
- El campo Frame en Marker Settings ahora empieza en 1 (coincide con la regla del timeline)
- Conversion interna: display = frame + 1, guardado = value - 1

### Undo/Redo — canvasUpdated faltante tras Ctrl+Z/Ctrl+Y
- `undo()` y `redo()` ahora emiten `canvasUpdated`, lo que refresca el timeline y layer panel
- Antes: solo el canvas se repintaba; las celdas del timeline y el layer panel quedaban congelados

### Tests
- 160/160 tests pasan

## [v2.6.2] — 2026-07-15

### H3 Regression — Guardado falla tras exportar GIF en Windows
- `commitAtomic()` reintroducido: H3 (v2.6.1) removio `QFile::remove()` creyendo que `QFile::rename()` sobreescribe en Windows, pero Qt 6.5 usa `MoveFileW` sin `MOVEFILE_REPLACE_EXISTING`. Si el archivo destino existe, el rename falla silenciosamente.
- **Fix**: rename primero, si falla y destino existe → remove + retry. Minimiza ventana TOCTOU.
- **Escenario**: exportar GIF → dialogo nativo cambia directorio de trabajo → guardar .fap en directorio con archivo existente → falla.
- **Archivos**: `src/io/document_manager.cpp:214-237`

### Dialogo de resolucion para exportacion (GIF + Video)
- GIF y video muestran dialogo Width x Height antes de codificar
- `ExportOptions` struct agregado a `video_export.hpp`
- `renderExportFrame()` con scaling: renderiza a tamaño canvas, luego escala
- **Archivos**: `src/io/video_export.hpp`, `src/io/video_export.cpp`, `src/ui_v2/main_window_v2.cpp`

### Mensajes de error de guardado mejorados
- Dialogos de error ahora incluyen `dm.lastError()` (motivo real del fallo)
- **Archivos**: `src/ui_v2/main_window_v2.cpp:695,778`

### Tests
- 160/160 tests pasan

## [v2.6] — 2026-07-13

### Copy Layer to All Frames
- Nuevo boton en Layer Panel: copia contenido, nombre y propiedades de una capa a todos los frames
- `Sequence::copyLayerToAllFrames()` con `shareDataFrom()` + `ensureUnique()` (copias independientes)
- `CopyLayerToFramesCommand` con undo/redo completo (backups por frame)
- 4 archivos modificados (+150 lineas)

### Video Export Unificado + Audio Mixing
- `exportVideo()` unificada: auto-detecta formato por extension (mp4/mov/webm)
- Audio mixing: `amix` filter con volumen por track, codec por contenedor (aac/pcm_s16le/libopus)
- MOV QuickTime Alpha (qtrle+argb), WebM VP9 Alpha (libvpx-vp9+yuva420p)
- `exportGIF()` exporta a resolucion completa (antes hardcodeado 320px)
- `MainWindowV2` delegado a `fap::exportVideo()` y `fap::exportGIF()` (-120 lineas duplicadas)
- `executeFFmpeg()` con timeout 120s + `.kill()` (antes infinito)

### Bug Fixes Criticos
- **Eraser no funciona**: `drawBrushStamp()` usaba `DestinationOut` sobre `strokeBuffer_` transparente (fix: unificado `SourceOver`)
- **Solapamiento nombres de capa**: `QListWidget::clear()` no destruye widgets (fix: `removeItemWidget` + `deleteLater`)
- **Crash al renombrar capa**: punteros crudos en lambda de rename (fix: `QPointer` guards)
- **FPS timer imprecision**: division entera `1000/24=41ms` vs `41.67ms` real (fix: `std::round(1000.0/fps)`)

### Icono .fap + Asociacion de Archivos
- `resources/icons/fap.ico` — 7 resoluciones (16-256px) generado desde logos PNG
- `scripts/embed_icon.py` — post-build Win32 `BeginUpdateResource` icon injector
- `src/platform/file_association.{hpp,cpp}` — registro `HKCU\Software\Classes\.fap`
- `registerFileAssociation()` llamado en constructor `MainWindowV2`

### Frame Clipboard — Click Derecho en Timeline
- Menu contextual en celdas del timeline: Copy Frame, Cut Frame, Paste Frame
- Portapapeles almacena `GroupLayer` completo (todas las capas + pixeles)
- `CutFrameCommand` y `PasteFrameCommand` con undo/redo completo
- `TimelinePanelV2` con `unique_ptr<GroupLayer> frameClipboard_`

### Tests
- 160/160 tests pasan

## [v2.5.1] — 2026-07-08

### Audio Persistence — Embedido en .fap ZIP
- **AudioTrackData** struct en Document (filepath, displayName, zipEntry, muted, volume)
- **Save**: metadata a `timeline.json` ("audio" array) + bytes a `audio/track_N.ext` en ZIP
- **Load**: `extractAudio()` descomprime a `%TEMP%/fap_audio_<PID>/` + reconstruye AudioTrackWidgets
- **UI**: `syncAudioToDocument()` antes de guardar, `addAudioTrackFromData()` al cargar
- 9 archivos modificados (+225/-1 lineas)

### Save As — Dialogo Carpeta vs Archivo
- `saveProjectAs()` muestra dialogo con 3 botones: "Single .fap File", "Project Folder", Cancel
- Modo carpeta: pide directorio + nombre de proyecto, copia audio a `audio/` junto al .fap
- Modo archivo: mismo comportamiento anterior

### Bug Fixes
- **Pixel offset on load**: `ensureContains(..., false)` en `readLayerData()` — evita expansion de buffer
- **Eraser tool on load**: `toolbox_panel_->setActiveTool(0)` en `openProject()` y `newProject()` — restaura Brush
- **Audio leaking between projects**: `clearAudioTracks()` en `newProject()` antes de `rebuildTracks()`
- 154/154 tests pasan

## [v2.5.0] — 2026-07-08

### File Format — Binary .fap (ZIP)
- Reemplazo del formato directorio por archivo ZIP unico (.fap)
- DocumentManager integrado con UI: atomic save (.tmp -> rename)
- Multi-secuencia completa: 11 campos de metadata por secuencia
- Viewport persistido: zoom/offset se guardan y restauran al reabrir
- setOrigin() directo en RasterLayer para restaurar posicion de contenido
- Limpieza de capas default al cargar (sin duplicados Layer 1 fantasma)
- hasContent_ flag serializado en ambos formatos (ZIP y directorio)

### Audio Decoder
- Reemplazo de QAudioDecoder por dr_libs: dr_wav, dr_mp3, dr_flac
- Decodificacion sincrona nativa (~10ms para 3 min), 800 peaks
- Header-only, public domain (CC0/MIT-0), sin dependencias externas
- Eliminado workaround QT_MEDIA_BACKEND=ffmpeg

### UI — Nuevos Botones e Iconos
- Boton Loop en timeline (toggle con icono, fondo naranja al activar)
- Rotate +15deg (R) y Rotate -15deg (L) con iconos dedicados
- Botones +/- Frame en timeline (add_frame, remove_frame icons)
- Botones FPS ahora usan iconos en vez de texto (-/+)
- SVG Export: Current Frame + All Frames (submenu en toolbar)
- Eliminado boton de capa vectorial del panel Layers
- Eliminados spinboxes RGBA del panel Color

### Timeline Fixes
- Playback inicia desde WA In si frame actual esta fuera del area
- Audio pause+seek+play en wrap del WA (loop mode)
- Waveform fix: repaint tras rebuildTracks() y onImportAudio()
- fit() centra viewport en el documento (no esquina superior izquierda)

### Project Cleanup
- Unificacion nombre: FreeAnimation2dStyle -> Free Animation Power
- Documentos obsoletos movidos a docs/archive/
- src_py/ marcado DEPRECATED, src/ui/ marcado PLACEHOLDER
- Politica de idioma documentada en AGENTS.md

## [v2.0.0] — 2026-07-02

### Text Tool Bugfix Triad
- Hit-test: boundingBox() reemplaza ancla estatica de 12x24px
- Delete: entries.erase() fuera del guard !text.isEmpty()
- Caret: ascent() directo en vez de *0.2 en calculo de posicion

### Previous
- Middle mouse button canvas panning
- Layer panel display desync fix (signal separation)
- 4-buffer CPU display pipeline
- Memory stress tests (15 tests, 1.6 GB peak)
- Layer origin offset support
- GPLv3 license
- Text tool redesign: font picker + character panel
- Brush undo ghosts fix, ghost rectangle artifact fix
- Buffer expansion fix, content clipping fix
- Qt raster tile seams fix
- Full V2 tool implementation (11 tools)
