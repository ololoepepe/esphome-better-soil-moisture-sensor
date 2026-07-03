# ESPHome: Better Soil Moisture Sensor

ESPHome external component for a custom **STM32-based 4-segment capacitive soil
moisture sensor** that exposes its readings over I²C.

The sensor MCU (STM32G030) does all the physics — RC-oscillator frequency
measurement, per-segment thermistor reading, dual temperature compensation
(exponential water model + linear air model) — and publishes a packed 40-byte
struct on the I²C bus. This project reads that struct and turns it into ESPHome
sensors.

## Quick start (recommended): package

The easiest way — one line, no sensor list to copy. The package pulls in the
component, sets up the I²C bus and all 12 entities for you:

```yaml
esphome:
  name: my-soil-sensor
esp32:
  board: esp32dev
  framework: {type: esp-idf}
wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password
api:
ota: {platform: esphome}

packages:
  soil: github://ololoepepe/esphome-better-soil-moisture-sensor/soil.package.yaml@v1.1.0
```

Override only what you need via `bsm_*` substitutions (all namespaced, so they
never clash with other packages):

```yaml
substitutions:
  bsm_sda_pin: GPIO16
  bsm_scl_pin: GPIO17
  bsm_name_prefix: "Bed 1"

packages:
  soil: github://ololoepepe/esphome-better-soil-moisture-sensor/soil.package.yaml@v1.1.0
```

| Substitution           | Default    | Description                          |
| ---------------------- | ---------- | ------------------------------------ |
| `bsm_sda_pin`          | `GPIO21`   | I²C SDA pin.                         |
| `bsm_scl_pin`          | `GPIO22`   | I²C SCL pin.                         |
| `bsm_i2c_id`           | `bsm_i2c`  | ID of the dedicated I²C bus.         |
| `bsm_address`          | `0x44`     | Sensor I²C address.                  |
| `bsm_update_interval`  | `30s`      | Polling interval.                    |
| `bsm_name_prefix`      | `Soil`     | Prefix for all 12 entity names.      |

> Pin a released tag (`@v1.1.0`) rather than `@main` so a later change to this
> repo never silently alters deployed devices.

## Advanced: use the component directly

If you need full control (custom bus sharing, only some segments, non-standard
names), skip the package and wire up the platform yourself. See
[`example.yaml`](example.yaml).

```yaml
external_components:
- source: github://ololoepepe/esphome-better-soil-moisture-sensor@v1.1.0
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

### Configuration variables

Each of the four segments (`_0` … `_3`) exposes up to four optional sensors.
Declare only what you need.

| Option           | Unit | Description                                    |
| ---------------- | ---- | ---------------------------------------------- |
| `humidity_N`     | %    | Compensated moisture (0–100%).                 |
| `temperature_N`  | °C   | Segment temperature.                           |
| `frequency_N`    | Hz   | Raw oscillator count (diagnostic/calibration). |
| `adc_N`          | —    | Raw thermistor ADC, 0–4095 (diagnostic).       |

Plus the standard `id`, `i2c_id`, `address` (default `0x44`), `update_interval`
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

## Versioning

Component and package are released together under the same tag. When cutting a
release, bump the version in **both** the git tag and the `@vX.Y.Z` reference
inside `soil.package.yaml` so the package always pulls a matching component.

## License

MIT — see [LICENSE](LICENSE).
