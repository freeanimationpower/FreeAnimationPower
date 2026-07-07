# Investigacion Tecnica: TVPaint Animation + FlipaClip → "Free Animation Power"

## Resumen Ejecutivo

Este documento analiza en profundidad el funcionamiento tecnico de TVPaint Animation y FlipaClip,
dos de los software de animacion 2D mas relevantes del mercado, con el objetivo de disenar
"Free Animation Power": un software de animacion 2D hibrido que combine capacidades vectoriales
y raster (bitmap) en una sola herramienta, inicialmente para PC pero con arquitectura escalable
a otras plataformas.

---

## 1. TVPAINT ANIMATION - Analisis Tecnico Profundo

### 1.1 Ficha Tecnica

| Atributo | Detalle |
|----------|---------|
| Desarrollador | TVPaint Developpement SARL (Lorraine, Francia) |
| Lenguaje | C++ |
| Primer release | 1991 (Amiga) |
| Version actual | TVPaint Animation 12 (Abril 2024) |
| Plataformas | Windows 10/11, macOS (Big Sur - Sequoia), Linux (Ubuntu LTS) |
| Tipo de graficos | **Bitmap/Raster puro** (no vectorial) |
| Licencia | Propietaria, pago unico |
| Arquitectura | 64-bit, CPU-bound (no requiere GPU dedicada) |

### 1.2 Arquitectura del Motor de Pintura y Pinceles

TVPaint es **exclusivamente raster/bitmap**. Esta es su decision arquitectonica mas
importante y la razon de su exito en la industria profesional de animacion.

**Motor de pinceles (Brush Engine):**
- Sistema de pinceles personalizables con motor de texturas duales (Dual Papers System)
- Los pinceles trabajan sobre un grid de pixeles, no sobre curvas matematicas
- "Paper texture" system: simula la interaccion entre diferentes tipos de papel y medios
  (lapiz, carboncillo, acuarela, oleo) mediante combinacion de texturas
- Algoritmo de preservacion de nitidez en transformaciones (nuevo en v12)
- Soporte para tabletas graficas con presion, inclinacion y rotacion del lapiz
- Sistema de estabilizacion de trazo

**Motor de Color:**
- CTG Layers (Smart Colorization) en version Professional
- Sistema de paletas personalizables
- Integracion con tabletas para muestreo de color

### 1.3 Sistema de Animacion

**Timeline y Capas:**
- Linea de tiempo basada en frames, no en segundos (enfoque "tradicional")
- Sistema de capas (layers) multiple con carpetas de capas (Layer Folders)
- Cada capa puede tener su propia duracion y cantidad de frames
- Los frames pueden contener dibujos independientes unos de otros

**Light Table (Mesa de luz):**
- Visualizacion de frames anteriores y posteriores (onion skinning)
- Modo "Out of Pegs" para ver frames fuera de registro
- Permite ver multiples frames simultaneamente con opacidad configurable
- Herramienta esencial para la animacion "pose a pose" y "straight ahead"

**Herramientas Profesionales de Animacion:**
- Puppet Layers (Bitmap Rigging): sistema de rigging sobre bitmap para animacion
  de personajes sin necesidad de repintar cada frame
- 2D Camera: camara virtual con keyframes para pans, zooms y rotaciones
- Storyboarding tools integrado
- Scan Cleaner: limpieza automatica de escaneos de papel
- Image Library: biblioteca de assets reutilizables
- FX Stack: efectos visuales en capas no destructivas (desenfoque, colorizacion, etc.)
- TVPaint Converter: conversion de formatos de video/imagen

### 1.4 Estructura del Proyecto/Archivo

- Formato nativo: .tvpp (TVPaint Project)
- Soporta exportacion a formatos estandar de video (MOV, AVI, MP4)
- Exportacion de secuencias de imagenes (PNG, JPEG, TIFF)
- Importacion de video para rotoscopia
- Manejo de audio para sincronizacion labial (lip-sync)

### 1.5 Puntos Clave de su Exito

