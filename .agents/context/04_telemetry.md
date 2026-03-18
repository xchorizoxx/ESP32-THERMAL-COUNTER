# Tarea: Telemetría y Comunicaciones

## Objetivo
Gestionar los hilos de red (WiFi, UDP, WebSocket) en el Core 0.

## Requisitos Técnicos
1. **SoftAP**: Configurar el ESP32 como Access Point `ThermalCounter`.
2. **Event Loop**: Manejar eventos de WiFi de forma asíncrona.
3. **Broadcaster**: Tarea que lee la cola de frames del Core 1 y emite paquetes UDP binarios a la red broadcast (`255.255.255.255`).
4. **Resilencia**: Si se pierde la conexión o el stack de red se satura, el sistema debe recuperarse sin afectar la detección térmica.
