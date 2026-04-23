#include "tracklet_fsm.hpp"
#include "esp_log.h"
#include <cmath>
#include "esp_timer.h"
#include "freertos/portmacro.h"  // portENTER_CRITICAL / portEXIT_CRITICAL (P02-fix)

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

int TrackletFSM::update(TrackletTracker& tracker, int& countIn, int& countOut,
                        CrossingEvent* outEvents, int maxEvents)
{
    int eventIdx = 0;
    auto addEvent = [&](const Tracklet& t, bool isIn) {
        if (eventIdx < maxEvents) {
            outEvents[eventIdx].id = t.id;
            outEvents[eventIdx].is_in = isIn;
            outEvents[eventIdx].count_in = (int16_t)countIn;
            outEvents[eventIdx].count_out = (int16_t)countOut;
            outEvents[eventIdx].temperature = t.avg_temperature;
            outEvents[eventIdx].timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
            eventIdx++;
        }
    };

    // P02-fix: Snapshot atómico de door_lines para evitar torn-reads durante
    // escrituras concurrentes del HTTP Server (Core 0) en esta configuración.
    ThermalConfig::DoorLineConfig dl_snap;
    portENTER_CRITICAL(&ThermalConfig::door_lines_mux);
    dl_snap = ThermalConfig::door_lines;
    portEXIT_CRITICAL(&ThermalConfig::door_lines_mux);

    // P03-fix: Puntero const — no usamos const_cast; escrituras van via setZoneState()
    const Tracklet* all = tracker.getTracks();

    // 1) Clean up FSM memory of inactive tracks
    for (int i = 0; i < ThermalConfig::MAX_TRACKS; i++) {
        if (states_[i].state == FsmState::UNBORN) continue;
        
        bool found = false;
        for (int j = 0; j < ThermalConfig::MAX_TRACKS; j++) {
            if (all[j].active && all[j].id == states_[i].id) {
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
        const Tracklet& t = all[i];
        if (!t.active || !t.isConfirmed()) continue;
        const float py = t.y();

        
        // Track counting debounce: prevent double-counting when multiple segments are crossed
        bool already_counted_in = false;
        bool already_counted_out = false;
        
        if (dl_snap.use_segments && dl_snap.num_lines > 0) {
            // Detección de cruce con lookback dinámico:
            FsmMemory* mem = findState(t.id);
            if (mem == nullptr) {
                mem = allocateState(t.id);
                if (!mem) continue;
            }

            // Decrementar cooldown si está activo
            if (mem->cross_streak > 0) {
                mem->cross_streak--;
                tracker.setZoneState(t.id, 2);  // P03-fix
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

                for (int li = 0; li < dl_snap.num_lines; li++) {
                    const CountingSegment& seg = dl_snap.lines[li];
                    if (!seg.enabled) continue;

                    int cross = checkSegmentCrossing(
                        prev_x, prev_y, curr_x, curr_y,
                        seg.x1, seg.y1, seg.x2, seg.y2
                    );

                    if (cross == 1 && !already_counted_out) {
                        countOut++;
                        already_counted_out = true;
                        mem->cross_streak = (int8_t)COOLDOWN;
                        addEvent(t, false); // OUT event
                        ESP_LOGI(TAG, "Track ID=%d crossed '%s' -> +1 OUT (temp=%.1f)", t.id, seg.name, t.avg_temperature);
                    } else if (cross == -1 && !already_counted_in) {
                        countIn++;
                        already_counted_in = true;
                        mem->cross_streak = (int8_t)COOLDOWN;
                        addEvent(t, true); // IN event
                        ESP_LOGI(TAG, "Track ID=%d crossed '%s' -> +1 IN (temp=%.1f)", t.id, seg.name, t.avg_temperature);
                    }
                }
            }
            tracker.setZoneState(t.id, 2); // P03-fix: Neutral display for segment mode

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
                
            } else if (mem->state == FsmState::TRACKING_IN) {
                if (py >= ThermalConfig::DEFAULT_LINE_EXIT_Y) { // Crossed OUT line
                    countOut++;
                    mem->state = FsmState::TRACKING_OUT;
                    addEvent(t, false); // OUT event
                    ESP_LOGI(TAG, "Track %d crossed IN->OUT. Temp: %.1f", t.id, t.avg_temperature);
                }
                
            } else if (mem->state == FsmState::TRACKING_OUT) {
                if (py <= ThermalConfig::DEFAULT_LINE_ENTRY_Y) { // Crossed IN line
                    countIn++;
                    mem->state = FsmState::TRACKING_IN;
                    addEvent(t, true); // IN event
                    ESP_LOGI(TAG, "Track %d crossed OUT->IN. Temp: %.1f", t.id, t.avg_temperature);
                }
            }
            
            // 3) Push logical states back into Tracklet for Web UI Highlighting
            if (mem->state == FsmState::TRACKING_IN) {
                tracker.setZoneState(t.id, 1);  // P03-fix
            } else if (mem->state == FsmState::TRACKING_OUT) {
                tracker.setZoneState(t.id, 3);  // P03-fix
            } else {
                tracker.setZoneState(t.id, 2);  // P03-fix
            }
        }
    }
    return eventIdx;
}
