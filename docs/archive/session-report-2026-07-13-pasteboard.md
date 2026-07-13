# Session Report — Pasteboard Transparency Experiment

**Fecha**: 2026-07-13
**Versión**: v2.5.1 (sin cambios finales)
**Estado**: Revertido al último commit (dbb54e3)
**Conclusión**: Mantener canvas fijo sin desborde de contenido al pasteboard

---

## 1. Objetivo

Permitir que los dibujos movidos fuera del canvas (rectángulo blanco de trabajo) sigan siendo visibles en el área de pasteboard (gris oscuro alrededor), reduciendo la opacidad de dicho fondo.

---

## 2. Iteraciones de prueba

### 2.1 Primer intento — Pasteboard semitransparente con slider

**Enfoque**: Añadir un slider en el Toolbox que controle la opacidad del pasteboard (0-100%, default 85%). En `paintEvent`, se dibuja el pasteboard con alpha + una pasada extra que renderiza todas las capas raster a su tamaño completo fuera del canvas.

**Resultado**:
- ✅ Inicialmente funcionó: dibujos movidos fuera del canvas eran visibles
- ❌ Al mover el dibujo muy lejos, el contenido terminaba fuera del viewport y parecía "desaparecer"
- ❌ El auto-pan que seguía al contenido (`offsetX -= dx * zoom`) causaba que el dibujo se viera "trabado" — el viewport lo perseguía pixel a pixel y nunca cambiaba de posición en pantalla

### 2.2 Segundo intento — Checkerboard + auto-pan corregido

**Enfoque**: Patrón checkerboard en el pasteboard (estilo Photoshop) + auto-pan basado en viewport bounds.

**Resultado**:
- ❌ El checkerboard añadía complejidad innecesaria
- ❌ El auto-pan con bounds seguía teniendo problemas — cuando el dibujo estaba en una esquina del buffer, el auto-pan no lo centraba correctamente

### 2.3 Tercer intento — Expansión del backgroundCache_

**Enfoque**: Hacer que `backgroundCache_` cubra todo el contenido de las capas (no solo el canvas). El cache se expande dinámicamente para abarcar el extent completo de todas las capas raster visibles.

**Resultado**:
- ❌ El canvas parecía "crecer" visualmente — el rectángulo blanco se expandía
- ❌ Contradice el requisito de que el canvas debe permanecer de tamaño fijo
- ❌ Los partial rebuilds del cache se rompían cuando el cache era más grande que el canvas

### 2.4 Cuarto intento — Pasteboard con límites de workspace

**Enfoque**: Workspace fijo de `canvas ± 3000px`. El pasteboard es semitransparente y las capas se dibujan a tamaño completo. Se añaden límites:
- Zoom out máximo = hasta que el workspace completo cabe en la ventana
- Canvas máximo = 3000×3000 px
- Canvas mínimo ancho = 1080 px
- Límite de movimiento en `commitMove` para mantener contenido dentro del workspace

**Resultado**:
- ❌ El brush empezó a recortar los bordes de los trazos (faltaba guard band de 2px)
- ❌ El `ensureContains` con `pad=true` en `drawBrushStamp` expandía agresivamente el buffer (512px growth pad) cada vez que el usuario dibujaba cerca del borde
- ❌ Esto causaba que `kMaxDim=10000` truncara el buffer por un lado cuando se expandía por el otro → PÉRDIDA DE DATOS
- ❌ El `ensureContains` con `pad=false` no cubría el feathering de los bordes del pincel

---

## 3. Bugs descubiertos durante la experimentación

| # | Bug | Causa raíz |
|---|-----|-----------|
| 1 | **Crash `std::out_of_range`** | `rootLayerForFrame()` (mutante) llamado en `paintEvent` creaba frames espurios |
| 2 | **Stroke buffer overlay desplazado** | `strokeBuffer_` dibujado en `(0,0)` canvas coords pero dabs en coords layer-local. Si `origin != (0,0)` → offset visual |
| 3 | **Brush deja de dibujar** | `drawBrushStamp` nunca llamaba `ensureContains` → fallaba silenciosamente fuera del buffer |
| 4 | **Contenido perdido por `kMaxDim`** | `ensureContains(pad=true)` expande 512px. Si buffer está en el límite, la expansión en un lado trunca el otro → datos perdidos |
| 5 | **`hasContent_` incorrecto en undo de Move** | `pushFullLayerUndo(layer, before)` — el 3er parámetro `hadContentBefore` default `false` no se pasaba |
| 6 | **Dibujo "trabado"** | Auto-pan seguía al contenido pixel a pixel → el dibujo nunca cambiaba de posición en pantalla |
| 7 | **Feathering recortado** | Sin guard band de 2px, los bordes suaves del pincel se cortaban en el límite del buffer |

---

## 4. Por qué se revirtió

El pipeline de display actual (4-buffer: `pixelBuffer_` → `backgroundCache_` → `paintEvent`) está diseñado con la invariante de que el canvas tiene un tamaño fijo (`docW × docH`). Intentar mostrar contenido fuera de estos límites requiere:

1. **Renderizado adicional** en `paintEvent` (pasada de pasteboard) — añade complejidad y posibles interferencias con el stroke buffer overlay
2. **Expansión dinámica de buffers** — el `ensureContains` con growth pad de 512px es agresivo y puede causar pérdida de datos al chocar con `kMaxDim`
3. **Gestión del viewport** — el auto-pan introduce comportamientos no intuitivos

Cada fix introducía nuevos bugs. La lógica de pantalla se volvía frágil.

**Decisión final**: Mantener el comportamiento original. El canvas es un espacio de trabajo fijo. Los dibujos que se mueven fuera del canvas no son visibles (el pasteboard permanece gris sólido `#1A1D24`). El usuario debe usar el Move tool con cuidado y mantener el contenido dentro del canvas.

---

## 5. Estado final del código

Sin cambios. El repositorio permanece en el commit `dbb54e3`.

---

## 6. Lecciones aprendidas

- El sistema de buffers de capa (`ensureContains`, `relocatePixels`, `kMaxDim`, `kGrowthPad`) es delicado. Cualquier cambio en cómo se expanden los buffers tiene efectos en cascada.
- La separación DATA/DISPLAY del pipeline de 4 buffers es correcta pero rígida. El `backgroundCache_` está acoplado al tamaño del canvas.
- Para una futura implementación de pasteboard transparente, se recomienda:
  - Refactorizar `buildBackgroundCache` para soportar un tamaño de cache variable
  - Separar el renderizado del canvas del renderizado del pasteboard en dos caches independientes
  - Usar un sistema de clip explícito en vez de depender de `kMaxDim`
  - Eliminar el `kGrowthPad` de 512px y usar expansión mínima con guard band fijo
