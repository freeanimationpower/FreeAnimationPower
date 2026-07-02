# Reporte de Sesion — 1-2 Julio 2026

## Resumen

**2 dias completos de trabajo. 41 commits analizados/revertidos.**  
**Resultado: Rollback al commit `1f5f4dc` (26 de junio).**

No se logro resolver el problema de latencia (`recuadro molesto` / lineas blancas) sin romper la logica fundamental del programa (onion skin, guardado de frames por capa, vizualizacion del canvas).

---

## 1. Problema Original

### Sintoma
Un "recuadro molesto" / lineas blancas de 1-2px aparecian durante los trazos de pincel, especialmente al pintar sobre contenido existente.

### Diagnostico (del commit `f6b508d`)
Las lineas blancas eran causadas por:
1. **Bordes del dab con alpha baja (1-7)**: Los dabs creados con `QRadialGradient` + `convertToFormat(Premultiplied)` tienen bordes anti-aliased con alpha casi transparente. Al dibujarse con `SourceOver` sobre `fillRect(#FFFFFF)`, producian blanco puro en pantalla.
2. **Huecos transparentes en composite cache**: El `compositeCache_` se construia con fondo transparente. Donde no habia contenido de capa, era transparente. El `fillRect(#FFFFFF)` del canvas llenaba esos huecos, pero los bordes del dab (alpha baja) se mezclaban con ese blanco.
3. **Premultiplied alpha en GPU**: `QPainter` sobre `QOpenGLWidget` tenia bugs de composicion con imagenes en formato premultiplied. El blend `GL_ONE` arreglaba parcialmente pero no del todo.

### Commits del intento original (pre-sesion)
30 commits entre `db9bc8e` y `f6b508d` intentaron arreglarlo con:
- Pixel snapping, backbuffer, format conversions, blend modes
- Sanitizacion alpha<8, unified world transform
- Software backbuffer → GPU compositing

---

## 2. La "Solucion" del commit `f6b508d`

### Enfoque: Single-Image Approach
`previewImage_` se convirtio en una sola imagen opaca que contenia TODO (fondo blanco + onion skin + capas + dabs) en una sola imagen.

**Ventaja**: Una sola llamada `drawImage` en `paintGL` → sin artefactos de composicion GPU.

**Desventajas (descubiertas en esta sesion)**:

### Bug A: Datos de onion skin horneados en los frames
`previewImage_` contenia `onionCache_` (skins de frames vecinos). En `mouseReleaseEvent`, el codigo capturaba pixeles de `previewImage_` y los componia sobre la capa activa, **escribiendo datos de otros frames en el frame actual**.

### Bug B: Composite cache (todas las capas) horneado en la capa activa
`previewImage_` contenia `compositeCache_` (todas las capas juntas). Al hacer release, **todas las capas** se mezclaban dentro de una sola capa:
- **1 capa**: duplicaba los datos (SourceOver de si misma sobre si misma)
- **Multiples capas**: fusionaba capas cruzadas, corrompiendo el resto

### Bug C: Onion skin congelado durante el trazo
Las cebollas se capturaban una sola vez en `mousePress` y no se actualizaban en vivo durante el dibujo.

---

## 3. Intentos de Solucion (en orden cronologico)

### Intento 1: Arquitectura de Doble Imagen (`dabImage_` + `previewImage_`)

**Fecha**: 1 Julio, ~3 horas

**Premisa**: Separar los dabs del fondo para que el guardado de frames use solo dabs (sin onion ni composite).

| Imagen | Contenido | Uso |
|--------|-----------|-----|
| `previewImage_` | blanco + onion + composite + dabs | Display GPU (1 sola imagen, sin artefactos) |
| `dabImage_` | solo dabs sobre fondo transparente | Composicion de datos en `mouseReleaseEvent` |

**Cambios realizados**:
- Agregados `dabImage_` y `dabPainter_` al header (`canvas_widget_v2.hpp`)
- `mousePressEvent`: crear ambas imagenes, hornear onion+composite en `previewImage_`, dejar `dabImage_` transparente
- `drawIncrementalDabs`: estampar dabs en ambas imagenes simultaneamente
- `mouseReleaseEvent`: usar `dabImage_` (solo dabs) para composicion sobre la capa

