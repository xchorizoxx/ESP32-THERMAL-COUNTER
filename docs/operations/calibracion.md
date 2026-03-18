# Guía de Calibración Térmica — Operaciones

El sistema provee una interfaz "In-Situ" alojada en la dirección `http://192.168.4.1/`. Desde allí, se deben calibrar los parámetros térmicos críticos tras la instalación física del dispositivo para asegurar precisión de detección nula en falsos positivos.

## Procedimiento de Puesta a Punto

1. **Montaje Cenital Base:** Verifica que la cámara mire al piso y que no existan aires acondicionados soplando calor/frío directo sobre la lente.
2. **Acceso UI:** Conéctate con un smartphone a la red Wi-Fi `ThermalCounter` e ingresa al navegador en `192.168.4.1`.

### Parámetros Críticos

A. **Temp. Biológica:** Temperatura umbral por debajo de la cual **todo** se considera piso inerte. Si el ambiente está muy frío (Invierno), puedes bajar esto a `21.0f`. Si estás en verano pesado (30°C ambiente), elévalo a `28.0f`.
B. **Delta Fondo:** Cantidad de grados Centígrados obligatoria por encima del piso (fondo). El algoritmo de media móvil (EMA) aprenderá el calor residual de una estufa si esta no se mueve en 10 minutos. Este valor asegura que un humano transitorio rompa el background dinámico. (Recomendado: `1.5` a `2.5`).
C. **Radio NMS:** Filtro de clústeres térmicos espaciales. Si instalas el sensor en un techo muy bajo (ej. 2.5 metros), un humano se verá "enorme" y el NMS debe ser más alto. Si el techo es muy alto (4.5 metros) las cabezas térmicas miden 2 píxeles, aquí el Radio NMS debe ser mínimo (1 o 2).

### Cierre
Una vez que en el Dashboard observes cruces fluidos bajo "Picos Crudos" (y su flecha vectorial amarilla correspondiente), haz clic en **[ GUARDAR EN FLASH ]** para que los valores dejen de ser volátiles y persistan a cortes de luces (NVS Save).
