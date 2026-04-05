/**
 * @file tracklet_tracker.cpp
 * @brief Implementation of TrackletTracker (A2).
 *
 * See tracklet_tracker.hpp for algorithm documentation.
 */

#include "tracklet_tracker.hpp"
#include <cmath>
#include <cstring>
#include "esp_log.h"

static const char* TAG = "TRACKLET";

// =========================================================================
//  Constructor
// =========================================================================

TrackletTracker::TrackletTracker() : next_id_(1)
{
    memset(tracks_, 0, sizeof(tracks_));
    for (int i = 0; i < ThermalConfig::MAX_TRACKS; i++) {
        tracks_[i].active = false;
    }
}

// =========================================================================
//  Private helpers
// =========================================================================

float TrackletTracker::computeCost(const Tracklet& t, const ThermalPeak& p) const
{
    const float dx = t.pred_x - p.x;
    const float dy = t.pred_y - p.y;
    const float dist = sqrtf(dx * dx + dy * dy);

    // Normalise distance to [0, 1] against the rejection gate
    const float dist_cost = dist / ThermalConfig::TRACK_MAX_DIST;

    // Temperature similarity cost: 5 °C difference → cost = 1.0
    const float temp_diff = fabsf(t.avg_temperature - p.temperature);
    const float temp_cost = temp_diff / 5.0f;

    return dist_cost  * (1.0f - ThermalConfig::TRACK_TEMP_WEIGHT)
         + temp_cost  * ThermalConfig::TRACK_TEMP_WEIGHT;
}

void TrackletTracker::predictAll()
{
    for (int i = 0; i < ThermalConfig::MAX_TRACKS; i++) {
        if (!tracks_[i].active) continue;
        tracks_[i].history.predict(&tracks_[i].pred_x, &tracks_[i].pred_y);
    }
}

// int TrackletTracker::findBestTrack(const ThermalPeak& p, bool* already_matched) const
// {
//     int   best_idx  = -1;
//     float best_cost = 1.0f;  // Rejection threshold: cost >= 1.0 → no match
// 
//     for (int i = 0; i < ThermalConfig::MAX_TRACKS; i++) {
//         if (!tracks_[i].active || already_matched[i]) continue;
//         const float cost = computeCost(tracks_[i], p);
//         if (cost < best_cost) {
//             best_cost = cost;
//             best_idx  = i;
//         }
//     }
//     return best_idx;
// }

Tracklet* TrackletTracker::allocateTrack()
{
    // First pass: look for an empty slot
    for (int i = 0; i < ThermalConfig::MAX_TRACKS; i++) {
        if (!tracks_[i].active) return &tracks_[i];
    }
    // Second pass: recycle the oldest unconfirmed track
    for (int i = 0; i < ThermalConfig::MAX_TRACKS; i++) {
        if (tracks_[i].active && !tracks_[i].isConfirmed()) {
            return &tracks_[i];
        }
    }
    // All slots occupied by confirmed tracks — cannot allocate
    return nullptr;
}

// =========================================================================
//  update() — Main pipeline entry point
// =========================================================================

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

    // Matriz de costos (N×N)
    float cost[HungarianAlgorithm::MAX_N][HungarianAlgorithm::MAX_N];
    
    // Construir matriz de costos
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            if (i < num_active && j < num_valid) {
                float c = computeCost(tracks_[active_idx[i]], peaks[valid_peaks[j]]);
                cost[i][j] = (c >= 1.0f) ? HungarianAlgorithm::INF : c; // Gate de rechazo
            } else {
                cost[i][j] = 0.0f; // Celda virtual (sin penalización)
            }
        }
    }

    // Resolver asignación óptima
    int hungarian_assignment[HungarianAlgorithm::MAX_N];
    HungarianAlgorithm::solve(cost, N, hungarian_assignment);

    // Extraer la asignación válida (rechazar virtuales y celdas INF)
    for (int i = 0; i < num_active; i++) {
        int peak_col = hungarian_assignment[i];
        if (peak_col >= 0 && peak_col < num_valid) {
            if (cost[i][peak_col] < HungarianAlgorithm::INF) {
                assignment[active_idx[i]] = valid_peaks[peak_col];
                peak_assigned[valid_peaks[peak_col]] = true;
            }
        }
    }
}