1. **Fidelidad al papel**: reproduce exactamente la experiencia de dibujo tradicional
2. **Bitmap puro**: cada trazo es pixel real, lo que da una sensacion organica que el
   vector no puede igualar
3. **Industria profesional**: usado en peliculas nominadas al Oscar (Song of the Sea,
   Wolfwalkers, The Red Turtle, Kitbull, Burrow)
4. **Rendimiento CPU**: no depende de GPU, lo que lo hace accesible en hardware modesto
5. **Comunidad**: foros activos, educacion en escuelas de animacion

---

## 2. FLIPACLIP - Analisis Tecnico Profundo

### 2.1 Ficha Tecnica

| Atributo | Detalle |
|----------|---------|
| Desarrollador | Visual Blasters LLC (Miami, EE.UU. - hermanos Meson de Argentina) |
| Lenguaje | No confirmado publicamente (probablemente C++ con motor grafico movil) |
| Primer release | 2 de abril de 2012 (Android) |
| Plataformas | Android, iOS, Windows, macOS, ChromeOS |
| Tipo de graficos | **Raster/Bitmap** (enfoque en simplicidad) |
| Licencia | Freeware + suscripcion (FlipaClip Plus) |
| Usuarios | 80+ millones de instalaciones, 6M MAU (2022) |

### 2.2 Arquitectura del Motor de Dibujo

**Enfoque en simplicidad y rendimiento movil:**
- Motor de dibujo bitmap optimizado para pantallas tactiles
- Herramientas basicas: lapiz, pincel, borrador, relleno (bucket), regla
- Regla personalizable como herramienta de guia
- Soporte para stylus (presion)
- Modo claro/oscuro y colores de acento personalizables

**Sistema de Capas:**
- Maximo 10 capas por frame
- Control de opacidad por capa
- Sistema sencillo de gestion de capas

### 2.3 Sistema de Animacion

**Flujo de trabajo:**
- Timeline simple basado en frames
- FPS configurable (frames per second)
- Onion skin (visualizacion de frames adyacentes)
- Frames Viewer: seleccion multiple de frames para reorganizar, duplicar, eliminar, compartir
- Animacion cuadro por cuadro (frame-by-frame tradicional)
- Rotoscopia: importacion de video e imagenes para calcar

**Sistema de Audio:**
- Importacion de archivos de audio
- Permite empezar con efectos de sonido y animar sobre ellos
- Sincronizacion basica

### 2.4 UI/UX y Diseno de Interaccion

**Diseno mobile-first:**
- Interfaz adaptativa: layout diferente en telefono vertical vs tablet horizontal
- Tutorial introductorio integrado
- Navegacion intuitiva con iconos grandes
- Comunidad integrada: compartir en YouTube, TikTok, Instagram
- Stacks de proyectos (arrastrar proyectos unos sobre otros para agrupar)

### 2.5 Modelo de Negocio

- Version gratuita con anuncios (algo intrusivos segun reviews)
- FlipaClip Plus (suscripcion paga) con funciones extra
- Enfoque en creadores jovenes (70% menores de 18 anos)
- 15 idiomas disponibles
- Comunidad activa en Discord

### 2.6 Puntos Clave de su Exito

1. **Simplicidad extrema**: curva de aprendizaje minima
2. **Mobile-first**: disenado para el dispositivo que los jovenes ya tienen
3. **Compartir integrado**: publicacion directa a redes sociales
4. **Gratuito con opcion premium**: baja barrera de entrada
5. **Comunidad vibrante**: concursos, challenges, contenido generado por usuarios

---

## 3. ANALISIS COMPARATIVO

| Caracteristica | TVPaint Animation | FlipaClip | Krita (referencia OS) |
|---------------|-------------------|-----------|------------------------|
| **Tipo grafico** | Raster puro | Raster | Raster + Vector |
| **Publico** | Profesional/Estudio | Principiante/Joven | Intermedio/Avanzado |
| **Motor pinceles** | Avanzado (dual paper) | Basico | Avanzado (9 motores) |
| **Rigging** | Puppet Layers (bitmap) | No | No |
| **Onion skin** | Avanzado (out of pegs) | Basico | Si |
| **Audio** | Si | Si | No |
| **Vector** | No | No | Si |
| **Open source** | No | No | Si (GPL) |
| **Precio** | Pago unico (caro) | Freemium | Gratis/Donacion |
| **Lenguaje** | C++ | ? | C++/Qt |
| **GPU req** | No | No | OpenGL 3.0+ |
| **Timeline** | Profesional | Simple | Medio |

