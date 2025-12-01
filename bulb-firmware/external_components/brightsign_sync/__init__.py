# brightsign_sync/__init__.py
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor, udp
from esphome.const import CONF_ID

# CONF_T_SENSOR = "t_sensor"
# CONF_X_SENSOR = "x_sensor"
AUTO_LOAD = ["udp", "json"]

brightsign_ns = cg.esphome_ns.namespace("brightsign_sync")
BrightsignSyncComponent = brightsign_ns.class_(
    "BrightsignSyncComponent",
    udp.UDPComponent,  # inherit from UDPComponent instead of plain Component
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BrightsignSyncComponent)
    }
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    # Register as a normal component (UDPComponent is already a Component subclass)
    await cg.register_component(var, config)
