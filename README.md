# ESPHome: Better Soil Moisture Sensor

ESPHome external component for a custom **STM32-based 4-segment capacitive soil
moisture sensor** that exposes its readings over I²C.

The sensor MCU (STM32G030) does all the physics — RC-oscillator frequency
measurement, per-segment thermistor reading, dual temperature compensation
(exponential water model + linear air model) — and publishes a packed 40-byte
struct on the I²C bus. This component reads that struct and turns it into
ESPHome sensors.

## Usage

No files to copy — reference it straight from GitHub:

```yaml
external_components:
  - source: github://ololoepepe/esphome-better-soil-moisture-sensor@main
    components: [soil_moisture]

i2c:
  sda: GPIO21
  scl: GPIO22

sensor:
  - platform: soil_moisture
    address: 0x44
    humidity_0: {name: "Soil H0"}
    # ... only the fields you want
```

See [`example.yaml`](example.yaml) for a full configuration.

> **Production tip:** pin a released tag (`@v1.0.0`) instead of `@main` so a
> later change to this repo never silently alters deployed devices.

## Configuration variables

Each of the four segments (`_0` … `_3`) exposes up to four optional sensors.
Declare only what you need.

| Option           | Unit | Description                                    |
| ---------------- | ---- | ---------------------------------------------- |
| `humidity_N`     | %    | Compensated moisture (0–100%).                 |
| `temperature_N`  | °C   | Segment temperature.                           |
| `frequency_N`    | Hz   | Raw oscillator count (diagnostic/calibration). |
| `adc_N`          | —    | Raw thermistor ADC, 0–4095 (diagnostic).       |

Plus the standard `id`, `address` (default `0x44`), `update_interval`
(default `30s`), and per-sensor options (`name`, `filters`, `id`, …).

## Wiring

4-wire I²C. Power the sensor board from **5 V** (its onboard AP2112K LDO needs
input headroom above 3.3 V); a common ground with the ESP32 is required. I²C
pull-ups are on the sensor board — do not add more, and do not enable the
ESP32's internal pull-ups.

| Sensor | ESP32          |
| ------ | -------------- |
| VCC    | 5V / VIN       |
| GND    | GND            |
| SDA    | GPIO21         |
| SCL    | GPIO22         |

## Data frame (for reference)

40 bytes, little-endian, `#pragma pack(1)`:

| Offset | Type        | Field            | Encoding             |
| ------ | ----------- | ---------------- | -------------------- |
| 0–7    | int16 × 4   | `humidity[4]`    | per-mille (÷10 = %)  |
| 8–15   | int16 × 4   | `temperature[4]` | deci-°C (÷10 = °C)   |
| 16–31  | uint32 × 4  | `raw_freq[4]`    | edge count / window  |
| 32–39  | uint16 × 4  | `raw_adc[4]`     | 0–4095               |

## License

MIT — see [LICENSE](LICENSE).
