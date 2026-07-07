# Status: Lens Pattern Refactor - Jun 17, 2026 (COMPLETADO)

> **Jul 2026:** El refactor fue completado. La arquitectura V2 (`src/ui_v2/`) es la interfaz principal. `CanvasWidgetV2` usa zero-copy wrappers para display. El crash de capas fue resuelto (ver `HANDOFF_REPORT_2026-06-18.md`). Build: `cmake -B build -DCMAKE_BUILD_TYPE=Release`.

## Objetivo
Eliminar `layerCache_` de `CanvasWidget` aplicando el Patron de Lente (Lens Pattern).
Flujo unidireccional Modelo -> UI sin almacenamiento intermedio.

## Archivos modificados (4)

### 1. `src/ui/canvas_widget.hpp`
- **Eliminado**: `layerCache_` (`std::map<std::pair<Layer*, int>, QImage>`)
- **Eliminado** (publico): `getLayerImage()`, `putLayerImage()`, `invalidateLayerCache()`
- **Agregado** (privado): `wrapRasterLayer(RasterLayer*)` - zero-copy QImage wrapper
- **Agregado** (privado): `readLayerPixels(Layer*)` - deep copy desde modelo
- **Agregado** (publico): `writeLayerPixels(Layer*, const QImage&)` - escribe de vuelta al modelo
- **Agregado**: forward declaration `class RasterLayer`
- Limpiado `clearCanvasCache()` y `setDocumentAndBrush()` (sin `layerCache_`)

### 2. `src/ui/canvas_widget.cpp`
Metodos nuevos:
- `wrapRasterLayer()`: Crea QImage que apunta directamente a `RasterLayer::pixelData()` via constructor zero-copy
- `readLayerPixels()`: Deep copy del buffer del modelo para operaciones de edicion
- `writeLayerPixels()`: Copia pixeles de QImage de vuelta al `RasterLayer::pixelData()`. Solo llama `resize()` si cambian dimensiones.

Metodos refactorizados (~15):
- `paintEvent`: Itera capas del modelo, usa `wrapRasterLayer` para display
- `commitStroke`: Usa `readLayerPixels` -> editar -> `writeLayerPixels`
- `doErase`, `drawShape`, `doFill`, `doText`: Mismo patron read/edit/write
- `compositeFrame`, `renderTintedFrame`: Zero-copy wrap para compositing
- `undo`/`redo`: `writeLayerPixels` para restaurar estado
- `startMove`/`commitMove`: `readLayerPixels`/`writeLayerPixels`
- `cutSelection`/`pasteClipboard`: `readLayerPixels`/`writeLayerPixels`
- `snapCurrentLayer`: `readLayerPixels`
- `shiftFrameData`: Solo maneja vector strokes (sin cache)
- `removeFrameData`: Simplificado
- `resizeCanvas`: Redimensiona capas del modelo directamente
- `clearCurrentFrame`: Llama `RasterLayer::clear()` en el modelo

Formato de pixeles: `QImage::Format_ARGB32_Premultiplied` (mismo que el codigo original)
- NOTA: El plan original decia `Format_RGBA8888` pero causaba crashes con QPainter en Windows.
  `ARGB32_Premultiplied` es el formato battle-tested del codigo original.

### 3. `src/ui/layer_panel.hpp` + `layer_panel.cpp`
- **Agregado**: `Layer* currentLayer() const` - devuelve puntero directo al Layer seleccionado
  en el panel (evita conversion de indices)

### 4. `src/ui/main_window.cpp`
- Cambiado `canvas_->putLayerImage()` -> `canvas_->writeLayerPixels()` (frame duplication)
- **CRASH FIX**: Agregado bounds check antes de acceder `layers[modelIdx]->name()` en
  el handler de `layerChanged`. Antes accedia fuera de limites cuando `currentModelIndex()`
  devolvia indice invalido durante manipulacion de capas.

## Estado actual
- **Compilacion**: Exitosa
- **Tests**: 45/45 pasan
- **App**: Compila y ejecuta

## Problema pendiente
La app se cierra al manipular capas (crear, bloquear, seleccionar). El ultimo fix
(bounds check en `main_window.cpp:452`) corrige un acceso fuera de limites que podria
causar el crash.

## Para probar manana
1. Abrir app
2. Dibujar en Capa 1
3. Bloquear Capa 1
4. Crear Capa 2
5. Seleccionar Capa 2 en el panel
6. Dibujar en Capa 2
7. Verificar que no crashea y que los contenidos estan aislados

## Notas tecnicas
- El cache por-frame se elimino. RasterLayer tiene un solo buffer. Los strokes vectoriales
  (`vectorStrokes_`) mantienen organizacion por frame.
- `wrapRasterLayer` crea QImage que NO es dueno de los datos. Debe usarse solo para
  lectura (display en paintEvent).
- `readLayerPixels` siempre hace deep copy (.copy()), seguro para editar con QPainter.
- `writeLayerPixels` solo hace resize si las dimensiones difieren (evita invalidar
  QImage wrappers que pudieran estar vivos).
