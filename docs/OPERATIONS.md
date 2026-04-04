# Operations and Maintenance

## Initial Deployment

### Prerequisites

- VS Code with ESP-IDF extension (v5.5 recommended)
- Python 3.6+ (for OTA script)
- USB-C to USB cable (for initial flash)
- Power supply 5V/500mA minimum

### First Flash (USB Mandatory)

1. **Configure target:**
   ```
   Ctrl+Shift+P → ESP-IDF: Set Espressif Device Target → esp32s3
   ```

2. **Full Clean:**
   ```
   Ctrl+Shift+P → ESP-IDF: Full Clean Project
   ```
   *Mandatory first time for correct OTA partitions*

3. **Flash and Monitor:**
   ```
   Click lightning icon (Build, Flash and Monitor)
   ```

4. **Verify startup:**
   - Logs should show "ThermalCounter" WiFi created
   - MLX90640 sensor initialized (or "retry via UI" if not connected)

### In-Situ Calibration

See dedicated document: [`CONFIGURATION.md`](CONFIGURATION.md)

Quick summary:
1. Connect to WiFi "ThermalCounter"
2. http://192.168.4.1 → adjust parameters
3. "Apply Settings" (test) → "Save to Flash" (persist)

## OTA Updates

Once deployed, no USB cable required for updates.

### Method 1: Python Script

```bash
# On PC connected to ThermalCounter WiFi
python scripts/ota_upload.py

# Or if ESP32 is on different network:
python scripts/ota_upload.py 192.168.1.5
```

The script:
- Automatically finds `build/DetectorPuerta.bin`
- Shows progress bar
- Verifies integrity post-flash
- Requests automatic restart

### Method 2: Web Panel

1. Connect to WiFi "ThermalCounter"
2. Open http://192.168.4.1
3. "OTA Update" panel → Select `.bin` file
4. Click "Flash Firmware"

### Anti-Bootloop Protection

The system implements automatic rollback:

| State | What it means | System action |
|-------|---------------|---------------|
| `PENDING_VERIFY` | New firmware flashed | Waits validation |
| `VALID` | Firmware confirmed functional | Marks as permanent |

**Flow:**
1. OTA flashes new version → `PENDING_VERIFY` state
2. ESP32 restarts → runs new version
3. `app_main()` verifies: WiFi OK + Sensor OK + HTTP OK
4. If all OK → `esp_ota_mark_app_valid_cancel_rollback()` → `VALID`
5. If crash before validation → bootloader automatically reverts to previous version

**Typical validation time:** 10-15 seconds post-restart.

## Health Monitoring

### Important Logs (Serial Monitor)

```
[TELEMETRY] Stack High Water Mark: 1234 words  # Sufficient memory
[TRACK_FSM] Track 5 crossed line 'Entry' -> +1 IN  # Counting working
[HTTP_SERVER] NVS: configuration saved successfully   # Persistence OK
```

### Warnings to Monitor

| Log | Meaning | Action |
|-----|---------|--------|
| `Sensor not initialized` | MLX90640 I2C failure | Check wiring, pull-ups |
| `Frame drop` (repeated) | IPC queue full | Reduce WebSocket clients, check network |
| `Stack High Water Mark < 500` | Possible stack overflow | Report, possible bug |
| `OTA partition not found` | Incorrect configuration | Full clean + rebuild |

## Preventive Maintenance

### Monthly

- Check logs for intermittent I2C errors
- Review counters (IN vs OUT difference should be reasonable)
- Verify WiFi integrity (stable connections)

### Quarterly

- Check thermal lens cleanliness (dust reduces apparent emissivity)
- Validate thermal calibration (seasonal changes)
- Review decoupling capacitors if new electrical installation nearby

### Annual

- Consider full recalibration if significant change in usage patterns
- Evaluate if seasonal thresholds require permanent adjustment

## Operational Troubleshooting

### Web Not Responding

1. Check ESP32 LED (should blink on startup)
2. Search for "ThermalCounter" network in WiFi (if not present, possible crash)
3. Reconnect serial monitor → verify boot logs
4. If bootloop: Full Clean + USB reflash

### Inconsistent Counting

1. "Radar" mode → verify clean detections
2. Adjust NMS radius according to ceiling height
3. Verify dead zones don't exclude real passage area
4. Check counting lines are correctly positioned

### Corrupt Firmware / Bootloop

The system should auto-revert, but if not:

1. Enter download mode: Hold BOOT + press RESET
2. Reflash via USB with known stable version
3. Verify `sdkconfig.defaults` has `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y`

## Key Files

| File | Role in OTA |
|------|-------------|
| `scripts/ota_upload.py` | Flash script from PC |
| `components/web_server/src/http_server.cpp` | POST `/update` handler |
| `main/main.cpp` | `esp_ota_mark_app_valid_cancel_rollback()` call |
| `sdkconfig.defaults` | Rollback + partitions configuration |
| `partitions.csv` | Layout factory + ota_0 + ota_1 |

## References

- Configuration: [`CONFIGURATION.md`](CONFIGURATION.md)
- Hardware: [`HARDWARE.md`](HARDWARE.md)
- Architecture: [`ARCHITECTURE.md`](ARCHITECTURE.md)