void TrackletTracker::update(const ThermalPeak* peaks, int numPeaks,
                             uint32_t timestamp)
{
    // --- Phase 1: Advance predictions for all active tracks ---
    predictAll();

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
            // P11-fix: Bucle do-while para garantizar que el nuevo ID no colisiona
            // con ningún track persistente activo, evitando fusiones temporales en FSM.
            do {
                slot->id = next_id_++;
                if (next_id_ == 0) next_id_ = 1;
                bool id_in_use = false;
                for (int k = 0; k < ThermalConfig::MAX_TRACKS; k++) {
                    if (tracks_[k].active && tracks_[k].id == slot->id && &tracks_[k] != slot) {
                        id_in_use = true;
                        break;
                    }
                }
                if (!id_in_use) break;
            } while (true);
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

    // --- Phase 3: Increment missed counter for unmatched active tracks ---
    for (int i = 0; i < ThermalConfig::MAX_TRACKS; i++) {
        if (!tracks_[i].active || matched_tracks[i]) continue;
        tracks_[i].missed++;
        // pred_x/pred_y were already advanced in predictAll()
    }

    // --- Phase 4: Expire stale tracks ---
    removeExpired();
}

// =========================================================================
//  removeExpired
// =========================================================================

void TrackletTracker::removeExpired()
{
    for (int i = 0; i < ThermalConfig::MAX_TRACKS; i++) {
        if (!tracks_[i].active) continue;
        
        // Memoria Proporcional: un track recién nacido no merece 750ms de memoria.
        // Tiempo de tolerancia de muerte (misses permitidos) = min(MAX_MISSED, detecciones_previas)
        int allowed_misses = tracks_[i].confirmed;
        if (allowed_misses > ThermalConfig::TRACK_MAX_MISSED) {
            allowed_misses = ThermalConfig::TRACK_MAX_MISSED;
        }

        if (tracks_[i].missed > allowed_misses) {
            ESP_LOGD(TAG, "Track ID=%u expired (missed=%u, allowed=%u, confirmed=%u)",
                     tracks_[i].id, tracks_[i].missed, allowed_misses, tracks_[i].confirmed);
            tracks_[i].active    = false;
            tracks_[i].confirmed = 0;
        }
    }
}

// =========================================================================
//  fillTrackArray — dense output compatible with MaskGenerator + IPC
// =========================================================================

void TrackletTracker::fillTrackArray(Track* out, int* out_count) const
{
    *out_count = 0;

    for (int i = 0; i < ThermalConfig::MAX_TRACKS; i++) {
        if (!tracks_[i].active || !tracks_[i].isConfirmed()) continue;

        Track& t   = out[*out_count];
        t.id       = tracks_[i].id;
        // Use EMA-smoothed display position to eliminate visual jumps in HUD
        t.x        = tracks_[i].display_x;
        t.y        = tracks_[i].display_y;
        t.active   = true;
        t.state_y  = tracks_[i].zone_state;
        t.age      = tracks_[i].missed;  // Exposes "coasting" frames

        // Velocity: media de las últimas min(count,4) muestras del historial.
        // Más robusta que el diff de 1 frame — elimina jitter del vector en el HUD.
        if (tracks_[i].history.count >= 2) {
            const int samples = (tracks_[i].history.count < 4) ? tracks_[i].history.count : 4;
            const int h       = tracks_[i].history.head;
            const int prev    = (h - samples + TrackHistory::CAPACITY) % TrackHistory::CAPACITY;
            t.v_x = (tracks_[i].history.entries[h].x - tracks_[i].history.entries[prev].x) / (float)samples;
            t.v_y = (tracks_[i].history.entries[h].y - tracks_[i].history.entries[prev].y) / (float)samples;
        } else {
            t.v_x = 0.0f;
            t.v_y = 0.0f;
        }

        (*out_count)++;
    }

    // Sort output by ascending ID for consistent HUD display ordering
    for (int i = 0; i < *out_count - 1; i++) {
        for (int j = i + 1; j < *out_count; j++) {
            if (out[j].id < out[i].id) {
                Track tmp = out[i];
                out[i]   = out[j];
                out[j]   = tmp;
            }
        }
    }
}