---

## 4. ARQUITECTURA PROPUESTA PARA "FREE ANIMATION POWER"

### 4.1 Vision

Un software de animacion 2D que combine:
- **Motor raster** (como TVPaint): para dibujo natural, organico, texturizado
- **Motor vectorial** (como Krita/Synfig): para lineas limpias, escalables, editables
- **Cambio fluido entre modos**: el usuario puede alternar entre trabajar en raster o
  vectorial dentro del mismo proyecto, incluso en la misma capa

### 4.2 Stack Tecnologico Recomendado

```
┌─────────────────────────────────────────────────────┐
│                  FREE ANIMATION POWER                 │
├─────────────────────────────────────────────────────┤
│  Lenguaje:        C++20 (core engine)                │
│  UI Framework:    Qt 6.x (cross-platform nativo)     │
│  Graficos 2D:     Skia (Google) + OpenGL/Vulkan      │
│  Vector:          Custom Bezier engine o librsvg     │
│  Raster:          Motor de pinceles propio           │
│  Animacion:       Timeline engine propio             │
│  Audio:           PortAudio + libsndfile             │
│  Video export:    FFmpeg                             │
│  Scripting:       Lua o Python (plugins)             │
│  Testing:         Google Test + Qt Test              │
│  Build:           CMake + vcpkg (gestor paquetes)    │
│  Licencia:        GPL-3.0 o AGPL (open source)      │
└─────────────────────────────────────────────────────┘
```

**Justificacion del stack:**
- **C++/Qt**: Mismo stack que Krita y TVPaint. Rendimiento maximo, madurez, documentacion
- **Skia**: Motor grafico 2D de Google (usado en Chrome, Flutter, Android). Excelente
  rendimiento para raster y soporte vectorial basico
- **FFmpeg**: Estandar de facto para encoding/decoding de video
- **vcpkg**: Gestor de paquetes C++ para Windows/Linux/macOS

### 4.3 Arquitectura del Motor Hibrido

```
┌──────────────────────────────────────────────────────────────────┐
│                     APPLICATION LAYER (Qt)                        │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────────────┐   │
│  │ Timeline │ │ Canvas   │ │ Toolbox  │ │ Property Editor  │   │
│  │ Panel    │ │ Viewport │ │ Panel    │ │ Panel            │   │
│  └──────────┘ └──────────┘ └──────────┘ └──────────────────┘   │
├──────────────────────────────────────────────────────────────────┤
│                      CORE ENGINE LAYER                           │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │                Hybrid Document Model                       │   │
│  │  ┌─────────┐  ┌─────────┐  ┌──────────┐  ┌───────────┐  │   │
│  │  │ Raster  │  │ Vector  │  │  Group   │  │ Animation │  │   │
│  │  │ Layer   │  │ Layer   │  │  Layer   │  │ Timeline  │  │   │
│  │  └─────────┘  └─────────┘  └──────────┘  └───────────┘  │   │
│  │  ┌──────────────────────────────────────────────────┐    │   │
│  │  │            Hybrid Stroke Engine                    │    │   │
│  │  │  ┌──────────┐  ┌──────────┐  ┌───────────────┐   │    │   │
│  │  │  │ Raster   │  │ Vector   │  │ Stroke        │   │    │   │
│  │  │  │ Strokes  │  │ Paths    │  │ Stabilizer    │   │    │   │
│  │  │  └──────────┘  └──────────┘  └───────────────┘   │    │   │
│  │  └──────────────────────────────────────────────────┘    │   │
│  │  ┌──────────────────────────────────────────────────┐    │   │
│  │  │              Brush Engine                           │    │   │
│  │  │  - Pixel brushes (raster)                          │    │   │
│  │  │  - Vector brushes (SVG paths along stroke)         │    │   │
│  │  │  - Paper texture simulation                        │    │   │
│  │  │  - Dual brush system (TVPaint-like)                │    │   │
│  │  └──────────────────────────────────────────────────┘    │   │
│  └──────────────────────────────────────────────────────────┘   │
├──────────────────────────────────────────────────────────────────┤
│                     RENDERING LAYER                              │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────────┐   │
│  │ Skia 2D  │  │ OpenGL   │  │ Vulkan   │  │ Software     │   │
│  │ Renderer │  │ Backend  │  │ Backend  │  │ Fallback     │   │
│  └──────────┘  └──────────┘  └──────────┘  └──────────────┘   │
├──────────────────────────────────────────────────────────────────┤
│                     PLATFORM ABSTRACTION                         │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────────┐   │
│  │ Windows  │  │ macOS    │  │ Linux    │  │ Mobile (fut) │   │
│  └──────────┘  └──────────┘  └──────────┘  └──────────────┘   │
└──────────────────────────────────────────────────────────────────┘
```

