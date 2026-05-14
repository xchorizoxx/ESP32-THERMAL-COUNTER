# Tracking and Counting Algorithm

This document describes the algorithms implemented in Stage A2 (TrackletTracker) and Stage A3 (TrackletFSM), which replace the previous Alpha-Beta system.

## TrackletTracker (Stage A2)

### Motivation

The previous tracker (Alpha-Beta) suffered from:
- Ghost IDs: tracks that persisted after the person left the field of view
- ID swapping: identity exchange when two people crossed paths
- Visually unstable positions in the HUD

TrackletTracker solves these problems through position history and proportional memory.

#### B. Sub-pixel Extraction (Stage A3-B1)

To avoid "jumping" between pixels, we calculate the exact position of the heat patch using **First-Order Thermal Moments** (local Center of Mass):

$$x_{sub} = \frac{\sum (x_i \cdot T_i)}{\sum T_i}$$
$$y_{sub} = \frac{\sum (y_i \cdot T_i)}{\sum T_i}$$

Where $T_i$ is the temperature of the pixels adjacent to the local maximum. This allows for fluid trajectories even on the low-resolution grid (32x24).

#### C. Geometric FOV Correction (Stage A3-B1)
### Phase 2: Tracking (Stage A3-B1)

The system maintains a history of up to 20 frames per person. Detection association is performed using a **Cost-based Assignment Algorithm** based on **Physical Euclidean Distance (in meters)**.

1.  **Ray Angles**: We calculate the angle $\theta$ from the optical axis based on the sub-pixel position.
2.  **Planar Projection**: We use the sensor height ($h = SENSOR\_HEIGHT\_M$) to find the real distance:
    -   $X_{meters} = h \cdot \tan(\theta_x)$
    -   $Y_{meters} = h \cdot \tan(\theta_y)$

This normalizes movement: a 1-meter step looks the same (same tracking cost) anywhere in the field of view.

### Data Structures

**TrackHistory (Circular Buffer):**
```cpp
struct TrackHistory {
    static constexpr int CAPACITY = 20;  // 20 frames @ 8Hz = 2.5s
    
    struct Entry {
        float x, y;      // Position [pixels]
        uint32_t ts;     // Timestamp [ticks]
    };
    
    Entry entries[CAPACITY];
    int head = 0;        // Insertion index
    int count = 0;       // Valid entries (0..20)
};
```

**Tracklet:**
```cpp
struct Tracklet {
    uint8_t id;                    // Unique ID (1..15)
    float x, y;                    // Filtered position for matching
    float vx, vy;                  // Velocity [px/frame]
    float display_x, display_y;    // Smoothed position for HUD
    
    TrackHistory history;          // 20-frame history
    float temperature;             // Associated peak temperature
    
    // Lifecycle
    uint8_t confirm_count = 0;      // Consecutive frames with match
    uint8_t missed_count = 0;       // Consecutive frames without match
    bool confirmed = false;         // Gate: confirm_count >= 3
    bool active = false;
    
    // FSM state for counting (populated by TrackletFSM)
    uint8_t zone_state = 0;         // 0=unborn, 1=in, 2=neutral, 3=out
};
```

### Matching Algorithm

For each frame:

**1. Prediction:**
```cpp
// Use history to estimate linear velocity
if (history.count >= 2) {
    int prev_idx = (head - 1 + CAPACITY) % CAPACITY;
    vx = (x - entries[prev_idx].x);  // Simple difference
    vy = (y - entries[prev_idx].y);
}

predicted_x = x + vx;
predicted_y = y + vy;
```

**2. Peak Association:**

For each peak detected post-NMS, calculate composite cost against all active tracks:

```cpp
float cost(Tracklet tracklet, Peak p) {
    float dx = tracklet.predicted_x - p.x;
    float dy = tracklet.predicted_y - p.y;
    float dist_sq = dx*dx + dy*dy;
    
    float temp_diff = abs(tracklet.temperature - p.temperature);
    
    return dist_sq + (temp_diff * temp_diff) * TRACK_TEMP_WEIGHT;
}
```

- Distance threshold: `TRACK_MAX_DIST_SQ` (64 px² = 8px radius)
- Temperature weight: `TRACK_TEMP_WEIGHT` (0.25f)

**3. Optimal Assignment (Greedy):**

Sort potential matches by ascending cost. For each match:
- If track unassigned AND peak unassigned → assign
- Mark track as updated, increment `confirm_count`, reset `missed_count`

**4. New Track Initialization:**

Unassigned peaks → new tracks:
- Find free slot in tracklets array
- Or recycle oldest unconfirmed track
- Initialize `confirm_count = 1`, `confirmed = false`

**5. Expiration:**

For tracks not updated:
- Increment `missed_count`
- Calculate max tolerance: `max_missed = min(TRACK_MAX_MISSED, confirm_count + 3)`
- If `missed_count > max_missed` → mark inactive

### Display Smoothing

Decoupling: physical position (for matching) vs visual position (for HUD):

