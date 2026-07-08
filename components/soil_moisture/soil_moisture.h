#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace soil_moisture {

class SoilMoisture : public PollingComponent, public i2c::I2CDevice {
 public:
  void set_humidity(uint8_t ch, sensor::Sensor *s) { this->humidity_[ch] = s; }
  void set_temperature(uint8_t ch, sensor::Sensor *s) { this->temperature_[ch] = s; }
  void set_frequency(uint8_t ch, sensor::Sensor *s) { this->frequency_[ch] = s; }
  void set_adc(uint8_t ch, sensor::Sensor *s) { this->adc_[ch] = s; }
  void set_retry_count(uint8_t n) { this->retry_count_ = n; }
  void set_retry_interval(uint16_t ms) { this->retry_interval_ms_ = ms; }

  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  // Single read+decode+publish attempt. Returns false only on I2C bus error.
  bool try_read_();
  // Non-blocking retry chain: on failure, reschedules itself via set_timeout.
  void attempt_(uint8_t tries_left);

  sensor::Sensor *humidity_[4]{nullptr, nullptr, nullptr, nullptr};
  sensor::Sensor *temperature_[4]{nullptr, nullptr, nullptr, nullptr};
  sensor::Sensor *frequency_[4]{nullptr, nullptr, nullptr, nullptr};
  sensor::Sensor *adc_[4]{nullptr, nullptr, nullptr, nullptr};

  uint8_t retry_count_{10};
  uint16_t retry_interval_ms_{30};
};

}  // namespace soil_moisture
}  // namespace esphome
