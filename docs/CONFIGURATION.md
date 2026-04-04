# Configuration and Calibration

## Web Interface Access

1. Connect to WiFi "ThermalCounter" (password: `counter1234`)
2. Open browser at http://192.168.4.1
3. Adjust parameters → **Apply Settings** (immediate test)
4. When satisfied → **Save to Flash** (NVS persistence)

## Critical Parameters

### Biological Temperature (°C)

Minimum threshold to consider a pixel as "human". Everything below is ignored.

| Environment | Recommended Value |
|-------------|-------------------|
| Winter / cold | 21-23°C |
| Normal office | 25-27°C |
| Summer / hot | 28-30°C |

**Symptoms of incorrect value:**
- Too low: false positives (warm cables, reflections)
- Too high: people not detected (especially with thick clothing)

### Background Delta T (°C)

Minimum contrast required vs learned background to consider a valid detection.

- Typical range: 1.5 - 2.5°C
- Low (<1.0): sensor noise generates false positives
- High (>3.0): low emissivity people (hats, thick hair) not detected

### EMA Alpha (Adaptation Speed)

Background learning speed: `B(t) = α·I(t) + (1-α)·B(t-1)`

| Value | Behavior | Use |
|-------|----------|-----|
| 0.01 | Learns very slow | Extremely stable environments |
| 0.05 | Standard (recommended) | Most installations |
| 0.10 | Learns fast | Frequent ambient temperature changes |
| >0.20 | Very fast | Risk: static people become "invisible" |

### NMS Radius (Non-Maximum Suppression)

Suppression radius to eliminate multiple detections of the same person.

- **NMS Center:** Radius in lens central zone (less distortion)
  - Low ceiling (2.5m): 8-16 pixels
  - Standard ceiling (3.5m): 4-8 pixels
  - High ceiling (4.5m): 2-4 pixels

- **NMS Edge:** Radius at edges (more optical distortion)
  - Generally half the center value

**Practical adjustment:**
1. Observe "Raw" mode in HUD
2. One person should generate exactly 1 green point
3. If multiple nearby points → increase radius
4. If small people not detected → decrease radius

### Counting Lines

**Segment Mode (Stage A3, recommended):**
- Draw arbitrary lines on canvas (not just horizontal)
- Supports diagonal lines, curves, multiple segments
- Up to 4 simultaneous lines

**Legacy Mode (horizontal Y lines):**
- `Line Entry Y`: Virtual north line (below = entry)
- `Line Exit Y`: Virtual south line (above = exit)

### Dead Zones

Lateral exclusion to ignore walls, pillars, or non-interest areas.

- `Dead Zone Left`: Pixels to ignore from left edge (0..32)
- `Dead Zone Right`: Pixels to ignore from right edge (0..32)

## Visualization Modes

| Mode | Description | Use |
|------|-------------|-----|
| **Normal** | Raw thermal image with tracks overlaid | Normal operation |
| **Background** | Learned background map | Verify correct learning |
| **Radar** | Difference (current - background) | Debug detections, see noise |

## Calibration Procedure (Steps)

### 1. Physical Installation

- Mount sensor at ~3.6m height, pointing vertically toward door
- Avoid air conditioning blowing directly on lens
- Ensure stable 3.3V power with decoupling capacitors

### 2. Initial Calibration

1. **Establish background:** Leave sensor on 5-10 minutes without people for EMA to learn empty floor

2. **Verify background:** Switch to "Background" mode. Should show uniform floor temperature image. If strange patterns, verify thermal mounting.

3. **Verify detections:**
   - "Radar" mode → cross door → red patch should appear (positive difference)
   - Verify patch disappears when leaving frame
   - If permanent residue remains, slightly increase EMA Alpha

4. **Adjust thresholds:**
   - Decrease `Biological Temp` until floor disappears (not detected)
   - Increase 2-3°C for safe margin
   - Adjust `Delta T` so only people detected (not smaller hot objects)

5. **Optimize tracking:**
   - Cross door slowly → ID should follow smoothly
   - Two people crossing → IDs should not swap
   - If swapping occurs, increase NMS radius or adjust `Temp Weight`

6. **Verify counting:**
   - 10 entry steps → IN counter should increment by 10
   - 10 exit steps → OUT counter should increment by 10
   - Verify segment mode draws lines correctly

7. **Save configuration:**
   - Click "Save to Flash"
   - Verify confirmation message in logs
   - Restart ESP32 → verify values persist

## Persistence (NVS)

Parameters are stored in ESP32 non-volatile flash (NVS):

- Namespace: `"thcfg"`
- Floats stored as int32 (value × 1000)
- Segment lines stored as JSON string
- Automatic load at startup

**Reset to defaults:** No direct button. Manually modify to standard values and save.

## Troubleshooting

| Symptom | Probable Cause | Solution |
|---------|---------------|----------|
| Continuous false positives | Biological temp too low | Increase 2-3°C |
| People not detected | Biological temp too high | Decrease 2-3°C |
| Double counting of person | Multiple thermal peaks | Increase NMS radius |
| People "fade" into background | EMA Alpha too high | Reduce to 0.05 |
| Background never stabilizes | EMA Alpha too low | Increase to 0.10 |
| IDs jump between people | Matching too permissive | Check TEMP_WEIGHT, reduce MAX_DIST |
| IDs disappear quickly | Low missed tolerance | Normal in A2, proportional memory |
| Counting in wrong direction | Lines inverted | Swap entry/exit or line direction |
