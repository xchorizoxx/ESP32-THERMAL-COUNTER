# FASE 4 — Algoritmo Húngaro de Asignación Óptima

## Identidad y alcance

Agente de implementación de algoritmo. Esta fase reemplaza el **Greedy nearest neighbor**
del `TrackletTracker` por el **Algoritmo de Kuhn-Munkres (Húngaro)**, que garantiza
la asignación de mínimo costo global entre detecciones y tracks.

### ¿Por qué es necesario en puertas anchas?

En puertas de 4+ metros con múltiples personas cruzando simultáneamente:
- 2 personas cruzándose generan 2 picos cuyas posiciones se intercambian entre frames
- Greedy asigna el pico 1 al track más cercano SIN considerar si esa asignación
  perjudica a otros tracks → IDs intercambiados → saltos visuales y conteos dobles
- El Húngaro minimiza el **costo total global** garantizando la mejor asignación
  para todos los tracks simultáneamente

**Archivos objetivo (únicos):**
1. `components/thermal_pipeline/include/tracklet_tracker.hpp` — declarar método hungarian
2. `components/thermal_pipeline/src/tracklet_tracker.cpp` — reemplazar `findBestTrack` greedy

---

## Algoritmo Húngaro — Descripción para implementación

El algoritmo de Kuhn-Munkres en O(N³) opera sobre una **matriz de costos N×N**:
- Filas = tracks activos (M tracks)
- Columnas = picos detectados (K picos no suprimidos)
- `cost[i][j]` = costo de asignar el pico j al track i (función `computeCost`)

Si M ≠ K, la matriz se cuadra añadiendo filas/columnas virtuales con costo 0.
Los picos virtuales representan "no hay detección" (track con miss).
Los tracks virtuales representan "nuevo track a inicializar".

**Pasos:**
1. Construir matriz de costos `cost[MAX_TRACKS][MAX_TRACKS]` (máx 15×15 = 225 floats)
2. Celdas que superan el gate (`cost >= 1.0`) → marcar como inválidas con `INF`
3. Reducción de filas: restar el mínimo de cada fila
4. Reducción de columnas: restar el mínimo de cada columna
5. Asignación cero: encontrar asignación que cubre todos los ceros con mínimo número de líneas
6. Si la asignación es completa → extraer asignaciones
7. Si no → ajustar y repetir desde paso 3

Para N ≤ 15 (MAX_TRACKS), esto ejecuta en microsegundos en el ESP32-S3.

---

## Cambio 4-A — Declarar método `hungarianMatch` en `tracklet_tracker.hpp`

### BUSCAR (en la sección `private`):
```
    int findBestTrack(const ThermalPeak& p, bool* already_matched) const;
```

### REEMPLAZAR POR:
```
    // findBestTrack reemplazado por Hungarian para asignacion optima global.
    // Se mantiene para referencia pero ya no se llama.
    // int findBestTrack(const ThermalPeak& p, bool* already_matched) const;

    /**
     * @brief Asignación óptima global (Algoritmo de Kuhn-Munkres / Húngaro).
     *
     * Construye la matriz de costos (tracks × picos) y encuentra la asignación
     * de mínimo costo total. Garantiza que cruces simultáneos de múltiples personas
     * no provoquen intercambio de IDs.
     *
     * @param peaks         Array de picos detectados (incluye suprimidos)
     * @param numPeaks      Número total de picos
     * @param assignment    Output: assignment[i] = índice del pico asignado al track i,
     *                      -1 si el track i no tiene asignación (miss)
     * @param peak_assigned Output: peak_assigned[p] = true si el pico p fue asignado
     */
    void hungarianMatch(const ThermalPeak* peaks, int numPeaks,
                        int* assignment, bool* peak_assigned) const;
```

---

## Cambio 4-B — Implementar `hungarianMatch` y reemplazar el dispatch en `tracklet_tracker.cpp`

### Añadir constante de costo inválido al inicio del archivo (después de los includes):

### BUSCAR:
```
static const char* TAG = "TRACKLET";
```

### REEMPLAZAR POR:
```
static const char* TAG = "TRACKLET";

static constexpr float HUNGARIAN_INF = 1e9f; // Marca de celda inválida/prohibida
```

---

### Implementar `hungarianMatch` (añadir antes de `TrackletTracker::update`):

### BUSCAR:
```
void TrackletTracker::update(const ThermalPeak* peaks, int numPeaks,
                             uint32_t timestamp)
```

