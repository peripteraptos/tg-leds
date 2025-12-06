# brightsign_sync/__init__.py
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import udp
from esphome.const import CONF_ID
from esphome.cpp_types import Component
from esphome.const import CONF_ID, CONF_PORT

AUTO_LOAD = ["json"]


my_udp_ns = cg.esphome_ns.namespace("my_udp")
MyUDPComponent = my_udp_ns.class_(
    "MyUDPComponent",
    Component,
)

CONFIG_SCHEMA = udp.UDP_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(MyUDPComponent),
        cv.Optional(CONF_PORT, default=1234): cv.port,
        # cv.Optional(CONF_ADDRESS, default="224.0.126.10"): cv.string,
    }
)


async def to_code(config):
    # parent = await cg.get_variable(config[CONF_UDP_ID])
    # cg.add(parent.add_address(config[CONF_ADDRESS]))

    var = cg.new_Pvariable(config[CONF_ID])

    await cg.register_component(var, config)

    # cg.add(parent.set_listen_port(config[CONF_PORT]))
    # cg.add(parent.set_listen_address(config[CONF_ADDRESS]))
    # cg.add(parent.set_should_listen())