### 4.4 Componentes Clave del Motor Hibrido

#### A. Hybrid Document Model
```
Document
├── Project Settings (FPS, resolution, color space)
├── Animation Timeline
│   ├── Keyframes
│   └── Tweens (interpolacion opcional para vector)
├── Layer Stack
│   ├── RasterLayer (grid de pixeles)
│   ├── VectorLayer (coleccion de paths bezier)
│   ├── GroupLayer (contenedor de otras capas)
│   ├── AudioLayer (pista de audio)
│   └── CameraLayer (camara virtual 2D con transforms)
└── Asset Library
    ├── Brushes
    ├── Palettes
    ├── Symbols (reusables)
    └── Textures/Papers
```

#### B. Motor de Pinceles Hibrido

**Pinceles Raster (tipo TVPaint):**
- Tip (punta del pincel): imagen bitmap con alpha
- Paper (textura de papel): imagen bitmap que modula la opacidad
- Dual brush: combina dos texturas para efectos complejos
- Parametros: tamano, opacidad, flujo, espaciado, angulo, dureza
- Estabilizador de trazo configurable
- Modos de fusion (blend modes): multiply, screen, overlay, etc.
- Soporte para presion, inclinacion, rotacion (tabletas Wacom/XP-Pen/Huion)

**Pinceles Vectoriales:**
- Trazo definido por curvas Bezier con ancho variable
- Pinceles de patron (pattern brushes): SVG deformado a lo largo del path
- Pinceles de dispersion (scatter brushes): objetos esparcidos
- Conversion automatica de trazo raster a vector (trazado de imagen)
- Los trazos vectoriales permanecen editables (nodos, tangentes)

#### C. Timeline Engine

```
Timeline
├── Tracks (pistas)
│   ├── LayerTrack (asociado a una capa)
│   ├── CameraTrack (movimientos de camara)
│   └── AudioTrack (audio)
├── Keyframes
│   ├── Position (x, y)
│   ├── Rotation
│   ├── Scale
│   ├── Opacity
│   └── Custom properties
├── Onion Skin Engine
│   ├── Previous frames (configurable: 1-N)
│   ├── Next frames (configurable: 1-N)
│   ├── Tint colors (rojo/azul para prev/next)
│   └── Opacity per frame
└── Playback Engine
    ├── Real-time preview
    ├── Looping (ping-pong, normal)
    └── Variable FPS
```

### 4.5 Filosofia de Escalabilidad

```
Fase 1: Desktop (Windows + Linux)
    ↓
Fase 2: macOS
    ↓
Fase 3: Tablets (Android/iOS con UI adaptativa)
    ↓
Fase 4: Web (WebAssembly + WebGL)
```

La arquitectura debe separar claramente:
1. **Core engine** (C++ puro, sin dependencias de UI)
2. **Rendering backend** (abstraccion sobre OpenGL/Vulkan/Software)
3. **UI layer** (Qt Widgets para desktop, Qt Quick para mobile)
4. **Platform abstraction** (file I/O, input events, clipboard, etc.)

