# Task: Telemetry and Communications

## Objective
Manage network threads (WiFi, UDP, WebSocket) on Core 0.

## Technical Requirements
1. **SoftAP**: Configure the ESP32 as an Access Point named `ThermalCounter`.
2. **Event Loop**: Handle WiFi events asynchronously.
3. **Broadcaster**: A task that reads the frame queue from Core 1 and emits binary UDP packets to the broadcast network (`255.255.255.255`).
4. **Resilience**: If the connection is lost or the network stack becomes saturated, the system must recover without affecting thermal detection.
