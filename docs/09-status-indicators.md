# Status Indicators & UI Feedback

This document describes the visual feedback system implemented via the built-in **WS2812 RGB LED (GPIO 48)**. The system uses a specialized `StatusLedManager` to provide high-fidelity state information with a premium aesthetic.

## 1. Core Logic & Aesthetics

The LED interface is designed for **maximum information density** and **low energy consumption**.

*   **Human-Centric Breathing**: Uses sine-wave modulation with **Gamma Correction** for smooth, organic brightness transitions.
*   **Priority System**: High-priority events (crossings) instantly override background status animations.
*   **Energy Saving**: Base brightness is restricted to **5-15%** to reduce power draw while ensuring high visibility.

---

## 2. Visual Code Reference

### System States (Breathing Patterns)

| State | Color | Pattern | Rationale |
| :--- | :--- | :--- | :--- |
| **Booting** | **Blue** | Steady / Pulse | System initialization. |
| **Idle** | **Cyan** | Slow Breathe (5s) | Heartbeat - system ready, 0 tracks. |
| **Fatal Error** | **Red** | Slow Breathe (5s) | I2C / Sensor communication failure. |

### Tracking Load (Resistor Color Code)

The system uses the standard resistor color code to indicate the number of active tracks (0-20).

| Value | Color | RGB Value (Low Brightness) | Pattern (Decades) |
| :---: | :--- | :--- | :--- |
| **0** | Smoke/Grey | `(20, 20, 20)` | N/A (Handled by Idle) |
| **1** | Brown | `(30, 12, 5)` | **Steady** (1-9) |
| **2** | Red | `(60, 0, 0)` | **Steady** (1-9) |
| **3** | Orange | `(60, 20, 0)` | **Steady** (1-9) |
| **4** | Yellow | `(50, 50, 0)` | **Steady** (1-9) |
| **5** | Green | `(0, 60, 0)` | **Steady** (1-9) |
| **6** | Blue | `(0, 0, 60)` | **Steady** (1-9) |
| **7** | Violet | `(40, 0, 60)` | **Steady** (1-9) |
| **8** | Grey | `(20, 20, 20)` | **Steady** (1-9) |
| **9** | White | `(50, 50, 50)` | **Steady** (1-9) |

#### Multi-Track Indicators (10+)
*   **10 - 19 Tracks**: The LED uses the **Double Blink** pattern. The color corresponds to the unit digit (e.g., 12 tracks = Double Red blink).
*   **20 Tracks (Limit)**: The LED uses a **Triple Blink Red** pattern to indicate maximum system load.

---

## 3. High-Priority Events

When a person crosses the counting lines, the LED provides instantaneous feedback:

| Event | Color | Pattern | Priority |
| :--- | :--- | :--- | :--- |
| **Entry (IN)** | **Green** | Strobe (50ms) | Absolute High |
| **Exit (OUT)** | **Blue** | Strobe (50ms) | Absolute High |

---

## 4. Implementation Details

The `StatusLedManager` is implemented as a C++ Singleton and runs on its own FreeRTOS task (Core 0). 

---

## 5. Console Logs (CLI Feedback)

To complement the hardware LED, the system uses a **colorized serial logging protocol**. This allows developers to monitor the system status remotely via a terminal (e.g., `idf.py monitor`) using a consistent color scheme.

| Message Category | Console Color | ANSI Code | Linked LED State/Event |
| :--- | :--- | :--- | :--- |
| **Detection (IN)** | **Cyan** | `\033[0;36m` | **Entry (IN)** Strobe |
| **Detection (OUT)** | **Magenta** | `\033[0;35m` | **Exit (OUT)** Strobe |
| **Network/System** | **Magenta** | `\033[0;35m` | **Booting** / **Idle** |
| **Fatal Errors** | **Red** | Default | **Fatal Error** (Red Breathe) |
| **Memory Info** | **White** | `\033[0;37m` | N/A (Muted by default) |

> [!NOTE]
> **Full-Line Colorization**: Unlike standard ESP-IDF logs, the system colorizes the **entire line** (including timestamp and tag) for critical events to ensure they stand out in high-traffic log environments.

> [!TIP]
> **Energy Optimization**: The WS2812 consumes ~1mA even when "off". By using low-duty cycle animations and low brightness levels, we minimize the active current draw to approximately **3-5mA** on average.

> [!WARNING]
> **Hardware Constraint**: This LED is digital (WS2812). It is controlled via the **RMT** peripheral. Do not attempt to use PWM (LEDC) on GPIO 48 as it will cause signal corruption.
