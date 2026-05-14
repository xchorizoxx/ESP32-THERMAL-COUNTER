#include "nms_suppressor.hpp"

void NmsSuppressor::suppress(ThermalPeak* peaks, int numPeaks, int radiusSq)
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

        const float xj = peaks[j].x;
        const float yj = peaks[j].y;

        // Suppress colder k-peaks within the radius
        for (int k = j + 1; k < numPeaks; k++) {
            if (peaks[k].suppressed) continue;

            const float dx = xj - peaks[k].x;
            const float dy = yj - peaks[k].y;
            const float d2 = dx * dx + dy * dy;

            if (d2 <= (float)radiusSq) {
                peaks[k].suppressed = true;
            }
        }
    }
}