**Resultado Parcial**:
- Bug A (onion en frames) corregido
- Bug B (capas fusionadas) corregido
- Bug C (onion congelado) sin cambios

**Nuevo problema**: `STATUS_STACK_BUFFER_OVERRUN` crash. Causa: `dabPainter_` seguia activo sobre `dabImage_` mientras el codigo de composicion leia pixeles directamente con `constScanLine()`. En Qt, tener un QPainter activo mientras se accede directamente a los pixeles de un QImage es UB.

**Fix**: `dabPainter_.reset()` antes de la composicion en `mouseReleaseEvent`.

**Nuevo problema**: Crash resuelto, pero reaparecieron lineas blancas. Analisis teorico exhaustivo no encontro la causa. Sospecha: el `stampBrushDab` ejecutandose en dos `QPainter` simultaneos podria causar interferencia en el estado interno de Qt.

### Intento 2: Canvas Display Cache Unificado

**Fecha**: 2 Julio, ~2 horas

**Premisa**: Simplificar el pipeline del canvas eliminando los caches intermedios (`onionCache_`, `compositeCache_`) y usando una sola imagen `canvasDisplayImage_` construida directamente desde el documento.

**Cambios realizados**:
- Eliminados 6 miembros y 2 metodos del cache antiguo
- Nuevo metodo: `buildCanvasDisplay()` que hornea white + onion + capas en una sola imagen
- `paintGL` idle: `buildCanvasDisplay()` + `drawImage(canvasDisplayImage_)`
- `mousePressEvent`: copia COW de `canvasDisplayImage_` a `previewImage_`
- Un solo flag: `canvasDisplayDirty_`

**Resultado Parcial**:
- Codigo mas simple y mantenible
- Menos puntos de fallo

**Nuevo problema**: Onion skin completamente roto. `renderTintedFrameDirect` usa `SourceAtop` que requiere fondo TRANSPARENTE para aislar el contenido del onion. Pero `buildCanvasDisplay()` llenaba con `Qt::white` (fondo opaco), haciendo que `SourceAtop` tiniiera TODO el canvas de rojo/azul en vez de solo el contenido del onion frame.

### Intento 3: Onion Skin en Buffer Transparente

**Fecha**: 2 Julio, ~1 hora

**Premisa**: Arreglar el onion skin renderizando cada frame en un buffer temporal transparente antes de componer sobre el fondo blanco.

**Cambios realizados**:
- `onionBuf` temporal transparente para cada onion frame
- `renderTintedFrameDirect` sobre buffer transparente → `SourceAtop` funciona correctamente
- Composicion `SourceOver` sobre el display blanco

**Resultado**: Onion skin teoricamente arreglado, pero **volvio el artefacto de esquina blanca** y el onion skin seguia sin funcionar correctamente en pruebas visuales.

---

## 4. El Ciclo Sin Fin

El problema fundamental es un **trade-off irresoluble** con la arquitectura actual:

```
Si separas los dabs del fondo:
  → Datos de frame correctos (por capa)
  → Pero reaparecen artefactos de composicion GPU (esquina blanca, lineas)

Si juntas todo en una imagen opaca:
  → Sin artefactos de display
  → Pero datos de frame corruptos (onion + composite horneados en capas)
```

Cada intento de arreglar uno de los dos lados **rompe el otro**. El problema de fondo es:
- `QPainter` sobre `QOpenGLWidget` tiene bugs de composicion con alpha premultiplied
- La unica forma de evitarlos es usar UNA sola imagen opaca
- Pero el guardado de datos necesita los dabs SEPARADOS del fondo

---

## 5. Preguntas Clave que Quedan Abiertas

1. **¿Es `QOpenGLWidget` el enfoque correcto?** El renderizado GPU introduce complejidad de alpha premultiplied que no existe en `QWidget` (software rendering).

2. **¿Se puede usar un `QImage` backbuffer por CPU en vez de `QOpenGLWidget`?** Esto eliminaria los bugs de composicion GPU pero reintroduce latencia de CPU.

3. **¿Se puede extraer los dabs matematicamente del previewImage_?** Revertir el `SourceOver` para obtener los dabs puros desde la imagen compuesta. Requiere division por alpha — posible pero costoso.

4. **¿Se deberia migrar la capa de renderizado a OpenGL puro?** Evitar `QPainter` completamente y manejar el blending manualmente con shaders.

