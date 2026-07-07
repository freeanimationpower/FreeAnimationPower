# Handoff Report: FreeAnimation2dStyle

**Fecha:** 2026-06-18 (actualizado 2026-07-07)
**Stack:** C++20, Qt 6.5+ Widgets, CMake 3.20+, GoogleTest
**Build:** Release | **Tests:** 54/54 pass | **Branch:** master

---

## 0. Estado Actual (Jul 2026)

### Commit: `ef692a0` — feat: add Import Image to layer panel with right-click context menu

### Lo que funciona
- **V2 UI completa**: MainWindowV2 con todos los paneles dockables (Toolbox, Layers, Color, Properties, Timeline)
- **Dibujo raster**: Brush y Eraser sobre RasterLayer con undo/redo (PaintCommandV2)
- **Sistema de capas**: Capas por frame independientes (Document::frames_), visibilidad, bloqueo, blend modes
- **Timeline**: Thumbnails con preview, playback controls, FPS, scroll, scrubbing, onion skin
- **PropertyEditorV2**: Size, Opacity, Hardness, Shape, Stabilizer, Pressure checkboxes
- **Blend modes**: 12 modos aplicados en composicion de capas via QPainter::setCompositionMode
- **Onion skin**: Frames prev/next con tintes rojo/azul y opacidad configurable
- **Tablet support**: InputManager traduce eventos Qt tablet (presion, tilt)
- **Formato .fap**: Save/load con JSON + PNG por frame
- **Exportacion**: MP4 y GIF via FFmpeg

### Archivos clave V2
```
src/ui_v2/
├── main_window_v2.hpp/.cpp     # Ventana principal V2 con docks
├── canvas_widget_v2.hpp/.cpp   # Canvas de dibujo central
├── toolbox_panel_v2.hpp/.cpp   # Panel de herramientas (izquierda)
├── layer_panel_v2.hpp/.cpp     # Panel de capas + blend mode
├── color_panel_v2.hpp/.cpp     # Selector de color
├── property_editor_v2.hpp/.cpp # Propiedades de pincel (derecha)
├── timeline_panel_v2.hpp/.cpp  # Timeline con thumbnails + playback
├── layer_lock_button.hpp/.cpp  # Boton de candado reutilizable
```

### Build command
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
.\build\Release\free-animation-2d-style.exe
```

---

## 1. Resolucion del Crash Anterior (Segfault en Capas)

### Sintoma original
Segmentation fault inmediato al ejecutar la secuencia: Dibujar en Capa 1 -> Bloquear Capa 1 -> Crear Capa 2 -> Dibujar en Capa 2. El Status Bar mostraba erroneamente "Cannot draw: Layer 1 is locked" indicando desincronizacion de capa activa.

### Causa raiz (multi-vector)
1. **Reentrancia en el ciclo de eventos de Qt** (`main_window.cpp:446`): `canvas_->repaint()` forzaba un `paintEvent()` sincrono durante la manipulacion del arbol de capas. `refreshLayerList()` en `layer_panel.cpp:383-386` emitia `currentRowChanged` -> `layerChanged(-1)` mientras los `QListWidgetItem` estaban en estado intermedio, provocando `wrapRasterLayer()` sobre memoria no inicializada.
2. **Punteros huerfanos en `LayerPanel`** (`layer_panel.cpp:20-26`): `QListWidgetItem::data` almacenaba punteros raw `Layer*` via `reinterpret_cast<quintptr>`. Al reconstruir la lista, items destruidos dejaban punteros stale.
3. **Invalidacion de memoria en `std::vector`** (`core/layer.hpp:94`): `GroupLayer` usaba `std::vector<std::unique_ptr<Layer>>`. Al insertar capas, la reasignacion del vector movia los `unique_ptr` (las capas en si mantienen direccion de heap estable, pero el array interno cambiaba).

### Correcciones estructurales aplicadas

| Archivo | Cambio |
|---------|--------|
| `core/layer.hpp` | `LayerUid` atomico inmutable (`std::atomic<uint64_t>`); `buffer_epoch_` en `RasterLayer` para deteccion de reasignaciones; `std::deque<std::unique_ptr<Layer>>` en `GroupLayer` (estabilidad de punteros en inserciones/borrados); metodos `layerByUid()`, `indexOfLayer()` |
| `core/layer.cpp` | Contador de UID monotónico; incremento de `buffer_epoch_` en `RasterLayer::resize()`; `GroupLayer::duplicateLayer()` refactorizado a usar `clone()` virtual con deep copy de `pixels_` |
| `ui/layer_panel.cpp` | Eliminados punteros raw: `QListWidgetItem` almacena `LayerUid`; lambdas de botones capturan UID en vez de `Layer*`; `refreshLayerList()` con reconciliacion explicita de items huerfanos; `rowsMoved` itera de abajo hacia arriba (`n-1` -> `0`) preservando la inversion modelo/UI |
| `ui/canvas_widget.cpp` | `resolveCurrentLayer()` unifica resolucion de capa activa (UID-first, fallback a indice); `inputPress()` establece `currentLayerUid_` + `currentLayerIdx_`; lock gate en 5 ubicaciones |
| `ui/main_window.cpp` | Eliminado `canvas_->repaint()` del handler `layerChanged` (solo `canvas_->update()` asincrono); `layerChanged` acepta `forcedModelIdx` del panel para evitar recalculacion sobre estado intermedio |

---

## 2. Estado Actual de la Arquitectura

### Flujo de renderizado

```
inputPress() -> resolveCurrentLayer() [UID] -> lock check ->
  snapCurrentLayer() [deep copy para undo] ->
  commitStroke() -> readLayerPixels() [deep copy] ->
  renderVectorStroke() [rasteriza trazo] ->
  writeLayerPixels() [copia fila por fila a pixels_] ->
  emit canvasUpdated() -> canvas_->update() [asincrono] ->
  paintEvent() -> clearRect -> paintOnionSkin() ->
    for (layer : currentRootLayer()) {
      if Raster: wrapRasterLayer() [zero-copy QImage] -> drawImage()
      if Vector: renderVectorStrokes(p, frame, layer->uid())
    }
