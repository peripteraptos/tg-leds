import esphome.codegen as cg
import esphome.config_validation as cv

from esphome.components.light.effects import register_rgb_effect
from esphome.components.light.types import LightEffect
from esphome.const import (
    CONF_ID,
    CONF_PORT,
    CONF_NAME,
)

AUTO_LOAD = ["socket"]
DEPENDENCIES = ["network", "light"]

light_sync_ns = cg.esphome_ns.namespace("light_sync")
LightSyncComponent = light_sync_ns.class_("LightSyncComponent", cg.Component)
LightSyncEffect = light_sync_ns.class_("LightSyncEffect", LightEffect)

CONF_HOST = "host"
CONF_CLIENT_ID = "client_id"
CONF_LIGHT_SYNC_ID = "light_sync_id"

# ----- Component schema -----
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(LightSyncComponent),
        cv.Required(CONF_HOST): cv.string,
        cv.Required(CONF_PORT): cv.port,
        cv.Optional(CONF_CLIENT_ID): cv.string,
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_server_host(config[CONF_HOST]))
    cg.add(var.set_server_port(config[CONF_PORT]))

    if CONF_CLIENT_ID in config:
        cg.add(var.set_client_id(config[CONF_CLIENT_ID]))
    else:
        # default client id: node name
        cg.add(var.set_client_id(cg.RawExpression("App.get_name()")))


# ----- Effect schema & registration -----
@register_rgb_effect(
    "light_sync",            # YAML key under effects
    LightSyncEffect,
    "Light Sync",            # Default effect name in UI
    {
        cv.GenerateID(CONF_LIGHT_SYNC_ID): cv.use_id(LightSyncComponent),
    },
)
async def light_sync_effect_to_code(config, effect_id):
    parent = await cg.get_variable(config[CONF_LIGHT_SYNC_ID])
    var = cg.new_Pvariable(effect_id, config[CONF_NAME])
    cg.add(var.set_light_sync(parent))
    return var