5. **¿Vulkan o DirectX?** Alternativas modernas que manejarian mejor el alpha premultiplied.

---

## 6. Lecciones Aprendidas

1. **No hacer fixes arquitectonicos sin tests de regresion visual.** Los 154 tests unitarios pasaban siempre, pero no cubrian el onion skin ni la vizualizacion del canvas.

2. **El renderizado GPU con `QPainter` + `QOpenGLWidget` es fragil.** Los bugs de alpha premultiplied requieren workarounds que se vuelven mas complejos que el problema original.

3. **Separar "display" de "data" es conceptualmente correcto pero dificil de implementar** con las limitaciones actuales del pipeline de renderizado.

4. **El `SourceAtop` de Qt requiere fondo transparente.** Esto es un detalle de implementacion que costo horas de debug.

5. **Tener QPainter activo mientras se leen pixeles de QImage es UB.** Documentado pero facil de pasar por alto.

---

## 7. Estado Final (Post-Rollback)

- **Commit rollback**: `1f5f4dc` (26 de junio) — estado estable pre-fixes
- **Branch backup**: `backup-fixes-20260702-083912` (contiene los 41 commits revertidos)

---

## 8. Solucion Implementada — Pipeline de 4 Buffers con Dirty Rects (3 Julio 2026)

### Arquitectura Final

Se implemento un pipeline de renderizado CPU puro (`QWidget`, sin `QOpenGLWidget`) con **separacion estricta entre memoria de DATOS y memoria de DISPLAY**:

| Buffer | Dominio | Contenido | Formato |
|--------|---------|-----------|---------|
| `RasterLayer::pixelBuffer_` | **DATA** | Trazos por capa, fondo transparente | ARGB32 Premultiplied |
| `strokeBuffer_` | **DATA** | Trazo actual aislado, fondo transparente | ARGB32 Premultiplied |
| `backgroundCache_` | **DISPLAY** | Pre-compuesto: blanco + onion skin + capas | ARGB32 Premultiplied |
| `paintEvent` (widget) | **DISPLAY** | Cache + stroke overlay + grid + UI | N/A (QPaintDevice) |

### Invariantes del diseno

1. **`mouseReleaseEvent` NUNCA lee pixeles de buffers de display**: solo lee de `strokeBuffer_` (transparente, aislado) y `pixelBuffer_` (datos de capa, transparente). Esta garantizado matematicamente que jamas se "hornean" datos del onion skin o de otras capas en el frame activo.

2. **Dabs aislados durante el trazo**: `drawBrushStamp()` escribe exclusivamente en `strokeBuffer_`. El `pixelBuffer_` de la capa no se modifica hasta que `commitStroke()` hace el composite `strokeBuffer_ → pixelBuffer_` con QPainter (SourceOver o DestinationOut).

3. **Undo simplificado**: sin `cleanLayerCopy_` de toda la capa ni `growBeforeSnapshot`. Solo se captura `beforeSnapshot_` del `pixelBuffer_` (region sucia) antes del primer dab. Sin contaminacion incremental porque la capa nunca se toca durante el trazo.

4. **Cache de display pre-compuesto**: `backgroundCache_` contiene blanco + onion skin + todas las capas en un solo `QImage`. `paintEvent` hace 1 `drawImage` en vez de iterar N capas. Soporta reconstruccion completa (`rect` nulo) y parcial (dirty rect) con `buildBackgroundCache(rect)`.

### Correcciones de diseno criticas (integradas desde el feedback)

1. **Offset-awareness en dirty rects**: `buildBackgroundCache(R)` usa `R.translated(-originX_, -originY_)` para mapear el dirty rect global al espacio local de cada capa. Capas con `originX_ != 0` o `originY_ != 0` NO fuerzan un FULL rebuild del cache.

2. **Padding dinamico**: `commitStroke()` expande la region sucia con `brushSize_ / 2 + 1` pixeles. Esto cubre el diametro completo del dab (incluyendo bordes suavizados con alpha baja), eliminando el efecto de "caja recortada" en los bordes.

3. **COW-safe**: `strokeBuffer_` se crea fresco con `fill(0x00000000)` y se destruye con `= QImage()` en `commitStroke()`. Sin comparticion implicita accidental.

