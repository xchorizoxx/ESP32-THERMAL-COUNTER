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
    return nullptr; // Sin espacio
}

void AlphaBetaTracker::evaluateCountingLogic(Track& track,
                                              int lineEntryY, int lineExitY,
                                              int& countIn, int& countOut)
{
    const float y = track.y;

    // TODO: Expandir a array por columna tras calibración visual.
    // Actualmente se usan líneas horizontales uniformes.
    // Con FOV 110° y puerta ancha, las líneas reales pueden ser curvas.

    if (track.estado_y == 0) {
        // Venía de zona superior → cruzó hacia abajo → IN
        if (y >= (float)lineExitY) {
            countIn++;
            track.estado_y = 2;
        }
    } else if (track.estado_y == 2) {
        // Venía de zona inferior → cruzó hacia arriba → OUT
        if (y <= (float)lineEntryY) {
            countOut++;
            track.estado_y = 0;
        }
    } else {
        // estado_y == 1 (zona neutra): el track apareció en medio de las líneas.
        // Si cruza una línea demostrando intención direccional, lo contamos para no perder multitudes.
        if (y >= (float)lineExitY && track.v_y > 0.05f) {
            countIn++;
            track.estado_y = 2;
        } else if (y <= (float)lineEntryY && track.v_y < -0.05f) {
            countOut++;
            track.estado_y = 0;
        } else {
            // Sin cruce válido, asentar estado sin contar
            if (y < (float)lineEntryY) {
                track.estado_y = 0;
            } else if (y >= (float)lineExitY) {
                track.estado_y = 2;
            }
        }
    }
}

void AlphaBetaTracker::update(const PicoTermico* picos, int numPicos,
                               float alpha, float beta,
                               int maxDistSq, int maxAge,
                               int lineEntryY, int lineExitY,
                               int& countIn, int& countOut)
{
    // --- Fase 1: Predicción ---
    // Proyectar posición futura de tracks activos
    for (int i = 0; i < 15; i++) {
        if (!tracks_[i].activo) continue;
        tracks_[i].x += tracks_[i].v_x;
        tracks_[i].y += tracks_[i].v_y;
    }

    // --- Fase 2: Asignación Greedy ---
    // Para cada pico sobreviviente, buscar el track activo más cercano
    bool trackMatched[15] = {};

    for (int p = 0; p < numPicos; p++) {
        if (picos[p].suprimido) continue;

        const float px = (float)picos[p].x;
        const float py = (float)picos[p].y;

        // Buscar track más cercano
        int bestTrack = -1;
        int bestDist2 = maxDistSq;

        for (int t = 0; t < 15; t++) {
            if (!tracks_[t].activo) continue;
            if (trackMatched[t]) continue; // <-- FIX: Evita que múltiples picos reclamen y sobrescriban el mismo track

            const float dx = px - tracks_[t].x;
            const float dy = py - tracks_[t].y;
            const int d2 = (int)(dx * dx + dy * dy);

            if (d2 <= bestDist2) {
                bestDist2 = d2;
                bestTrack = t;
            }
        }

        if (bestTrack >= 0) {
            // --- Fase 3: Corrección Alpha-Beta ---
            Track& t = tracks_[bestTrack];
            const float ex = px - t.x;
            const float ey = py - t.y;

            t.x  = t.x + alpha * ex;
            t.y  = t.y + alpha * ey;
            t.v_x = t.v_x + beta * ex;
            t.v_y = t.v_y + beta * ey;
            t.age = 0;
            trackMatched[bestTrack] = true;

            // Evaluar lógica de conteo
            evaluateCountingLogic(t, lineEntryY, lineExitY, countIn, countOut);
        } else {
            // Pico sin track → crear nuevo track
            Track* freeSlot = findFreeTrack();
            if (freeSlot != nullptr) {
                freeSlot->id       = nextId_++;
                freeSlot->x        = px;
                freeSlot->y        = py;
                freeSlot->v_x      = 0.0f;
                freeSlot->v_y      = 0.0f;
                freeSlot->age      = 0;
                freeSlot->activo   = true;
                // Determinar estado inicial según posición
                freeSlot->estado_y = (py < (float)lineEntryY) ? 0 :
                                     (py >= (float)lineExitY) ? 2 : 1;
            }
        }
    }

    // --- Fase 4: Envejecimiento ---
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
