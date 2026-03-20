# Blob Detection and Predictive Tracking

Once we have the thermal image _subtracted from the background_ (only seeing what is hotter than the floor), we need to convert those "heat spots" (blobs) into countable entities.

## 1. Topology (Peak Detection)

We do not look for complex contours; we apply a mathematical method called 2D peak detection. 
A pixel is considered a "Peak" if:
1. Its net temperature (vs. background) is > `DELTA_T_FONDO`.
2. Its absolute temperature is > `TEMP_BIOLOGICO_MIN` (ensures it is a human or animal, not a warm cable).
3. It is strictly hotter than the 8 pixels immediately around it.

## 🛡️ Non-Maximum Suppression (NMS)

To prevent a person from being detected multiple times due to their size, we apply a circular neighborhood filter.

### NMS Algorithm
1. All detected peaks are sorted by temperature from highest to lowest.
2. For the hottest peak, all other peaks within a radius $R$ (`nms_radius` parameter) are removed.
3. This is repeated for the next hottest surviving peak.

This process ensures that only the thermal **center of mass** is processed by the tracker.

---

## 📈 Tracking with Alpha-Beta Filter

The system uses an Alpha-Beta Filter to predict the position of objects in the next frame, compensating for measurement noise.

### State Equations
For each component $(x, y)$:

1. **Prediction**:
   $$\hat{x}_{k} = x_{k-1} + v_{k-1} \cdot \Delta t$$
2. **Position Update**:
   $$x_{k} = \hat{x}_{k} + \alpha \cdot (z_{k} - \hat{x}_{k})$$
3. **Velocity Update**:
   $$v_{k} = v_{k-1} + \frac{\beta}{\Delta t} \cdot (z_{k} - \hat{x}_{k})$$

Where $z_k$ is the current measurement and $\alpha, \beta$ are the filter gains. This allows the system to "understand" the inertia of human movement, ignoring erratic jumps from a single frame.

---

We have a list of "Raw Peaks" (X,Y position) in the *current frame*. How do we know who is who from the *previous frame*?

We use an Alpha-Beta tracker, which is a simplified version of the famous Kalman Filter.
The tracker assigns each person not only a Position, but a **Velocity**.

1. **Prediction:** Using the Person's previous velocity, their expected position in this frame is calculated.
2. **Matching:** The closest "Raw Peak" to that predicted position is sought (provided the distance does not exceed `MAX_MATCH_DIST_SQ`).
3. **Update:**
   * If a match is found, the error between the predicted and measured position is calculated. 
   * The position is corrected using `ALPHA_TRK` (how much to trust the sensor vs. physics).
   * The velocity is corrected using `BETA_TRK`.
   * **Crowd Protection (V2.5):** A "Track Matched" check was implemented to prevent two close people from "stealing" the same track. This maintains unique identities even in critical crossings.

## 4. Velocity Vectors (V2)

Starting with version 2, the system calculates a direction vector `(vx, vy)` for each track. 
- **Calculation:** Based on the position difference corrected by the Alpha-Beta filter between successive frames.
- **Visualization:** In the Tactical HUD, this vector is drawn as a thin yellow arrow. The arrow's length is proportional to the speed, allowing for visual prediction of crossing direction.

## 5. Counting Logic and Intent Inference

Counting occurs strictly by comparing the person's position between frame `T-1` and frame `T`, but now with **Intent Inference**:

1. **Standard Crossing:** If the track crosses from the upper zone to the lower zone (or vice versa) having originated outside the neutral zone.
2. **Spawn in Neutral Zone (V2.5):** If a person "appears" (spawns) directly between the counting lines (because the sensor lost them for a second), the system does not ignore the crossing. Instead, it evaluates their **Y Velocity Vector**:
   - If they cross the lower line with `v_y > 0.05`, **Entry** intent is inferred.
   - If they cross the upper line with `v_y < -0.05`, **Exit** intent is inferred.

This bidirectional logic ensures that almost no person escapes without being counted, even with temporary occlusions.
