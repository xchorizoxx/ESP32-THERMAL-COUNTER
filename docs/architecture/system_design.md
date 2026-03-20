# System Design: Internal Concurrency & Data Flow

This document details the internal architecture of the Thermal People Counter, focusing on how it leverages the ESP32-S3's dual-core capabilities and FreeRTOS primitives for deterministic performance.

## 🧵 Concurrency Model

The system operates two primary tasks pinned to different CPU cores to prevent network overhead from interfering with time-critical vision processing.

### Task Table

| Task Name | Core | Priority | Stack Size | Responsibility |
|-----------|------|----------|------------|----------------|
| `ThermalPipe` | 1 (APP) | 24 (Max) | 6 KB (Static) | Non-blocking Sensor reading & Vision processing |
| `TelemetryTX` | 0 (PRO) | 2 | 3.5 KB (Static) | UDP Broadcast & WebSocket streaming |
| `HTTP Server` | 0 (PRO) | 5 | 16 KB | Handling Web Panel, API & Binary HUD |

## 🔄 Lifecycle Diagram

```mermaid
sequenceDiagram
    participant S as Sensor (MLX90640)
    participant P as Pipeline (Core 1)
    participant Q as FreeRTOS Queue
    participant T as Telemetry (Core 0)
    participant W as Web Client (HUD)

    loop 16 times per second
        P->>S: Get Raw Frame (I2C)
        S-->>P: 32x24 Matrix
        P->>P: Process Algorithm (5 Steps)
        P->>Q: Send IpcPacket (Count + Heatmap)
        T->>Q: Receive Packet
        T->>W: UDP/WebSocket Broadcast
    end

    W->>P: Update Config (via Web UI)
```

## 🧠 Memory Management Strategy

To ensure long-term stability in industrial/continuous environments, the system strictly follows a **Static Allocation Policy**:

1. **Static Task Buffers**: Task stacks and TCBs are pre-allocated at compile time using `xTaskCreateStatic`.
2. **Static Queues**: Communication buffers between cores are fixed size and statically allocated.
3. **No `new` or `malloc` in Loops**: Dynamic memory is only used during initialization (`app_main`). Once the system is running, the heap remains untouched.

### Why this matters?
The MLX90640 driver performs heavy floating-point math. By isolating this on Core 1 and using static memory, we eliminate the risk of "Task Starvation" or "Heap Fragmentation" causing unpredictable resets during critical counting windows.
