#pragma once
/**
 * @file tracklet_fsm.hpp
 * @brief Autonomous Grid-based Finite State Machine for Counting.
 *
 * Employs an O(1) Unified Bitmap (Region of Interest Map) to evaluate
 * tracklet positions against custom counting lines, curves, and exclusion zones.
 * Validates counting cleanly regardless of compass orientation.
 */

#include "tracklet_tracker.hpp"
#include <cstdint>

// Logical state inside the Tracklet
enum class FsmState : uint8_t {
    UNBORN = 0,         ///< Newly detected, searching for valid initial region
    GHOST  = 1,         ///< Born in ZONE_DEAD (Exclusion). Ignored forever.
    TRACKING_IN = 2,    ///< Valid track, last registered touching the IN region
    TRACKING_OUT = 3    ///< Valid track, last registered touching the OUT region
};

class TrackletFSM {
public:
    TrackletFSM();

    /**
     * @brief Evaluates tracklet positions against the autonomous zone map.
     * Updates IN/OUT accumulators instantly O(1).
     *
     * @param tracker   TrackletTracker instance with valid positions
     * @param countIn   Reference to the IN accumulator
     * @param countOut  Reference to the OUT accumulator
     */
    void update(TrackletTracker& tracker, int& countIn, int& countOut);

    /**
     * @brief Injects a new autonomous counting zone map (768 bytes: 32x24).
     * Used later when receiving UI configurations.
     */
    void setZoneMap(const uint8_t* new_map);

    /**
     * @brief Direct pointer to the active 768-byte Map buffer.
     */
    const uint8_t* getZoneMap() const { return zone_map_; }

private:
    struct FsmMemory {
        uint8_t  id;       // Tracklet ID
        FsmState state;    // Logical FSM state
    };
    
    FsmMemory states_[ThermalConfig::MAX_TRACKS];
    uint8_t zone_map_[ThermalConfig::TOTAL_PIXELS];

    FsmMemory* findState(uint8_t id);
    FsmMemory* allocateState(uint8_t id);
};
