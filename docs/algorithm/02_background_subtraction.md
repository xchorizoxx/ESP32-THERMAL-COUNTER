# Background Extraction and Selective EMA

The first and most crucial step of the pipeline is distinguishing between the "empty floor" and "a person." Since ambient temperature changes throughout the day, we cannot have a fixed pre-programmed temperature.

## EMA (Exponential Moving Average)

EMA stands for **Exponential Moving Average**. It is an algorithm that blends the "historical" value (what it learned in the past) with the "new" value (the current frame from the sensor).

In the code, it is controlled by `EMA_ALPHA`:
* `NewBackgroundValue = (Actual * EMA_ALPHA) + (PreviousBackground * (1 - EMA_ALPHA))`

### How to calibrate `EMA_ALPHA`?
`EMA_ALPHA` determines the **adaptation speed**:
* **If `EMA_ALPHA` is near 0 (e.g., 0.01):** The background changes very, very slowly. It is highly stable against noise, but if a hot box is left on the floor, the system will take a long time to "get used" to the box and stop seeing it as a static person.
* **If `EMA_ALPHA` is large (e.g., 0.20):** The background adapts almost instantly. This is problematic because a person who stands still for 2 seconds will become "invisible" (absorbed by the background).
* **Recommended value (0.05 to 0.10):** Allows for fast adaptation to air currents without absorbing people in normal movement.

## Selectivity and Feedback Mask

To prevent a person who walks very slowly (or stops to talk beneath the sensor) from being learned as "floor," we use the **Feedback Mask**.

Step 5 of the pipeline (after identifying people) projects an exclusion square (controlled by `MASK_HALF_SIZE`) around each person. 
When the next frame arrives, the background updater (Step 1) **completely ignores** the pixels covered by this mask. For those pixels, `NewBackgroundValue = PreviousBackground`. 

This ensures the floor is only updated with information from areas where we are 100% sure no one is present.

## Contrast (Delta T)

The `BACKGROUND_DELTA_T` parameter represents how many degrees Celsius above the "learned background" temperature something must be to catch our attention.
* **If `BACKGROUND_DELTA_T` is very low (e.g., 0.5 °C):** Excessive thermal noise and "ghost false positives" will occur.
* **If `BACKGROUND_DELTA_T` is very high (e.g., 3.0 °C):** Hot objects will be ignored. If a person has thick hair or a cold hat (e.g., 22°C) and the floor is 20°C, the difference is only 2°C, and we would not detect them.
* **Recommended value (1.0 to 1.5):** Perfect balance to reject normal sensor fluctuations while still capturing biological heat.

## 🧪 The Dynamic Background Model

To detect hot objects (people), the system needs to know the ambient temperature at rest. We use an **Exponential Moving Average (EMA)** model.

### Processing Chain Integration
1. **Acquisition**: Raw MLX90640 frame.
2. **Pre-processing (A1)**: Chess fusion and **Kalman per-pixel filter**.
3. **Background Update**: The EMA map is derived from the **filtered_frame_**.

### The EMA Equation
For each pixel $(x, y)$, the background model $B$ is updated in each frame $t$ according to:

$$B_{t} = (1 - \alpha) \cdot B_{t-1} + \alpha \cdot I_{t}$$

Where:
- $B_t$: New background value.
- $B_{t-1}$: Previous background.
- $I_t$: Current captured thermal image.
- $\alpha$: Learning factor (`LEARNING_RATE`). A high $\alpha$ learns quickly (useful in highly changing environments), a low $\alpha$ is more stable against noise.

### Selective Learning (Feedback Loop)
The major challenge with a simple EMA model is that if a person stands still, they "disappear" because the background eventually learns their temperature. To prevent this, we implement a **Track Exclusion Filter**:

```cpp
if (pixel_is_tracked[x][y]) {
    // Freeze learning: do not update the background at this coordinate
    new_background[x][y] = old_background[x][y];
} else {
    // Apply normal EMA
    new_background[x][y] = apply_ema(old_background[x][y], current_frame[x][y]);
}
```

This mechanism ensures that active tracks maintain their contrast against the background regardless of how long they stay in the same position.

---

## Radar Mode (Subtraction)
The system allows direct visualization of the subtraction result: `CurrentImage - BackgroundImage`. 
In the visual interface, this is known as "Radar Mode." It is useful for:
1. **Verifying Noise Level:** If the background is well-learned, the image should appear mostly black in empty areas.
2. **Detecting Work or "Hot Obstacles":** If there is an object that flickers or emits intermittent heat, it will be clearly visible in this mode.
3. **Ghost Debugging:** If a person was mistakenly "absorbed" by the background, picking them up will leave a "cold hole" (a blue ghost) in subtraction mode.
