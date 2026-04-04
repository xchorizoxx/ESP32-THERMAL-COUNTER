#include "tracklet_fsm.hpp"
#include "esp_log.h"
#include <cmath>

static const char* TAG = "TRACK_FSM";

TrackletFSM::TrackletFSM() {
    memset(states_, 0, sizeof(states_));
}

TrackletFSM::FsmMemory* TrackletFSM::findState(uint8_t id) {
    for (int i = 0; i < ThermalConfig::MAX_TRACKS; i++) {
        if (states_[i].id == id && states_[i].state != FsmState::UNBORN) {
            return &states_[i];
        }
    }
    return nullptr;
}

TrackletFSM::FsmMemory* TrackletFSM::allocateState(uint8_t id) {
    for (int i = 0; i < ThermalConfig::MAX_TRACKS; i++) {
        if (states_[i].state == FsmState::UNBORN) {
            states_[i].id = id;
            return &states_[i];
        }
    }
    return nullptr;
}

int TrackletFSM::checkSegmentCrossing(
    float prev_x, float prev_y,
    float curr_x, float curr_y,
    float sx1, float sy1,
    float sx2, float sy2)
{
    // Intersección paramétrica de dos segmentos finitos:
    //   Track:   P(t) = prev + t*(curr-prev),   t ∈ [0,1]
    //   Linea:   Q(u) = (sx1,sy1) + u*((sx2,sy2)-(sx1,sy1)),  u ∈ [0,1]
    // Solo cuenta si t ∈ [0,1] (cruce en este frame) Y u ∈ [0,1] (dentro del segmento).

    const float d1x = curr_x - prev_x;
    const float d1y = curr_y - prev_y;
    const float d2x = sx2 - sx1;
    const float d2y = sy2 - sy1;

    const float denom = d1x * d2y - d1y * d2x;
    if (fabsf(denom) < 1e-6f) return 0;   // Paralelos o colineales

    const float t = ((sx1 - prev_x) * d2y - (sy1 - prev_y) * d2x) / denom;
    const float u = ((sx1 - prev_x) * d1y - (sy1 - prev_y) * d1x) / denom;

    if (t < 0.0f || t > 1.0f) return 0;   // El track no cruzo en este frame
    if (u < 0.0f || u > 1.0f) return 0;   // Cruce fuera de los extremos del segmento

    // denom > 0: track cruzo de izquierda a derecha respecto al vector del segmento
    return (denom > 0.0f) ? 1 : -1;
}

