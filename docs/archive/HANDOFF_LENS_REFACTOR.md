# Informe de Handoff - Free Animation Power
## Lens Pattern Refactor - Estado al 17 Jun 2026

---

# 1. Vision General del Software

**FreeAnimation2dStyle** (v2.0.0) es un software de animacion 2D profesional con motor hibrido **vector + raster**. Permite crear animaciones cuadro-por-cuadro combinando capas de pixeles (bitmap) y capas vectoriales (Bezier), renderizadas por CPU al estilo TVPaint.

### Proposito principal
- Dibujo digital cuadro-por-cuadro con pinceles personalizables
- Papel texturizado y dinamicas de presion (tableta grafica)
- Sistema de capas con modos de blend, opacidad, visibilidad y bloqueo
- Timeline con keyframes, onion skinning y reproduccion
- Exportacion a video via FFmpeg y formato de archivo propio (.fap)

### Flujo conceptual
1. El usuario dibuja con herramientas (pincel, borrador, relleno, texto, formas, mover, seleccionar)
2. Los strokes se rasterizan sobre capas `RasterLayer` (buffer `std::vector<uint32_t>` en memoria CPU)
3. `CanvasWidget` renderiza las capas compositadas en pantalla via QPainter
4. El `LayerPanel` gestiona la seleccion, visibilidad, bloqueo y orden de capas
5. El `TimelinePanel` maneja la navegacion entre frames

---

# 2. Arquitectura del Sistema

### Stack tecnologico
| Componente | Tecnologia |
|-----------|-----------|
| Lenguaje | C++20 |
| UI | Qt 6.5+ (Widgets, no QML) |
| Build | CMake 3.20+ |
| Tests | GoogleTest v1.14.0 |
| Motor grafico | CPU raster + vector (Bezier), QPainter |
| Video | FFmpeg (proceso externo) |
| Archivos | ZIP container con PNG frames + JSON metadata |

### Patrones de diseno (existentes + en implementacion)
- **MVC**: `Document` (Modelo) -> `CanvasWidget` (Vista/Controlador) -> `LayerPanel` (Vista/Controlador)
- **Lens Pattern** (en implementacion): `wrapRasterLayer()` crea QImage zero-copy que apunta directamente al buffer del modelo, eliminando el cache intermedio `layerCache_`
- **Observer via Qt Signals/Slots**: `canvasUpdated`, `layerChanged`, `toolChanged`, etc.
- **NonCopyable**: Las clases core heredan de `NonCopyable` (Document, Layer, etc.)

### Flujo de datos PRINCIPAL (post-refactor)
```
[Input] -> inputPress() -> currentLayer() -> activeLayer->locked()? ->
  -> snapCurrentLayer() -> readLayerPixels() [deep copy del modelo]
  -> commitStroke() -> renderVectorStroke sobre deep copy ->
  -> writeLayerPixels() -> RasterLayer::pixelData() [escribe al modelo]
  -> this->update() -> paintEvent() -> wrapRasterLayer() [zero-copy desde modelo]
  -> QPainter::drawImage() [render directo]
```

### Estructura de capas en el modelo
```
Document
  └── root_layer_ (GroupLayer)
       ├── RasterLayer "Layer 1" (pixels_: vector<uint32_t>, width*height)
       ├── RasterLayer "Layer 2"
       └── ...
```

**Formato de pixel en RasterLayer**:
- `setPixel()` almacena: `(r | g<<8 | b<<16 | a<<24)` → uint32 = `0xAABBGGRR`
- En memoria little-endian (x86): bytes `[R, G, B, A]`
- Lectura: `pixelAt()` lee como `(p&0xFF, (p>>8)&0xFF, (p>>16)&0xFF, (p>>24)&0xFF)`
- **Formato QImage usado para display/edicion**: `QImage::Format_ARGB32_Premultiplied`
  - NOTA: Hay un desajuste R/B entre el modelo y este formato QImage. En QImage ARGB32 (little-endian),
    los bytes se interpretan como [B, G, R, A] mientras el modelo almacena [R, G, B, A].
    Este swap R/B se cancela en el ciclo read/write actual y NO causa problemas visuales.
    El plan original proponia `Format_RGBA8888` para corregir esto, pero causaba crashes.

---

# 3. Mapa de Archivos y Rutas (File Tree)

