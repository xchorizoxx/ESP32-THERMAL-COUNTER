#include "tracklet_fsm.hpp"
#include "esp_log.h"

static const char* TAG = "TRACK_FSM";

TrackletFSM::TrackletFSM() {
    memset(states_, 0, sizeof(states_));
    // Default Map Initialization (A3 testing baseline):
    // Emulates a Top-Down entry/exit logic by writing values directly
    // into the map. Zone 1 (IN) = top 10 rows. Zone 2 (OUT) = bottom 10 rows.
    // Zone 0 (FREE) = middle. Zone 255 (DEAD) = Leftmost and Rightmost walls.
    for (int y = 0; y < ThermalConfig::MLX_ROWS; y++) {
        for (int x = 0; x < ThermalConfig::MLX_COLS; x++) {
            int idx = y * ThermalConfig::MLX_COLS + x;
            
            if (y < 10) {
                zone_map_[idx] = 1; // ZONE_IN
            } else if (y > 13) {
                zone_map_[idx] = 2; // ZONE_OUT
            } else {
                zone_map_[idx] = 0; // ZONE_FREE (Pasillo)
            }
            
            // Exclusión experimental: un rectángulo central de 18x11 (deja 7 píxeles a cada lado en X)
            // Cualquier ruido/calor que aparezca de la nada dentro de este cuadro es fantasma instantáneo.
            if (y >= 6 && y <= 16 && x >= 7 && x <= 24) {
                zone_map_[idx] = 255; // ZONE_DEAD (Ghost trigger central)
            }
        }
    }
}

void TrackletFSM::setZoneMap(const uint8_t* new_map) {
    memcpy(zone_map_, new_map, ThermalConfig::TOTAL_PIXELS);
    ESP_LOGI(TAG, "Autonomous unified zone map updated (768 bytes).");
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
    // Look for an UNBORN (empty/recycled slot)
    for (int i = 0; i < ThermalConfig::MAX_TRACKS; i++) {
        if (states_[i].state == FsmState::UNBORN) {
            states_[i].id = id;
            return &states_[i];
        }
    }
    return nullptr;
}

void TrackletFSM::update(TrackletTracker& tracker, int& countIn, int& countOut) {
    const Tracklet* tracks = tracker.getTracks();

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

        // O(1) Matrix Lookup
        int px = (int)t.x();
        int py = (int)t.y();
        
        // Force bounds explicitly to prevent arbitrary segfaults
        if (px < 0) px = 0;
        if (px > ThermalConfig::MLX_COLS - 1) px = ThermalConfig::MLX_COLS - 1;
        if (py < 0) py = 0;
        if (py > ThermalConfig::MLX_ROWS - 1) py = ThermalConfig::MLX_ROWS - 1;
        
        uint8_t map_val = zone_map_[py * ThermalConfig::MLX_COLS + px];
        
        FsmMemory* mem = findState(t.id);
        
        // Allocate FSM for newly confirmed tracks
        if (mem == nullptr) {
            mem = allocateState(t.id);
            if (!mem) continue; 
        }
        
        // If the track is a GHOST, force abandonment for telemetry visually but skip maths
        if (mem->state == FsmState::GHOST) {
            const_cast<Tracklet&>(t).zone_state = 3; 
            continue;
        }

        // FSM Execution Core
        if (mem->state == FsmState::UNBORN) {
            if (map_val == 255) { // ZONE_DEAD (Exclusion wall)
                mem->state = FsmState::GHOST;
                ESP_LOGI(TAG, "Track %d spawned in ZONE_DEAD. Ghosted.", t.id);
            } else if (map_val == 1) { // ZONE_IN
                mem->state = FsmState::TRACKING_IN;
            } else if (map_val == 2) { // ZONE_OUT
                mem->state = FsmState::TRACKING_OUT;
            }
            // If ZONE_FREE (0), stay UNBORN until contacting a relevant territory
            
        } else if (mem->state == FsmState::TRACKING_IN) {
            if (map_val == 2) { // Touched OUT directly -> +1 OUT
                countOut++;
                mem->state = FsmState::TRACKING_OUT;
                ESP_LOGI(TAG, "Track %d crossed IN->OUT. CntOUT: %d", t.id, countOut);
            }
            
        } else if (mem->state == FsmState::TRACKING_OUT) {
            if (map_val == 1) { // Touched IN directly -> +1 IN
                countIn++;
                mem->state = FsmState::TRACKING_IN;
                ESP_LOGI(TAG, "Track %d crossed OUT->IN. CntIN: %d", t.id, countIn);
            }
        }
        
        // 3) Push logical states back into Tracklet for UX Web Telemetry rendering
        if (mem->state == FsmState::GHOST) {
            const_cast<Tracklet&>(t).zone_state = 3;
        } else if (mem->state == FsmState::TRACKING_IN) {
            const_cast<Tracklet&>(t).zone_state = 0; // Usually rendered as green
        } else if (mem->state == FsmState::TRACKING_OUT) {
            const_cast<Tracklet&>(t).zone_state = 2; // Usually rendered as blue 
        } else {
            const_cast<Tracklet&>(t).zone_state = 1; // UNBORN or FREE (Gray/Neutral)
        }
    }
}