### REEMPLAZAR POR:
```
void TrackletTracker::hungarianMatch(const ThermalPeak* peaks, int numPeaks,
                                     int* assignment, bool* peak_assigned) const
{
    // Recopilar tracks activos
    int active_idx[ThermalConfig::MAX_TRACKS];
    int num_active = 0;
    for (int i = 0; i < ThermalConfig::MAX_TRACKS; i++) {
        if (tracks_[i].active) active_idx[num_active++] = i;
    }

    // Recopilar picos válidos (no suprimidos)
    int valid_peaks[ThermalConfig::MAX_TRACKS];
    int num_valid = 0;
    for (int p = 0; p < numPeaks && num_valid < ThermalConfig::MAX_TRACKS; p++) {
        if (!peaks[p].suppressed) valid_peaks[num_valid++] = p;
    }

    // Inicializar outputs
    for (int i = 0; i < ThermalConfig::MAX_TRACKS; i++) assignment[i] = -1;
    for (int p = 0; p < numPeaks; p++) peak_assigned[p] = false;

    if (num_active == 0 || num_valid == 0) return;

    // Dimensión de la matriz cuadrada (máx de los dos conjuntos)
    const int N = (num_active > num_valid) ? num_active : num_valid;

    // Matriz de costos: cost[track_row][peak_col]
    // Usar array estático para evitar heap allocation en el loop
    static float cost[ThermalConfig::MAX_TRACKS][ThermalConfig::MAX_TRACKS];
    static float u[ThermalConfig::MAX_TRACKS + 1]; // potencial de fila
    static float v[ThermalConfig::MAX_TRACKS + 1]; // potencial de columna
    static int   p_idx[ThermalConfig::MAX_TRACKS + 1]; // track asignado a cada columna
    static int   way[ThermalConfig::MAX_TRACKS + 1];

    // Construir matriz de costos (N×N)
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            if (i < num_active && j < num_valid) {
                float c = computeCost(tracks_[active_idx[i]], peaks[valid_peaks[j]]);
                cost[i][j] = (c >= 1.0f) ? HUNGARIAN_INF : c; // Gate de rechazo
            } else {
                cost[i][j] = 0.0f; // Celda virtual (sin penalización)
            }
        }
    }

    // Algoritmo Húngaro — implementación Jonker-Volgenant simplificado
    // u[i] = potencial de fila i (1-indexed), v[j] = potencial de columna j
    for (int i = 0; i <= N; i++) { u[i] = 0.0f; v[i] = 0.0f; p_idx[i] = 0; }

    for (int i = 1; i <= N; i++) {
        p_idx[0] = i;
        int j0 = 0;
        static float minv[ThermalConfig::MAX_TRACKS + 1];
        static bool  used[ThermalConfig::MAX_TRACKS + 1];
        for (int j = 0; j <= N; j++) { minv[j] = HUNGARIAN_INF; used[j] = false; }

        do {
            used[j0] = true;
            int i0 = p_idx[j0];
            float delta = HUNGARIAN_INF;
            int j1 = -1;

            for (int j = 1; j <= N; j++) {
                if (!used[j]) {
                    float cur = cost[i0-1][j-1] - u[i0] - v[j];
                    if (cur < minv[j]) {
                        minv[j] = cur;
                        way[j] = j0;
                    }
                    if (minv[j] < delta) {
                        delta = minv[j];
                        j1 = j;
                    }
                }
            }

            for (int j = 0; j <= N; j++) {
                if (used[j]) {
                    u[p_idx[j]] += delta;
                    v[j] -= delta;
                } else {
                    minv[j] -= delta;
                }
            }
            j0 = j1;
        } while (p_idx[j0] != 0);

        do {
            p_idx[j0] = p_idx[way[j0]];
            j0 = way[j0];
        } while (j0);
    }

    // Extraer la asignación válida (rechazar virtuales y celdas INF)
    for (int j = 1; j <= N; j++) {
        int track_row = p_idx[j] - 1; // 0-indexed
        int peak_col  = j - 1;        // 0-indexed
        if (track_row < num_active && peak_col < num_valid) {
            if (cost[track_row][peak_col] < HUNGARIAN_INF) {
                assignment[active_idx[track_row]] = valid_peaks[peak_col];
                peak_assigned[valid_peaks[peak_col]] = true;
            }
        }
    }
}

void TrackletTracker::update(const ThermalPeak* peaks, int numPeaks,
                             uint32_t timestamp)
```

---

### Reemplazar el dispatch de matching en `update()` (Greedy → Húngaro):