```
free animation power/
├── CMakeLists.txt          # Build system. Target: free-animation-2d-style.exe + fap-tests.exe
├── AGENTS.md               # Instrucciones para agentes AI (skills, build commands, conventions)
├── CONTEXT.md              # Lenguaje de dominio, entidades core, decisiones de arquitectura
├── STATUS_LENS_REFACTOR.md # ESTE INFORME - Estado actual del refactor
├── src/
│   ├── main.cpp            # Entry point - crea QApplication y MainWindow
│   │
│   ├── core/               # MODELO DE DATOS (Qt-independent)
│   │   ├── types.hpp/.cpp       # Color, Vec2, StrokePoint, LayerType, BlendMode, NonCopyable
│   │   ├── layer.hpp/.cpp       # Layer(base), RasterLayer, VectorLayer, GroupLayer
│   │   ├── document.hpp/.cpp    # Document (canvas size, FPS, root_layer_)
│   │   ├── timeline.hpp/.cpp    # Timeline (frames, keyframes, playback)
│   │   ├── stroke.hpp/.cpp      # Stroke data structures
│   │   ├── canvas.hpp/.cpp      # Canvas model (no usado directamente por la UI actual)
│   │   ├── project.cpp          # Project management
│   │   └── undo_manager.hpp/.cpp # Undo/redo stack (UndoManager)
│   │
│   ├── engine/             # MOTORES DE RENDERIZADO (Qt-independent)
│   │   ├── brush/
│   │   │   ├── brush_engine.hpp/.cpp  # BrushEngine - procesa input en strokes
│   │   │   ├── brush_preset.cpp       # Presets de pinceles
│   │   │   └── paper_texture.cpp      # Textura de papel
│   │   ├── raster/
│   │   │   ├── raster_engine.hpp/.cpp # RasterEngine - buffer de pixeles con fill/composite
│   │   │   └── raster_stroke.cpp      # Rasterizacion de strokes
│   │   ├── vector/
│   │   │   ├── bezier_path.hpp/.cpp   # BezierPath - curvas Bezier, flatten, evaluate
│   │   │   ├── vector_engine.hpp/.cpp # VectorEngine
│   │   │   └── vector_stroke.cpp
│   │   ├── compositor/
│   │   │   └── compositor.hpp/.cpp    # Compositor - composita GroupLayer -> RasterEngine
│   │   ├── animation/
│   │   │   ├── onion_skin.cpp         # Logica de onion skin
│   │   │   ├── playback.cpp           # Control de playback
│   │   │   └── timeline_engine.cpp    # Motor de timeline
│   │   └── common/
│   │       └── blend_utils.hpp        # Utilidades de blend modes
│   │
│   ├── ui/                 # UI CON Qt WIDGETS
│   │   ├── main_window.hpp/.cpp  # MainWindow - ventana principal, menus, docks, signals
│   │   ├── canvas_widget.hpp/.cpp # CanvasWidget - renderizado, input, herramientas
│   │   ├── layer_panel.hpp/.cpp  # LayerPanel - lista de capas, vis/lock/blend
│   │   ├── timeline_panel.hpp/.cpp # TimelinePanel - timeline visual
│   │   ├── toolbox_panel.hpp/.cpp # ToolboxPanel - herramientas, brush settings
│   │   ├── color_panel.hpp/.cpp  # ColorPanel - selector de color
│   │   ├── property_editor.hpp/.cpp # PropertyEditor
│   │   └── icons.hpp             # Iconos SVG para botones
│   │
│   ├── ui_v2/              # VERSIONES V2 DE PANELES (alternativas, no en build principal)
│   │   └── main_window_v2, color_panel_v2, layer_panel_v2, toolbox_panel_v2
│   │
│   ├── io/                 # ENTRADA/SALIDA
│   │   ├── document_io.cpp      # Save/Load de documentos
│   │   ├── file_format.cpp      # Formato .fap (ZIP + JSON + PNG)
│   │   └── video_export.cpp     # Exportacion a video via FFmpeg
│   │
│   └── platform/           # PLATAFORMA
│       ├── input_manager.hpp/.cpp   # Input manager
│       └── tablet_handler.hpp/.cpp  # Soporte de tableta grafica
│
├── tests/                  # GOOGLETEST SUITE (45 tests)
│   ├── test_document.cpp   # 2 tests: DefaultValues, SetCanvasSize
│   ├── test_timeline.cpp   # 5 tests: playback state, frame nav, keyframes
│   ├── test_layer.cpp      # 11 tests: RasterLayer(7) + VectorLayer(4)
│   ├── test_brush_engine.cpp # 3 tests: default preset, stroke cycle, presets
│   ├── test_bezier.cpp     # 5 tests: empty path, line, evaluate, flatten, curve
│   ├── test_compositor.cpp # 6 tests: compositing, opacity, visibility, order
│   ├── test_undo.cpp       # 7 tests: undo/redo raster/vector, stack limits
│   └── test_file_format.cpp # 1 test: save/load roundtrip
│
├── src_py/                 # PROTOTIPO PYTHON (referencia)
│   ├── main_window.py, canvas.py, document.py
│   ├── brush_engine.py, tools.py, panels.py
│   └── timeline.py, types_enums.py
│
├── docs/
│   ├── architecture.md     # Documentacion de arquitectura
│   └── build-instructions.md
│
└── resources/
    ├── brushes/default/presets.json
    ├── palettes/default.json
    └── textures/
```

