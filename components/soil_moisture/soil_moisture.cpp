#include "soil_moisture.h"
#include "esphome/core/log.h"

namespace esphome {
namespace soil_moisture {

static const char *const TAG = "soil_moisture";

// On-wire layout (little-endian, matches the STM32 #pragma pack(1) struct):
//   [ 0.. 7] int16  humidity[4]     per-mille, 0..1000
//   [ 8..15] int16  temperature[4]  deci-degC, e.g. 274 = 27.4 C
//   [16..31] uint32 raw_freq[4]     edge count over a fixed window
//   [32..39] uint16 raw_adc[4]      0..4095
static const uint8_t FRAME_LEN = 40;

void SoilMoisture::update() {
  // Start a read, retrying on I2C failure. The sensor MCU disables IRQs while
  // sampling (~25-34 ms per segment) and stretches SCL past the ESP-IDF 13 ms
  // cap, so a read landing there fails. Between full measurement cycles the MCU
  // idles ~500 ms. Retrying with an interval > one segment window, across a span
  // > one full MCU cycle, is guaranteed to hit that idle window -> fresh data,
  // never a silently skipped reading. Retries are non-blocking (set_timeout).
  this->attempt_(this->retry_count_);
}

void SoilMoisture::attempt_(uint8_t tries_left) {
  if (this->try_read_()) {
    this->status_clear_warning();
    return;
  }
  if (tries_left > 1) {
    this->set_timeout("bsm_retry", this->retry_interval_ms_,
                      [this, tries_left]() { this->attempt_(tries_left - 1); });
  } else {
    // All retries exhausted: the sensor is genuinely unreachable. This is a real
    // fault (unlike the expected per-attempt IDF timeouts, which are DEBUG), so
    // log it at ERROR — visible even at the quiet default log level.
    ESP_LOGE(TAG, "I2C read failed after %u attempts", this->retry_count_);
    this->status_set_warning();
  }
}

bool SoilMoisture::try_read_() {
  uint8_t b[FRAME_LEN];
  if (this->read(b, FRAME_LEN) != i2c::ERROR_OK)
    return false;

  auto i16 = [&](uint8_t o) -> int16_t {
    return (int16_t) ((uint16_t) b[o] | ((uint16_t) b[o + 1] << 8));
  };
  auto u16 = [&](uint8_t o) -> uint16_t {
    return (uint16_t) ((uint16_t) b[o] | ((uint16_t) b[o + 1] << 8));
  };
  auto u32 = [&](uint8_t o) -> uint32_t {
    return (uint32_t) b[o] | ((uint32_t) b[o + 1] << 8) | ((uint32_t) b[o + 2] << 16) |
           ((uint32_t) b[o + 3] << 24);
  };

  float hum[4], tmp[4];
  uint32_t frq[4];
  uint16_t adc[4];
  for (uint8_t i = 0; i < 4; i++) {
    hum[i] = i16(0 + 2 * i) / 10.0f;   // per-mille -> %
    tmp[i] = i16(8 + 2 * i) / 10.0f;   // deci-degC -> C
    frq[i] = u32(16 + 4 * i);
    adc[i] = u16(32 + 2 * i);
  }

  for (uint8_t i = 0; i < 4; i++) {
    if (this->humidity_[i] != nullptr)
      this->humidity_[i]->publish_state(hum[i]);
    if (this->temperature_[i] != nullptr)
      this->temperature_[i]->publish_state(tmp[i]);
    if (this->frequency_[i] != nullptr)
      this->frequency_[i]->publish_state((float) frq[i]);
    if (this->adc_[i] != nullptr)
      this->adc_[i]->publish_state((float) adc[i]);
  }

  // Consolidated data line at INFO under our own tag (the core per-sensor logs
  // are DEBUG). Hidden at the default quiet level; the data still reaches Home
  // Assistant over the API regardless of log verbosity.
  ESP_LOGI(TAG, "H %.1f/%.1f/%.1f/%.1f %%  T %.1f/%.1f/%.1f/%.1f C  F %u/%u/%u/%u",
           hum[0], hum[1], hum[2], hum[3], tmp[0], tmp[1], tmp[2], tmp[3],
           (unsigned) frq[0], (unsigned) frq[1], (unsigned) frq[2], (unsigned) frq[3]);
  return true;
}

void SoilMoisture::dump_config() {
  ESP_LOGCONFIG(TAG, "Better Soil Moisture Sensor (STM32 I2C slave):");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG, "  Retries: %u x %u ms", this->retry_count_, this->retry_interval_ms_);
  for (uint8_t i = 0; i < 4; i++) {
    LOG_SENSOR("  ", "Humidity", this->humidity_[i]);
    LOG_SENSOR("  ", "Temperature", this->temperature_[i]);
    LOG_SENSOR("  ", "Frequency", this->frequency_[i]);
    LOG_SENSOR("  ", "ADC", this->adc_[i]);
  }
}

}  // namespace soil_moisture
}  // namespace esphome