### BUSCAR (el bloque completo de la Fase 2 de update):
```
    // --- Phase 2: Greedy matching — each non-suppressed peak → best track ---
    bool matched_tracks[ThermalConfig::MAX_TRACKS] = {false};

    for (int p = 0; p < numPeaks; p++) {
        if (peaks[p].suppressed) continue;

        const int best = findBestTrack(peaks[p], matched_tracks);

        if (best >= 0) {
            // Update existing track
            Tracklet& t = tracks_[best];
            t.history.push((float)peaks[p].x, (float)peaks[p].y, timestamp);
            t.pred_x         = (float)peaks[p].x;
            t.pred_y         = (float)peaks[p].y;
            // EMA for display position — smooths visual jumps without affecting prediction
            const float alpha = ThermalConfig::TRACK_DISPLAY_SMOOTH;
            t.display_x = alpha * (float)peaks[p].x + (1.0f - alpha) * t.display_x;
            t.display_y = alpha * (float)peaks[p].y + (1.0f - alpha) * t.display_y;
            // Fast EMA for temperature (weight 0.2 on new measurement)
            t.avg_temperature = t.avg_temperature * 0.8f + peaks[p].temperature * 0.2f;
            t.confirmed       = (t.confirmed < 255) ? t.confirmed + 1 : 255;
            t.missed          = 0;
            matched_tracks[best] = true;

        } else {
            // No match → spawn a new track
            Tracklet* slot = allocateTrack();
            if (slot) {
                memset(slot, 0, sizeof(Tracklet));
                slot->active          = true;
                slot->id              = next_id_++;
                if (next_id_ == 0) next_id_ = 1;  // Skip reserved ID=0
                slot->confirmed       = 1;
                slot->missed          = 0;
                slot->avg_temperature = peaks[p].temperature;
                slot->pred_x          = (float)peaks[p].x;
                slot->pred_y          = (float)peaks[p].y;
                // Init display position to raw position (no history yet to smooth from)
                slot->display_x       = (float)peaks[p].x;
                slot->display_y       = (float)peaks[p].y;
                slot->zone_state      = 1;  // Neutral by default — FSM corrects in A3
                slot->history.push((float)peaks[p].x, (float)peaks[p].y, timestamp);
                ESP_LOGD(TAG, "New track ID=%u at (%.1f, %.1f) temp=%.1f°C",
                         slot->id, slot->pred_x, slot->pred_y, slot->avg_temperature);
            } else {
                ESP_LOGW(TAG, "Track pool full — peak at (%u,%u) dropped",
                         peaks[p].x, peaks[p].y);
            }
        }
    }
```

### REEMPLAZAR POR:
```
    // --- Phase 2: Hungarian optimal assignment — minimizes total cost globally ---
    static int  assignment[ThermalConfig::MAX_TRACKS];   // assignment[track_i] = peak_idx
    static bool peak_assigned[ThermalConfig::MAX_TRACKS]; // peak_assigned[peak_p] = true if used
    bool matched_tracks[ThermalConfig::MAX_TRACKS] = {false};

    hungarianMatch(peaks, numPeaks, assignment, peak_assigned);

    // Apply assignments from Hungarian result
    for (int i = 0; i < ThermalConfig::MAX_TRACKS; i++) {
        if (!tracks_[i].active) continue;
        const int pi = assignment[i];
        if (pi >= 0) {
            // Track matched to peak pi
            Tracklet& t = tracks_[i];
            t.history.push(peaks[pi].x, peaks[pi].y, timestamp);
            t.pred_x          = peaks[pi].x;
            t.pred_y          = peaks[pi].y;
            const float alpha = ThermalConfig::TRACK_DISPLAY_SMOOTH;
            t.display_x = alpha * peaks[pi].x + (1.0f - alpha) * t.display_x;
            t.display_y = alpha * peaks[pi].y + (1.0f - alpha) * t.display_y;
            t.avg_temperature = t.avg_temperature * 0.8f + peaks[pi].temperature * 0.2f;
            t.confirmed       = (t.confirmed < 255) ? t.confirmed + 1 : 255;
            t.missed          = 0;
            matched_tracks[i] = true;
        }
    }

    // Spawn new tracks for unmatched peaks
    for (int p = 0; p < numPeaks; p++) {
        if (peaks[p].suppressed || peak_assigned[p]) continue;
        Tracklet* slot = allocateTrack();
        if (slot) {
            memset(slot, 0, sizeof(Tracklet));
            slot->active          = true;
            slot->id              = next_id_++;
            if (next_id_ == 0) next_id_ = 1;
            slot->confirmed       = 1;
            slot->missed          = 0;
            slot->avg_temperature = peaks[p].temperature;
            slot->pred_x          = peaks[p].x;
            slot->pred_y          = peaks[p].y;
            slot->display_x       = peaks[p].x;
            slot->display_y       = peaks[p].y;
            slot->zone_state      = 2; // Neutral — FSM corrige en A3
            slot->history.push(peaks[p].x, peaks[p].y, timestamp);
            ESP_LOGD(TAG, "New track ID=%u at (%.1f, %.1f) temp=%.1f°C",
                     slot->id, slot->pred_x, slot->pred_y, slot->avg_temperature);
        } else {
            ESP_LOGW(TAG, "Track pool full — peak at (%.1f,%.1f) dropped",
                     peaks[p].x, peaks[p].y);
        }
    }
```

---

## Verificación tras Fase 4

1. Compilar sin errores. El uso de arreglos `static` dentro de `hungarianMatch` y `update`
   evita allocs en el heap (≈ 2 KB en stack estático).
2. Con 2-3 tracks activos visible en el HUD, cruzar rápidamente con dos personas:
   los IDs NO deben intercambiarse entre frames.
3. Revisar log serial: los `New track ID=X` deben aparecer solo para picos genuinamente
   nuevos, no por reasignaciones erróneas.
4. Notificar al usuario para compilar y flashear antes de pasar a Fase 5.