### Responsabilidades clave por archivo

| Archivo | Rol |
|---------|-----|
| `canvas_widget.hpp/.cpp` | **OBJETIVO DEL REFACTOR**. Renderiza capas, maneja input de dibujo, undo/redo local |
| `layer_panel.hpp/.cpp` | Gestiona UI de capas. Mapea lista visual (top=front) a modelo (index 0=bottom). Storea `Layer*` en QListWidgetItem::data |
| `main_window.hpp/.cpp` | Composicion de docks, signal wiring. Handler `layerChanged` llama `repaint()` |
| `core/layer.hpp/.cpp` | `RasterLayer`: `pixels_` (vector<uint32_t>), `pixelData()`, `width()`, `height()`, `resize()` |
| `core/document.hpp/.cpp` | `Document`: `width_`=1920, `height_`=1080, `root_layer_`, `ensureDefaultLayer()` crea "Layer 1" |
| `engine/brush/brush_engine.hpp` | `BrushEngine`: `beginStroke/addPoint/endStroke`, `BrushPreset`, `BrushDynamics` |
| `engine/compositor/compositor.hpp` | `Compositor::composite(GroupLayer, RasterEngine)` - usado solo en tests, no en UI |
| `engine/raster/raster_engine.hpp` | `RasterEngine`: buffer de pixeles con `fill()`, `pixels()`, `clear()` |

---

# 4. Hitos y Problemas Resueltos

### Hitos completados (segun AGENTS.md)
- [x] Hito 1-11: Core engine, capas, timeline, brushes
- [x] Hito 12: LayerPanel con Drag-and-Drop
- [x] Hito 17: Unificacion de formato de pixel
- [x] 45 tests unitarios pasando (RasterLayer, VectorLayer, Compositor, Undo, etc.)

### Bugs arreglados en ESTA sesion de refactor
1. **Eliminacion de `layerCache_`**: Causaba corrupcion de memoria y desincronizacion al reordenar capas. Sustituido por Lens Pattern (zero-copy wrappers).
2. **Formato de pixel**: El plan decia `Format_RGBA8888` pero causaba crashes con QPainter en Windows. Revertido a `Format_ARGB32_Premultiplied` (el original battle-tested).
3. **`writeLayerPixels` no llama `resize()` si dimensiones no cambian**: Evita reasignacion innecesaria del buffer que podria invalidar QImage wrappers vivos.
4. **Bounds check en `main_window.cpp:452`**: `layers[modelIdx]->name()` se ejecutaba sin verificar bounds. Agregado `if (modelIdx < layers.size())`.
5. **`LayerPanel::currentLayer()`**: Nuevo metodo que devuelve `Layer*` directo del item seleccionado. Evita conversion de indices `currentModelIndex()` que podia devolver valores incorrectos.
6. **`inputPress` usa `currentLayer()`**: Sincronizacion directa por puntero, no por indice.

### Componentes estables
- Core data model (Document, Layer, RasterLayer, VectorLayer, GroupLayer)
- Brush engine (presets, dynamics)
- Bezier path engine
- Compositor engine (usado en tests)
- Timeline engine
- File format (.fap save/load)
- Todos los tests unitarios

---

# 5. Estado Actual del Desarrollo

**Fase**: Refactorizacion del mecanismo de renderizado de capas (Lens Pattern) - **EN PROGRESO**

### Que se completo del plan original:
| Item del plan | Estado |
|--------------|--------|
| Eliminar `layerCache_` y `getLayerImage`/`putLayerImage` de la interfaz publica | Hecho |
| Implementar `wrapRasterLayer()` zero-copy | Hecho |
| Reescribir `paintEvent` para iterar sobre el modelo | Hecho |
| Reescribir `commitStroke` para flujo unidireccional | Hecho |
| Actualizar todos los metodos que usaban el cache (~15 metodos) | Hecho |
| Ejecutar tests | 45/45 pasan |