---

## 5. PLAN DE DESARROLLO POR FASES

### Fase 0: Fundacion (3-4 meses)

```
Objetivo: Repositorio, tooling, arquitectura base
```

| Tarea | Descripcion |
|-------|-------------|
| Setup proyecto | CMake + vcpkg + CI/CD (GitHub Actions) |
| Arquitectura core | Definir interfaces: Document, Layer, Stroke, Timeline |
| Canvas rendering | Viewport con zoom/pan/rotacion via Skia |
| Input handling | Eventos de raton y tableta grafica |
| Undo/Redo system | Command pattern para historial de acciones |
| Formato archivo | .fap (Free Animation Power) document format |

**Entregable:** Aplicacion que abre una ventana con un canvas blanco, permite hacer
trazos basicos raster con un pincel simple y tiene undo/redo funcional.

---

### Fase 1: Motor Raster Core (4-6 meses)

```
Objetivo: Dibujo raster comparable a TVPaint basico
```

| Tarea | Descripcion |
|-------|-------------|
| Brush engine v1 | Sistema de pinceles con tip, spacing, size, opacity |
| Tablet support | Presion, inclinacion, rotacion via Windows Ink / Wacom API |
| Brush presets | Carga/guardado de configuraciones de pincel |
| Layers raster | Capas multiples, orden, visibilidad, opacidad, blend modes |
| Color system | Selector de color HSV/RGB, paletas guardables |
| Paper texture | Sistema de textura de papel (modulacion de opacidad) |
| Eraser tools | Borrador suave, borrador duro |
| Selection tools | Seleccion rectangular, eliptica, lazo |
| Fill tool | Bucket fill con tolerancia configurable |
| Transform tools | Mover, escalar, rotar, deformar seleccion |

**Entregable:** Software de dibujo raster funcional con capas, pinceles personalizables,
y soporte completo de tableta grafica.

---

### Fase 2: Sistema de Animacion (3-4 meses)

```
Objetivo: Timeline + onion skin + export
```

| Tarea | Descripcion |
|-------|-------------|
| Timeline panel | UI de timeline con tracks, keyframes, scrubber |
| Frame management | Agregar, duplicar, eliminar, reordenar frames |
| Onion skin | Visualizacion de frames anterior/siguiente con tintes |
| Playback | Reproduccion en tiempo real con FPS configurable |
| Light table | Igual que onion skin pero mas avanzado |
| Audio track | Importar y reproducir audio en timeline |
| Export video | Render a MP4/GIF/PNG sequence via FFmpeg |
| Animatic mode | Modo de storyboard con frames como miniaturas |

**Entregable:** Timeline funcional que permite animacion cuadro por cuadro con
onion skin, reproduccion, y exportacion a formatos estandar.

---

### Fase 3: Motor Vectorial (4-5 meses)

```
Objetivo: Capas vectoriales hibridas
```

| Tarea | Descripcion |
|-------|-------------|
| Vector layer | Nueva capa de tipo vectorial |
| Bezier engine | Creacion y edicion de curvas Bezier |
| Vector brushes | Pinceles que generan paths vectoriales |
| Node editing | Edicion de nodos, tangentes, tipos de nodo |
| Fill & stroke | Relleno solido/gradiente, trazo configurable |
| Vector transforms | Escalar/rotar sin perdida de calidad |
| Raster↔Vector | Conversion de trazo raster a vector y viceversa |
| SVG import/export | Importar y exportar SVG |

**Entregable:** Sistema completo de dibujo vectorial que coexiste con capas raster
en el mismo documento.

---

### Fase 4: Herramientas Profesionales (4-6 meses)

```
Objetivo: Features avanzadas tipo TVPaint Pro
```

