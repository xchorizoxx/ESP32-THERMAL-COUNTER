# Task: Integration and Boot System

## Objective
Unite all components in `app_main()` and manage the system lifecycle.

## Technical Requirements
1. **Boot Order**: NVS → WiFi → Queues → Pipeline → Web Server.
2. **Core Distribution**: Ensure intensive tasks are pinned (`xTaskCreatePinnedToCore`) to the correct core.
3. **Watchdogs**: Activate WDT on both main tasks.
4. **OTA Validation**: At the end of a successful boot, mark the firmware as "Valid" to cancel rollback.
5. **Statistics**: Periodically monitor RAM and CPU cycles.
