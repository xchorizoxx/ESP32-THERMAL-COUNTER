#include "alpha_beta_tracker.hpp"
#include <cstring>
#include <cmath>

AlphaBetaTracker::AlphaBetaTracker()
    : nextId_(1)
{
    memset(tracks_, 0, sizeof(tracks_));
}

void AlphaBetaTracker::update(const ThermalPeak* peaks, int numPeaks,
                              float alpha, float beta,
                              int maxDistSq, int maxAge,
                              int lineEntryY, int lineExitY,
                              int& countIn, int& countOut)
{
    // --- Phase 1: Prediction & Age Update ---
    for (int i = 0; i < 15; i++) {
        if (!tracks_[i].active) continue;

        // Prediction: Position = Position + Velocity
        tracks_[i].x += tracks_[i].v_x;
        tracks_[i].y += tracks_[i].v_y;
        tracks_[i].age++;

        // Expiration check
        if (tracks_[i].age > (uint8_t)maxAge) {
            tracks_[i].active = false;
        }
    }

    // --- Phase 2: Data Association (Greedy Nearest Neighbor) ---
    bool peakAssigned[15] = {false}; // MAX_PEAKS

    for (int i = 0; i < 15; i++) {
        if (!tracks_[i].active) continue;

        int bestPeak = -1;
        float minDistSq = (float)maxDistSq;

        for (int p = 0; p < numPeaks; p++) {
            if (peaks[p].suppressed || peakAssigned[p]) continue;

            float dx = (float)peaks[p].x - tracks_[i].x;
            float dy = (float)peaks[p].y - tracks_[i].y;
            float d2 = dx * dx + dy * dy;

            if (d2 < minDistSq) {
                minDistSq = d2;
                bestPeak = p;
            }
        }

        if (bestPeak != -1) {
            // Phase 3: Update with Alpha-Beta Filter
            float residualX = (float)peaks[bestPeak].x - tracks_[i].x;
            float residualY = (float)peaks[bestPeak].y - tracks_[i].y;

            // Update state
            tracks_[i].x   += alpha * residualX;
            tracks_[i].y   += alpha * residualY;
            tracks_[i].v_x += beta  * residualX;
            tracks_[i].v_y += beta  * residualY;

            // Logic for counting (In/Out)
            evaluateCountingLogic(tracks_[i], lineEntryY, lineExitY, countIn, countOut);

            tracks_[i].age = 0; // Reset age on match
            peakAssigned[bestPeak] = true;
        }
    }

    // --- Phase 4: Initialization of New Tracks ---
    for (int p = 0; p < numPeaks; p++) {
        if (peaks[p].suppressed || peakAssigned[p]) continue;

        Track* freeTrack = findFreeTrack();
        if (freeTrack) {
            freeTrack->id       = nextId_++;
            if (nextId_ == 0) nextId_ = 1; // Avoid 0
            freeTrack->x        = (float)peaks[p].x;
            freeTrack->y        = (float)peaks[p].y;
            freeTrack->v_x      = 0;
            freeTrack->v_y      = 0;
            freeTrack->age      = 0;
            freeTrack->active   = true;
            // Initial state based on line entry position
            freeTrack->state_y = (peaks[p].y < 12) ? 0 : 2;
        }
    }
}

Track* AlphaBetaTracker::findFreeTrack() {
    for (int i = 0; i < 15; i++) {
        if (!tracks_[i].active) return &tracks_[i];
    }
    return nullptr;
}

void AlphaBetaTracker::evaluateCountingLogic(Track& track,
                                               int lineEntryY, int lineExitY,
                                               int& countIn, int& countOut)
{
    // Threshold hysteresis state machine
    // state_y: 0 (Upper), 1 (Neutral), 2 (Lower)
    
    int currentZone = 1; // Neutral
    if (track.y < (float)lineEntryY) currentZone = 0;
    else if (track.y > (float)lineExitY) currentZone = 2;

    if (track.state_y == 0 && currentZone == 2) {
        countIn++;
        track.state_y = 2;
    } else if (track.state_y == 2 && currentZone == 0) {
        countOut++;
        track.state_y = 0;
    } else if (currentZone != 1 && track.state_y == 1) {
        // Recover from neutral state
        track.state_y = currentZone;
    }
}

int AlphaBetaTracker::getActiveCount() const {
    int count = 0;
    for (int i = 0; i < 15; i++) {
        if (tracks_[i].active) count++;
    }
    return count;
}

int AlphaBetaTracker::getMaxTracks() const {
    return 15;
}