| Tarea | Descripcion |
|-------|-------------|
| 2D Camera | Camara virtual con keyframes (pan, zoom, rotacion) |
| Puppet rigging | Sistema de huesos/rigging para capas raster (mesh deformation) |
| Smart colorization | Relleno automatico de areas cerradas (tipo CTG layers) |
| FX Stack | Efectos no destructivos (blur, glow, color correction) |
| Scan cleaner | Limpieza de lineas escaneadas |
| Reference layers | Capas de referencia para guia de dibujo |
| Clone layers | Capas clonadas que reflejan cambios de la original |
| Mask layers | Mascaras de capa para recortes no destructivos |
| Scripting API | API Lua/Python para plugins y automatizacion |

**Entregable:** Software de animacion profesional competitivo con TVPaint.

---

### Fase 5: Pulido y Distribucion (3-4 meses)

```
Objetivo: Release publico y construccion de comunidad
```

| Tarea | Descripcion |
|-------|-------------|
| Performance optimization | Perfilado y optimizacion de rendimiento |
| UI/UX polish | Refinamiento de la interfaz, temas, accesibilidad |
| Localization | Sistema de traduccion (espanol, ingles, frances, etc.) |
| Documentation | Manual de usuario, tutoriales en video, API docs |
| Website | Pagina web, foro, comunidad Discord |
| Installers | Windows (.msi/.exe), Linux (.AppImage/.deb/.rpm) |
| Auto-updater | Sistema de actualizaciones automaticas |
| Testing suite | Tests unitarios, de integracion, de regresion visual |

**Entregable:** Version 1.0 publica lista para distribucion.

---

## 6. CONSIDERACIONES TECNICAS CRITICAS

### 6.1 Rendimiento del Pincel

El pincel es la interaccion mas frecuente. Debe responder en <16ms (60fps).
Estrategias:
- Usar tiles/caching para solo repintar la region afectada del canvas
- Procesar eventos de tablet en un hilo separado (input thread)
- Usar GPU para composicion de capas (texture atlas), pero CPU para rasterizacion
  de pinceles (como TVPaint)
- Pre-renderizar tips de pinceles como texturas

### 6.2 Memoria de Proyectos de Animacion

Una animacion de 24fps a 1080p con 100 frames y 10 capas = ~6GB de datos raster.
Estrategias:
- Almacenamiento sparse (solo guardar frames que tienen contenido)
- Compresion sin perdida en memoria (RLE, LZ4)
- Streaming desde disco para animaciones muy largas
- Sistema de cache LRU para frames no visibles

### 6.3 Precision Vectorial

Los pinceles vectoriales deben:
- Simplificar puntos sobre la marcha (Ramer-Douglas-Peucker)
- Interpolar presion/tamano a lo largo del path
- Permitir edicion post-dibujo sin perder la intencion original

### 6.4 Formato de Archivo

```
.fap (ZIP container)
├── document.json        (metadata, settings, timeline)
├── layers/
│   ├── layer_0/
│   │   ├── frames/
│   │   │   ├── frame_0000.png    (raster frames)
│   │   │   ├── frame_0001.png
│   │   │   └── ...
│   │   └── meta.json
│   ├── layer_1/
│   │   ├── strokes.json          (vector strokes)
│   │   └── meta.json
│   └── ...
├── assets/
│   ├── brushes/
│   ├── palettes/
│   └── textures/
└── audio/
    └── track_0.wav
```

---

## 7. COMPARACION CON COMPETIDORES OPEN SOURCE

| Software | Ventajas | Desventajas para nuestro proposito |
|----------|----------|-------------------------------------|
| **Krita** | Excelente motor raster, vector basico, animacion basica, GPL | Animacion es secundaria, pinceles vectoriales limitados, timeline rigido |
| **Pencil2D** | Ligero, simple, open source | Solo raster, timeline simple, no vector |
| **Synfig** | Animacion vectorial potente, bones, cutout | No tiene motor raster serio, UI compleja |
| **OpenToonz** | Profesional (Studio Ghibli), efectos, GPL | Codigo legacy complejo, UI confusa, solo raster |
| **Glaxnimate** | Vectorial moderno, animacion | Solo vector, proyecto joven |
| **Blender (Grease Pencil)** | 2D en espacio 3D, poderoso | Demasiado complejo para 2D puro, no es software de dibujo |

