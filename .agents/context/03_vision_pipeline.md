# Tarea: Pipeline de Visión Térmica

## Objetivo
Implementar los 5 pasos del algoritmo de visión artificial en el Core 1, optimizando para el ESP32-S3 (FPU).

## Requisitos Técnicos
1. **Filtro Espacial**: Implementar interpolación o suavizado para eliminar el patrón de "ajedrez".
2. **Fondo EMA**: El fondo debe aprenderse solo cuando no hay movimiento detectado (máscara).
3. **NMS**: Supresión de no-máximos usando distancia Euclidiana al cuadrado (para evitar `sqrt`).
4. **Tracking Alpha-Beta**: Mantener la identidad de los tracks (IDs persistentes) mientras se mueven por el FOV.
5. **Conteo por Líneas**: Dos líneas virtuales (p. ej. Y=11 y Y=13) que detectan dirección IN/OUT.

## Notas de Performance
- Todo el procesamiento debe ocurrir en <40ms.
- Usar `float` nativo. No usar `double`.