void TrackletFSM::update(TrackletTracker& tracker, int& countIn, int& countOut) {
    Tracklet* tracks = const_cast<Tracklet*>(tracker.getTracks());

    // 1) Clean up FSM memory of inactive tracks
    for (int i = 0; i < ThermalConfig::MAX_TRACKS; i++) {
        if (states_[i].state == FsmState::UNBORN) continue;
        
        bool found = false;
        for (int j = 0; j < ThermalConfig::MAX_TRACKS; j++) {
            if (tracks[j].active && tracks[j].id == states_[i].id) {
                found = true;
                break;
            }
        }
        
        if (!found) {
            states_[i].state = FsmState::UNBORN; // Free slot
            states_[i].id = 0;
        }
    }

    // 2) Process each confirmed active track
    for (int i = 0; i < ThermalConfig::MAX_TRACKS; i++) {
        const Tracklet& t = tracks[i];
        if (!t.active || !t.isConfirmed()) continue;
        const float py = t.y();

        
        // Track counting debounce: prevent double-counting when multiple segments are crossed
        bool already_counted_in = false;
        bool already_counted_out = false;
        
        if (ThermalConfig::door_lines.use_segments && ThermalConfig::door_lines.num_lines > 0) {
            // Detección de cruce con lookback dinámico:
            //
            // PROBLEMA RAZ: a 8 Hz efectivos y 3.6m de altura con FOV 110°,
            //   una persona a 1 m/s solo mueve ~0.12 px/frame → con lookback=1 el
            //   vector es demasiado corto para cruzar ninguna línea.
            //
            // SOLUCIÓN: usar las últimas LOOKBACK muestras del historial CRUDO
            //   (no el display_x suavizado con EMA) para construir un vector más largo
            //   y robusto. Cooldown post-cruce evita doble conteo.
            FsmMemory* mem = findState(t.id);
            if (mem == nullptr) {
                mem = allocateState(t.id);
                if (!mem) continue;
            }

            // Decrementar cooldown si está activo
            if (mem->cross_streak > 0) {
                mem->cross_streak--;
                tracks[i].zone_state = 2;
                continue; // Evitar doble conteo mientras dure el cooldown
            }

            // Necesitamos al menos 2 muestras para tener vector
            if (t.history.count >= 2) {
                // Lookback dinámico: máximo 6 frames, o lo disponible
                const int LOOKBACK = (t.history.count >= 6) ? 6 : (int)(t.history.count - 1);
                const int COOLDOWN = LOOKBACK + 2; // Frames de espera post-cruce

                // Posición actual: historial crudo (no display_x EMA-suavizado)
                float curr_x = t.history.entries[t.history.head].x;
                float curr_y = t.history.entries[t.history.head].y;

                // Posición N frames atrás
                int prev_idx = (t.history.head - LOOKBACK + TrackHistory::CAPACITY) % TrackHistory::CAPACITY;
                float prev_x = t.history.entries[prev_idx].x;
                float prev_y = t.history.entries[prev_idx].y;

                for (int li = 0; li < ThermalConfig::door_lines.num_lines; li++) {
                    const CountingSegment& seg = ThermalConfig::door_lines.lines[li];
                    if (!seg.enabled) continue;

                    int cross = checkSegmentCrossing(
                        prev_x, prev_y, curr_x, curr_y,
                        seg.x1, seg.y1, seg.x2, seg.y2
                    );

                    if (cross == 1 && !already_counted_out) {
                        countOut++;
                        already_counted_out = true;
                        mem->cross_streak = (int8_t)COOLDOWN;
                        ESP_LOGI(TAG, "Track ID=%d crossed '%s' -> +1 OUT (lb=%d)", t.id, seg.name, LOOKBACK);
                    } else if (cross == -1 && !already_counted_in) {
                        countIn++;
                        already_counted_in = true;
                        mem->cross_streak = (int8_t)COOLDOWN;
                        ESP_LOGI(TAG, "Track ID=%d crossed '%s' -> +1 IN (lb=%d)", t.id, seg.name, LOOKBACK);
                    }
                }
            }
            tracks[i].zone_state = 2; // Neutral display for segment mode 

        } else {
            // Modo legacy: usar lineEntryY / lineExitY horizontal
            FsmMemory* mem = findState(t.id);
            
            // Allocate FSM for newly confirmed valid tracks
            if (mem == nullptr) {
                mem = allocateState(t.id);
                if (!mem) continue; 
            }

            // FSM Execution Core
            if (mem->state == FsmState::UNBORN) {
                if (py <= ThermalConfig::DEFAULT_LINE_ENTRY_Y) {
                    mem->state = FsmState::TRACKING_IN;
                } else if (py >= ThermalConfig::DEFAULT_LINE_EXIT_Y) {
                    mem->state = FsmState::TRACKING_OUT;
                }
                // Stay UNBORN if in the middle neutral zone
                
            } else if (mem->state == FsmState::TRACKING_IN) {
                if (py >= ThermalConfig::DEFAULT_LINE_EXIT_Y) { // Crossed OUT line
                    countOut++;
                    mem->state = FsmState::TRACKING_OUT;
                    ESP_LOGI(TAG, "Track %d crossed IN->OUT. CntOUT: %d", t.id, countOut);
                }
                
            } else if (mem->state == FsmState::TRACKING_OUT) {
                if (py <= ThermalConfig::DEFAULT_LINE_ENTRY_Y) { // Crossed IN line
                    countIn++;
                    mem->state = FsmState::TRACKING_IN;
                    ESP_LOGI(TAG, "Track %d crossed OUT->IN. CntIN: %d", t.id, countIn);
                }
            }
            
            // 3) Push logical states back into Tracklet for Web UI Highlighting
            // JS Palette: 1=Green(Norte/IN), 2=Amber(Neutral/Unborn), 3=Cyan(Sur/OUT)
            if (mem->state == FsmState::TRACKING_IN) {
                tracks[i].zone_state = 1; 
            } else if (mem->state == FsmState::TRACKING_OUT) {
                tracks[i].zone_state = 3; 
            } else {
                tracks[i].zone_state = 2; 
            }
        }
    }
}
