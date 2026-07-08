# ESPHome: Better Soil Moisture Sensor

ESPHome external component for a custom **STM32-based 4-segment capacitive soil
moisture sensor** that exposes its readings over I²C.

The sensor MCU (STM32G030) does all the physics — RC-oscillator frequency
measurement, per-segment thermistor reading, dual temperature compensation
(exponential water model + linear air model) — and publishes a packed 40-byte
struct on the I²C bus. This project reads that struct and turns it into ESPHome
sensors.

## Quick start (recommended): package

One line, no sensor list to copy. The package pulls in the component, sets up
the I²C bus and all 12 entities:

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
  soil: github://ololoepepe/esphome-better-soil-moisture-sensor/soil.package.yaml@v1.1.2
```

Override only what you need via `bsm_*` substitutions (all namespaced, so they
never clash with other packages):

```yaml
substitutions:
  bsm_sda_pin: GPIO16
  bsm_scl_pin: GPIO17
  bsm_name_prefix: "Bed 1"
  bsm_log_level: WARN      # silence routine per-update state logs

packages:
  soil: github://ololoepepe/esphome-better-soil-moisture-sensor/soil.package.yaml@v1.1.2
```

| Substitution           | Default    | Description                                   |
| ---------------------- | ---------- | --------------------------------------------- |
| `bsm_sda_pin`          | `GPIO21`   | I²C SDA pin.                                  |
| `bsm_scl_pin`          | `GPIO22`   | I²C SCL pin.                                  |
| `bsm_i2c_id`           | `bsm_i2c`  | ID of the dedicated I²C bus.                  |
| `bsm_address`          | `0x44`     | Sensor I²C address.                           |
| `bsm_update_interval`  | `30s`      | Polling interval.                             |
| `bsm_name_prefix`      | `Soil`     | Prefix for all 12 entity names.               |
| `bsm_retry_count`      | `10`       | Read attempts before giving up (see below).   |
| `bsm_retry_interval`   | `30ms`     | Delay between attempts.                        |
| `bsm_log_level`        | `DEBUG`    | `sensor` log tag level; `WARN` hides spam.    |

> Pin a released tag (`@v1.1.2`) rather than `@main` so a later change to this
> repo never silently alters deployed devices.

## Reliable single-shot reads (retry)

The sensor MCU briefly disables interrupts while sampling and is unreachable for
~25–34 ms per segment, stretching SCL past the ESP-IDF 13 ms timeout cap — so a
read landing in that window fails. Between full measurement cycles the MCU idles
~500 ms.

The component retries a failed read (**non-blocking**, via a scheduler timeout —
`loop()`, API and Wi-Fi keep running) until one attempt lands in the idle
window. This matters for **wake-measure-sleep** deployments where there is only
one read per wake: without it, a collision would leave the device with stale or
no data.

Determinism: pick `retry_interval` larger than one segment window (>34 ms is
safest; the 30 ms default is fine in practice because the idle gap dominates)
and `retry_count × retry_interval` larger than one full MCU cycle (~130 ms) yet
shorter than the idle gap (~500 ms). Defaults `10 × 30 ms = 300 ms` satisfy
this; typically the 1st–2nd attempt already succeeds (idle ≈ 79% of the cycle).

## Advanced: use the component directly

For full control (shared bus, only some segments, custom names) skip the package
— see [`example.yaml`](example.yaml).

```yaml
external_components:
- source: github://ololoepepe/esphome-better-soil-moisture-sensor@v1.1.2
  components: [soil_moisture]

i2c:
  sda: GPIO21
  scl: GPIO22

sensor:
- platform: soil_moisture
  address: 0x44
  retry_count: 10
  retry_interval: 30ms
  humidity_0: {name: "Soil H0"}
  # ... only the fields you want
```

### Configuration variables

| Option           | Default | Description                                    |
| ---------------- | ------- | ---------------------------------------------- |
| `address`        | `0x44`  | I²C address.                                   |
| `update_interval`| `30s`   | Polling interval.                              |
| `retry_count`    | `10`    | Read attempts before a warning is logged.      |
| `retry_interval` | `30ms`  | Delay between attempts.                         |

Per segment (`_0` … `_3`), all optional — declare only what you need:

| Option           | Unit | Description                                    |
| ---------------- | ---- | ---------------------------------------------- |
| `humidity_N`     | %    | Compensated moisture (0–100%).                 |
| `temperature_N`  | °C   | Segment temperature.                           |
| `frequency_N`    | Hz   | Raw oscillator count (diagnostic/calibration). |
| `adc_N`          | —    | Raw thermistor ADC, 0–4095 (diagnostic).       |

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
release, bump the version in **both** the git tag and the `@vX.Y.Z` references
inside `soil.package.yaml` so the package always pulls a matching component.

## License

MIT — see [LICENSE](LICENSE).
