# brightsign_sync/__init__.py
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import udp
from esphome.components.udp import CONF_UDP_ID
from esphome.const import CONF_ID
from esphome.cpp_types import Component, Parented
from esphome.const import CONF_ID, CONF_PORT, CONF_ADDRESS

# CONF_T_SENSOR = "t_sensor"
# CONF_X_SENSOR = "x_sensor"
AUTO_LOAD = ["json", "udp"]

DEPENDENCIES = ["udp", "mdns"]


brightsign_ns = cg.esphome_ns.namespace("brightsign_sync")
BrightsignSyncComponent = brightsign_ns.class_(
    "BrightsignSyncComponent",
    Component,
    Parented.template(
        udp.UDPComponent
    ),  # inherit from UDPComponent instead of plain Component
)

CONFIG_SCHEMA = udp.UDP_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(BrightsignSyncComponent),
        cv.Optional(CONF_PORT, default=1539): cv.port,
        # cv.Optional(CONF_ADDRESS, default="224.0.126.10"): cv.string,
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_UDP_ID])
    # cg.add(parent.add_address(config[CONF_ADDRESS]))

    var = cg.new_Pvariable(config[CONF_ID])

    await cg.register_component(var, config)
    await cg.register_parented(var, parent)

    cg.add(parent.set_listen_port(config[CONF_PORT]))
    # cg.add(parent.set_listen_address(config[CONF_ADDRESS]))
    cg.add(parent.set_should_listen())
