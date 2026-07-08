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
  soil: github://ololoepepe/esphome-better-soil-moisture-sensor/soil.package.yaml@v1.1.3
```

Override only what you need via `bsm_*` substitutions (all namespaced, so they
never clash with other packages):

```yaml
substitutions:
  bsm_sda_pin: GPIO16
  bsm_scl_pin: GPIO17
  bsm_name_prefix: "Bed 1"
  bsm_logger_level: INFO       # show a readings line per update

packages:
  soil: github://ololoepepe/esphome-better-soil-moisture-sensor/soil.package.yaml@v1.1.3
```

| Substitution           | Default    | Description                                   |
| ---------------------- | ---------- | --------------------------------------------- |
| `bsm_id`               | `bsm_soil` | Platform id (for `component.update:`).        |
| `bsm_sda_pin`          | `GPIO21`   | I²C SDA pin.                                  |
| `bsm_scl_pin`          | `GPIO22`   | I²C SCL pin.                                  |
| `bsm_i2c_id`           | `bsm_i2c`  | ID of the dedicated I²C bus.                  |
| `bsm_address`          | `0x44`     | Sensor I²C address.                           |
| `bsm_update_interval`  | `30s`      | Polling interval, or `never` (event-driven).  |
| `bsm_name_prefix`      | `Soil`     | Prefix for all 12 entity names.               |
| `bsm_retry_count`      | `10`       | Read attempts before giving up.               |
| `bsm_retry_interval`   | `30ms`     | Delay between attempts.                        |
| `bsm_logger_level`     | `ERROR`    | Device log verbosity (see Logging).           |

> Pin a released tag (`@v1.1.3`) rather than `@main` so a later change to this
> repo never silently alters deployed devices.

## Event-driven / low-power (wake-measure-sleep)

For battery nodes that wake, read once, and sleep, do not drive measurements off
a background timer — set the poll to `never` and trigger a read yourself. This
also makes the very first reading happen immediately, not after one interval.

```yaml
substitutions:
  bsm_update_interval: never

packages:
  soil: github://ololoepepe/esphome-better-soil-moisture-sensor/soil.package.yaml@v1.1.3

esphome:
  on_boot:
    priority: -100
    then:
    - component.update: bsm_soil     # the only measurement trigger

deep_sleep:
  sleep_duration: 30min                # THIS sets the measurement period
```

The retry logic guarantees this single read returns fresh data even if it
collides with the MCU's sampling window. If powering the sensor via a high-side
MOSFET, enable it in `on_boot` before the read and allow ~0.6-0.7 s for the MCU
to boot and take its first measurement.

## Reliable single-shot reads (retry)

The sensor MCU briefly disables interrupts while sampling and is unreachable for
~25-34 ms per segment, stretching SCL past the ESP-IDF 13 ms timeout cap — so a
read landing in that window fails. Between full measurement cycles the MCU idles
~500 ms.

The component retries a failed read (**non-blocking**, via a scheduler timeout —
`loop()`, API and Wi-Fi keep running) until one attempt lands in the idle
window. Determinism: `retry_count × retry_interval` must exceed one full MCU
cycle (~130 ms) yet stay under the idle gap (~500 ms). Defaults `10 × 30 ms =
300 ms` satisfy this; typically the 1st-2nd attempt already succeeds (idle ≈ 79%
of the cycle). If all attempts fail, an ERROR is logged and the entity is marked
failed for that cycle.

## Logging

`bsm_logger_level` (package) / `logger: level:` (direct) is a **compile-time
ceiling** — messages above it are stripped from the firmware, so raising it
needs a reflash. Sensor values reach Home Assistant over the API regardless of
log level; these settings only affect the console/UART log.

| Level          | What you see in the log                                    |
| -------------- | --------------------------------------------------------- |
| `ERROR` (def.) | Only genuine faults (e.g. sensor unreachable after retries). |
| `INFO`         | + one consolidated readings line per update.              |
| `DEBUG`        | + internals, incl. the expected transient IDF i2c timeouts during retries. |

The expected per-attempt "I2C hardware timeout" messages come from ESP-IDF and
sit at DEBUG, so they never appear unless you explicitly build at DEBUG.

## Advanced: use the component directly

See [`example.yaml`](example.yaml) for the full direct form (shared bus, subset
of segments, custom names).

### Configuration variables

| Option           | Default | Description                                    |
| ---------------- | ------- | ---------------------------------------------- |
| `address`        | `0x44`  | I²C address.                                   |
| `update_interval`| `30s`   | Polling interval, or `never`.                  |
| `retry_count`    | `10`    | Read attempts before an error is logged.       |
| `retry_interval` | `30ms`  | Delay between attempts.                         |

Per segment (`_0` … `_3`), all optional:

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
inside `soil.package.yaml`.

## License

MIT — see [LICENSE](LICENSE).
