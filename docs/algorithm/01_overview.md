# Overview del Algoritmo de Detección Térmica

El sistema de visión térmica para el conteo de personas transforma los datos brutos del sensor MLX90640 (una matriz de 32x24 píxeles) en eventos de conteo ("Entrada" / "Salida").

Para lograr esto de forma robusta frente a cambios de temperatura ambiente y ruido del sensor, el sistema emplea un **Pipeline (Tubería) de Procesamiento de 5 Pasos** que se ejecuta a 16 hercios (16 veces por segundo).

## Los 5 Pasos del Pipeline

1. **Fondo Dinámico (EMA Selectiva):** El sistema "aprende" cuál es la temperatura normal del suelo y las paredes vacías. Si algo cambia de forma gradual (el sol calienta el piso), el fondo se adapta. Si algo cambia rápido (entra una persona), resalta contra este fondo.
2. **Detección de Picos (Topología):** Busca los píxeles que están significativamente más calientes que el fondo _y_ que el resto de píxeles a su alrededor. Estos "picos de calor" son los candidatos a ser cabezas/hombros de personas.
3. **Supresión de No-Máximos (NMS):** Como una persona es más grande que un solo píxel, su cuerpo genera un "bulto" de calor con varios píxeles calientes. La NMS se asegura de que solo nos quedemos con el píxel _más caliente_ de cada bulto, eliminando los picos redundantes cercanos.
4. **Tracking (Filtro Alpha-Beta) y Conteo:** Una vez que tenemos los puntos (cabezas), el sistema los rastrea frame a frame. Predice a dónde se van a mover y comprueba si han cruzado las líneas virtuales de entrada o salida.
5. **Máscara de Retroalimentación:** Los lugares donde actualmente hay personas no deben ser aprendidos como "suelo". Se genera una "sombra" alrededor de los tracks activos para decirle al Paso 1 que **congele** el aprendizaje del fondo en esas zonas.

## Telemetría de Salud (Ta)
Además del flujo de visión, el sistema monitorea la **Temperatura Ambiente (Ta)** interna del sensor. Este valor es crítico para que la librería matemática de Melexis pueda compensar la lectura infrarroja. Un valor de `Ta` entre 30°C y 50°C es normal debido al calor generado por el propio ESP32 y el módulo WiFi.

En los siguientes documentos de esta carpeta se detalla el funcionamiento exacto de cada paso y cómo calibrar sus parámetros asociados.
