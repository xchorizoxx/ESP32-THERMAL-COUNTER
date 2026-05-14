# Hardware Specifications

## Electrical Connections

### MLX90640 → ESP32-S3

| MLX90640 | ESP32-S3 | Note |
|----------|----------|------|
| VCC | 3.3V | Stable LDO required. 23mA peak sensor + 150mA peak WiFi |
| GND | GND | Short return path, low impedance |
| SDA | GPIO 8 | Pull-up 1kΩ - 2.2kΩ mandatory for 400kHz |
| SCL | GPIO 9 | Pull-up 1kΩ - 2.2kΩ mandatory for 400kHz |

### Decoupling Capacitors (Recommended)

For cables >15cm between ESP32 and sensor:
- 100nF ceramic (fast) between VCC-GND near sensor
- 10uF electrolytic/tantalum (slow) in parallel

### Current Consumption

| Component | Typical Current | Peak Current |
|-----------|-----------------|--------------|
| MLX90640 | 23mA | 25mA |
| ESP32-S3 (WiFi active) | 80mA | 150mA |
| MicroSD Card (Writing) | 15mA | 50mA |
| DS3231 RTC | <1mA | 1mA |
| **Total system** | **~135mA** | **~225mA** |

## Sensor Characteristics

**Melexis MLX90640**
- Type: Infrared thermopile array
- Resolution: 32 columns × 24 rows = 768 pixels
- Field of View (FOV): 110° × 75° (standard version)
- Temperature range: -40°C to +300°C
- Accuracy: ±1.5°C typical (±3°C maximum)
- NETD (noise): <100mK typical @ 1Hz
- Maximum frequency: 64 Hz (16 Hz used in this project)
- Interface: I2C (100kHz - 1MHz, Fast Mode+ supported)

## I2C Considerations

### Pull-ups

Critical pull-up resistances for clean signals at 400kHz:

| Pull-up | Rise Time | Suitable for |
|---------|-----------|---------------|
| 4.7kΩ | ~500ns | 100kHz (Standard) |
| 2.2kΩ | ~250ns | 400kHz (Fast) |
| 1.0kΩ | ~120ns | 1MHz (Fast Mode+) |

Slow rise time causes data corruption and intermittent I2C errors.

### Clock Speed

Project configured at **400kHz (Fast Mode)**:
- Balance between speed and reliability
- MLX90640 frame transfer (~2KB) in ~40ms
- Compatible with typical 10-20cm wiring

## ESP32-S3 Pins (Summary)

| GPIO | Function | ESP32-S3 Pin | Peripheral | Function |
| :--- | :--- | :--- |
| GPIO 8 | MLX90640 | I2C0 SDA | Thermal Sensor |
| GPIO 9 | MLX90640 | I2C0 SCL | Thermal Sensor |
| GPIO 1 | DS3231 | I2C1 SDA | Real Time Clock |
| GPIO 2 | DS3231 | I2C1 SCL | Real Time Clock |
| GPIO 11 | MicroSD | SPI2 MOSI | Storage |
| GPIO 13 | MicroSD | SPI2 MISO | Storage |
| GPIO 12 | MicroSD | SPI2 SCK | Storage |
| GPIO 14 | MicroSD | SPI2 CS | Storage |
| GPIO 0 | BOOT Button | Hold 2s for USB Network Activation |
| GPIO 48 | RGB LED | WS2812 Status |
| 5V / GND | ALL | Shared Power Supply |

---

## Status LED (RGB)
The ESP32-S3 built-in NeoPixel (GPIO 48) provides real-time feedback of the system state:
- **Blue**: System booting and hardware initialization.
- **Green**: Operating mode. WiFi SoftAP and Thermal Vision are active.
- **Purple**: **USB Network Mode** is currently enabled (192.168.4.1 / 192.168.7.1).

## Physical Controls
- **BOOT Button (GPIO 0)**: 
  - To enable the USB network interface (RNDIS/ECM), hold the BOOT button for **2.0 seconds** until the LED turns purple. 
  - This mode allows you to view the web dashboard via a USB cable connected to your PC, without disconnecting from your local WiFi.

### RTC (DS3231)
- Dedicated I2C1 bus (GPIO 1/2) to avoid clock-stretching issues from the sensor
- Address: 0x68
- Battery backup (CR1220) for time persistence during power loss

### MicroSD (SPI)
- Bus: SPI2 (Shared with future peripherals if needed)
- MOSI: GPIO 11
- MISO: GPIO 13  
- SCK: GPIO 12
- CS: GPIO 14
- Formatted as FAT32 (up to 32GB supported natively)
- Mount point: `/sdcard`

### OV2640 Camera (Optional)
- Not implemented in current version
- Would require PSRAM and second parallel bus
- Consult roadmap if needed

## Mechanical Installation

### Mounting Height

| Height | Coverage Area | Resolution per Person | Use |
|--------|---------------|-------------------------|-----|
| 2.5m | 4.8m × 3.3m | ~6×4 pixels | Narrow corridors |
| 3.6m (std) | 7.0m × 4.8m | ~4×3 pixels | Standard door |
| 5.0m | 9.7m × 6.7m | ~3×2 pixels | Wide lobbies |

### Inclination Angle

**Vertical** mounting recommended (perpendicular to floor):
- Minimal geometric distortion
- Entry/exit symmetry
- Easy line calibration

Incline only if specific lateral coverage is required.

## References

- MLX90640 Datasheet: `docs/reference/MLX90640_Datasheet.md`
- ESP32-S3 TRM: [Espressif Technical Reference](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/i2c.html)
