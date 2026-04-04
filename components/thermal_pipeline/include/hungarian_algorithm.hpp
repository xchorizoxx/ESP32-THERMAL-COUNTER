/**
 * @file hungarian_algorithm.hpp
 * @brief Kuhn-Munkres (Hungarian) optimal assignment algorithm — standalone module.
 *
 * Solves the classic assignment problem in O(N³) using the Jonker-Volgenant
 * potential-based approach. Designed for N ≤ MAX_N (15) to avoid heap
 * allocation — all working arrays are stack-local or static.
 *
 * Usage:
 *   float cost[N][N];
 *   int   assignment[N];   // assignment[row] = col assigned to that row
 *   HungarianAlgorithm::solve(cost, N, assignment);
 *
 * Convention:
 *   - cost[i][j] < HUNGARIAN_INF  → valid assignment (finite cost)
 *   - cost[i][j] >= HUNGARIAN_INF → prohibited assignment (virtual or gated)
 *   - assignment[i] = -1          → row i received no valid assignment
 *
 * FASE 4 — Thermal Counter optimal assignment.
 */

#pragma once
#include <cstdint>

namespace HungarianAlgorithm {

/// Sentinel cost value for invalid / gated cells.
static constexpr float INF = 1e9f;

/// Maximum matrix dimension supported (must match ThermalConfig::MAX_TRACKS).
static constexpr int MAX_N = 15;

/**
 * @brief Solve an N×N assignment problem in-place.
 *
 * @param cost        Square cost matrix [MAX_N][MAX_N].
 *                    Only the top-left N×N sub-matrix is read.
 *                    Entries >= INF are treated as prohibited.
 * @param N           Actual dimension of the problem (1 ≤ N ≤ MAX_N).
 * @param assignment  Output: assignment[i] = column assigned to row i.
 *                    -1 if row i received no valid assignment.
 */
void solve(const float cost[MAX_N][MAX_N], int N, int assignment[MAX_N]);

} // namespace HungarianAlgorithm
