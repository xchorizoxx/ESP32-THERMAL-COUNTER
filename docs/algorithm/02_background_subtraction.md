# Extracción de Fondo y EMA Selectiva

El primer paso y el más crucial del pipeline es saber qué es "suelo vacío" frente a qué es "una persona". Como la temperatura del ambiente cambia a lo largo del día, no podemos tener una temperatura fija preprogramada.

## EMA (Exponential Moving Average)

EMA significa **Media Móvil Exponencial**. Es un algoritmo que mezcla el valor "histórico" (lo que aprendió en el pasado) con el valor "nuevo" (el frame actual del sensor).

En el código se controla con `EMA_ALPHA`:
* `NuevoValorFondo = (Actual * EMA_ALPHA) + (FondoAnterior * (1 - EMA_ALPHA))`

### ¿Cómo calibrar `EMA_ALPHA`?
`EMA_ALPHA` determina la **velocidad de adaptación**:
* **Si `EMA_ALPHA` es cercano a 0 (ej. 0.01):** El fondo cambia muy, muy lento. Es altamente estable frente al ruido, pero si dejan una caja caliente en el suelo, el sistema tardará mucho tiempo en "acostumbrarse" a la caja y dejar de verla como una persona estática.
* **Si `EMA_ALPHA` es grande (ej. 0.20):** El fondo se adapta casi al instante. Esto es problemático porque una persona que se quede quieta por 2 segundos se volverá "invisible" (absorbida por el fondo).
* **Valor recomendado (0.05 a 0.10):** Permite adaptarse a las corrientes de aire rápido sin absorber a personas en movimiento normal.

## Selectividad y Máscara de Bloqueo

Para evitar que una persona que camina muy lento (o se detiene a hablar debajo del sensor) sea aprendida como "suelo", usamos la **Máscara de Bloqueo**.

El Paso 5 del pipeline (después de identificar a las personas) proyecta un cuadrado de exclusión (controlado por `MASK_HALF_SIZE`) alrededor de cada persona. 
Cuando llega el siguiente frame, el actualizador de fondo (Paso 1) **ignora por completo** los píxeles que están cubiertos por esta máscara. Para esos píxeles, `NuevoValorFondo = FondoAnterior`. 

Esto asegura que el suelo solo se actualice con información de áreas donde estamos 100% seguros de que no hay nadie.

## Contraste (Delta T)

El parámetro `DELTA_T_FONDO` representa cuántos grados Celsius por encima de la temperatura del "fondo aprendido" tiene que estar algo para llamar nuestra atención.
* **Si `DELTA_T_FONDO` es muy bajo (ej. 0.5 °C):** Generará excesivo ruido térmico y "falsos fantasmas".
* **Si `DELTA_T_FONDO` es muy alto (ej. 3.0 °C):** Ignorará cosas calientes. Si una persona tiene pelo grueso o una gorra fría (ej. a 22°C) y el suelo está a 20°C, la diferencia es solo de 2°C, y no la detectaríamos.
* **Valor recomendado (1.0 a 1.5):** Equilibrio perfecto para rechazar fluctuaciones normales del sensor y aun así captar calor biológico.

## Modo Radar (Sustracción)
El sistema permite visualizar directamente el resultado de la resta: `ImagenActual - ImagenFondo`. 
En la interfaz visual, esto se conoce como "Modo Radar". Es útil para:
1. **Verificar el Nivel de Ruido:** Si el fondo está bien aprendido, la imagen debería verse mayormente negra en las áreas vacías.
2. **Detectar Obras o "Obstáculos Calientes":** Si hay un objeto que parpadea o emite calor intermitente, se verá claramente en este modo.
3. **Depuración de Fantasmas:** Si una persona fue "absorbida" por el fondo por error, al levantarse dejará un "hueco de frío" (un fantasma azul) en el modo sustracción.
