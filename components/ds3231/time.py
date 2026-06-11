import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TIME_ID, CONF_UPDATE_INTERVAL
from esphome.components import time, i2c

CONF_I2C_ADDRESS = "i2c_address"
CONF_READ_INTERVAL = "read_interval"

DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["time"]

ds3231_ns = cg.esphome_ns.namespace("ds3231")
DS3231 = ds3231_ns.class_("DS3231", time.RealTimeClock, i2c.I2CDevice)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(DS3231),
        cv.Optional(CONF_I2C_ADDRESS, default=0x68): cv.int_range(0x00, 0xFF),
        cv.Optional(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
        cv.Optional(CONF_UPDATE_INTERVAL, default="15min"): cv.positive_time_period_minutes,
        cv.Optional(CONF_READ_INTERVAL, default="1min"): cv.positive_time_period_minutes,
    }
).extend(cv.COMPONENT_SCHEMA).extend(i2c.i2c_device_schema(0x68))

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if CONF_TIME_ID in config:
        reftime = await cg.get_variable(config[CONF_TIME_ID])
        cg.add(var.set_reftime(reftime))

    if CONF_UPDATE_INTERVAL in config:
        cg.add(var.set_update_interval_min(config[CONF_UPDATE_INTERVAL]))

    if CONF_READ_INTERVAL in config:
        cg.add(var.set_read_interval_min(config[CONF_READ_INTERVAL]))
