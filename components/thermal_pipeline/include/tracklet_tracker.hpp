#pragma once
/**
 * @file tracklet_tracker.hpp
 * @brief Step 4-A2 — Tracklet-based person tracker with position history.
 *
 * Replaces AlphaBetaTracker. Key improvements:
 *   - 20-frame position history per track (circular buffer)
 *   - Linear velocity prediction before matching
 *   - Composite match cost: Euclidean distance + temperature similarity
 *   - Track confirmation gate: track only visible after TRACK_CONFIRM_FRAMES hits
 *   - Graceful expiry: track removed after TRACK_MAX_MISSED consecutive misses
 *
 * RESPONSIBILITIES:
 *   This class manages identities and positions ONLY.
 *   Counting logic lives in TrackletFSM (Etapa A3).
 *
 * Memory: ~22 KB for 15 tracks with 20-entry history each.
 */

#include "thermal_types.hpp"
#include "thermal_config.hpp"
#include "hungarian_algorithm.hpp"
#include <cstring>
#include <cstdint>

// =========================================================================
//  TrackHistory — Circular position buffer (last N frames)
// =========================================================================

/**
 * @brief Circular buffer of positional observations for one track.
 *
 * Stores up to CAPACITY (x, y, tick) entries. Enables linear velocity
 * prediction without a full Kalman filter.
 */
struct TrackHistory {
    static constexpr int CAPACITY = 20;

    struct Entry {
        float    x;
        float    y;
        uint32_t tick;  ///< xTaskGetTickCount() at observation time
    };

    Entry entries[CAPACITY];
    int   count;  ///< Valid entries [0..CAPACITY]
    int   head;   ///< Index of the most recent entry

    TrackHistory() : count(0), head(0)
    {
        memset(entries, 0, sizeof(entries));
    }

    /** @brief Push a new observation. Overwrites oldest entry when full. */
    void push(float x, float y, uint32_t tick)
    {
        head = (head + 1) % CAPACITY;
        entries[head] = {x, y, tick};
        if (count < CAPACITY) count++;
    }

    /**
     * @brief Linear prediction for the next frame.
     *
     * Uses mean velocity over the last min(count, 4) frames.
     * Falls back to the latest position when count < 2.
     */
    void predict(float* out_x, float* out_y) const
    {
        if (count < 2) {
            *out_x = entries[head].x;
            *out_y = entries[head].y;
            return;
        }
        const int samples  = (count < 4) ? count : 4;
        const int prev_idx = (head - samples + CAPACITY) % CAPACITY;
        const float dx = entries[head].x - entries[prev_idx].x;
        const float dy = entries[head].y - entries[prev_idx].y;
        *out_x = entries[head].x + dx / (float)samples;
        *out_y = entries[head].y + dy / (float)samples;
    }

    float latestX() const { return entries[head].x; }
    float latestY() const { return entries[head].y; }
};

// =========================================================================
//  Tracklet — Single tracked person
// =========================================================================

/**
 * @brief Internal representation of one tracked person.
 *
 * `zone_state` is intentionally kept here so that TrackletFSM (A3)
 * can operate directly on Tracklet objects without a separate lookup.
 */
struct Tracklet {
    uint8_t      id;               ///< Unique ID (1-255, cycles avoiding 0)
    bool         active;           ///< False = slot is free
    uint8_t      confirmed;        ///< Consecutive hit count (saturates at 255)
    uint8_t      missed;           ///< Consecutive miss count
    float        avg_temperature;  ///< EMA of observed temperature (for matching)
    float        pred_x;           ///< Predicted X for the current frame
    float        pred_y;           ///< Predicted Y for the current frame
    // EMA-smoothed positions for HUD display — decoupled from raw matching positions.
    // Eliminates per-frame pixel jumps without affecting prediction accuracy.
    // Alpha 0.4: weights recent observation ~60% heavier than history.
    float        display_x;        ///< Smoothed X for HUD rendering
    float        display_y;        ///< Smoothed Y for HUD rendering
    uint8_t      zone_state;       ///< HUD: 1=legacy IN / spawn default, 2=neutral or segment mode, 3=legacy OUT
    float        peak_temp;        ///< Current peak temperature in °C (W4)
    TrackHistory history;

    /** @brief Latest confirmed X position (sub-pixel). */
    float x() const { return history.latestX(); }
    /** @brief Latest confirmed Y position (sub-pixel). */
    float y() const { return history.latestY(); }

