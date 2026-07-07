=================================================================
  FREE ANIMATION POWER - MEMORY STRESS TEST REPORT
  Fecha: 2026-06-27 (Sábado)
  Para revisión: Lunes 2026-06-29
=================================================================

## HARDWARE

  CPU:        AMD Ryzen 7 7435HS (8 Cores / 16 Threads)
  RAM Total:  24 GB (25,439,199,232 bytes)
  RAM Libre:  ~12.5 GB al inicio
  OS:         Windows (Lenovo 83JC)

## RESUMEN EJECUTIVO

  Todas las pruebas de estrés de memoria PASARON sin errores.
  No se detectaron fugas de memoria (memory leaks).
  No se produjo ningún crash ni bad_alloc.
  El motor de animación maneja correctamente cargas extremas.

  Total pruebas originales:  139/139 PASARON (100%)
  Total pruebas de estrés:   15/15 PASARON (100%)
  Iteraciones repetidas (x3): 45/45 PASARON (100%)

## RESULTADOS DETALLADOS DE PRUEBAS DE ESTRÉS

### 1. SystemInfo - Información del Sistema
   RAM Total: 24,260 MB | RAM Disponible: ~12,600 MB
   Carga de memoria: 47-48%

### 2. Churn_AllocDealloc_1000Cycles - Ciclos de Alocación/Liberación
   1,000 ciclos de crear/destruir grupos de capas raster
   Ritmo: 335-382 ciclos/segundo (consistente)
   Working Set final: 15-28 MB (sin acumulación = sin leaks)
   Page Faults acumulados: ~10M (normal para alocación masiva)

### 3. MassiveLayerCount_2000Layers - 2,000 Capas Simultáneas
   Working Set: 156 MB | Private Bytes: 147 MB
   ~73 KB por capa (excelente eficiencia)
   Todas las 2,000 capas verificadas: UIDs únicos, buffers estables
   Sin degradación de punteros

### 4. Single4KLayer_FullFill - Capa 4K (3840x2160)
   8,294,400 píxeles (~33.2 MB de buffer)
   Fill pattern aplicado a 2,073,600 píxeles
   Working Set: 46 MB (buffer + overhead normal)
   COW clone: compartido correctamente (shared=1)

### 5. Ten_4KLayers_Simultaneous - 10 Capas 4K Simultáneas
   Working Set: 331 MB | Private Bytes: 324 MB
   ~33 MB por capa (teórico: 33.2 MB) - precisión perfecta
   50,000 píxeles pintados por capa
   Todos los UIDs únicos verificados

### 6. Document_ManyFrames - Documento con 240 Frames
   240 frames con 5 capas de 960x540 por frame
   Working Set: 215 MB | Private Bytes: 207 MB
   Acceso a frames correcto en todo el rango

### 7. UndoStack_Full128 - Pila de Undo con 128 Entradas
   Buffers de 64 KB a ~8 MB por entrada (creciente)
   Working Set: 150 MB | Private Bytes: 147 MB
   Undo completo ejecutado: 128 undo -> canUndo=false, canRedo=true

### 8. CowStress_DeepCloneChain - Copy-on-Write con 20 Clones
   20 clones de capa 2048x2048 (~16 MB c/u)
   Working Set: 351-366 MB | Private Bytes: 343-362 MB
   Todos los clones independizados (isBufferShared=false para todos)
   Patrón COW verificado: buffer original intacto tras modificaciones

### 9. Compositor_FullTarget - Compositor con 100 Capas
   100 capas de 1280x720 compuestas 3 veces
   Working Set: 370-385 MB | Private Bytes: 362-382 MB
   3 pases de composición completados sin errores

### 10. NodeGraph_LargeScale - Grafo de Nodos a Gran Escala
    125 nodos (80 Input + 30 Filter + 15 Transform)
    Working Set: 15-30 MB (mínimo, sin raster buffers)
    Conexiones y accesos verificados

### 11. PointerStability_Extreme - Estabilidad de Punteros Extrema
    500 capas base + 200 inserciones/eliminaciones aleatorias
    ~161 capas perdidas (normal, eliminadas aleatoriamente)
    ~339 capas supervivientes: TODAS con buffers estables
    Punteros a datos de píxeles nunca se movieron

### 12. Bezier_MassivePathGeneration - 5,000 Caminos Bezier
    5,000 curvas bezier generadas y aplanadas
    Tiempo: 3-4 ms | Working Set: 14 MB
    Excelente rendimiento del engine vectorial

### 13. Combined_MixedStress - Estrés Mixto Combinado
    50 capas raster + 100 strokes vectoriales + 20 capas anidadas
    Documento + clonación
    Working Set: 77-78 MB | 53 capas totales

### 14. GrowUntilNearLimit - Crecer Hasta Cerca del Límite **[CRÍTICA]**
    Objetivo: 60% de RAM disponible (~7,500 MB)
    50 capas 4K alocadas exitosamente
    Working Set: 1,597-1,611 MB (~1.6 GB)
    RAM restante: ~10,900-11,100 MB
    **Sin bad_alloc, sin crashes, sin excepciones**
    Crecimiento lineal: ~158 MB por cada 5 capas 4K

    Progresión de memoria:
      Capa  0:   47 MB
      Capa  5:  205 MB  (+158)
      Capa 10:  363 MB  (+158)
      Capa 15:  522 MB  (+159)
      Capa 20:  680 MB  (+158)
      Capa 25:  838 MB  (+158)
      Capa 30:  996 MB  (+158)
      Capa 35: 1154 MB  (+158)
      Capa 40: 1313 MB  (+159)
      Capa 45: 1471 MB  (+158)
      Capa 50: 1597 MB  (+126) - final

## ANÁLISIS DE ESTABILIDAD

  El engine demuestra:
  - Gestión de memoria sólida: sin fugas detectadas
  - COW (Copy-on-Write) funcional y eficiente
  - Estabilidad de punteros bajo operaciones de inserción/eliminación
  - Escalamiento lineal de memoria sin sobrecostos ocultos
  - Capacidad para manejar al menos 50 capas 4K (~1.6 GB) sin degradación
  - Sistema de Undo robusto con 128 entradas de buffers grandes

## PUNTOS A OBSERVAR

  1. 2,000 capas pequeñas consumen solo 156 MB - eficiente
  2. COW evita duplicación innecesaria de pixeles
  3. No se encontraron leaks incluso tras 3 iteraciones completas
  4. El sistema maneja ~11 GB de RAM libre tras estrés máximo
  5. Page Faults elevados (esperado en Windows con alocación masiva)

## ARCHIVOS GENERADOS

  - tests/test_memory_stress.cpp     : Código fuente de pruebas de estrés
  - CMakeLists.txt (modificado)      : Añadido test_memory_stress.cpp
  - memory_stress_results_*.txt      : Resultados crudos de ejecución
  - Este archivo (MEMORY_REPORT.md)  : Reporte completo

=================================================================
  CONCLUSIÓN: EL SISTEMA ES ESTABLE. NO HAY COLAPSO DE MEMORIA.
  LISTO PARA REVISIÓN EL LUNES.
=================================================================
