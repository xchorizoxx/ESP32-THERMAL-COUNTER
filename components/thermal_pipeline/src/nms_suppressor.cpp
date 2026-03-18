#include "nms_suppressor.hpp"

void NmsSuppressor::suppress(PicoTermico* picos, int numPicos,
                             int rCenterSq, int rEdgeSq,
                             int centerXMin, int centerXMax)
{
    if (numPicos <= 1) return;

    // Paso 1: Insertion sort por temperatura descendente
    // N <= 15, O(N²) es óptimo para este tamaño
    for (int i = 1; i < numPicos; i++) {
        PicoTermico key = picos[i];
        int j = i - 1;
        while (j >= 0 && picos[j].temperatura < key.temperatura) {
            picos[j + 1] = picos[j];
            j--;
        }
        picos[j + 1] = key;
    }

    // Paso 2: Supresión — El pico más caliente domina en su vecindad
    for (int j = 0; j < numPicos; j++) {
        if (picos[j].suprimido) continue;

        const int xj = picos[j].x;
        const int yj = picos[j].y;

        // Radio² según posición (central vs borde del lente)
        const int radiusSq = (xj >= centerXMin && xj <= centerXMax)
                             ? rCenterSq
                             : rEdgeSq;

        // Suprimir picos k más fríos dentro del radio
        for (int k = j + 1; k < numPicos; k++) {
            if (picos[k].suprimido) continue;

            const int dx = (int)xj - (int)picos[k].x;
            const int dy = (int)yj - (int)picos[k].y;
            const int d2 = dx * dx + dy * dy;

            if (d2 <= radiusSq) {
                picos[k].suprimido = true;
            }
        }
    }
}
