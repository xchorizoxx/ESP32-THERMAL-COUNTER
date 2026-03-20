# Detección de Blobs y Tracking Predictivo

Una vez que tenemos la imagen térmica _sustraída del fondo_ (solo vemos lo que está más caliente que el suelo), necesitamos convertir esas "manchas de calor" (blobs) en entidades contables.

## 1. Topología (Peak Detection)

No buscamos contornos complejos, aplicamos un método matemático llamado detección de picos 2D. 
Un píxel se considera un "Pico" si:
1. Su temperatura neta (vs el fondo) es > `DELTA_T_FONDO`.
2. Su temperatura absoluta es > `TEMP_BIOLOGICO_MIN` (garantiza que es un humano o animal, y no un cable tibio).
3. Es estrictamente más caliente que los 8 píxeles inmediatamente a su alrededor.

## 🛡️ Supresión de No-Máximos (NMS)

Para evitar que una persona sea detectada múltiples veces debido a su tamaño, aplicamos un filtro de vecindad circular.

### Algoritmo NMS
1. Se ordenan todos los picos detectados por temperatura de mayor a menor.
2. Para el pico más caliente, se eliminan todos los demás picos que se encuentren dentro de un radio $R$ (parámetro `nms_radius`).
3. Se repite para el siguiente pico superviviente más caliente.

Este proceso garantiza que solo el **centro de masa** térmico sea procesado por el tracker.

---

## 📈 Tracking con Filtro Alpha-Beta

El sistema utiliza un Filtro Alpha-Beta para predecir la posición de los objetos en el siguiente frame, compensando el ruido de medición.

### Ecuaciones de Estado
Para cada componente $(x, y)$:

1. **Predicción**:
   $$\hat{x}_{k} = x_{k-1} + v_{k-1} \cdot \Delta t$$
2. **Actualización de Posición**:
   $$x_{k} = \hat{x}_{k} + \alpha \cdot (z_{k} - \hat{x}_{k})$$
3. **Actualización de Velocidad**:
   $$v_{k} = v_{k-1} + \frac{\beta}{\Delta t} \cdot (z_{k} - \hat{x}_{k})$$

Donde $z_k$ es la medición actual y $\alpha, \beta$ son las ganancias del filtro. Esto permite que el sistema "entienda" la inercia del movimiento humano, ignorando saltos erráticos de un solo frame.

---

Tenemos una lista de "Picos Crudos" (posición X,Y) en el *frame actual*. ¿Cómo sabemos quién es quién del *frame anterior*?

Usamos un rastreador Alpha-Beta, que es una versión simplificada del famoso Filtro de Kalman.
El rastreador le asigna a cada persona no solo una Posición, sino una **Velocidad**.

1. **Predicción:** Usando la velocidad anterior de la Persona 1, se calcula dónde debería estar en este frame.
2. **Emparejamiento:** Se busca el "Pico Crudo" más cercano a esa posición predicha (siempre que la distancia no exceda `MAX_MATCH_DIST_SQ`).
3. **Actualización:**
   * Si se encuentra pareja, se calcula el error entre lo predicho y lo medido. 
   * Se corrige la posición usando `ALPHA_TRK` (qué tanto confío en el sensor frente a la física).
   * Se corrige la velocidad usando `BETA_TRK`.
   * **Protección de Multitudes (V2.5):** Se implementó una verificación de "Track Matched" que impide que dos personas muy juntas "roben" el mismo track. Esto mantiene identidades únicas incluso en cruces críticos.

## 4. Vectores de Velocidad (V2)

A partir de la versión 2, el sistema calcula un vector director `(vx, vy)` para cada track. 
- **Cálculo:** Se basa en la diferencia de posición corregida por el filtro Alpha-Beta entre frames sucesivos.
- **Visualización:** En el HUD Táctico, este vector se dibuja como una flecha amarilla delgada. La longitud de la flecha es proporcional a la rapidez, permitiendo predecir visualmente el sentido de paso.

## 5. Lógica de Conteo e Inferencia de Intención

El conteo ocurre estrictamente comparando la posición de la persona entre el frame `T-1` y el frame `T`, pero ahora con **Inferencia de Intención**:

1. **Cruce Estándar:** Si el track cruza de la zona superior a la inferior (o viceversa) habiendo nacido fuera de la zona neutra.
2. **Spawn en Zona Neutra (V2.5):** Si una persona "aparece" (spawn) directamente entre las líneas de conteo (porque el sensor la perdió un segundo), el sistema no ignora el cruce. En su lugar, evalúa su **Vector de Velocidad Y**:
   - Si cruza la línea inferior con `v_y > 0.05`, se infiere intención de **Entrada**.
   - Si cruza la línea superior con `v_y < -0.05`, se infiere intención de **Salida**.

Esta lógica bidireccional garantiza que casi ninguna persona se escape sin ser contada, incluso ante oclusiones temporales.
