import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import text, uartex
from esphome.const import CONF_ID
from .. import uartex_ns, cmd_t, \
    _uartex_declare_type
from ..const import CONF_COMMAND_TEXT, CONF_UARTEX_ID

DEPENDENCIES = ['uartex']
UARTExText = uartex_ns.class_('UARTExText', text.Text, cg.PollingComponent)

CONFIG_SCHEMA = cv.All(text.TEXT_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(UARTExText),
    cv.GenerateID(CONF_UARTEX_ID): _uartex_declare_type,
    cv.Required(CONF_COMMAND_TEXT): cv.returning_lambda,
}).extend(cv.polling_component_schema('60s')))

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await text.register_text(var, config)
    await uartex.register_uartex_device(var, config)
    
    templ = await cg.templatable(config[CONF_COMMAND_TEXT], [(cg.std_string.operator('const'), 'str')], cmd_t)
    cg.add(var.set_command_text(templ))

    