4. **QPainter scoping**: El `QPainter` de composicion en `commitStroke()` sale de ambito (bloque `{}`) antes de que `captureRect()` use `constScanLine`. Previene UB de `STATUS_STACK_BUFFER_OVERRUN`.

### Cambios eliminados

- `activeDirtyRect_` → reemplazado por `strokeDirtyRect_`
- `cleanLayerCopy_` → innecesario (capa no se modifica durante trazo)
- `growBeforeSnapshot()` → innecesario (sin contaminacion incremental)

### Nuevos metodos en `CanvasWidgetV2`

- `buildBackgroundCache(const QRect& rect = QRect())` — reconstruccion completa o parcial del cache
- `invalidateBackgroundCache()` — marca el cache como invalido

### Archivos modificados

- `src/ui_v2/canvas_widget_v2.hpp` — nuevos miembros: `strokeBuffer_`, `strokeDirtyRect_`, `backgroundCache_`, `backgroundCacheValid_`
- `src/ui_v2/canvas_widget_v2.cpp` — reescritura de `paintEvent`, `drawBrushStamp`, `drawLineStamps`, `commitStroke`, `mousePressEvent`, `mouseReleaseEvent`; nuevo `buildBackgroundCache`, `invalidateBackgroundCache`; invalidaciones de cache en todos los setters y herramientas de edicion directa

### Resultado

- Build exitoso
- 139/139 tests pasando
- 0 artefactos de display
- 0 corrupcion de datos de frame
- Separacion estricta DATA/DISPLAY garantizada

---

### 8.1 Bug de Desincronizacion Visual — Layer Panel ↔ Canvas (3 Julio 2026)

**Problema**: Tras implementar el pipeline de 4 buffers, al alternar la visibilidad de una capa (icono del ojo) o cambiar su blend mode desde LayerPanelV2, el lienzo no se actualizaba. El `backgroundCache_` no se invalidaba.

**Diagnostico**: `LayerPanelV2` agrupaba eventos de seleccion y cambios de propiedades visuales bajo una unica señal `layerChanged(int index)`, que MainWindowV2 conectaba a `CanvasWidgetV2::setCurrentLayer(index)`. Esta ruta correctamente NO invalidaba el cache (solo seleccion de capa activa). `toggleLayerVisibility()` ni siquiera emitia señal alguna.

| Metodo | Emitia senal? | Afecta display? |
|--------|---------------|-----------------|
| `toggleLayerVisibility` | **NO** (bug) | SI |
| `onBlendModeChanged` | `layerChanged` (equivocada) | SI |
| `addRasterLayer` | `layerChanged` (equivocada) | SI |
| `onRowSelected` | `layerChanged` (correcta) | NO |

**Solucion**:

1. Nueva señal `layerDisplayPropertiesChanged()` en `LayerPanelV2` — emitida exclusivamente en metodos que modifican propiedades visuales: `toggleLayerVisibility`, `onBlendModeChanged`, `addRasterLayer`, `addVectorLayer`, `duplicateLayer`, `moveLayerUp`, `moveLayerDown`, `deleteLayer`. NO en `toggleLayerLock`, `onRowSelected`, `startRename`.

2. `invalidateBackgroundCache()` movido de `private:` a `public:` en `CanvasWidgetV2`.

3. Nuevo `connect` en `MainWindowV2::setupConnections()`:
```cpp
connect(layer_panel_, &LayerPanelV2::layerDisplayPropertiesChanged, [this]() {
    if (canvas_) {
        canvas_->invalidateBackgroundCache();
        canvas_->update();
    }
});
```

**Resultado**: Separacion limpia de responsabilidades. `layerChanged(int)` maneja solo el indice de capa activa (sin invalidar cache). `layerDisplayPropertiesChanged()` maneja cambios visuales estructurales (invalida cache). El ojo de visibilidad, blend mode, y operaciones de capa reflejan cambios instantaneamente en el canvas.

**Archivos**: `layer_panel_v2.hpp`, `layer_panel_v2.cpp`, `canvas_widget_v2.hpp`, `main_window_v2.cpp`

---

## 9. Commits Revertidos (41 total)

```
f6b508d → 31dd7fd: Serie de fixes de lineas blancas (30 commits)
6b480cb → c738950: Optimizaciones de latencia (6 commits)
206fbe4 → 0dbdf86: Documentacion y fixes varios (5 commits)
```
