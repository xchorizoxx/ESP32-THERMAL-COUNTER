#include "tracklet_fsm.hpp"
#include "esp_log.h"

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
    float sdx = sx2 - sx1;
    float sdy = sy2 - sy1;

    float side_prev = sdx * (prev_y - sy1) - sdy * (prev_x - sx1);
    float side_curr = sdx * (curr_y - sy1) - sdy * (curr_x - sx1);

    if (side_prev > 0.0f && side_curr <= 0.0f) return  1;  // Izq->Der
    if (side_prev < 0.0f && side_curr >= 0.0f) return -1;  // Der->Izq

    float min_x = sx1 < sx2 ? sx1 : sx2;
    float max_x = sx1 > sx2 ? sx1 : sx2;
    float min_y = sy1 < sy2 ? sy1 : sy2;
    float max_y = sy1 > sy2 ? sy1 : sy2;

    const float MARGIN = 0.5f;
    if (curr_x < min_x - MARGIN || curr_x > max_x + MARGIN) return 0;
    if (curr_y < min_y - MARGIN || curr_y > max_y + MARGIN) return 0;

    return 0;
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

        int px = (int)t.x();
        int py = (int)t.y();
        
        // Exclusión Vertical (Zonas Muertas)
        // Tracks outside the configured vertical bounds are ignored completely.
        if (px < ThermalConfig::DEFAULT_DEAD_ZONE_LEFT || px > ThermalConfig::DEFAULT_DEAD_ZONE_RIGHT) {
            tracks[i].zone_state = 2; // Render as Amber/Neutral
            continue; 
        }
        
        // Track counting debounce: prevent double-counting when multiple segments are crossed
        bool already_counted_in = false;
        bool already_counted_out = false;
        
        if (ThermalConfig::door_lines.use_segments && ThermalConfig::door_lines.num_lines > 0) {
            // Nuevo modo: Segmentos dibujables
            if (t.history.count >= 2) {
                int prev_idx = (t.history.head - 1 + TrackHistory::CAPACITY) % TrackHistory::CAPACITY;
                float prev_x = t.history.entries[prev_idx].x;
                float prev_y = t.history.entries[prev_idx].y;

                for (int li = 0; li < ThermalConfig::door_lines.num_lines; li++) {
                    const CountingSegment& seg = ThermalConfig::door_lines.lines[li];
                    if (!seg.enabled) continue;

                    int cross = checkSegmentCrossing(
                        prev_x, prev_y, t.x(), t.y(),
                        seg.x1, seg.y1, seg.x2, seg.y2
                    );

                    if (cross == 1 && !already_counted_out) {
                        countOut++;
                        already_counted_out = true;
                        ESP_LOGI(TAG, "Track ID=%d crossed line '%s' -> +1 OUT", t.id, seg.name);
                    } else if (cross == -1 && !already_counted_in) {
                        countIn++;
                        already_counted_in = true;
                        ESP_LOGI(TAG, "Track ID=%d crossed line '%s' -> +1 IN", t.id, seg.name);
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
