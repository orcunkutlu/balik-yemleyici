import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import display
from esphome.const import CONF_ID

# Dependency
DEPENDENCIES = ["spi"]

# Namespace
fishmaster_ui_ns = cg.esphome_ns.namespace("fishmaster_ui")
FishMasterUI = fishmaster_ui_ns.class_(
    "FishMasterUI", display.DisplayBuffer, cg.PollingComponent
)

# ===== DOĞRU SCHEMA: FULL_DISPLAY_SCHEMA =====
CONFIG_SCHEMA = display.FULL_DISPLAY_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(FishMasterUI),
}).extend(cv.polling_component_schema("1s"))

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await display.register_display(var, config)