```

### Gestion de memoria

- `RasterLayer::pixels_`: `std::vector<uint32_t>` contiguo en CPU, formato `0xAABBGGRR` little-endian
- `wrapRasterLayer()`: `QImage` zero-copy via `reinterpret_cast<const uchar*>(pixels_.data())`. Tracking de `buffer_epoch_` para detectar reasignaciones.
- `readLayerPixels()`: `.copy()` explicito antes de retornar (protege el buffer original durante edicion)
- `writeLayerPixels()`: `std::copy` fila por fila al buffer de la capa. Solo llama a `raster->resize()` si las dimensiones difieren.

### Arquitectura multi-frame (Cel Animation)

- `Document::frames_`: `std::map<int, unique_ptr<GroupLayer>>` - pila de capas independiente por fotograma
- `Document::rootLayerForFrame(int)`: auto-crea capa por defecto si el frame esta vacio
- `Document::shiftFrameData()` / `removeFrameData()`: propagacion de insercion/borrado de frames
- `CanvasWidget::currentRootLayer()`: redirige a `doc_->rootLayerForFrame(currentFrame_)`
- `LayerPanel::setFrame(int)`: limpia `list_widget_` y reconstruye desde el nuevo frame
- `MainWindow`: `frameChanged` -> `layer_panel_->setFrame(idx)` + `canvas_->update()`
- `file_format.cpp` v2: formato JSON con array `"frames"` por frame, PNGs `F{frame}_L{layer}.png`

### Mapeo UI <-> Modelo (Z-Order)

- `refreshLayerList()`: `modelIdx = n - 1 - listIdx` (invierte: top UI = foreground = ultimo en modelo)
- `rowsMoved`: itera `for (listRow = n-1; listRow >= 0; --listRow)` (consistente con inversion)
- `paintEvent()`: itera `layers_.begin()` -> `layers_.end()` (indice 0 = fondo, dibujado primero)

### Onion Skin

- `renderTintedFrame(p, frameIdx, tint)`: usa `doc_->rootLayerForFrame(frameIdx)` para leer las capas del frame fantasma
- `paintOnionSkin()`: gate `if (!onionEnabled_) return;`, pasado en rojo, futuro en azul, opacidad decreciente
- UI: `QToolButton` con `MenuButtonPopup` en toolbar, checkable, menu con "Range: 1/2/3/5"

---

## 3. Estado de Issues Conocidos

### Build y Tests

- **Build:** Release compila limpio. Warnings preexistentes de AutoMoc en archivos sin Q_OBJECT (no introducidos por nuestros cambios).
- **Tests:** 54/54 pasan (0 regresiones).
  - `DocumentTest`: 2/2
  - `TimelineTest`: 8/8
  - `LayerTest`: 2/2
  - `LayerMemoryTest`: 9/9
  - `BrushEngineTest`: 3/3
  - `BezierTest`: 5/5
  - `FileFormatTest`: 1/1
  - `RasterLayerTest`: 7/7
  - `VectorLayerTest`: 4/4
  - `CompositorTest`: 6/6
  - `UndoManagerTest`: 7/7

### Problemas potenciales no bloqueantes

1. **`RawStroke::layerUid` en undo/redo vectorial:** `vectorUndo_` guarda snapshots completos del frame. Si las capas se reordenan entre undo y redo, los UIDs siguen siendo validos pero la asignacion visual podria ser sutilmente incorrecta al restaurar `vectorStrokes_[frame]` completo.
2. **`duplicateLayer` solo soporta `RasterLayer` y `VectorLayer`:** `GroupLayer` tiene `clone()` pero `duplicateLayer` del panel solo opera sobre capas hoja. La clonacion recursiva de grupos funciona en el modelo pero no esta expuesta en la UI.
3. **`file_format.cpp` carga v1:** Al cargar archivos antiguos (v1), crea clones de la plantilla de capas para cada frame. Los datos de pixeles por frame se leen de PNGs `L{li}_{frame}.png`. Si el archivo v1 no tiene PNGs validos, las capas nacen vacias.

---

## 4. Intentos Fallidos y Lecciones Aprendidas

### Intentos descartados

| Intento | Resultado | Razon del fallo |
|---------|-----------|-----------------|
| Corregir `moveLayer` con `if (toIndex > fromIndex) --toIndex` | Revertido | Rompio `CompositorTest.MoveLayerChangesOrder`. La formula original es correcta: al borrar `fromIndex`, `toIndex` ya apunta a la posicion deseada en el contenedor encogido. |
| `std::reverse(newOrder)` en `rowsMoved` | Reemplazado | Funcionaba pero era un segundo pase O(n). Se reemplazo por iteracion inversa directa `for (int listRow = n-1; listRow >= 0; --listRow)`. |
| `canvas_->repaint()` en `layerChanged` | Eliminado | Causaba reentrancia. `update()` asincrono es suficiente y seguro. |
| `QColor(70,15,15)` para fondo de capa bloqueada | Reemplazado | Demasiado oscuro. Cambiado a `QColor(255,0,0,13)` (5% opacidad roja). |
| `commitStroke` con `vs.layerIndex = currentLayerIdx_` | Reemplazado | Indice volatil tras reorden. Cambiado a `vs.layerUid = layer->uid()`. |
| `renderVectorStrokes` fuera del bucle de capas en `paintEvent` | Movido adentro | Pintaba todos los vectores encima independientemente del Z-order. Ahora se invoca por capa con `layer->uid()`. |
| `renderTintedFrame` usando `currentRootLayer()` ignorando `frameIdx` | Corregido | La tela de cebolla siempre mostraba el frame actual en vez del frame fantasma. |
| `setOnionEnabled()` sobreescribiendo `onionPrev_`/`onionNext_` a 0 | Corregido | Al re-activar, `onionPrev_` ya era 0 y no se restauraba. `onionEnabled_` es ahora un flag independiente. |

### Archivos clave modificados (recuento final)

```
src/core/layer.hpp          - UID, buffer_epoch_, deque, clone() virtual
src/core/layer.cpp          - clone() impl, UID counter, deep copy en duplicateLayer
src/core/document.hpp       - frames_ map, rootLayerForFrame(), shiftFrameData()
src/core/document.cpp       - per-frame layer stacks, default layer creation
src/ui/canvas_widget.hpp    - currentRootLayer(), onionEnabled_, setOnionRange()
src/ui/canvas_widget.cpp    - resolveCurrentLayer(), UID-based render, per-layer vectors, onion skin fix
src/ui/layer_panel.hpp      - UID-based items, setFrame(), currentFrame(), root()
src/ui/layer_panel.cpp      - rowsMoved inversion, lock/vis lambdas con rootLayerForFrame, inline rename
src/ui/main_window.cpp      - repaint() removido, forcedModelIdx, onion skin QToolButton
src/io/file_format.cpp      - v2 format: per-frame JSON + F{frame}_L{layer}.png
tests/test_layer_memory.cpp - 9 tests: UID, buffer epoch, pointer stability, crash sequence
resources/resources.qrc     - SVG icons registrados
resources/icons/*.svg       - 9 iconos SVG (eye, lock, new_layer, duplicate, delete, move_up, move_down)
```

---

## 5. Proximos Pasos Recomendados

1. **UI: Controles de Onion Skin en panel izquierdo** - Slider de opacidad + spinboxes de frames prev/next en `toolbox_panel.cpp`
2. **Undo/Redo multi-frame** - `UndoEntry` actualmente solo guarda `layerIdx`; deberia guardar `frameIdx` + `layerUid` para operaciones cross-frame
3. **Preview de reproduccion** - El timeline tiene senal `togglePlayPause` no conectada a reproduccion real de frames
4. **Exportacion de video** - `compositeFrame` ya esta actualizado para per-frame; probar exportacion real con FFmpeg
5. **Renderizado de `VectorLayer` pura** - La UI actual solo crea `RasterLayer`; `VectorLayer` existe en el modelo pero no es accesible desde el panel
