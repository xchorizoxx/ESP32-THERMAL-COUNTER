#pragma once
/**
 * @file tracklet_fsm.hpp
 * @brief Autonomous O(1) Finite State Machine for Counting.
 *
 * Employs mathematical line boundaries to evaluate tracklet positions
 * and exclusion walls (dead zones) without relying on heavy bitmaps.
 */

#include "tracklet_tracker.hpp"
#include <cstdint>

// Logical state inside the Tracklet
enum class FsmState : uint8_t {
    UNBORN = 0,         ///< Newly detected, searching for valid initial region
    TRACKING_IN = 1,    ///< Valid track, last registered touching the IN region
    TRACKING_OUT = 2    ///< Valid track, last registered touching the OUT region
};

class TrackletFSM {
public:
    TrackletFSM();

    /**
     * @brief Evaluates tracklet positions against configured mathematical lines.
     * Updates IN/OUT accumulators instantly O(1).
     *
     * @param tracker   TrackletTracker instance with valid positions
     * @param countIn   Reference to the IN accumulator
     * @param countOut  Reference to the OUT accumulator
     */
    void update(TrackletTracker& tracker, int& countIn, int& countOut);

private:
    struct FsmMemory {
        uint8_t  id;       // Tracklet ID
        FsmState state;    // Logical FSM state
    };
    
    FsmMemory states_[ThermalConfig::MAX_TRACKS];

    FsmMemory* findState(uint8_t id);
    FsmMemory* allocateState(uint8_t id);

    static int checkSegmentCrossing(
        float prev_x, float prev_y,   // Posición anterior del track
        float curr_x, float curr_y,   // Posición actual del track
        float sx1, float sy1,          // Punto inicio del segmento
        float sx2, float sy2           // Punto fin del segmento
    );
};
