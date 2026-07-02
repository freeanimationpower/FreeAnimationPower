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

## 7. Estado Final

- **Commit actual**: `1f5f4dc` (26 de junio) — estado estable pre-fixes
- **Branch backup**: `backup-fixes-20260702-083912` (contiene los 41 commits revertidos)
- **Proximos pasos**: Definir una estrategia de renderizado que no dependa de `QPainter` + `QOpenGLWidget` para el compositing, o migrar a OpenGL/Vulkan puro.

---

## 8. Commits Revertidos (41 total)

```
f6b508d → 31dd7fd: Serie de fixes de lineas blancas (30 commits)
6b480cb → c738950: Optimizaciones de latencia (6 commits)
206fbe4 → 0dbdf86: Documentacion y fixes varios (5 commits)
```
