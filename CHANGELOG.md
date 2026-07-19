# Changelog — Free Animation Power

## [v2.9] — 2026-07-19

### Tablet event dispatching fix
- Al conectar Wacom, el canvas dejaba de responder (no dibujaba, no paneaba)
- Causa: `tabletEvent()` aceptaba eventos sin despachar a handlers de mouse
- Fix: `tabletEvent()` genera `QMouseEvent` sinteticos y los despacha via `QApplication::sendEvent()`
- `TabletPress → MouseButtonPress`, `TabletMove → MouseMove` (pen down), `TabletRelease → MouseButtonRelease`
- Presion (`tabletPressure_`) y eraser (`tabletEraser_`) se actualizan antes del despacho
- `sendEvent()` en lugar de llamar directa para que undo funcione correctamente
- **Archivos**: `canvas_widget_v2.cpp:1743-1796`, `canvas_widget_v2.hpp`

### Line Boil — Efecto de linea vibrante por secuencia
- Efecto no destructivo: ruido posicional bilineal por celdas 8x8 px
- Desplazamiento maximo configurable: 0-10px por slider
- Aplicado en dominio DISPLAY (buildBackgroundCache + renderExportFrame)
- Nunca modifica pixelBuffer_ — backward compatible con archivos existentes
- UI: checkbox "Boil" + slider 0-100 en header de SequenceTrackWidget
- Persistente: `line_boil_enabled` + `line_boil_strength` en timeline.json
- **Archivos**: `sequence.{hpp,cpp}`, `raster_effect.{hpp,cpp}` (nuevo), `canvas_widget_v2.cpp`, `video_export.cpp`, `timeline_panel_v2.cpp`, `document_manager.cpp`, `app_state.{hpp,cpp}`, `CMakeLists.txt`

### Sequence Lock — Candado visual + feedback
- Boton lock: `:checked` → fondo rojo `#FF4A4A`, `:!checked` → fondo gris oscuro
- Celdas bloqueadas: overlay rojo semi-transparente sobre el area de celdas
- isSequenceLocked() verifica activeSequence().locked() — candado por secuencia individual
- Nueva secuencia: desbloqueada por defecto (locked_ = false)
- Desbloquear no borra pixel data — el estado `locked` ya persiste en timeline.json
- **Archivos**: `timeline_panel_v2.cpp`, `canvas_widget_v2.cpp`

### Frame Keyboard Shortcuts — `+` y `-`
- `+` o `=` → duplicar frame (revela oculto primero, crea nuevo si todos visibles)
- `-` → ocultar frame (reduce durationFrames_, preserva datos)
- Qt auto-repeat: mantener presionada la tecla ejecuta la accion secuencialmente
- Canvas emite addFrameRequested() / hideFrameRequested() → MainWindow enruta a Timeline
- addFrame() y hideFrame() movidos a public en TimelinePanelV2
- **Archivos**: `canvas_widget_v2.{hpp,cpp}`, `main_window_v2.cpp`, `timeline_panel_v2.{hpp,cpp}`

## [v2.8] — 2026-07-16

### Video Clip Import
- Importar clips de video al timeline como tracks (similar a audio)
- Limites: max 60 segundos, max 1920x1080 (dialogo de advertencia con opcion de truncar/escalar)
- Botones identicos a AudioTrackWidget: ▲▼ mover, 🔊 mute, ✕ delete
- Slider de opacidad (0-100%) + slider de volumen (0-100%)
- Tira de thumbnails: 1 thumbnail cada 30 frames (320x180)
- Frames decodificados on-demand via FFmpeg (PNG pipe, ~50ms/frame)
- LRU cache de 50 frames full-res (thread-safe)
- Video renderizado encima de las capas de dibujo en el canvas
- Opacidad en tiempo real: slider invalida cache del canvas inmediatamente
- Guardado en .fap: video/track_N.ext embebido en ZIP
- Extraccion a %TEMP%/fap_video_<PID>/ al cargar
- Retrocompatible: archivos sin array "video" cargan sin error
- **Archivos**: `document.hpp`, `video_decoder.{hpp,cpp}`, `video_track_widget.{hpp,cpp}`, `timeline_panel_v2.{hpp,cpp}`, `canvas_widget_v2.{hpp,cpp}`, `main_window_v2.{hpp,cpp}`, `document_manager.cpp`, `CMakeLists.txt`

### Detachable Onion Skin y Canvas
- OnionSkinPanel extraido de ToolboxPanelV2 en su propio widget + QDockWidget
- QFormLayout para alinear los spinboxes, sin botones (NoButtons), 0-10
- Labels completos: "Previous frames:", "Next frames:"
- Canvas movido a QDockWidget desprendible (antes centralWidget fijo)
- CanvasSizePanel extraido de ToolboxPanelV2 en su propio dock ('CANVAS SIZE')
- 8 docks: TOOLS (left), ONION SKIN (left), LAYERS (right), COLOR (right), PROPERTIES (right), CANVAS SIZE (right), CANVAS (center), TIMELINE (bottom)
- Todos los docks: Movable | Floatable con barra de titulo naranja
- ToolboxPanelV2: ajustado al contenido (52px ancho, sin stretch, sin QScrollArea)
- **Archivos**: `onion_skin_panel.{hpp,cpp}` (nuevo), `canvas_size_panel.{hpp,cpp}` (nuevo), `toolbox_panel_v2.{hpp,cpp}`, `main_window_v2.{hpp,cpp}`

### Soporte de tabletas (Wacom, Huion, Xencelabs, XP-Pen)
- Canvas responde a presion del lapiz via QTabletEvent nativo de Qt 6
- Tamaño del pincel: radio * (0.2 + 0.8 * presion) — 20% min, 100% max
- Opacidad: multiplicada por presion (trazos mas suaves con menos presion)
- Borrador de tablet: detecta y cambia automaticamente a modo eraser
- Sin tablet: comportamiento identico al mouse (presion=1.0)
- Sin SDKs externos — usa API nativa de Qt 6
- **Archivos**: `canvas_widget_v2.{hpp,cpp}`

### Video + dibujo — rectangulo blanco sobre video al calcar
- Al dibujar sobre una capa raster con video activo, la reconstruccion parcial del cache llenaba el area con blanco pero no recomponia los frames de video
- Fix: con video tracks activos, la reconstruccion parcial se escala a completa (solo en commitStroke, no en cada dab)
- **Archivos**: `canvas_widget_v2.cpp`

### Posicion de tracks de audio/video preservada al guardar/cargar
- Nuevo campo `layoutPosition` en `AudioTrackData` y `VideoTrackData`
- Al guardar: captura `tracksLayout_->indexOf(widget)` para cada track
- Al cargar: `insertWidget` en la posicion guardada (default 9999 = final)
- Retrocompatible con archivos viejos
- **Archivos**: `document.hpp`, `document_manager.cpp`, `timeline_panel_v2.{hpp,cpp}`, `main_window_v2.cpp`

### Tests
- 160/160 tests pasan

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
