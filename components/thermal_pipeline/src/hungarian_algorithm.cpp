/**
 * @file hungarian_algorithm.cpp
 * @brief Kuhn-Munkres (Hungarian) optimal assignment — FASE 4 implementation.
 *
 * Uses the Jonker-Volgenant potential-based O(N³) formulation.
 * All working arrays are static (no heap/stack overflow risk on ESP32-S3).
 *
 * Reference: "Solving the assignment problem by relaxation",
 *             Jonker & Volgenant, 1986.
 *
 * Memory:
 *   Static working storage = 15×15×4 + 4×(15+1)×4 = ~1.1 KB
 *   (lives in DRAM BSS segment, initialized once)
 */

#include "hungarian_algorithm.hpp"
#include <cstring>
#include "esp_log.h"

static const char* TAG = "HUNGARIAN";

namespace HungarianAlgorithm {

void solve(const float cost[MAX_N][MAX_N], int N, int assignment[MAX_N])
{
    // -----------------------------------------------------------------------
    //  Guard: trivial or oversized problem
    // -----------------------------------------------------------------------
    if (N <= 0 || N > MAX_N) {
        ESP_LOGE(TAG, "Invalid N=%d (MAX_N=%d)", N, MAX_N);
        for (int i = 0; i < MAX_N; i++) assignment[i] = -1;
        return;
    }

    // -----------------------------------------------------------------------
    //  Working arrays (static → zero-cost on each call entry)
    //  Using 1-indexed convention (index 0 is a sentinel).
    // -----------------------------------------------------------------------
    static float u[MAX_N + 1];   // row potentials   (1..N)
    static float v[MAX_N + 1];   // column potentials (1..N)
    static int   p[MAX_N + 1];   // p[j] = row assigned to column j (0 = unassigned)
    static int   way[MAX_N + 1]; // back-pointer for augmenting path

    static float minv[MAX_N + 1]; // minimum reduced cost for each column
    static bool  used[MAX_N + 1]; // column visited in current augmentation

    memset(u,   0, sizeof(u));
    memset(v,   0, sizeof(v));
    memset(p,   0, sizeof(p));
    memset(way, 0, sizeof(way));

    // -----------------------------------------------------------------------
    //  Main loop: augment N times (once per row)
    // -----------------------------------------------------------------------
    for (int i = 1; i <= N; i++) {
        // p[0] carries row i as "unmatched" sentinel for the augmenting scan
        p[0] = i;
        int j0 = 0;

        for (int j = 0; j <= N; j++) {
            minv[j] = INF;
            used[j] = false;
        }

        do {
            used[j0] = true;
            const int i0    = p[j0];
            float     delta = INF;
            int       j1    = -1;

            for (int j = 1; j <= N; j++) {
                if (used[j]) continue;

                // Reduced cost of assigning row i0 to column j
                const float rc = cost[i0 - 1][j - 1] - u[i0] - v[j];
                if (rc < minv[j]) {
                    minv[j] = rc;
                    way[j]  = j0;
                }
                if (minv[j] < delta) {
                    delta = minv[j];
                    j1    = j;
                }
            }

            // Saturate: if all unvisited columns are unreachable, abort row
            if (j1 < 0) {
                ESP_LOGD(TAG, "Row %d: no reachable column (fully gated)", i);
                j0 = 0; // FIX: Prevent invalid augmenting path trace
                break;
            }

            // Apply dual update
            for (int j = 0; j <= N; j++) {
                if (used[j]) {
                    u[p[j]] += delta;
                    v[j]    -= delta;
                } else {
                    minv[j] -= delta;
                }
            }
            j0 = j1;

        } while (p[j0] != 0);

        // Trace augmenting path back to anchor
        do {
            p[j0] = p[way[j0]];
            j0    = way[j0];
        } while (j0);
    }

    // -----------------------------------------------------------------------
    //  Extract assignment: p[j] = row that won column j
    //  Reject virtual assignments (cost >= INF)
    // -----------------------------------------------------------------------
    for (int i = 0; i < MAX_N; i++) assignment[i] = -1;

    for (int j = 1; j <= N; j++) {
        const int row = p[j] - 1;  // 0-indexed row
        const int col = j - 1;     // 0-indexed column
        if (row >= 0 && row < N && col < N) {
            if (cost[row][col] < INF) {
                assignment[row] = col;
            }
        }
    }
}

} // namespace HungarianAlgorithm
