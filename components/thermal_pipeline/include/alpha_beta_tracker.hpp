#pragma once
/**
 * @file alpha_beta_tracker.hpp
 * @brief Step 4 — Tracking with Alpha-Beta Filter + Counting Logic.
 *
 * Tracks people between frames using an Alpha-Beta predictive filter
 * and counts entries/exits with a hysteresis state machine.
 */

#include "thermal_types.hpp"

class AlphaBetaTracker {
public:
    AlphaBetaTracker();

    /**
     * @brief Updates tracks with current frame peaks and counts crossings.
     * @param peaks       Peaks array of the frame (including suppressed)
     * @param numPeaks    Total peaks in the array
     * @param alpha       Filter position gain
     * @param beta        Filter velocity gain
     * @param maxDistSq   Maximum distance² to match peak with track
     * @param maxAge      Maximum frames without update before removing track
     * @param lineEntryY  Upper virtual line (entry)
     * @param lineExitY   Lower virtual line (exit)
     * @param countIn     Entry counter (cumulative, in-out)
     * @param countOut    Exit counter (cumulative, in-out)
     */
    void update(const ThermalPeak* peaks, int numPeaks,
                float alpha, float beta,
                int maxDistSq, int maxAge,
                int lineEntryY, int lineExitY,
                int& countIn, int& countOut);

    /**
     * @brief Read access to the tracks array for the map.
     */
    const Track* getTracks() const { return tracks_; }

    /**
     * @brief Maximum number of tracks (matches MAX_TRACKS).
     */
    int getMaxTracks() const;

    /**
     * @brief Returns the number of active tracks.
     */
    int getActiveCount() const;

private:
    Track   tracks_[15]; // MAX_TRACKS hardcoded to avoid circular dependency
    uint8_t nextId_;

    /**
     * @brief Searches for or creates a free track to assign a new peak.
     * @return Pointer to track, or nullptr if no space.
     */
    Track* findFreeTrack();

    /**
     * @brief Evaluates the hysteresis state machine for counting.
     * @param track      Track to evaluate
     * @param lineEntryY Upper virtual line
     * @param lineExitY  Lower virtual line
     * @param countIn    In counter
     * @param countOut   Out counter
     */
    void evaluateCountingLogic(Track& track,
                               int lineEntryY, int lineExitY,
                               int& countIn, int& countOut);
};
