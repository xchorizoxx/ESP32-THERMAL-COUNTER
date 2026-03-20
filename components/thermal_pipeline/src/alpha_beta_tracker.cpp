#include "alpha_beta_tracker.hpp"
#include <cstring>
#include <climits>

AlphaBetaTracker::AlphaBetaTracker()
    : nextId_(1)
{
    memset(tracks_, 0, sizeof(tracks_));
    for (int i = 0; i < 15; i++) {
        tracks_[i].activo = false;
    }
}

int AlphaBetaTracker::getMaxTracks() const
{
    return 15;
}

int AlphaBetaTracker::getActiveCount() const
{
    int count = 0;
    for (int i = 0; i < 15; i++) {
        if (tracks_[i].activo) count++;
    }
    return count;
}

Track* AlphaBetaTracker::findFreeTrack()
{
    for (int i = 0; i < 15; i++) {
        if (!tracks_[i].activo) return &tracks_[i];
    }
    return nullptr; // Out of space
}

void AlphaBetaTracker::evaluateCountingLogic(Track& track,
                                              int lineEntryY, int lineExitY,
                                              int& countIn, int& countOut)
{
    const float y = track.y;

    // TODO: Expand to array per column after visual calibration.
    // Currently using uniform horizontal lines.
    // With 110° FOV and wide door, real lines may be curved.

    if (track.estado_y == 0) {
        // Came from upper zone → crossed downwards → IN
        if (y >= (float)lineExitY) {
            countIn++;
            track.estado_y = 2;
        }
    } else if (track.estado_y == 2) {
        // Came from lower zone → crossed upwards → OUT
        if (y <= (float)lineEntryY) {
            countOut++;
            track.estado_y = 0;
        }
    } else {
        // estado_y == 1 (neutral zone): track appeared between lines.
        // If it crosses a line showing directional intent, we count it to avoid missing crowds.
        if (y >= (float)lineExitY && track.v_y > 0.05f) {
            countIn++;
            track.estado_y = 2;
        } else if (y <= (float)lineEntryY && track.v_y < -0.05f) {
            countOut++;
            track.estado_y = 0;
        } else {
            // No valid crossing, settle state without counting
            if (y < (float)lineEntryY) {
                track.estado_y = 0;
            } else if (y >= (float)lineExitY) {
                track.estado_y = 2;
            }
        }
    }
}

void AlphaBetaTracker::update(const PicoTermico* peaks, int numPeaks,
                               float alpha, float beta,
                               int maxDistSq, int maxAge,
                               int lineEntryY, int lineExitY,
                               int& countIn, int& countOut)
{
    // --- Phase 1: Prediction ---
    // Project future position of active tracks
    for (int i = 0; i < 15; i++) {
        if (!tracks_[i].activo) continue;
        tracks_[i].x += tracks_[i].v_x;
        tracks_[i].y += tracks_[i].v_y;
    }

    // --- Phase 2: Greedy Assignment ---
    // For each surviving peak, search for the nearest active track
    bool trackMatched[15] = {};

    for (int p = 0; p < numPeaks; p++) {
        if (peaks[p].suprimido) continue;

        const float px = (float)peaks[p].x;
        const float py = (float)peaks[p].y;

        // Search for nearest track
        int bestTrack = -1;
        int bestDist2 = maxDistSq;

        for (int t = 0; t < 15; t++) {
            if (!tracks_[t].activo) continue;
            if (trackMatched[t]) continue; // <-- FIX: Prevents multiple peaks from claiming same track

            const float dx = px - tracks_[t].x;
            const float dy = py - tracks_[t].y;
            const int d2 = (int)(dx * dx + dy * dy);

            if (d2 <= bestDist2) {
                bestDist2 = d2;
                bestTrack = t;
            }
        }

        if (bestTrack >= 0) {
            // --- Phase 3: Alpha-Beta Correction ---
            Track& t = tracks_[bestTrack];
            const float ex = px - t.x;
            const float ey = py - t.y;

            t.x   = t.x + alpha * ex;
            t.y   = t.y + alpha * ey;
            t.v_x = t.v_x + beta * ex;
            t.v_y = t.v_y + beta * ey;
            t.age = 0;
            trackMatched[bestTrack] = true;

            // Evaluate counting logic
            evaluateCountingLogic(t, lineEntryY, lineExitY, countIn, countOut);
        } else {
            // Peak without track → create new track
            Track* freeSlot = findFreeTrack();
            if (freeSlot != nullptr) {
                freeSlot->id       = nextId_++;
                freeSlot->x        = px;
                freeSlot->y        = py;
                freeSlot->v_x      = 0.0f;
                freeSlot->v_y      = 0.0f;
                freeSlot->age      = 0;
                freeSlot->activo   = true;
                // Determine initial state based on position
                freeSlot->estado_y = (py < (float)lineEntryY) ? 0 :
                                     (py >= (float)lineExitY) ? 2 : 1;
            }
        }
    }

    // --- Phase 4: Aging ---
    for (int i = 0; i < 15; i++) {
        if (!tracks_[i].activo) continue;
        if (!trackMatched[i]) {
            tracks_[i].age++;
            if (tracks_[i].age > (uint8_t)maxAge) {
                tracks_[i].activo = false;
            }
        }
    }
}
