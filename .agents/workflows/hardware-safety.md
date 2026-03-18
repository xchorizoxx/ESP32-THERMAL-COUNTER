---
description: Protocolo de Seguridad para Modificaciones de Hardware y Capa Lógica
---

# ⚠️ PROTOCOLO DE SEGURIDAD DE HARDWARE (OBLIGATORIO)

Este documento establece las reglas críticas para cualquier agente IA que interactúe con el proyecto "Detector de Puerta". La violación de estas reglas puede comprometer la estabilidad del sistema físico.

## 1. Modificaciones de Hardware y Bus
- **PROHIBIDO** modificar frecuencias de reloj (I2C/SPI), números de pines (GPIO), o configuraciones de voltajes/pull-ups sin una consulta previa al usuario.
- Cualquier propuesta de cambio en el bus (ej. bajar de 1MHz a 400kHz) debe ir acompañada de una justificación técnica basada en el Datasheet y el comportamiento observado.
- El usuario debe dar un "Visto Bueno" (Aprobar) explícito en el chat antes de que el agente aplique el cambio en `.cpp` o `.h`.

## 2. Cambios Estructurales en el Pipeline
- No se deben alterar los métodos de lectura de sensores (ej. pasar de lecturas bloqueantes a asíncronas) si esto afecta la cadencia de frames establecida, a menos que se diagnostique un fallo crítico.
- Si se detectan artefactos visuales (ej. patrón de ajedrez en MLX90640), el agente debe proponer el filtro o cambio técnico primero, explicar su impacto en el consumo de CPU, y esperar aprobación.

## 3. Prioridad de Estabilidad
- En sistemas embebidos, la estabilidad del sensor y la integridad de la EEPROM son prioritarias sobre la "limpieza" estética del código. 
- Cualquier cambio que pueda causar un "Panic" (Crash) en el Core 1 debe ser comunicado como "Cambio de Alto Riesgo".

---
// turbo-all
