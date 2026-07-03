import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_TEMPERATURE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
)

CODEOWNERS = ["@ololoepepe"]
DEPENDENCIES = ["i2c"]

soil_moisture_ns = cg.esphome_ns.namespace("soil_moisture")
SoilMoisture = soil_moisture_ns.class_(
    "SoilMoisture", cg.PollingComponent, i2c.I2CDevice
)

HUMIDITY = [f"humidity_{i}" for i in range(4)]
TEMPERATURE = [f"temperature_{i}" for i in range(4)]
FREQUENCY = [f"frequency_{i}" for i in range(4)]
ADC = [f"adc_{i}" for i in range(4)]

_schema = {cv.GenerateID(): cv.declare_id(SoilMoisture)}

for _k in HUMIDITY:
    _schema[cv.Optional(_k)] = sensor.sensor_schema(
        unit_of_measurement="%",
        icon="mdi:water-percent",
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_HUMIDITY,
        state_class=STATE_CLASS_MEASUREMENT,
    )
for _k in TEMPERATURE:
    _schema[cv.Optional(_k)] = sensor.sensor_schema(
        unit_of_measurement="\u00b0C",
        icon="mdi:thermometer",
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    )
for _k in FREQUENCY:
    _schema[cv.Optional(_k)] = sensor.sensor_schema(
        unit_of_measurement="Hz",
        icon="mdi:sine-wave",
        accuracy_decimals=0,
        state_class=STATE_CLASS_MEASUREMENT,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    )
for _k in ADC:
    _schema[cv.Optional(_k)] = sensor.sensor_schema(
        accuracy_decimals=0,
        state_class=STATE_CLASS_MEASUREMENT,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    )

CONFIG_SCHEMA = (
    cv.Schema(_schema)
    .extend(cv.polling_component_schema("30s"))
    .extend(i2c.i2c_device_schema(0x44))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    setters = [
        ("set_humidity", HUMIDITY),
        ("set_temperature", TEMPERATURE),
        ("set_frequency", FREQUENCY),
        ("set_adc", ADC),
    ]
    for setter, keys in setters:
        for i, key in enumerate(keys):
            if key in config:
                sens = await sensor.new_sensor(config[key])
                cg.add(getattr(var, setter)(i, sens))
