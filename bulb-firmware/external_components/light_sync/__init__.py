from esphome import config_validation as cv, codegen as cg
from esphome.const import CONF_ID
from esphome.components import output

DEPENDENCIES = ["network"]  # important for host + wifi/ethernet

CONF_R_OUTPUT = "r_out"
CONF_G_OUTPUT = "g_out"
CONF_B_OUTPUT = "b_out"

CONF_SERVER_HOST = "server_host"
CONF_SERVER_PORT = "server_port"
CONF_CLIENT_ID = "client_id"


light_sync_ns = cg.esphome_ns.namespace("light_sync")
LightSync = light_sync_ns.class_("LightSync", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(LightSync),

        cv.Required(CONF_R_OUTPUT): cv.use_id(output.FloatOutput),
        cv.Required(CONF_G_OUTPUT): cv.use_id(output.FloatOutput),
        cv.Required(CONF_B_OUTPUT): cv.use_id(output.FloatOutput),

        cv.Required(CONF_SERVER_HOST): cv.string,  # hostname or IP
        cv.Optional(CONF_SERVER_PORT, default=4242): cv.port,
        cv.Required(CONF_CLIENT_ID): cv.string,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Outputs
    r_out = await cg.get_variable(config[CONF_R_OUTPUT])
    g_out = await cg.get_variable(config[CONF_G_OUTPUT])
    b_out = await cg.get_variable(config[CONF_B_OUTPUT])
    cg.add(var.set_outputs(r_out, g_out, b_out))

    cg.add(var.set_server_host(config[CONF_SERVER_HOST]))
    cg.add(var.set_server_port(config[CONF_SERVER_PORT]))
    cg.add(var.set_client_id(config[CONF_CLIENT_ID]))