### Que FALTA:
- Verificar que la app no crashea durante uso normal (crear capas, bloquear, dibujar)
- Verificar manualmente la secuencia critica: dibujar Capa 1, bloquear, crear Capa 2, dibujar, ocultar/mostrar
- Posiblemente agregar almacenamiento de pixeles por-frame en el modelo (RasterLayer solo tiene un buffer)

---

# 6. El Problema Bloqueante (CRITICO)

### Descripcion
La aplicacion se cierra (crash/segfault) al manipular capas - especificamente al:
1. Dibujar en Capa 1
2. Bloquear Capa 1 (click en icono de candado)
3. Crear Capa 2 (boton "+")
4. Intentar seleccionar/dibujar en Capa 2

El usuario reporta que el crash ocurre "mientras manipulo los layer", no necesariamente al dibujar.

### Comportamiento observado
- Mensaje en status bar: "Cannot draw: Layer 1 is locked" (cuando deberia estar en Capa 2)
- Cierre subito de la aplicacion sin mensaje de error visible
- Tests unitarios: 45/45 pasan (no cubren el scenario UI)

### Causa raiz identificada y corregida
**`main_window.cpp` linea 452** (AHORA CORREGIDO):
```cpp
// CODIGO ORIGINAL (con bug):
statusBar()->showMessage(
    QString("Active: %1 (layer %2)%3")
        .arg(QString::fromStdString(layers[modelIdx]->name()))  // CRASH AQUI
        .arg(modelIdx + 1).arg(lockStatus), 3000);
```
El acceso `layers[modelIdx]->name()` ocurria SIN verificar que `modelIdx` estuviera dentro de bounds. `currentModelIndex()` retorna 0 como default cuando `list_widget_->currentRow()` es -1 (sin seleccion). Si la lista de capas esta vacia o en estado inconsistente durante manipulacion rapida, `modelIdx=0` pero `layers.size()=0` -> **segfault**.

### Correccion aplicada (pendiente de verificacion)
```cpp
// CODIGO CORREGIDO:
if (modelIdx >= 0 && static_cast<size_t>(modelIdx) < layers.size()) {
    QString lockStatus = layers[modelIdx]->locked() ? " [LOCKED]" : "";
    statusBar()->showMessage(...);
}
```

### Enfoques INTENTADOS que NO funcionaron
1. ~~Usar `currentModelIndex()` con sync condicional (`if modelIdx != currentLayerIdx_`).~~ No resolvia el problema de seleccion incorrecta.
2. ~~Mantener `getLayerImage`/`putLayerImage` como helpers privados.~~ Reemplazado por `readLayerPixels`/`writeLayerPixels`.
3. ~~Usar `Format_RGBA8888` (segun plan original).~~ **Causaba crashes** de QPainter en Windows. Revertido a `Format_ARGB32_Premultiplied`.
4. ~~Llamar `resize()` en `writeLayerPixels` siempre.~~ Optimizado: solo cuando dimensiones difieren.
5. ~~`repaint()` sincrono en handler `layerChanged`.~~ Puede causar reentrancia. El bounds check deberia mitigarlo, pero `repaint()` podria necesitar ser reemplazado por `update()` asincrono.

### Hipotesis pendientes si el crash continua
- **H1**: `repaint()` sincrono en `main_window.cpp:446` causa paintEvent anidado durante manipulacion de capas. Probar cambiar a solo `update()`.
- **H2**: `wrapRasterLayer` recibe RasterLayer con buffer invalido (ya mitigado con null checks).
- **H3**: `LayerPanel::currentLayer()` retorna puntero a Layer que ya fue eliminado del modelo (stale pointer en QListWidgetItem::data).
- **H4**: El `refreshLayerList()` en `LayerPanel` manipula items mientras hay signal handlers activos que acceden a los mismos items.

### Proximos pasos sugeridos
1. Probar la app con el bounds check corregido
2. Si sigue crash: cambiar `canvas_->repaint()` por `canvas_->update()` en el handler `layerChanged` de main_window (linea 446)
3. Si sigue crash: investigar H3/H4 - posible stale pointer en `currentLayer()`
4. Agregar test de integracion para el scenario: crear capa, bloquear, crear otra capa, seleccionar, dibujar