**Oportunidad de "Free Animation Power":** Ningun competidor open source ofrece
un hibrido raster+vector bien integrado con timeline de animacion profesional.
Krita es lo mas cercano pero su animacion es un afterthought.

---

## 8. RECURSOS Y REFERENCIAS

### 8.1 Librerias Clave

| Proposito | Libreria | Licencia |
|-----------|----------|----------|
| Motor grafico 2D | [Skia](https://skia.org) | BSD |
| UI Framework | [Qt 6](https://qt.io) | LGPL/GPL |
| Video encoding | [FFmpeg](https://ffmpeg.org) | LGPL/GPL |
| Audio I/O | [PortAudio](http://portaudio.com) | MIT |
| Manipulacion imagenes | [OpenImageIO](https://openimageio.org) | BSD |
| Algebra lineal | [Eigen](https://eigen.tuxfamily.org) | MPL2 |
| Compresion | [LZ4](https://github.com/lz4/lz4) | BSD |
| JSON parsing | [nlohmann/json](https://github.com/nlohmann/json) | MIT |
| Testing | [Google Test](https://github.com/google/googletest) | BSD |
| Parsing SVG | [lunasvg](https://github.com/sammycage/lunasvg) | MIT |
| Gestor paquetes | [vcpkg](https://vcpkg.io) | MIT |
| Fuente UI | [Material Design Icons](https://materialdesignicons.com) | Apache 2.0 |

### 8.2 Software de Referencia para Estudiar

1. **Krita** (C++/Qt) - https://invent.kde.org/graphics/krita
   - Motor de pinceles mas avanzado del open source
   - Arquitectura de capas y mascaras
   - Timeline de animacion
   - Vector tools

2. **Pencil2D** (C++/Qt) - https://github.com/pencil2d/pencil
   - Timeline simple y efectivo
   - Onion skin
   - Ligero y rapido

3. **MyPaint** (C++/Python) - https://github.com/mypaint/mypaint
   - Motor de pinceles procedurales excelente
   - Sistema de pinceles libmypaint (usado tambien por Krita y GIMP)

4. **OpenToonz** (C++/Qt) - https://github.com/opentoonz/opentoonz
   - Sistema de efectos (FX schematic)
   - Scan cleaner algorithm
   - Xsheet timeline (estilo profesional japones)

---

## 9. CONCLUSIONES Y PROXIMOS PASOS

### Lo que aprendimos de TVPaint:
- El **bitmap puro** con buen motor de pinceles y texturas de papel crea resultados
  organicos que los estudios profesionales prefieren
- La **simplicidad del timeline** (basado en frames, no en segundos) es clave para
  animadores tradicionales
- No necesitas GPU para ser profesional
- Las herramientas de **pre-produccion** (storyboarding) y **post-produccion** (FX stack)
  integradas son un gran diferenciador

### Lo que aprendimos de FlipaClip:
- La **simplicidad extrema** atrae a millones de usuarios
- El **mobile-first** abre un mercado masivo de creadores jovenes
- Una **comunidad vibrante** (Discord, challenges, redes sociales) multiplica el crecimiento
- El modelo **freemium** con funcionalidades basicas gratuitas y avanzadas de pago
  funciona bien para software creativo

### Proximos Pasos Inmediatos:

1. [ ] Crear repositorio Git con estructura CMake base
2. [ ] Configurar CI/CD con GitHub Actions (build + test)
3. [ ] Implementar "Hello Canvas": ventana Qt + area de dibujo con Skia
4. [ ] Implementar primer trazo raster simple (lapiz sin presion)
5. [ ] Agregar soporte de tableta grafica (presion)
6. [ ] Implementar sistema de capas basico
7. [ ] Implementar timeline simple con onion skin
8. [ ] Agregar primer pincel vectorial (bezier stroke)
9. [ ] Integrar exportacion via FFmpeg
10. [ ] Release v0.1 MVP

---

*Documento generado como analisis tecnico para el proyecto "Free Animation Power" - Junio 2026*
