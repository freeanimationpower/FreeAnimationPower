# INFORME COMPLETO DE ARQUITECTURA: Free Animation Power v1.0.0

> **Version:** 1.0.0 | **Fecha:** Julio 2026 | **Autor:** Eduardo Fierro Duque
> **Licencia:** GPLv3 | **Lenguaje:** C++20 | **Framework:** Qt 6.5+ | **Tests:** 160/160
> **Arquitectura:** Motor hibrido vector + raster con pipeline de renderizado CPU 4-buffer

---

## INDICE

1. [Resumen Ejecutivo](#1-resumen-ejecutivo)
2. [Arquitectura General](#2-arquitectura-general)
3. [Modelo de Datos — Core](#3-modelo-de-datos--core)
4. [Capa Engine](#4-capa-engine)
5. [Pipeline de Renderizado (Display Pipeline)](#5-pipeline-de-renderizado)
6. [Capa UI V2](#6-capa-ui-v2)
7. [Sistema de Timeline](#7-sistema-de-timeline)
8. [Capa IO — Entrada/Salida](#8-capa-io--entradasalida)
9. [Exportacion de Video y GIF](#9-exportacion-de-video-y-gif)
10. [Capa Platform](#10-capa-platform)
11. [Sistema de Build](#11-sistema-de-build)
12. [Formato de Archivo .fap](#12-formato-de-archivo-fap)
13. [Historial de Bugs y Soluciones](#13-historial-de-bugs-y-soluciones)
14. [Sistema de Tests](#14-sistema-de-tests)
15. [Estructura del Proyecto](#15-estructura-del-proyecto)

---

## 1. RESUMEN EJECUTIVO

Free Animation Power es una aplicacion de animacion 2D profesional con motor hibrido vector + raster. Permite animacion cuadro-por-cuadro tradicional, pintura digital e ilustracion vectorial en un solo entorno unificado.

### Stack Tecnologico

| Componente | Tecnologia |
|-----------|-----------|
| Lenguaje | C++20 |
| UI Framework | Qt 6.5+ (Widgets, Multimedia, SVG) |
| Compresion | miniz 3.0.2 (zlib) |
| Export video/GIF | FFmpeg (externo, via subprocess) |
| Decodificacion audio | dr_libs (WAV, MP3, FLAC — nativo C) |
| Decodificacion video | FFmpeg + ffprobe (via subprocess) |
| Tests | GoogleTest 1.14.0 |
| Build | CMake 3.20+ |

### Capacidades Principales

- **17 herramientas de dibujo**: Brush, Eraser, Fill, Text, Line, Rect, Ellipse, Move, Select, Hand, ColorPicker, PencilRetouch, RulerLine, RulerEllipse, DeformMesh, TweenEdit
- **Multi-secuencia**: Timeline con secuencias independientes, cada una con sus propios frames, capas, undo stack y configuracion
- **Timeline completo**: Marcadores estilo After Effects, Work Area con loop, FPS variable, ocultacion no destructiva de frames (+/-), copy/paste/cut de celdas
- **Capas**: Raster, Vector y Group con 12 blend modes, opacidad, visibilidad y bloqueo por capa
- **Audio**: Import WAV/MP3/FLAC, waveform visual, mute/volume, playback sincronizado con timeline
- **Video**: Import MP4/MOV/WebM como track, thumbnails, opacidad, composicion sobre capas de dibujo
- **Tableta grafica**: Soporte nativo Qt 6 para Wacom, Huion, Xencelabs, XP-Pen con presion, tamano y opacidad
- **Onion skinning**: Frames anteriores/siguientes con opacidad configurable
- **Line Boil**: Efecto no destructivo de linea vibrante organica por secuencia
- **Undo/Redo**: Por secuencia, profundidad 100, con comandos compuestos
- **Persistencia**: Formato .fap v2 (ZIP binario) con save atomico, deduplicacion de pixeles y respaldo legacy v1/v2/v3
- **Exportacion**: MP4 H.264, MOV QuickTime Alpha, WebM VP9 Alpha, GIF animado, SVG multi-frame, PNG sequence
- **Interfaz**: 8 docks desacoplables con tema oscuro (#252830 / #FF4800 accent), tipografia Avenir LT Std

### Metricas

| Metrica | Valor |
|---------|-------|
| Archivos fuente C++ | 98 (.hpp + .cpp + .h) |
| Lineas de codigo estimadas | ~45,000 |
| Tests unitarios | 160 |
| Docks de UI | 8 desacoplables |
| Herramientas de dibujo | 17 |
| Blend modes | 12 |
| Formatos de audio soportados | 3 (WAV, MP3, FLAC) |
| Formatos de video export | 5 (MP4, MOV, WebM, GIF, PNG seq) |

---

## 2. ARQUITECTURA GENERAL

### Diagrama de Capas

| Capa | Componentes |
|------|-------------|
| **UI (Qt 6 Widgets)** | MainWindowV2, CanvasWidgetV2, TimelinePanelV2, ToolboxPanelV2, ColorPanelV2, LayerPanelV2, PropertyEditorV2, OnionSkinPanel, CanvasSizePanel, AudioTrackWidget, VideoTrackWidget, BusyOverlay, LayerLockButton |
| **AppState (Central Hub)** | Document, ToolState, AudioEngine, RulerTool, TweenEngine, DeformationEngine, FrameThumbnailCache, PencilRetouchEngine |
| **Engine Layer** | **brush/**: BrushEngine, BrushPreset, ABRImporter, PaperTexture, PencilRetouch, RulerTool **raster/**: RasterEngine, RasterStroke, RasterEffect **vector/**: VectorEngine, VectorStroke, BezierPath **compositor/**: Compositor, NodeGraph **deformation/**: DeformationMesh, DeformationEngine **animation/**: AudioEngine, AudioFileDecoder, VideoDecoder, TweenEngine, FrameThumbnailCache |
| **Core Data Model** | Document, Sequence (Marker, LineBoil), Layer (Raster/Vector/Group), UndoManager, ToolState, Types (Color, Vec2, enums) |
| **Platform Layer** | IO: DocumentManager, FileFormat, VideoExport, SVGExport / Win32: FileAssociation |

### Flujo de Datos Principal

1. **Input**: QTabletEvent / QMouseEvent → CanvasWidgetV2
2. **Tool Dispatch**: ToolState::activeTool() → dispatch a herramienta activa
3. **Brush Engine**: BrushEngine::beginStroke → addPoint x N → endStroke
4. **Stroke Render**:
   - Raster: dab stamps → strokeBuffer_ (aislado) → commitStroke() → pixelBuffer_ (SourceOver/DestinationOut)
   - Vector: createPathFromPoints() → BezierPath → VectorStroke
5. **Compositing**: buildBackgroundCache() → backgroundCache_ (fondo blanco + onion skin + capas + video)
6. **Display**: paintEvent() → QPainter sobre QWidget

### AppState — Central State Hub

`AppState` (QObject) es el singleton logico que posee y orquesta todos los subsistemas:

| Subsistema | Tipo | Rol |
|-----------|------|-----|
| `document_` | `unique_ptr<Document>` | Modelo de datos: canvas, secuencias, capas, audio/video tracks |
| `tool_state_` | `unique_ptr<ToolState>` | 37 propiedades de herramienta con signals Qt |
| `audio_engine_` | `unique_ptr<AudioEngine>` | Motor de audio para playback |
| `ruler_tool_` | `unique_ptr<RulerTool>` | Guias de regla (lineal, radial, arco) |
| `tween_engine_` | `unique_ptr<TweenEngine>` | Interpolacion de keyframes |
| `deformation_engine_` | `unique_ptr<DeformationEngine>` | Deformacion de malla |
| `thumbnail_cache_` | `unique_ptr<FrameThumbnailCache>` | Cache de miniaturas de frame |
| `pencil_retouch_` | `unique_ptr<PencilRetouchEngine>` | Retoque de lapiz |

**Signals principales**: `documentChanged()`, `currentFrameChanged(int)`, `activeLayerIndexChanged(int)`, `activeSequenceChanged(int)`, `canvasSizeChanged(int,int)`.

---

## 3. MODELO DE DATOS — CORE

### 3.1 Document (`document.hpp`)

Contenedor raiz del proyecto. Posee:

| Campo | Tipo | Descripcion |
|-------|------|-------------|
| `width_`, `height_` | `int` | Dimensiones del canvas (default 1920x1080) |
| `sequences_` | `vector<unique_ptr<Sequence>>` | Secuencias del proyecto (minimo 1) |
| `audioTracks_` | `vector<AudioTrackData>` | Metadatos de pistas de audio |
| `videoTracks_` | `vector<VideoTrackData>` | Metadatos de pistas de video |
| `filepath_` | `string` | Ruta del archivo .fap |
| `modified_` | `bool` | Flag de cambios sin guardar |

Metodos clave:
- `addSequence()` / `removeSequence()` / `duplicateSequence()` — gestion multi-secuencia
- `rootLayerForFrame(frame)` — acceso a capas por secuencia activa + frame
- `undoManager()` — delega al UndoManager de la secuencia activa

### 3.2 Sequence (`sequence.hpp`)

Una secuencia es una linea de tiempo independiente con sus propios frames, capas y configuracion.

| Campo | Tipo | Default | Descripcion |
|-------|------|---------|-------------|
| `uid_` | `uint64_t` | auto | Identificador unico inmutable |
| `name_` | `string` | "Sequence 1" | Nombre de secuencia |
| `visible_` | `bool` | true | Visibilidad en timeline |
| `opacity_` | `float` | 1.0 | Opacidad de composicion |
| `locked_` | `bool` | false | Bloqueo de edicion |
| `current_frame_` | `int` | 0 | Frame actual |
| `total_frames_` | `int` | 24 | Total de frames con datos |
| `fps_` | `int` | 24 | Frames por segundo |
| `playing_` | `bool` | false | Estado de reproduccion |
| `looping_` | `bool` | false | Loop activo |
| `workAreaStart_` | `int` | 0 | In point del area de trabajo |
| `workAreaEnd_` | `int` | 0 | Out point (0 = fin) |
| `durationFrames_` | `int` | 1 | Frames visibles (≤ totalFrames) |
| `lineBoil_` | `LineBoil` | disabled | Efecto linea vibrante |
| `frames_` | `map<int, unique_ptr<GroupLayer>>` | — | Datos de capas por frame |
| `markers_` | `vector<Marker>` | — | Marcadores de timeline |
| `undo_manager_` | `UndoManager` | — | Pila undo/redo por secuencia |

**Metodos de navegacion**: `advanceFrame()`, `nextFrame()`, `prevFrame()`, `goToStart()`, `goToEnd()` — todos respetan `workAreaStart_`/`workAreaEnd_` y `durationFrames_`.

### 3.3 Layer (`layer.hpp`)

Jerarquia de capas con tipo polimorfico:

```
Layer (abstracta)
├── RasterLayer — Buffer de pixeles ARGB32, dimensiones variables
├── VectorLayer — Coleccion de VectorStroke con BezierPath
├── GroupLayer — Compuesto con children (vector<unique_ptr<Layer>>)
├── AudioLayer — Datos de audio
└── CameraLayer — Transformaciones de camara
```

**Propiedades comunes de Layer**: `uid` (uint64 unico), `name`, `visible`, `opacity` (0-1), `blendMode` (12 modos), `locked`.

**RasterLayer**:
- `pixelBuffer_` (`shared_ptr<PixelBuffer>`) — Buffer COW con `vector<uint32_t>` ARGB32
- `originX_`, `originY_` — Offset del buffer respecto al canvas
- `hasContent_` — Flag O(1) para deteccion de contenido (sin escanear pixeles)
- `ensureContains(x,y,w,h,pad)` — Expansion del buffer con guard band de 2px
- `ensureUnique()` — Deep copy COW si el buffer es compartido

### 3.4 ToolState (`tool_state.hpp`)

37 propiedades con Q_PROPERTY y signals Qt para binding bidireccional con UI:

| Categoria | Propiedades |
|-----------|-------------|
| Herramienta | `activeTool` (17 valores enum ToolType) |
| Pincel | `brushSize`, `brushOpacity`, `brushHardness`, `brushShape`, `pressureSize`, `pressureOpacity`, `stabilizerLevel` |
| Color | `primaryColor`, `sampledColor`, `colorVariationType`, `colorVariationCount` |
| Canvas | `canvasWidth`, `canvasHeight` |
| Onion Skin | `onionEnabled`, `onionPrevFrames`, `onionNextFrames`, `onionOpacity` |
| Relleno | `fillType` |
| Linea | `lineStyle` |
| Texto | `textString`, `textFont`, `textLeading`, `textTracking`, `textAlignment`, `textAntiAliasing`, `textUnderline`, `textStrikethrough` |

### 3.5 UndoManager (`undo_manager.hpp`)

Pila undo/redo por secuencia con:
- Profundidad maxima: 100 comandos
- `UndoCommand` abstracto con `execute()`, `undo()`, `redo()`
- `UndoGroup` para agrupar multiples comandos atomicos
- Comandos especializados: `PaintCommandV2`, `LayerModifyCommand`, `CutFrameCommand`, `PasteFrameCommand`, `CopyLayerToFramesCommand`

### 3.6 Marker (`sequence.hpp`)

Marcadores estilo After Effects:

```cpp
struct Marker {
    int frame = 0;          // Posicion 0-based
    int duration = 0;       // 0 = punto simple, >0 = rango
    std::string comment;    // Titulo visible en ruler
    std::string detail;     // Notas largas (dialogo)
    int colorLabel = 0;     // 0-8: 9 colores
    int64_t uid = 0;        // ID unico
};
```

### 3.7 LineBoil (`sequence.hpp`)

Efecto no destructivo de linea vibrante:

```cpp
struct LineBoil {
    bool enabled = false;
    float strength = 2.0f;  // 0-10 pixeles max desplazamiento
    int seed = 42;
};
```

### 3.8 Tipos Fundamentales (`types.hpp`)

| Tipo | Descripcion |
|------|-------------|
| `Color` | RGBA punto flotante (0-1) con soporte premultiplicado |
| `Vec2` | Vector 2D con operaciones aritmeticas |
| `Rect` | Rectangulo con interseccion y union |
| `StrokePoint` | Punto de stroke: posicion, presion, tilt, rotacion, timestamp |
| `LayerType` | enum: Raster, Vector, Group, Audio, Camera |
| `BlendMode` | enum: 12 modos de fusion |
| `ToolType` | enum: 17 herramientas |
| `BrushMode` | enum: Raster, Vector, Hybrid |
| `FillType` | enum: Solid, Fabric, Ramp |

---

## 4. CAPA ENGINE

### 4.1 Brush Engine (`engine/brush/`)

Motor de pincel con estado de stroke. Procesa puntos de entrada y genera stamps de pincel.

| Archivo | Clase | Funcion |
|---------|-------|---------|
| `brush_engine.hpp/cpp` | `BrushEngine` | Maquina de estados del pincel: beginStroke/addPoint/endStroke |
| `brush_preset.cpp` | — | Factory de presets: Round Soft, Round Hard, Flat, Calligraphy, Eraser |
| `paper_texture.cpp` | — | Simulacion de grano de papel (escala de grises modula opacidad) |
| `abr_importer.hpp/cpp` | `ABRImporter` | Importacion de brushes Photoshop (.abr v1, v2, v6+) |
| `pencil_retouch.hpp/cpp` | `PencilRetouchEngine` | Retoque de lapiz: ajuste de grosor, suavizado |
| `ruler_guide.hpp/cpp` | `RulerTool` | Guias de regla: lineal, radial, arco, snapping |

**Tipos clave**:
- `BrushTip` — Imagen de stamp, spacing, hardness, angulo, redondez
- `BrushDynamics` — Curvas de respuesta a presion/tamano/opacidad/flow, tilt
- `BrushPreset` — Compuesto de tip + dynamics + color + blend mode + textura

### 4.2 Raster Engine (`engine/raster/`)

Motor de renderizado de pixeles.

| Archivo | Clase | Funcion |
|---------|-------|---------|
| `raster_engine.hpp/cpp` | `RasterEngine` | Renderizado de stamps de pincel en buffer de pixeles |
| `raster_stroke.cpp` | `RasterStroke` | Serie de stamps conectados con blending |
| `raster_effect.hpp/cpp` | — | `applyLineBoil()` — desplazamiento bilineal por ruido 8x8 celdas |

### 4.3 Vector Engine (`engine/vector/`)

Motor de trazados vectoriales.

| Archivo | Clase | Funcion |
|---------|-------|---------|
| `bezier_path.hpp/cpp` | `BezierPath` | Path Bezier cubico: moveTo, lineTo, curveTo, evaluateAt, tangentAt, flattenPoints |
| `vector_engine.hpp/cpp` | `VectorEngine` | Gestion de strokes vectoriales, simplificacion de paths |
| `vector_stroke.cpp` | `VectorStroke` | Par (BezierPath + atributos de render: color, ancho, opacidad) |

### 4.4 Compositor (`engine/compositor/`)

Motor de composicion de capas.

| Archivo | Clase | Funcion |
|---------|-------|---------|
| `compositor.hpp/cpp` | `Compositor` | Composicion de GroupLayer (painter's algorithm: bottom→top) con blend modes |
| `node_graph.hpp/cpp` | `NodeGraph` | DAG de nodos de composicion: InputNode, BlendNode, FilterNode, OutputNode |

### 4.5 Deformation (`engine/deformation/`)

Motor de deformacion de malla.

| Archivo | Clase | Funcion |
|---------|-------|---------|
| `deformation_mesh.hpp/cpp` | `DeformationMesh` | Malla 2D con puntos de control, `mapPointBilinear()` |
| `deformation_engine.hpp/cpp` | `DeformationEngine` | Aplicacion de mallas de deformacion a capas |

### 4.6 Animation (`engine/animation/`)

Subsistemas de animacion.

| Archivo | Clase | Funcion |
|---------|-------|---------|
| `audio_engine.hpp/cpp` | `AudioEngine` | Motor de audio con QMediaPlayer |
| `audio_file_decoder.hpp/cpp` | — | `decodeAudioFile()` via dr_libs (WAV/MP3/FLAC) → `AudioDecodeResult` |
| `dr_wav.h`, `dr_mp3.h`, `dr_flac.h` | — | Librerias nativas C (David Reid, dominio publico) |
| `tween_engine.hpp/cpp` | `TweenEngine` | Interpolacion de keyframes: Linear, EaseIn/Out/InOut |
| `frame_thumbnail.hpp/cpp` | `FrameThumbnailCache` | Cache LRU de miniaturas de frame |
| `video_decoder.hpp/cpp` | — | `decodeVideoFrame()` via FFmpeg, `probeVideoMetadata()` via ffprobe, cache LRU 50 frames |

### 4.7 Common (`engine/common/`)

| Archivo | Funcion |
|---------|---------|
| `blend_utils.hpp` | `unpackPixel()` / `packPixel()` — conversion ARGB32 ↔ struct. `blendPixels()` con blend modes. |

---

## 5. PIPELINE DE RENDERIZADO

### Arquitectura de 4 Buffers (CPU)

El canvas usa un pipeline de renderizado CPU con separacion estricta DATA/DISPLAY:

| Buffer | Dominio | Contenido |
|--------|---------|-----------|
| `RasterLayer::pixelBuffer_` | DATA | Pixeles de capa, fondo transparente, fuente de verdad |
| `strokeBuffer_` | DATA | Stroke actual aislado, ARGB32 transparente |
| `backgroundCache_` | DISPLAY | Pre-composicion: fondo blanco + onion skin + capas + video |
| `paintEvent` QPainter | DISPLAY | Composicion final: cache + stroke overlay + grid + UI |

### Invariantes del Pipeline

1. `mouseReleaseEvent` NUNCA lee de buffers DISPLAY (solo `strokeBuffer_` y `pixelBuffer_`)
2. Dabs se escriben en `strokeBuffer_` aislado durante el stroke, se compositan a `pixelBuffer_` en commit
3. `buildBackgroundCache(rect)` soporta rebuilds completos y parciales (offset-aware dirty rect)
4. Padding dinamico: `brushSize/2 + 1` para cubrir feathering de dabs
5. Capas con `originX_`/`originY_` usan `rect.translated(-originX, -originY)` — sin rebuild completo forzado
6. Si hay video tracks activos, partial rebuild escala a full rebuild (el video necesita contexto completo)

### Flujo de Renderizado

```
paintEvent()
    ├── backgroundCache_ (ya pre-computado por buildBackgroundCache)
    │   ├── Fondo blanco
    │   ├── Onion skin frames (semi-transparentes, coloreados)
    │   ├── Capas de dibujo (bottom→top, con blend modes y opacidad)
    │   └── Video frames (sobre capas de dibujo, con opacidad)
    ├── strokeBuffer_ (stroke actual en progreso, overlay)
    ├── Grid (opcional, 64x64)
    └── UI overlays (cursor de herramienta, seleccion, etc.)
```

---

## 6. CAPA UI V2

### 6.1 MainWindowV2 (`main_window_v2.hpp/cpp`)

Ventana principal con:
- **Tema oscuro**: `#252830` paneles, `#000000` toolbar/titlebar, `#FF4800` accent naranja
- **8 QDockWidgets** desacoplables (Movable | Floatable) con titulos naranja
- **Toolbar superior**: New, Open, Save, Undo, Redo, Flip H, Rotate ±15°, Fit, Grid, Export Video/GIF, Help
- **Centralizado de signals**: Conexion de todos los docks al AppState central
- **Gestion de archivos**: `newProject()`, `openProject()`, `saveProject()`, `saveProjectAs()`
- **BusyOverlay**: Overlay de espera con spinner animado durante operaciones bloqueantes
- **closeEvent()**: Dialogo Save/Discard/Cancel si hay cambios sin guardar
- **File association**: Registro de .fap en Windows Registry via `FileAssociation`
- **Audio/Video sync**: `syncAudioToDocument()` / `syncVideoToDocument()` capturan estado antes de save

### 6.2 Layout de Docks

| Posicion | Dock | Widget | Contenido |
|----------|------|--------|-----------|
| Left | TOOLS | ToolboxPanelV2 | 17 botones de herramienta, 36px icons |
| Left | ONION SKIN | OnionSkinPanel | Checkbox + spinners prev/next + slider opacidad |
| Right | LAYERS | LayerPanelV2 | Lista de capas con visibilidad/bloqueo/blend mode/reorden |
| Right | COLOR | ColorPanelV2 | QColorDialog + 9 circulos MRU |
| Right | PROPERTIES | PropertyEditorV2 | Editor contextual: brush, color, fill, texto, fuente |
| Right | CANVAS SIZE | CanvasSizePanel | Width × Height + boton Apply |
| Center | CANVAS | CanvasWidgetV2 | Superficie de dibujo con pipeline 4-buffer |
| Bottom | TIMELINE | TimelinePanelV2 | Multi-secuencia, frames, audio, video, markers, WA |

### 6.3 CanvasWidgetV2 (`canvas_widget_v2.hpp/cpp`)

Superficie central de dibujo. Componente mas grande del proyecto (~2500+ lineas).

**Herramientas implementadas (17)**:

| ToolType | Funcion |
|----------|---------|
| Brush | Dibujo a mano alzada con presion, estabilizador, punta configurable |
| Eraser | Borrado con modo DestinationOut |
| ColorPicker | Cuentagotas con lupa de preview |
| Fill | Relleno con tolerancia (Solid, Fabric, Ramp) |
| Text | Texto multi-linea con fuente, leading, tracking, alineacion |
| Line | Linea recta con snapping y estilos |
| Rectangle | Rectangulo relleno/borde |
| Ellipse | Elipse relleno/borde |
| Move | Desplazamiento de capa |
| Select | Seleccion rectangular con operaciones |
| Hand | Pan de canvas (middle-click + drag) |
| PencilRetouch | Retoque de grosor de trazos |
| RulerLine | Regla lineal con snapping |
| RulerEllipse | Regla eliptica con snapping |
| DeformMesh | Malla de deformacion |
| TweenEdit | Edicion de interpolacion |

**Tableta grafica**: `WA_TabletTracking` + `tabletEvent()` override. Sintetiza `QMouseEvent` desde `QTabletEvent` para despachar a handlers de mouse. Soporta presion (`tabletPressure_` 0.0-1.0), deteccion de borrador (`tabletEraser_`), y fallback a mouse.

**Gestion de capas**: `pushFullLayerUndo()` con `hadContentBefore` para restaurar correctamente `hasContent_` en undo/redo.

### 6.4 Paneles de UI

| Panel | Funcionalidad |
|-------|--------------|
| **ToolboxPanelV2** | 17 botones de herramienta (36px), QButtonGroup exclusivo. Tight fit: 52px ancho minimo, sin scroll. |
| **ColorPanelV2** | Selector de color QColorDialog + 9 circulos de colores recientes (MRU). |
| **LayerPanelV2** | Lista de capas (QListWidget). Visibilidad (ojo), bloqueo (candado), nombre editable (QLineEdit con QPointer guards), blend mode (QComboBox), opacidad (QSlider). Botones: nueva raster/vector, duplicar, copiar a todos los frames, mover arriba/abajo, eliminar. |
| **PropertyEditorV2** | Editor sensible a herramienta activa. Muestra: brush size/opacity/hardness/shape/pressure/stabilizer, color picker, fill type, text font/leading/tracking/alignment, line style. |
| **OnionSkinPanel** | Checkbox enabled, QSpinBox prev frames (0-10), QSpinBox next frames (0-10), QSlider opacity. Emite `settingsChanged()`. |
| **CanvasSizePanel** | QSpinBox width, QSpinBox height (1-8192), boton Apply. Emite `canvasResized(w,h)`. |
| **BusyOverlay** | Widget overlay semi-transparente con spinner animado 12-lineas naranja. Bloquea mouse/keyboard via eventFilter. Timer 150ms keep-alive durante processEvents. |

---

## 7. SISTEMA DE TIMELINE

### 7.1 TimelinePanelV2 (`timeline_panel_v2.hpp/cpp`)

Panel de timeline con arquitectura de rebuild no destructivo:

**Estructura de layout**:
```
Top bar (fijo)
  ├── Botones de secuencia: ◀ Sequence ▼ ▶  +  Duplicate  ✕
  ├── FPS: [−] [24] [+]  |  Loop checkbox
  ├── Work Area/Duration: [WA In] [WA Out] [Duration]
  └── Botones: ◆ Marker  ± Frame  + Track ▾

Ruler (fijo, 22px altura)
  ├── Ticks de frame
  ├── Work Area shading (naranja semi-transparente)
  ├── Marcadores (triangulos coloreados + bandas de duracion + titulos)
  └── Playhead (linea roja vertical)

QScrollArea [tracksContainer_ con QVBoxLayout]
  ├── SequenceTrackWidget[] (desde Document, orden de indice)
  ├── AudioTrackWidget[] (preservados, movimiento libre)
  ├── VideoTrackWidget[] (preservados, movimiento libre)
  └── QSpacerItem (stretch)

QScrollBar horizontal (fijo, scroll compartido)
```

### 7.2 Rebuild No Destructivo

| Componente | Dominio | Comportamiento en rebuild |
|-----------|---------|--------------------------|
| `SequenceTrackWidget` | Core (`Document::sequences()`) | Destruido y recreado desde AppState |
| `AudioTrackWidget` | UI (`audioTrackWidgets_`) | Extraido con `removeWidget()`, preservado, reinsertado (nunca delete) |
| `VideoTrackWidget` | UI (`videoTrackWidgets_`) | Extraido con `removeWidget()`, preservado, reinsertado (nunca delete) |

### 7.3 SequenceTrackWidget

Widget de track de secuencia con:
- **Header**: Nombre, visibilidad (ojo), opacidad (slider), bloqueo (candado rojo #FF4A4A)
- **Celdas de frame**: Render hasta `durationFrames_`. Celdas con contenido = icono de capa, vacias = gris tenue, ocultas = dim. Locked = overlay rojo semi-transparente.
- **Line Boil**: Checkbox "Boil" + slider 0-100
- **Frame +/- buttons**: Mostrar/ocultar frames (no destructivo: `durationFrames_` ≤ `totalFrames_`)

### 7.4 AudioTrackWidget

Pista de audio con:
- **Header**: Nombre, botones ▲▼ (mover), mute (SVG), volumen (slider), ✕ (eliminar)
- **Waveform**: Visualizacion de forma de onda decodificada via dr_libs (800 peaks)
- **Playback**: QMediaPlayer sincronizado con timeline (rate = fps/24.0)
- **Layout position**: Persistido en .fap via `AudioTrackData::layoutPosition`

### 7.5 VideoTrackWidget

Pista de video con:
- **Header**: Nombre, botones ▲▼ (mover), mute, volumen, opacidad (slider), ✕ (eliminar)
- **Thumbnail strip**: 1 thumbnail cada 30 frames, pre-decodificado a 320x180
- **Decodificacion**: FFmpeg on-demand con cache LRU de 50 frames full-res
- **Composicion**: Video frames sobre capas de dibujo, opacidad via QPainter::setOpacity()
- **Layout position**: Persistido en .fap via `VideoTrackData::layoutPosition`

### 7.6 RulerWidget

Regla de tiempo con:
- **Ticks**: Numerados cada 5 frames, marcas finas cada frame
- **Work Area**: Sombreado naranja `rgba(255,72,0,80)` entre in/out points
- **Marcadores**: Triangulos de 8-12px coloreados (9 colores), bandas de duracion, titulos blancos 7px bold sobre pill oscuro
- **Interacciones**:
  - Hover ±5px de borde WA → cursor SizeHorCursor (redimension)
  - Click en borde WA → drag para redimensionar in/out
  - Click en ruler → set playhead + drag = scrubbing
  - Doble click en marker → dialogo Marker Settings
  - Ctrl+click en marker → eliminar

### 7.7 Work Area y Duracion

| Concepto | Variable | Significado |
|----------|----------|-------------|
| Total Frames | `totalFrames_` | Frames con datos de pixel |
| Duration Frames | `durationFrames_` | Frames visibles (≤ totalFrames) |
| Work Area In | `workAreaStart_` | Inicio del loop/playback |
| Work Area Out | `workAreaEnd_` | Fin del loop (0 = usar duracion) |

- `effectiveWorkAreaEnd()` = `workAreaEnd_ > 0 ? min(workAreaEnd_, durationFrames_) : durationFrames_`
- `advanceFrame()` avanza dentro del work area, wrappea al WA start al final
- Audio transport sync: FPS change → `setPlaybackRate(fps/24.0)` en todos los tracks
- Timer: `round(1000.0 / fps)` ms para precision de punto flotante

### 7.8 Frame Hiding No Destructivo

| Boton | Accion |
|-------|--------|
| `+` | Revela frame oculto si existe; crea nuevo solo si todos visibles |
| `-` | Reduce `durationFrames_` — oculta el ultimo frame visible, preserva datos |
| Delete (click derecho) | Unica forma de eliminar permanentemente datos de frame |

---

## 8. CAPA IO — ENTRADA/SALIDA

### 8.1 DocumentManager (`document_manager.hpp/cpp`)

Manejador principal del formato .fap v2 (ZIP binario via miniz).

**Metodos principales**:

| Metodo | Funcion |
|--------|---------|
| `save(Document&)` | Guardado atomico: escribe a .tmp, rename al final |
| `load(Document&, path)` | Carga completa: manifiesto, timeline, capas, audio, video |
| `commitAtomic(tmp, final)` | Rename atomico con fallback Windows (remove + retry) |

**Save path**:
1. `writeManifest()` — version, canvas, active_sequence, viewport, markers
2. `writeTimeline()` — frames[], layers[], audio[], video[] con metadatos
3. `writeLayerData()` — PNGs de capas + JSONs de metadata + audio/video bytes

**Load path**:
1. `readManifest()` — restaura canvas, viewport, secuencias, markers
2. `readTimeline()` — restaura frames, capas, audio/video metadata
3. `readLayerData()` — carga PNGs → `RasterLayer::pixelData()` (con `pad=false`)
4. `extractAudio()` — extrae ZIP entries a `%TEMP%/fap_audio_<PID>/`

**Caracteristicas**:
- **Save atomico**: Escribe a .tmp, rename al final. Si rename falla en Windows (target existe), remove + retry.
- **Pixel deduplication**: Buffers compartidos detectados via `shared_ptr::use_count()`, escritos una sola vez
- **Audio embedding**: Archivos de audio incrustados como `audio/track_N.ext` en el ZIP
- **Video embedding**: Archivos de video incrustados como `video/track_N.ext` en el ZIP
- **Path sanitization**: `QFileInfo(fileName).fileName()` en extractAudio previene path traversal
- **ProcessEvents**: Cada 20 capas durante load mantiene UI responsivo
- **Layout position**: Audio/video tracks persisten su posicion en el layout

### 8.2 FileFormat (`file_format.cpp`)

Legacy format v1/v2/v3 (directorios) para backward compatibility.

| Version | Estructura | Estado |
|---------|-----------|--------|
| v1 | `frames/L0_0.png` + `project.json` | Legacy |
| v2 | `frames/L{layer}_{frame}.png` + `manifest.json` | Legacy |
| v3 | `layers/S0/` subdirectorios | Legacy |

### 8.3 SerializationCommon (`serialization_common.hpp/cpp`)

Conversiones enum ↔ string compartidas: `LayerType`, `BlendMode`.

---

## 9. EXPORTACION DE VIDEO Y GIF

### 9.1 Video Export (`video_export.hpp/cpp`)

API publica unificada con auto-deteccion de formato por extension:

| Funcion | Proposito |
|---------|-----------|
| `renderExportFrame(doc, frameIdx, opts)` | Renderiza un frame a QImage para export |
| `exportVideo(doc, outputPath, fps)` | Exporta video con FFmpeg |
| `exportGIF(doc, outputPath, fps)` | Exporta GIF animado con FFmpeg |
| `exportPNGSequence(doc, outputDir, fps)` | Exporta secuencia de PNGs |

**Formatos soportados** (auto-detectados por extension):

| Extension | Codec Video | Pix Format | Codec Audio | Background |
|-----------|------------|------------|-------------|------------|
| `.mp4` | libx264 | yuv420p | aac | Blanco opaco |
| `.mov` | qtrle | argb | pcm_s16le | Transparente (alpha) |
| `.webm` | libvpx-vp9 | yuva420p | libopus | Transparente (alpha) |

**Audio mixing**: Pistas no muteadas con volumenes individuales → `amix` filter de FFmpeg, `-shortest` para duracion.

**ExportOptions struct**: `width`, `height` (escalado independiente de canvas), `transparentBg`.

**Timeout**: `proc.waitForFinished(120000)` — 120 segundos maximo, `.kill()` si se excede.

### 9.2 GIF Export

- Usa paleta de 256 colores con dithering
- Exporta a resolucion completa de canvas (no hardcodeado)
- Indexado de frames corregido (0-based)

### 9.3 SVG Export (`svg_export.hpp/cpp`)

Exporta frames individuales o secuencia multi-frame a formato SVG.

---

## 10. CAPA PLATFORM

### 10.1 FileAssociation (`platform/file_association.hpp/cpp`)

Registro de asociacion de archivos .fap en Windows:

- `registerFileAssociation()` — Crea entradas en `HKCU\Software\Classes\.fap` y `HKCU\Software\Classes\FAP.Document`
- Llamado desde `MainWindowV2` constructor al inicio
- Icono: `fap.ico` incrustado en el .exe via `embed_icon.py` post-build

### 10.2 Tablet Input

Manejo nativo de tabletas graficas via Qt 6 `QTabletEvent` en `CanvasWidgetV2`:
- `WA_TabletTracking` habilitado
- `tabletEvent()` override: TabletPress → MouseButtonPress, TabletMove → MouseMove, TabletRelease → MouseButtonRelease
- `QApplication::sendEvent()` para despachar a handlers de mouse (garantiza procesamiento correcto de undo)
- `tabletPressure_` (0.0-1.0) modula tamano y opacidad del pincel
- `tabletEraser_` detecta borrador via `QPointingDevice::PointerType::Eraser`
- Marcas soportadas: Wacom, Huion, Xencelabs, XP-Pen

---

## 11. SISTEMA DE BUILD

### 11.1 CMakeLists.txt (304 lineas → 286 lineas post-limpieza)

```cmake
cmake_minimum_required(VERSION 3.20)
project(FreeAnimationPower VERSION 1.0.0 LANGUAGES CXX)
```

**Targets**:
- `free-animation-power` — Ejecutable principal
- `fap-tests` — Test suite con GoogleTest

**Dependencias**:

| Dependencia | Version | Tipo |
|-------------|---------|------|
| Qt6::Core, Qt6::Gui, Qt6::Widgets, Qt6::Multimedia, Qt6::Svg | 6.5+ | find_package |
| miniz | 3.0.2 | FetchContent (static lib) |
| GoogleTest | 1.14.0 | FetchContent (solo si FAP_BUILD_TESTS=ON) |
| FFmpeg | 4.0+ | Externo (runtime, no linkeado) |
| dwmapi | — | Windows solo (linkeado) |

**Opciones CMake**:
- `FAP_BUILD_TESTS` (ON/OFF)
- `FAP_DIAGNOSTIC_TRACER` (ON/OFF) — tracer de sesion para diagnostico

**Post-build (Windows)**:
1. `embed_icon.py` — Inyecta `fap.ico` como recurso Win32 en el .exe
2. `windeployqt` — Despliega DLLs de Qt

**Estructura de fuentes**:

| Grupo | Headers | Sources |
|-------|---------|---------|
| CORE | document, sequence, layer, types, tool_state, app_state, undo_manager, tracer* | = |
| ENGINE | raster_engine, raster_effect, vector_engine, bezier_path, brush_engine, ruler_guide, pencil_retouch, abr_importer, blend_utils, compositor, node_graph, deformation_mesh, deformation_engine, audio_engine, tween_engine, frame_thumbnail, audio_file_decoder, video_decoder | = + raster_stroke, vector_stroke, brush_preset, paper_texture |
| UI | main_window_v2, canvas_widget_v2, timeline_panel_v2, toolbox_panel_v2, color_panel_v2, layer_panel_v2, layer_lock_button, property_editor_v2, audio_track_widget, video_track_widget, onion_skin_panel, canvas_size_panel, busy_overlay | = |
| IO | document_manager, serialization_common, svg_export, video_export | = + file_format |
| PLATFORM | file_association | = |

---

## 12. FORMATO DE ARCHIVO .FAP

### ZIP Structure (v2)

```
proyecto.fap (ZIP renombrado)
├── manifest.json       — version, canvas size, active_sequence, viewport, markers[]
├── timeline.json       — per-sequence: frames[], layers[], audio[], video[]
├── audio/track_N.ext   — archivos de audio incrustados
├── video/track_N.ext   — archivos de video incrustados
└── layers/
    └── S{seq}/
        └── frame_{f}/
            ├── layer_{ll}.png   — Datos de pixeles (PNG comprimido)
            └── layer_{ll}.json  — Metadatos de capa
```

### manifest.json

```json
{
  "version": 3,
  "canvas": { "width": 1920, "height": 1080 },
  "active_sequence": 0,
  "viewport": { "zoom": 1.0, "offsetX": 0, "offsetY": 0 },
  "sequences": [
    {
      "name": "Sequence 1",
      "uid": 123456789,
      "fps": 24,
      "totalFrames": 48,
      "visible": true,
      "opacity": 1.0,
      "locked": false,
      "workAreaStart": 0,
      "workAreaEnd": 0,
      "durationFrames": 48,
      "looping": false,
      "currentFrame": 0,
      "line_boil_enabled": false,
      "line_boil_strength": 2.0,
      "markers": [
        {
          "frame": 12,
          "duration": 6,
          "comment": "Climax",
          "detail": "La escena culminante del acto 2",
          "color": 1
        }
      ]
    }
  ]
}
```

### timeline.json

```json
{
  "sequences": [
    {
      "frames": [
        {
          "index": 0,
          "layers": [
            {
              "type": "raster",
              "name": "LINEA",
              "visible": true,
              "opacity": 1.0,
              "blend_mode": "normal",
              "locked": false,
              "origin_x": 0,
              "origin_y": 0,
              "has_content": true,
              "pixel_file": "layers/S0/frame_0/layer_0.png",
              "meta_file": "layers/S0/frame_0/layer_0.json"
            }
          ]
        }
      ]
    }
  ],
  "audio": [
    {
      "filepath": "C:/Users/.../song.mp3",
      "display_name": "song.mp3",
      "zip_entry": "audio/track_0.mp3",
      "muted": false,
      "volume": 80,
      "layout_position": 1
    }
  ],
  "video": [
    {
      "filepath": "C:/Users/.../clip.mp4",
      "display_name": "clip.mp4",
      "zip_entry": "video/track_0.mp4",
      "width": 1920,
      "height": 1080,
      "fps": 30,
      "total_frames": 1800,
      "muted": false,
      "volume": 80,
      "opacity": 1.0,
      "layout_position": 2
    }
  ]
}
```

### Caracteristicas Clave

- **Atomic save**: Escritura a .tmp, rename al finalizar. Si rename falla en Windows (target existente), se hace remove + retry.
- **Pixel deduplication**: Capas que comparten `pixelBuffer_` (clonadas) se escriben una sola vez. Deteccion via `shared_ptr::use_count()` > 1.
- **ViewState persistido**: Zoom, offsetX, offsetY guardados en manifiesto.
- **11 campos de metadata**: Por secuencia: name, fps, totalFrames, visible, opacity, locked, workAreaStart/End, durationFrames, looping, currentFrame.
- **2 campos de LineBoil**: enabled, strength.
- **Markers**: Array JSON con frame, duration, comment, detail, color.
- **Layer metadata**: origin_x/y, hasContent, opacity, blendMode, locked.
- **Audio embedding**: Archivos incrustados como ZIP entries, extraidos a temp en load.
- **Video embedding**: Archivos incrustados como ZIP entries, extraidos a temp en load.
- **Layout position**: Posicion en timeline persistida para audio/video tracks.
- **Backward compatible**: Formato legacy v1/v2/v3 (directorios) cargable via `file_format.cpp`.

---

## 13. HISTORIAL DE BUGS Y SOLUCIONES

### Bugs Criticos Corregidos

| ID | Bug | Root Cause | Fix | Version |
|----|-----|-----------|-----|---------|
| C1 | ABR importer endian swap roto | `uint16_t` promovido a `int` antes de bit-shift | Cast a `uint16_t` despues de shifts | v2.6.1 |
| C2 | Deformacion mesh bilinear Y incorrecto | `p11.y * fy * fy` en vez de `p11.y * fx * fy` | Corregido factor de peso | v2.6.1 |
| C3 | `smoothstep()` NaN en hardness=1.0 | Division 0/0 cuando edges iguales | Guard `if (edge0 == edge1) return (x >= edge1) ? 1.0f : 0.0f` | v2.6.1 |
| C4 | Pixel byte order conflict (R/B swap) | `blend_utils.hpp` usaba 0xAABBGGRR, raster/qimage usaban 0xAARRGGBB | Estandarizado a 0xAARRGGBB (Qt native) | v2.6.1 |
| C5 | Path traversal en audio extraction | `displayName` usado como filename sin sanitizar | `QFileInfo(rawName).fileName()` | v2.6.1 |
| H3 | Atomic save data loss (TOCTOU) | `commitAtomic` eliminaba target antes de rename | Remove condicional solo si rename falla | v2.6.1 / v2.6.2 |
| — | Pixel offset en load | `ensureContains(pad=true)` expandia buffer en cada load | `pad=false` en load path | v2.5.1 |
| — | Tool state desync en load | `resetDocument()` → eraser, pero UI mostraba brush | `toolbox_panel_->setActiveTool(0)` despues de load | v2.5.1 |
| — | Multi-seq data loss en load | `readTimeline()` creaba secuencias duplicadas | Usar `sequenceAt(si)` en vez de `addSequence()` | v2.5.2 |
| — | Eraser no produce efecto | `strokeBuffer_` inicializado transparente, blend `dst * (255-srcA)/255` siempre 0 | Unificar blend en stamp accumulation; distincion en commit | v2.6 |
| — | Undo/redo no refresca timeline/layers | `undo()` y `redo()` no emitian `canvasUpdated` | `emit canvasUpdated()` en ambos | v2.7 |

### Bugs Altos Corregidos

| ID | Bug | Fix | Version |
|----|-----|-----|---------|
| H1 | `pushFullLayerUndo` sin `hadContentBefore` (6 call sites) | Pasar `hadBefore` como 3er argumento | v2.6.1 |
| H2 | Stale sequence callbacks | Limpiar callbacks de TODAS las secuencias al hacer switch | v2.6.1 |
| H7 | Compositor z-order invertido | `begin()/end()` en vez de `rbegin()/rend()` | v2.6.1 |
| H8 | NodeGraph dangling pointers tras remove | Segundo loop para limpiar `outputs_` back-references | v2.6.1 |
| — | Layer rename crash (use-after-free) | `QPointer` guards en lambda de rename | v2.5.2 |
| — | Layer names overlap visual | `removeItemWidget()` + `deleteLater()` antes de `clear()` | v2.6 |
| — | Playback timer imprecision (FPS drift) | `round(1000.0/fps)` en vez de `1000/fps` | v2.6 |
| — | Video + drawing partial rebuild (rectangulos blancos) | Escalar a full rebuild si hay video tracks | v2.8 |
| — | Tablet event no despacha dibujo | Sintetizar QMouseEvent y despachar via sendEvent() | v2.9 |

### Bugs Medios Corregidos

| Bug | Fix | Version |
|-----|-----|---------|
| Close sin confirmacion de cambios | `closeEvent()` con Save/Discard/Cancel | v2.5.2 |
| FFmpeg `waitForFinished(-1)` hang | Timeout 120s + `.kill()` | v2.5.2 |
| `NodeGraph::evaluate()` ignora target | Copiar OutputNode → target via memcpy | v2.5.2 |
| Legacy load omite `setHasContent(true)` | Agregar flag en 3 load paths legacy | v2.5.2 |
| Layer rename perdido en focus change | `editingFinished` + `committed` guard | v2.5.2 |
| BusyOverlay crash (filter doble) | `filterInstalled_` guard flag | v2.9.1 |
| Video decoder recursion guard | `thread_local s_inDecoder` flag | v2.9.1 |
| Audio/video layout position lost on save/load | `layoutPosition` field en Data structs | v2.8 |

---

## 14. SISTEMA DE TESTS

### 14.1 Suite de Tests (160 tests, 16 archivos)

| Archivo | Tests | Modulo cubierto |
|---------|-------|-----------------|
| `test_document.cpp` | 8+ | Document, Sequence, canvas resize, markers, work area, line boil |
| `test_layer.cpp` | 10+ | RasterLayer: buffer, pixel read/write, blend, clear, ensureContains, clone, hasContent |
| `test_layer_memory.cpp` | 12+ | UID inmutable, buffer epoch, COW shareDataFrom/clone/ensureUnique, pixel dedup, GroupLayer |
| `test_compositor.cpp` | 8+ | Compositor: single/multi layer, z-order, blend modes |
| `test_undo.cpp` | 9+ | UndoManager: execute/undo/redo, depth limit, multi-seq isolation, merge, UndoGroup |
| `test_brush_engine.cpp` | 3+ | BrushEngine: presets, begin/add/end stroke |
| `test_bezier.cpp` | 4+ | BezierPath: evaluateAt, flattenPoints, length |
| `test_file_format.cpp` | 6+ | Save/load round-trip, layer names, ZIP binary, multi-seq |
| `test_audio_engine.cpp` | 8+ | AudioTrack: mute/solo, volume clamp, add/remove clips |
| `test_deformation.cpp` | 6+ | DeformationMesh: bounds, mapPointBilinear, original/current |
| `test_node_graph.cpp` | 10+ | NodeGraph: create, connect, disable, delete, evaluate, dangling ptr cleanup |
| `test_ruler_guide.cpp` | 7+ | RulerTool: linear/radial guides, snap, arc |
| `test_tween_engine.cpp` | 10+ | applyEasing, interpolation float/int/color, property tween |
| `test_pencil_retouch.cpp` | 5+ | PencilRetouch: modes, strength, stroke detection |
| `test_abr_importer.cpp` | 3+ | ABR import: file validation, brush tip conversion |
| `test_memory_stress.cpp` | 15 | Stress: 2000 layers, 500-frame loop, alloc/dealloc, dedup, save/load, multi-seq |

### 14.2 Ejecucion

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build
```

Los tests compilan contra los mismos `.cpp` de implementacion que el ejecutable principal (`CORE_SOURCES`, `ENGINE_SOURCES`, `IO_SOURCES`, `PLATFORM_SOURCES`), garantizando que las pruebas ejercen el codigo real.

---

## 15. ESTRUCTURA DEL PROYECTO

```
FREE ANIMATION POWER/
├── CMakeLists.txt              — Build system (286 lineas)
├── CMakePresets.json           — Presets de CMake
├── README.md                   — README principal (espanol, 706 lineas)
├── AGENTS.md                   — Instrucciones para agentes AI (1022 lineas)
├── CONTEXT.md                  — Glosario de dominio (36 lineas)
├── CHANGELOG.md                — Historial de versiones (269 lineas)
├── LICENSE                     — GPLv3
├── .clang-format               — Formateador de codigo
├── .editorconfig               — Configuracion de editor
├── .gitignore                  — Reglas de ignorados
├── .pre-commit-config.yaml     — Hooks pre-commit
├── run.bat                     — Script de ejecucion rapida
├── skills-lock.json            — Lock de skills
│
├── _design_source/             — Archivos de diseno fuente (no en build)
│   ├── icons/                  — PNGs fuente del disenador + .ai
│   ├── logos/                  — Logos .ai/.psd/.png fuente
│   ├── fap_traces/             — Trazas de diagnostico historicas
│   └── free_animation_power.py — Prototipo Python original v0.2
│
├── src/
│   ├── main.cpp                — Punto de entrada (172 lineas)
│   │
│   ├── core/                    — Modelo de datos (11 archivos .hpp/.cpp)
│   │   ├── types.hpp/cpp        — Tipos fundamentales, enums
│   │   ├── document.hpp/cpp     — Contenedor raiz del proyecto
│   │   ├── sequence.hpp/cpp     — Secuencia con frames, markers, WA, LineBoil
│   │   ├── layer.hpp/cpp        — Jerarquia de capas (Raster/Vector/Group)
│   │   ├── tool_state.hpp/cpp   — Estado de herramienta (37 props, QObject)
│   │   ├── app_state.hpp/cpp    — Hub central de estado
│   │   ├── undo_manager.hpp/cpp — Pila undo/redo por secuencia
│   │   └── diagnostic/          — Tracer de diagnostico (opcional)
│   │
│   ├── engine/
│   │   ├── raster/              — Motor de pixeles
│   │   │   ├── raster_engine.hpp/cpp
│   │   │   ├── raster_stroke.cpp
│   │   │   └── raster_effect.hpp/cpp  (LineBoil)
│   │   ├── vector/              — Motor vectorial
│   │   │   ├── vector_engine.hpp/cpp
│   │   │   ├── vector_stroke.cpp
│   │   │   └── bezier_path.hpp/cpp
│   │   ├── brush/               — Motor de pincel
│   │   │   ├── brush_engine.hpp/cpp
│   │   │   ├── brush_preset.cpp
│   │   │   ├── paper_texture.cpp
│   │   │   ├── abr_importer.hpp/cpp
│   │   │   ├── pencil_retouch.hpp/cpp
│   │   │   └── ruler_guide.hpp/cpp
│   │   ├── compositor/          — Composicion de capas
│   │   │   ├── compositor.hpp/cpp
│   │   │   └── node_graph.hpp/cpp
│   │   ├── deformation/         — Deformacion de malla
│   │   │   ├── deformation_mesh.hpp/cpp
│   │   │   └── deformation_engine.hpp/cpp
│   │   ├── animation/           — Animacion, audio, video
│   │   │   ├── audio_engine.hpp/cpp
│   │   │   ├── audio_file_decoder.hpp/cpp
│   │   │   ├── video_decoder.hpp/cpp
│   │   │   ├── tween_engine.hpp/cpp
│   │   │   ├── frame_thumbnail.hpp/cpp
│   │   │   ├── dr_wav.h         — Decoder WAV nativo
│   │   │   ├── dr_mp3.h         — Decoder MP3 nativo
│   │   │   └── dr_flac.h        — Decoder FLAC nativo
│   │   └── common/
│   │       └── blend_utils.hpp  — Utilidades de blending
│   │
│   ├── ui_v2/                   — Interfaz Qt 6 Widgets (13 widgets)
│   │   ├── main_window_v2.hpp/cpp
│   │   ├── canvas_widget_v2.hpp/cpp     — Lienzo de dibujo
│   │   ├── timeline_panel_v2.hpp/cpp    — Timeline multi-secuencia
│   │   ├── toolbox_panel_v2.hpp/cpp     — Herramientas
│   │   ├── color_panel_v2.hpp/cpp       — Selector de color
│   │   ├── layer_panel_v2.hpp/cpp       — Panel de capas
│   │   ├── property_editor_v2.hpp/cpp   — Editor contextual
│   │   ├── audio_track_widget.hpp/cpp   — Pista de audio
│   │   ├── video_track_widget.hpp/cpp   — Pista de video
│   │   ├── onion_skin_panel.hpp/cpp     — Onion skin
│   │   ├── canvas_size_panel.hpp/cpp    — Tamano de canvas
│   │   ├── layer_lock_button.hpp/cpp    — Boton lock/unlock
│   │   └── busy_overlay.hpp/cpp         — Overlay de espera
│   │
│   ├── io/                      — Entrada/Salida
│   │   ├── document_manager.hpp/cpp     — .fap v2 (ZIP binario)
│   │   ├── file_format.cpp             — Legacy v1/v2/v3
│   │   ├── video_export.hpp/cpp        — Video/GIF/PNG export
│   │   ├── svg_export.hpp/cpp          — SVG export
│   │   └── serialization_common.hpp/cpp — Enum/string helpers
│   │
│   └── platform/               — Codigo especifico de plataforma
│       └── file_association.hpp/cpp    — Windows .fap registry
│
├── tests/                      — 16 archivos, 160 tests
│   ├── test_document.cpp
│   ├── test_layer.cpp
│   ├── test_layer_memory.cpp
│   ├── test_compositor.cpp
│   ├── test_undo.cpp
│   ├── test_brush_engine.cpp
│   ├── test_bezier.cpp
│   ├── test_file_format.cpp
│   ├── test_audio_engine.cpp
│   ├── test_deformation.cpp
│   ├── test_node_graph.cpp
│   ├── test_ruler_guide.cpp
│   ├── test_tween_engine.cpp
│   ├── test_pencil_retouch.cpp
│   ├── test_abr_importer.cpp
│   └── test_memory_stress.cpp
│
├── resources/                  — Assets del build
│   ├── resources.qrc           — Qt Resource Collection
│   ├── brushes/default/         — Presets de pincel
│   ├── fonts/                   — Avenir LT Std (.otf, .ttc)
│   ├── icons/                   — Iconos de UI (PNG + SVG)
│   │   ├── toolbar/             — 15 iconos de barra superior
│   │   ├── tools/               — 16 iconos de herramientas
│   │   ├── layers/              — 10 iconos de capas
│   │   ├── timeline/            — 7 iconos de timeline
│   │   ├── fap.ico              — Icono Windows multi-res
│   │   └── splash.png           — Splash screen
│   ├── palettes/default.json    — Paleta de colores
│   └── textures/                — Texturas de papel
│
├── scripts/
│   └── embed_icon.py           — Post-build: inyecta .ico en .exe
│
├── docs/                       — Documentacion
│   ├── architecture.md          — Documento formal de arquitectura (EN)
│   ├── build-instructions.md    — Instrucciones de compilacion (EN)
│   ├── INFORME_ARQUITECTURA_V1.md — ESTE DOCUMENTO (ES)
│   ├── report-acetato-nle-2026-07-03.md — Reporte timeline multi-seq (ES)
│   ├── auditoria-2026-07-10.md  — Auditoria de bugs (ES)
│   ├── diagnostico-persistente-2026-07-10.md — Diagnostico de bugs (ES)
│   ├── INFORME_COMPLETO_FAP.md  — Informe legacy v2.5.1 (ES)
│   └── archive/                 — 14 reportes de sesion historicos
│
├── .agents/skills/             — 37 agent skills para AI
├── .github/workflows/ci.yml    — CI pipeline
└── .opencode/                  — Configuracion opencode
```

---

## NOTAS FINALES

### Convenciones del Proyecto

- **Namespace**: `fap` para todo el codigo
- **Headers**: `#pragma once`
- **Codigo engine**: Independiente de Qt (sin QObject/Qt types en `engine/`)
- **Codigo UI**: Qt 6 Widgets
- **Idioma codigo**: Ingles (comentarios, identificadores)
- **Idioma docs**: Espanol para reportes principales, Ingles para docs tecnicos

### Deuda Tecnica Pendiente

1. Tests de coverage para `CanvasWidgetV2` (sin tests directos del widget de dibujo)
2. Tests de integracion para el pipeline completo save→load
3. Implementacion de capas VectorLayer en UI (modelo existe, UI pendiente)
4. Implementacion completa de VectorEngine con renderizado vectorial nativo
5. Modos de blending no implementados completamente en NodeGraph
6. Soporte de capas Camera y Audio en timeline

### Limpieza Realizada (v1.0.0)

- **12 archivos muertos eliminados**: `stroke`, `canvas`, `input_manager`, `tablet_handler`, `document_io`, `timeline_engine`, `playback`, `onion_skin` (.hpp/.cpp)
- **3 headers huerfanos agregados a CMakeLists.txt**: `video_export.hpp`, `layer_lock_button.hpp`, `test_memory_stress.cpp`
- **Archivos de diseno movidos a `_design_source/`**: `icons/`, `logos/`, `fap_traces/`, `free_animation_power.py`
- **Logs y resultados de tests eliminados del repo**
- **Fuentes duplicadas eliminadas**: `resources/fonts/mejoras/`
- **Version bump**: v2.0.0 → v1.0.0 en CMakeLists.txt, main.cpp

---

> **Documento generado el 22 de Julio, 2026.**
> **Version del proyecto: 1.0.0 — Release inicial.**