    /** @brief True once the track has accumulated enough detections. */
    bool isConfirmed() const
    {
        return confirmed >= ThermalConfig::TRACK_CONFIRM_FRAMES;
    }
};

// =========================================================================
//  TrackletTracker
// =========================================================================

/**
 * @brief Manages a pool of Tracklets. Called once per pipeline frame.
 *
 * update() phases:
 *   1. Predict — advance pred_x/pred_y using history
 *   2. Match   — Hungarian optimal assignment (Kuhn-Munkres, O(N³))
 *   3. Miss    — increment missed counter for unmatched tracks
 *   4. Expire  — deactivate tracks that exceeded TRACK_MAX_MISSED
 *
 * After update(), call fillTrackArray() to obtain a dense Track[] array
 * compatible with MaskGenerator and the IPC telemetry packet.
 */
class TrackletTracker {
public:
    TrackletTracker();

    /**
     * @brief Processes one frame of detections.
     * @param peaks     Peak array from NmsSuppressor (includes suppressed peaks)
     * @param numPeaks  Total entries in peaks[]
     * @param timestamp xTaskGetTickCount() for this frame
     */
    void update(const ThermalPeak* peaks, int numPeaks, uint32_t timestamp);

    /**
     * @brief Fills a dense Track[] array with confirmed, active tracklets.
     *
     * Produces a format compatible with MaskGenerator and TelemetryPayload.
     * Velocity is computed from the last two history entries.
     *
     * @param out       Output array [ThermalConfig::MAX_TRACKS]
     * @param out_count Number of entries written
     */
    void fillTrackArray(Track* out, int* out_count) const;
    const Tracklet* getTracks()         const { return tracks_; }
    int             getMaxTracks()      const { return ThermalConfig::MAX_TRACKS; }

    /** @brief Clears all tracks and resets ID counter. */
    void reset();

    /**
     * @brief P03-fix: Establece el zone_state de un track por ID.
     *
     * Permite que TrackletFSM modifique zone_state sin necesitar const_cast.
     * Reemplaza el acceso directo `tracks[i].zone_state = X` desde la FSM.
     *
     * @param id         Track ID to modify
     * @param zone_state New zone state (1=IN, 2=neutral, 3=OUT)
     */
    void setZoneState(uint8_t id, uint8_t zone_state)
    {
        for (int i = 0; i < ThermalConfig::MAX_TRACKS; i++) {
            if (tracks_[i].active && tracks_[i].id == id) {
                tracks_[i].zone_state = zone_state;
                return;
            }
        }
    }

private:
    Tracklet tracks_[ThermalConfig::MAX_TRACKS];
    uint8_t  next_id_;  ///< Rolling ID counter (1..255)

    // --- Matching helpers ---
    /**
     * @brief Composite cost between a track prediction and an observed peak.
     *
     * cost = dist_cost * (1 - TRACK_TEMP_WEIGHT) + temp_cost * TRACK_TEMP_WEIGHT
     * Returns > 1.0 if distance exceeds TRACK_MAX_DIST (used as rejection gate).
     */
    float computeCost(const Tracklet& t, const ThermalPeak& p) const;

    // findBestTrack replaced by hungarianMatch for globally optimal assignment.
    // Kept here for reference; no longer called.
    // int findBestTrack(const ThermalPeak& p, bool* already_matched) const;

    /**
     * @brief Globally optimal assignment via Kuhn-Munkres (Hungarian) algorithm.
     *
     * Builds a cost matrix (active tracks × valid peaks) and delegates to
     * HungarianAlgorithm::solve(). Guarantees that simultaneous crossings
     * do NOT cause ID swaps.
     *
     * @param peaks         Detected peaks array (may include suppressed entries)
     * @param numPeaks      Total entries in peaks[]
     * @param assignment    Output: assignment[i] = index of peak assigned to track i,
     *                      -1 if track i has no assignment this frame (miss)
     * @param peak_assigned Output: peak_assigned[p] = true if peak p was assigned
     */
    void hungarianMatch(const ThermalPeak* peaks, int numPeaks,
                        int* assignment, bool* peak_assigned) const;

    /**
     * @brief Returns a free slot, recycling unconfirmed tracks when full.
     * @return Pointer to usable Tracklet, or nullptr if all slots are confirmed.
     */
    Tracklet* allocateTrack();

    void predictAll();
    void removeExpired();
};
