import esphome.codegen as cg
import esphome.config_validation as cv

ds3231_ns = cg.esphome_ns.namespace("ds3231")

DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["time"]