```cpp
// Physics: raw filtered position
x = matched_peak.x;
y = matched_peak.y;

// Visual: EMA smoothed to eliminate stutter
display_x = TRACK_DISPLAY_SMOOTH * x + (1 - TRACK_DISPLAY_SMOOTH) * display_x_prev;
display_y = TRACK_DISPLAY_SMOOTH * y + (1 - TRACK_DISPLAY_SMOOTH) * display_y_prev;
```

`TRACK_DISPLAY_SMOOTH = 0.4f` provides smoothing without perceptible lag.

## TrackletFSM (Stage A3)

Finite state machine for bidirectional counting with configurable line support.

### FSM States per Track

```cpp
enum class FsmState {
    UNBORN = 0,      // Not started (in neutral or dead zone)
    TRACKING_IN,     // Detected in north/entry zone
    TRACKING_OUT     // Detected in south/exit zone
};

struct FsmMemory {
    uint8_t id;           // Associated track ID
    FsmState state;       // Current state
};
```

### Segment Mode (New)

Supports up to 4 arbitrary counting lines (not horizontal):

```cpp
struct CountingSegment {
    float x1, y1;     // Start point [0..31], [0..23]
    float x2, y2;     // End point
    uint8_t id;       // ID 1..4
    char name[16];    // "Main Entrance", etc.
    bool enabled;
};
```

**Crossing Detection:**

Segment-to-segment intersection algorithm with direction test:

```cpp
int checkSegmentCrossing(float prev_x, float prev_y, 
                          float curr_x, float curr_y,
                          float sx1, float sy1, float sx2, float sy2) {
    // Counting segment vector
    float sdx = sx2 - sx1, sdy = sy2 - sy1;
    
    // Cross product to determine side
    float side_prev = sdx*(prev_y-sy1) - sdy*(prev_x-sx1);
    float side_curr = sdx*(curr_y-sy1) - sdy*(curr_x-sx1);
    
    // Crossed from left to right of segment?
    if (side_prev > 0 && side_curr <= 0) return 1;   // +1 OUT
    if (side_prev < 0 && side_curr >= 0) return -1;  // +1 IN
    return 0;  // No crossing
}
```

**Per-Track Debounce:**

Prevents double-counting when a track crosses multiple segments in the same frame:

```cpp
bool already_counted_in = false;
bool already_counted_out = false;

for (each segment) {
    int cross = checkSegmentCrossing(...);
    if (cross == 1 && !already_counted_out) {
        countOut++;
        already_counted_out = true;
    }
    if (cross == -1 && !already_counted_in) {
        countIn++;
        already_counted_in = true;
    }
}
```

### Legacy Mode (Horizontal Lines)

For simple installations, supports horizontal Y-line mode:

```
Entry Line: Y = DEFAULT_LINE_ENTRY_Y  (north zone)
Exit Line:  Y = DEFAULT_LINE_EXIT_Y   (south zone)
Dead Zone:  X < DEFAULT_DEAD_ZONE_LEFT or X > DEFAULT_DEAD_ZONE_RIGHT
```

FSM Transitions:
- UNBORN + y <= entry_line → TRACKING_IN
- UNBORN + y >= exit_line  → TRACKING_OUT  
- TRACKING_IN + y >= exit_line → TRACKING_OUT, countOut++
- TRACKING_OUT + y <= entry_line → TRACKING_IN, countIn++

### HUD Integration

TrackletFSM writes `zone_state` back into the tracklet:

```cpp
// Web Interface Colors
switch (tracklet.zone_state) {
    case 1: color = GREEN;   break;  // TRACKING_IN
    case 2: color = AMBER;   break;  // Neutral / segment mode
    case 3: color = CYAN;    break;  // TRACKING_OUT
    default: color = GRAY;   break;  // UNBORN / dead zone
}
```

## Comparison with Previous Alpha-Beta

| Aspect | Alpha-Beta (Legacy) | TrackletTracker (A2) |
|---------|-------------------|---------------------|
| Prediction | Simple α-β filter | 20-frame history |
| Matching | Distance only | Distance + temperature |
| Confirmation gate | Immediate | 3 consecutive frames |
| Expiration memory | Fixed (5 frames) | Proportional to age |
| Visualization | Same as physics | Separate EMA (smoothed) |

## Configurable Parameters

```cpp
// In thermal_config.hpp
constexpr int   TRACK_CONFIRM_FRAMES  = 3;
constexpr int   TRACK_MAX_MISSED      = 12;
constexpr float TRACK_MAX_DIST        = 8.0f;
constexpr float TRACK_TEMP_WEIGHT     = 0.25f;
constexpr float TRACK_DISPLAY_SMOOTH  = 0.4f;
```

## References

- Implementation: `components/thermal_pipeline/src/tracklet_tracker.cpp`
- FSM: `components/thermal_pipeline/src/tracklet_fsm.cpp`
- Structures: `components/thermal_pipeline/include/thermal_types.hpp`
