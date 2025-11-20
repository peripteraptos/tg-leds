from esphome import config_validation as cv, codegen as cg
from esphome.const import CONF_ID

from esphome.components import output

DEPENDENCIES = ["wifi", "output"]

sync_sequence_light_ns = cg.esphome_ns.namespace("sync_sequence_light")
SyncSequenceLight = sync_sequence_light_ns.class_("SyncSequenceLight", cg.Component)

CONF_R_OUTPUT = "r_output"
CONF_G_OUTPUT = "g_output"
CONF_B_OUTPUT = "b_output"
CONF_SERVER_HOST = "server_host"
CONF_SERVER_PORT = "server_port"
CONF_SYNC_PORT = "sync_port"
CONF_UNIQUE_ID = "unique_id"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SyncSequenceLight),

            cv.Required(CONF_R_OUTPUT): cv.use_id(output.FloatOutput),
            cv.Required(CONF_G_OUTPUT): cv.use_id(output.FloatOutput),
            cv.Required(CONF_B_OUTPUT): cv.use_id(output.FloatOutput),

            cv.Optional(CONF_SERVER_HOST, default="light-sequencer.local"): cv.string_strict,
            cv.Optional(CONF_SERVER_PORT, default=9000): cv.port,
            cv.Optional(CONF_SYNC_PORT, default=9001): cv.port,
            cv.Optional(CONF_UNIQUE_ID): cv.string_strict,
        }
    ).extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Outputs
    r_out = await cg.get_variable(config[CONF_R_OUTPUT])
    g_out = await cg.get_variable(config[CONF_G_OUTPUT])
    b_out = await cg.get_variable(config[CONF_B_OUTPUT])
    cg.add(var.set_outputs(r_out, g_out, b_out))

    # Server config
    cg.add(var.set_server_host(config[CONF_SERVER_HOST]))
    cg.add(var.set_server_port(config[CONF_SERVER_PORT]))
    cg.add(var.set_sync_port(config[CONF_SYNC_PORT]))

    # Optional fixed unique id
    if CONF_UNIQUE_ID in config:
        cg.add(var.set_unique_id(config[CONF_UNIQUE_ID]))
