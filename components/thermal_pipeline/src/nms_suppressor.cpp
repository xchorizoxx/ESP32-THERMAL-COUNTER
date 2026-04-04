#include "nms_suppressor.hpp"

void NmsSuppressor::suppress(ThermalPeak* peaks, int numPeaks,
                             int rCenterSq, int rEdgeSq,
                             int centerXMin, int centerXMax)
{
    if (numPeaks <= 1) return;

    // Step 1: Insertion sort by descending temperature
    // N <= 15, O(N²) is optimal for this size
    for (int i = 1; i < numPeaks; i++) {
        ThermalPeak key = peaks[i];
        int j = i - 1;
        while (j >= 0 && peaks[j].temperature < key.temperature) {
            peaks[j + 1] = peaks[j];
            j--;
        }
        peaks[j + 1] = key;
    }

    // Step 2: Suppression — The hottest peak dominates in its vicinity
    for (int j = 0; j < numPeaks; j++) {
        if (peaks[j].suppressed) continue;

        const int xj = peaks[j].x;
        const int yj = peaks[j].y;

        // Radius² according to position (center vs lens edge)
        const int radiusSq = (xj >= centerXMin && xj <= centerXMax)
                             ? rCenterSq
                             : rEdgeSq;

        // Suppress colder k-peaks within the radius
        for (int k = j + 1; k < numPeaks; k++) {
            if (peaks[k].suppressed) continue;

            const int dx = (int)xj - (int)peaks[k].x;
            const int dy = (int)yj - (int)peaks[k].y;
            const int d2 = dx * dx + dy * dy;

            if (d2 <= radiusSq) {
                peaks[k].suppressed = true;
            }
        }
    }
}
