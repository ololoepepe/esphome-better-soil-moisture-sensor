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
  uint8_t b[FRAME_LEN];
  // Plain read (no register pointer): the STM32 slave transmits the whole struct.
  if (this->read(b, FRAME_LEN) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "I2C read failed (slave busy in acquisition window?)");
    this->status_set_warning();
    return;
  }
  this->status_clear_warning();

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

  for (uint8_t i = 0; i < 4; i++) {
    if (this->humidity_[i] != nullptr)
      this->humidity_[i]->publish_state(i16(0 + 2 * i) / 10.0f);     // per-mille -> %
    if (this->temperature_[i] != nullptr)
      this->temperature_[i]->publish_state(i16(8 + 2 * i) / 10.0f);  // deci-degC -> C
    if (this->frequency_[i] != nullptr)
      this->frequency_[i]->publish_state((float) u32(16 + 4 * i));   // raw count
    if (this->adc_[i] != nullptr)
      this->adc_[i]->publish_state((float) u16(32 + 2 * i));         // raw ADC
  }
}

void SoilMoisture::dump_config() {
  ESP_LOGCONFIG(TAG, "Better Soil Moisture Sensor (STM32 I2C slave):");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  for (uint8_t i = 0; i < 4; i++) {
    LOG_SENSOR("  ", "Humidity", this->humidity_[i]);
    LOG_SENSOR("  ", "Temperature", this->temperature_[i]);
    LOG_SENSOR("  ", "Frequency", this->frequency_[i]);
    LOG_SENSOR("  ", "ADC", this->adc_[i]);
  }
}

}  // namespace soil_moisture
}  // namespace esphome
