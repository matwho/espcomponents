[![latest version](https://img.shields.io/github/release/eigger/espcomponents?display_name=tag&include_prereleases&label=latest%20version)](https://github.com/eigger/espcomponents/releases)
# Read and Parse UART Data in ESPHome


ESPHome has been able to send data to a device via the UART using the [ESPHome UART Bus component](https://esphome.io/components/uart.html). However, reading data sent back from the device has been complicated. This either involved creating a custom component (which is now [not possible](https://esphome.io/guides/contributing.html#a-note-about-custom-components)) or using a [hack in the debug logging system](https://community.home-assistant.io/t/how-to-uart-read-without-custom-component/491950).


This ESPHome external component makes it easy to read UART data from a device. It also allows for the data it to be filtered, extracted and formatted as ESPHome sensor values. It can optionally then send data back to the device.

You can include this ESPHome external component with the following:
```
external_components:
  - source: github://eigger/espcomponents/releases/latest
    components: [ uartex ]
    refresh: 1day
```

The component performs the following loop read/match/process/reply:

1) Read in bytes from a device via the UART until the end of data block is detected.
2) If the header matches, it then passes the block received to all the sensors that have been configured to process the data block.
3) If configured, it then sends bytes back to the device via the UART.

The end of a block is detected when any of the following occur:

1) If `rx_cheksum:` is specified, then a CRC is calculated after each byte is received and if it matches in the data then reading stops. This method is convenient but might be caught out by a false match in the data.
2) If `rx_length` is specified it will stop reading after the specified number of bytes have been read.
3) Or if `rx_timeout` has been reached it will stop and then process the block. While reading in the block this component will be blocking, this may cause warnings if the timeout is greater than 30mSec


For the read part of the loop this diagram shows the flow, this example uses the `rx_checksum` method to detect the end of a block of data:

![read data flow](docs/uartex_drawing.png)


Base uartex set up
=============

The read/match/process/reply loop is configured in the `uartex:` section of configuration.

```
# Example configuration entry
uartex:
  rx_timeout: 10ms
  tx_delay: 50ms
  tx_timeout: 500ms
  tx_retry_cnt: 3

  rx_header: [0x02, 0x01]
  rx_header_mask: [0xFF, 0xFF]

  rx_footer: [0x0D, 0x0A]
  tx_header: "\x02\x01"
  tx_footer: "\r\n"

  rx_checksum: add
  tx_checksum: add
```
## Configuration variables:

- **rx_timeout** *(Optional, [Time](https://esphome.io/guides/configuration-types#config-time))*: Data Receive Timeout. Defaults to 10ms. Max 2000ms. The length of time from receving the first byte the component continues receving or waiting for data before processing it. While waiting this component is blocking so use values over 30 ms wth caution.

- **rx_length** *(Optional, int)*: The length of the received data, use when the data length is known to be fixed. Max 256

- **rx_checksum** *(Optional, enum, lambda)*: Checksum method for received data, where a single byte is used for the checksum in the data block. The checksum is calculated every time a byte is received if the next byte matches this value it is assumed the end of the data block has been reached and the data is processed.  Possible options are:
  - **add** - use additive CRC calculation
  - **xor** - use xor CRC calculation
  - **lambda** - you can specify your own CRC calculation. The lambda is expected to return a `uint8_t` value. Two variables are set up `uint8_t* data` an array of all the data in the block not including the header data & `uint16_t len` the length of the data array.
    ```
    # Example configuration entry
    rx_checksum: !lambda |-
      uint32_t crc = 0x02 + 0x01;
      for (int i = 0; i < len; i++)
      {
        crc += data[i];           # data(0) is the first byte of data after the header.
      }
      return crc & 0x00FF;
    ```
  - 
- **rx_checksum2** *(Optional, enum or lambda)*: Checksum method for received data, where a 2 bytes are used for the checksum in the data block. The checksum is calculated every time a byte is received if the next 2 bytes matches this value it is assumed the end of the data block has been reached and the data is processed.  Possible options are:

  - **add** - use additive CRC calculation

  - **xor** - use xor CRC calculation

  - **lambda** - you can specify your own CRC calculation. The lambda is expected to return a vector\<uint8_t\> value. Two variables are set up `uint8_t* data` an array of all the data in the block not including the header data & `uint16_t len` the length of the data array.

- **rx_header** *(Optional, array)*: Header data required to match in data stream for processing to take place, if no match this data block is ignored.

- **rx_header_mask** *(Optional, array)*: Mask for received header data

- **rx_footer** *(Optional, array)*: Footer of Data to be Received

- **tx_delay** *(Optional, Time)* : Delay before data sent back to device. Defaults to 50ms. Max 2000ms

- **tx_timeout** *(Optional, Time)* : ACK Reception Timeout. Defaults to 50ms. Max 2000ms  This is the time the component waits, before assuming an ACK has not been sent by the device. It tries to send command again for `tx_retry_cnt` number of times before giving up.

- **tx_retry_cnt** *(Optional, int)* Defaults to 3. Max 10: Number of times component will attempt to send data after not receiving an ACK in the time specified by `tx_timeout`

- **tx_ctrl_pin** *(Optional, gpio)* : Control PIN GPIO, some RS485 converter hardware requires a controlling a GPIO pin, this will be set high when transmitting data.

- **tx_header** *(Optional, array)* : Header added to data head of data when sent.

- **tx_footer** *(Optional, array)* : Footer added to end of data to be sent.

- **tx_checksum** *(Optional, enum, lambda)* : Checksum added to data to be sent, where a single byte is used for the checksum at the end of the data block. Possible options are:
  - **add** - use additive CRC calculation
  - **xor** - use xor CRC calculation
  - **lambda** - you can specify your own CRC calculation. The lambda is expected to return a `uint8_t` value. Two variables are set up `uint8_t* data` an array of all the data in the block not including the header data & `uint16_t len` the length of the data array.

- **tx_checksum2** *(Optional, enum or lambda)* : Checksum added to data sent, where a 2 bytes are used for the checksum at the end of the data block. Possible options are:

  - **add** - use additive CRC calculation

  - **xor** - use xor CRC calculation

  - **lambda** - you can specify your own CRC calculation. The lambda is expected to return a vector\<uint8_t\> value. Two variables are set up `uint8_t* data` an array of all the data in the block not including the header data & `uint16_t len` the length of the data array.
  - vector\<uint8_t\> = (uint8_t* data, uint16_t len) :shrug:

- **on_read** *(Optional, lambda)* : Event when data block read. Two variables are set up `uint8_t* data` an array of all the data in the block not including the header data & `uint16_t len` the length of the data array.There is no return value.

- **on_write** *(Optional, lambda)* : Event when data block sent. Two variables are set up `uint8_t* data` an array of all the data in the block not including the header data & `uint16_t len` the length of the data array. There is no return value.

- **version** *(Optional)* : Creates a sensor to send the version number  of this component to ESPHome
- **error** *(Optional)* : Creates a sensor to send error messages from this component to ESPHome
- **log** *(Optional)* : Creates a sensor to send log message from this component to ESPHome, this includes the raw bytes received from the UART, this is very useful for reverse engineering a data stream.

### Checksum Calculation

The CRC checksum is calculated in the following way:
```
    uint16_t crc = 0;
    switch(checksum)
    {
    case CHECKSUM_XOR:
        for (uint8_t byte : header) { crc ^= byte; }
        for (uint8_t byte : data) { crc ^= byte; }
        break;
    case CHECKSUM_ADD:
        for (uint8_t byte : header) { crc += byte; }
        for (uint8_t byte : data) { crc += byte; }
        break;
    case CHECKSUM_XOR_NO_HEADER:
        for (uint8_t byte : data) { crc ^= byte; }
        break;
    case CHECKSUM_ADD_NO_HEADER:
        for (uint8_t byte : data) { crc += byte; }
        break;
    }
    return crc;
```
For `tx_checksum` and `rx_checksum` the value used is `crc & 0xFF`.
For `tx_checksum2` and `rx_checksum2` the value used is 2 bytes `crc >> 8` and  `crc &0xFF`.


Uartex Entities
===============

The entities that match on and then convert raw data in the data recieved are described bellow. When a packet arrives, it is first parsed by `uartex:` and then distributed to all uartex entities. 

State Matching in all Entities
=============================

All Uartex entities can have an optional state specified. It is common with RS485 or similar networks to have multiple devices connected to a common serial line. The `state:` is used to identify the device that sent the packet. Only when this matches will the entity process the data, other entities can be specified that may match this state and these will also be checked and processed. This allows different entities to be configured to respond to different devices on an RS484 network.

An entity specified without `state:` will match on every data block received. 
```
# Example configuration entry for a sensor with state matching
sensor:
  - platform: uartex
    state: 
      data: [0x01, 0x02]
      mask: [0xff, 0xff]
      offset: 0
      inverted: False
    ...
```
or in a short form:
```
# Example configuration entry
sensor:
  - platform: uartex
    state: [0x01, 0x02]
    ...
```

If the data packet matches the `data:`, the entity proceeds to process the raw data, otherwise it ignores this data. So with this example, the entity will process the data:

| Data Rx   | 0x02 | 0x01 | 0x01 | 0x02 | 0x00 | CRC  | 0x0D | 0x0A  |
| -----     | :--: | :--: | :--: | :--: | :--: | :--: | :--: | :---: |
| Offset    | head | head | 0    | 1    | 2    | 3    | 4    | 5     |

## Configuration variables
- **data** *(Required, array or string)*: The value or values used to match in the data, for example [0x01, 0x02] or "\r\n"
- **mask** *(Optional, array or string)*: Defaults to []
- **offset** *(Optional, int)*: Defaults to 0. The first byte after the header has index 0.
- **inverted** *(Optional, bool)*: Defaults to False.

Send a Command
==============

This sends a command to device and optionally checks for an acknowledgement (ACK) back.
```
# Example configuration entry
command: 
  cmd: [0x01, 0x02, 0x01]
  ack: [0xff]
```

This would send to the device via the UART the following data:
| header | 0x01 | 0x02 | 0x01 | CRC  | footer |
| :----: | :--: | :--: | :--: | :--: | :--:   |

And will expect back an ACK of:
| header | 0xff |  CRC  | footer  |
| :----: | :--: | :---: | :-----: | 

The test for the ACK will be influenced by `tx_delay:`, `tx_timeout:` and `tx_retry_cnt:` specified in the `uartex:` section

## Configuration variables
- **cmd** *(Required, array or string)*: Command to be sent.
- **ack** *(Optional, array or string)*: ACK to be checked, defaults to [].


State Number Sensor
===================

This publishes to ESPHome a number taken from the data block.
```
# Example configuration entry
sensor:
  - platform: uartex
    state_num: 
    offset: 2
    length: 1
    precision: 0
    signed: false
    endian: "big"
    filters:
      - multiply: 0.035

value = 0x02 
```
With this data received:

| header | 0x00 | 0x01 | 0x02 | CRC  | footer |
| :----: | :--: | :--: | :--: | :--: | :--:   |

This sensor will sends the value 2.0*0.035 = 0.07 to ESPHome.

## Configuration variables
- **offset** *(Required, int)*: (0 ~ 128) Offset from start of data block, 0 is the first byte after the header.
- **length** *(Optional, int)*: Defaults to 1. (1 ~ 4) The number of bytes to consume.
- **precision** *(Optional, int): Defaults to 0. (0 ~ 5). The precision of data read or published :shrug:
- **signed** *(Optional, bool)*: Defaults to true. Sets the bytes to be processed as signed or unsigned.
- **endian** *(Optional, enum)*: Defaults to "big". Sets the endian for the bytes to be processed as big endian or little endian.

Binary Sensor
=============

This binary sensor will check a byte within the received data block. The mask can be used to test for a specific bit within that byte.

```
# Example configuration entry
binary_sensor:
  - platform: uartex
    name: Binary_Sensor1
    state: [0x02, 0x03]
    state_on:
      offset: 2
      data: [0x01]
    state_off:
      offset: 2
      data: [0x00]
```
With this data received the sensor will report 'on'

| header | 0x02 | 0x03 | 0x01 | CRC  | footer |
| :----: | :--: | :--: | :--: | :--: | :--:   |

With this data received the sensor will report 'off'

| header | 0x02 | 0x03 | 0x00 | CRC  | footer |
| :----: | :--: | :--: | :--: | :--: | :--:   |

### Configuration variables
- **state** *(Optional, state)*: The value matched wiuth the byte within the  
- state_on (Required, state): 
- state_off (Required, state): 
- command_update (Optional, command or lambda): 
  - command = (void)
<hr/>

## uartex.button
```
packet on) 0x02 0x01 0x02 0x03 0x01 (add)checksum 0x0D 0x0A
   offset) head head 0    1    2

button:
  - platform: uartex
    name: "Elevator Call"
    icon: "mdi:elevator"
    command_on: 
      data: [0x02, 0x03, 0x01]
```
### Configuration variables
- command_on (Required, command or lambda): 
  - command = (void)
<hr/>



To Be Completed
===============

<!--
uartex.light
============
```
packet on) 0x02 0x01 0x02 0x03 0x01 (add)checksum 0x0D 0x0A
   offset) head head 0    1    2
packet on ack) 0x02 0x01 0x02 0x13 0x01 (add)checksum 0x0D 0x0A
packet off) 0x02 0x01 0x02 0x03 0x00 (add)checksum 0x0D 0x0A
packet off ack) 0x02 0x01 0x02 0x03 0x00 (add)checksum 0x0D 0x0A

light:
  - platform: uartex
    name: "Room 0 Light 1"
    id: room_0_light_1
    state: 
      data: [0x02, 0x03]
      mask: [0xff, 0x0f]
    state_on:
      offset: 2
      data: [0x01]
    state_off:
      offset: 2
      data: [0x00]
    command_on:
      data: [0x02, 0x03, 0x01]
      ack: [0x02, 0x13, 0x01]
    command_off: !lambda |-
      return {{0x02, 0x03, 0x00}, {0x02, 0x13, 0x00}};
```
### Configuration variables
- state (Optional, state): 
- state_on (Required, state): 
- state_off (Required, state): 
- state_brightness (Optional, lambda):
  - float = (uint8_t* data, uint16_t len)
- command_on (Required, command or lambda): 
  - command = (void)
- command_off (Required, command or lambda): 
  - command = (void)
- command_brightness (Optional, lambda): 
  - command = (float x)
- command_update (Optional, command or lambda): 
  - command = (void)
<hr/>




## uartex.climate
```
packet off) 0x02 0x01 0x02 0x03 0x00 target current (add)checksum 0x0D 0x0A
    offset) head head 0    1    2    3      4
packet off ack) 0x02 0x01 0x02 0x13 0x00 target current (add)checksum 0x0D 0x0A
packet heat) 0x02 0x01 0x02 0x03 0x01 target current (add)checksum 0x0D 0x0A
packet heat ack) 0x02 0x01 0x02 0x13 0x01 target current (add)checksum 0x0D 0x0A

climate:
  - platform: uartex
    name: "Room 0 Heater"
    id: room_0_heater
    visual:
      min_temperature: 5 °C
      max_temperature: 30 °C
      temperature_step: 1 °C
    state: 
      data: [0x02, 0x03]
      mask: [0xff, 0x0f]
    state_temperature_current:
      offset: 4
      length: 1
      precision: 0
    state_temperature_target:
      offset: 3
      length: 1
      precision: 0
    state_off:
      offset: 2
      data: [0x00]
    state_heat:
      offset: 2
      data: [0x01]
    command_off:
      data: [0x02, 0x03, 0x00]
      ack: [0x02, 0x13, 0x00]
    command_heat: !lambda |-
      float target = id(room_0_heater).target_temperature;
      return {{0x02, 0x03, 0x01, (uint8_t)target, 0x00},{0x02, 0x13, 0x01}};
    command_temperature: !lambda |-
      // @param: const float x
      float target = x;
      return {{0x02, 0x03, 0x01, (uint8_t)target, 0x00},{0x02, 0x13, 0x01}};
```
### Configuration variables
- state (Optional, state): 
- state_off (Required, state): 
- state_temperature_current (Optional, state_num or lambda):
  - float = (uint8_t* data, uint16_t len)
- state_temperature_target (Optional, state_num or lambda):
  - float = (uint8_t* data, uint16_t len)
- state_humidity_current (Optional, state_num or lambda):
  - float = (uint8_t* data, uint16_t len)
- state_humidity_target (Optional, state_num or lambda):
  - float = (uint8_t* data, uint16_t len)
- state_cool (Optional, state): 
- state_heat (Optional, state): 
- state_fan_only (Optional, state): 
- state_dry (Optional, state): 
- state_auto (Optional, state): 
- state_swing_off (Optional, state): 
- state_swing_both (Optional, state): 
- state_swing_vertical (Optional, state): 
- state_swing_horizontal (Optional, state): 
- state_fan_on (Optional, state): 
- state_fan_off (Optional, state): 
- state_fan_auto (Optional, state): 
- state_fan_low (Optional, state): 
- state_fan_medium (Optional, state): 
- state_fan_high (Optional, state): 
- state_fan_middle (Optional, state): 
- state_fan_focus (Optional, state): 
- state_fan_diffuse (Optional, state): 
- state_fan_quiet (Optional, state): 
- state_preset_none (Optional, state): 
- state_preset_home (Optional, state): 
- state_preset_away (Optional, state): 
- state_preset_boost (Optional, state): 
- state_preset_comfort (Optional, state): 
- state_preset_eco (Optional, state): 
- state_preset_sleep (Optional, state): 
- state_preset_activity (Optional, state): 
- state_custom_fan (Optional, lambda): 
  - std::string = (uint8_t* data, uint16_t len)
- state_custom_preset (Optional, lambda): 
  - std::string = (uint8_t* data, uint16_t len)
- command_off (Optional, command or lambda): 
  - command = (void)
- command_temperature (Optional, lambda): 
  - command = (float x)
- command_humidity (Optional, lambda): 
  - command = (float x)
- command_cool (Optional, command or lambda): 
  - command = (void)
- command_heat (Optional, command or lambda): 
  - command = (void)
- command_fan_only (Optional, command or lambda): 
  - command = (void)
- command_dry (Optional, command or lambda): 
  - command = (void)
- command_auto (Optional, command or lambda): 
  - command = (void)
- command_swing_off (Optional, command or lambda): 
  - command = (void)
- command_swing_both (Optional, command or lambda): 
  - command = (void)
- command_swing_vertical (Optional, command or lambda): 
  - command = (void)
- command_swing_horizontal (Optional, command or lambda): 
  - command = (void)
- command_fan_on (Optional, command or lambda): 
  - command = (void)
- command_fan_off (Optional, command or lambda): 
  - command = (void)
- command_fan_auto (Optional, command or lambda): 
  - command = (void)
- command_fan_low (Optional, command or lambda): 
  - command = (void)
- command_fan_medium (Optional, command or lambda): 
  - command = (void)
- command_fan_high (Optional, command or lambda): 
  - command = (void)
- command_fan_middle (Optional, command or lambda): 
  - command = (void)
- command_fan_focus (Optional, command or lambda): 
  - command = (void)
- command_fan_diffuse (Optional, command or lambda): 
  - command = (void)
- command_fan_quiet (Optional, command or lambda): 
  - command = (void)
- command_preset_none (Optional, command or lambda): 
  - command = (void)
- command_preset_away (Optional, command or lambda): 
  - command = (void)
- command_preset_boost (Optional, command or lambda): 
  - command = (void)
- command_preset_comfort (Optional, command or lambda): 
  - command = (void)
- command_preset_eco (Optional, command or lambda): 
  - command = (void)
- command_preset_sleep (Optional, command or lambda): 
  - command = (void)
- command_preset_activity (Optional, command or lambda): 
  - command = (void)
- command_update (Optional, command or lambda): 
  - command = (void)
- command_custom_fan (Required, lambda): 
  - command = (std::string str)
- command_custom_preset (Required, lambda): 
  - command = (std::string str)
- custom_fan_mode (Optional, list): A list of custom fan mode for this climate
- custom_preset (Optional, list): A list of custom preset mode for this climate
<hr/>

## uartex.fan
```
packet off) 0x02 0x01 0x02 0x03 0x00 speed (add)checksum 0x0D 0x0A
    offset) head head 0    1    2    3
packet off ack) 0x02 0x01 0x02 0x13 0x00 speed (add)checksum 0x0D 0x0A
packet on) 0x02 0x01 0x02 0x03 0x01 speed (add)checksum 0x0D 0x0A
packet on ack) 0x02 0x01 0x02 0x13 0x01 speed (add)checksum 0x0D 0x0A

fan:
  - platform: uartex
    name: "Fan1"
    state:
      data: [0x02, 0x03]
      mask: [0xff, 0x0f]
    state_on:
      offset: 2
      data: [0x01]
    state_off:
      offset: 2
      data: [0x00]
    command_on:
      data: [0x02, 0x03, 0x01]
      ack: [0x02, 0x13]
    command_off:
      data: [0x02, 0x03, 0x00]
      ack: [0x02, 0x13]
    command_speed: !lambda |-
      // @param: const float x
      return {
                {0x02, 0x03, 0x01, (uint8_t)x},
                {0x02, 0x13}
              };
    state_speed: !lambda |-
      // @param: const uint8_t *data, const unsigned short len
      // @return: const float
      {
        return data[3];
      }
```
### Configuration variables
- state (Optional, state): 
- state_on (Required, state): 
- state_off (Required, state): 
- state_speed (Optional, lambda):
  - float = (uint8_t* data, uint16_t len)
- state_preset (Optional, lambda): 
  - std::string = (uint8_t* data, uint16_t len)
- command_on (Required, command or lambda): 
  - command = (void)
- command_off (Required, command or lambda): 
  - command = (void)
- command_speed (Optional, lambda): 
  - command = (float x)
- command_preset (Required, lambda): 
  - command = (std::string str)
- command_update (Optional, command or lambda): 
  - command = (void)
- preset_modes (Optional, list): A list of preset modes for this fan
<hr/>

## uartex.lock
```
packet unlock) 0x02 0x01 0x02 0x03 0x00 (add)checksum 0x0D 0x0A
       offset) head head 0    1    2
packet unlock ack) 0x02 0x01 0x02 0x13 0x00 (add)checksum 0x0D 0x0A
packet lock) 0x02 0x01 0x02 0x03 0x01 (add)checksum 0x0D 0x0A
packet lock ack) 0x02 0x01 0x02 0x13 0x01 (add)checksum 0x0D 0x0A

lock:
  - platform: uartex
    name: "Lock1"
    state:
      data: [0x02, 0x03]
      mask: [0xff, 0x0f]
    state_locked:
      offset: 2
      data: [0x01]
    state_unlocked:
      offset: 2
      data: [0x00]
    state_locking:
      offset: 2
      data: [0x02]
    state_unlocking:
      offset: 2
      data: [0x03]
    state_jammed:
      offset: 2
      data: [0x04]
    command_lock:
      data: [0x02, 0x03, 0x01]
      ack: [0x02, 0x13]
    command_unlock:
      data: [0x02, 0x03, 0x00]
      ack: [0x02, 0x13]
```
### Configuration variables
- state (Optional, state): 
- state_locked (Optional, state): 
- state_unlocked (Optional, state): 
- state_locking (Optional, state): 
- state_unlocking (Optional, state): 
- state_jammed (Optional, state): 
- command_lock (Optional, command or lambda): 
  - command = (void)
- command_unlock (Optional, command or lambda): 
  - command = (void)
<hr/>

## uartex.number
```
packet) 0x02 0x01 0x02 0x03 0x00 number (add)checksum 0x0D 0x0A
offset) head head 0    1    2    3
packet ack) 0x02 0x01 0x02 0x13 0x00 number (add)checksum 0x0D 0x0A

number:
  - platform: uartex
    name: "Number1"
    state:
      data: [0x02, 0x03]
      mask: [0xff, 0x0f]
    max_value: 10
    min_value: 1
    step: 1
    state_number:
      offset: 3
      length: 1
      precision: 0
    command_number: !lambda |-
      // @param: const float x
      return {
                {0x02, 0x03, 0x00, (uint8_t)x},
                {0x02, 0x13}
              };
```
### Configuration variables
- state (Optional, state): 
- state_number (Optional, state_num or lambda):
  - float = (uint8_t* data, uint16_t len)
- command_number (Optional, lambda): 
  - command = (float x)
- command_update (Optional, command or lambda): 
  - command = (void)
<hr/>

## uartex.sensor
```
packet) 0x02 0x01 0x02 0x03 0x00 value (add)checksum 0x0D 0x0A
offset) head head 0    1    2    3
sensor:
  - platform: uartex
    name: Sensor1
    state: [0x02, 0x03, 0x00]
    state_number:
      offset: 3
      length: 1
      precision: 0
```
### Configuration variables
- state (Optional, state): 
- state_number (Optional, state_num or lambda): 
  - float = (uint8_t* data, uint16_t len)
- lambda (Optional, lambda): 
  - float = (uint8_t* data, uint16_t len)
- command_update (Optional, command or lambda): 
  - command = (void)
<hr/>

## uartex.switch
```
packet on) 0x02 0x01 0x02 0x03 0x01 (add)checksum 0x0D 0x0A
   offset) head head 0    1    2
packet on ack) 0x02 0x01 0x02 0x13 0x01 (add)checksum 0x0D 0x0A
packet off) 0x02 0x01 0x02 0x03 0x00 (add)checksum 0x0D 0x0A
packet off ack) 0x02 0x01 0x02 0x03 0x00 (add)checksum 0x0D 0x0A

switch:
  - platform: uartex
    name: "Switch1"
    state: 
      data: [0x02, 0x03]
      mask: [0xff, 0x0f]
    state_on:
      offset: 2
      data: [0x01]
    state_off:
      offset: 2
      data: [0x00]
    command_on:
      data: [0x02, 0x03, 0x01]
      ack: [0x02, 0x13, 0x01]
    command_off: !lambda |-
      return {{0x02, 0x03, 0x00}, {0x02, 0x13, 0x00}};
```
### Configuration variables
- state (Optional, state): 
- state_on (Required, state): 
- state_off (Required, state): 
- command_on (Required, command or lambda): 
  - command = (void)
- command_off (Required, command or lambda): 
  - command = (void)
- command_update (Optional, command or lambda): 
  - command = (void)
<hr/>

## uartex.text
```
text:
  - platform: uartex
    name: "Text"
    command_text: !lambda |-
      return {
                {0x0F, 0x01, 0x01},
                {0x0F, 0x01}
              };
```
### Configuration variables
- command_text (Required, lambda): 
  - command = (std::string str)
<hr/>

## uartex.text_sensor
```
packet on) 0x02 0x01 0x02 0x03 0x01 (add)checksum 0x0D 0x0A
   offset) head head 0    1    2
packet off) 0x02 0x01 0x02 0x03 0x00 (add)checksum 0x0D 0x0A

text_sensor:
  - platform: uartex
    name: "Text Sensor"
    state: [0x02, 0x03]
    lambda: |-
      {
        if (data[2] == 0x01) return "ON";
        else return "OFF";
      }
```
### Configuration variables
- state (Optional, state): 
- lambda (Optional, lambda): 
  - std::string = (uint8_t* data, uint16_t len)
- command_update (Optional, command or lambda): 
  - command = (void)
<hr/>

## uartex.valve
```
packet open) 0x02 0x01 0x02 0x03 0x01 (add)checksum 0x0D 0x0A
offset) head head 0    1    2    3
packet open ack) 0x02 0x01 0x02 0x13 0x01 (add)checksum 0x0D 0x0A
packet close) 0x02 0x01 0x02 0x03 0x00 (add)checksum 0x0D 0x0A
packet close ack) 0x02 0x01 0x02 0x03 0x00 (add)checksum 0x0D 0x0A

valve:
  - platform: uartex
    name: "Valve1"
    state: 
      data: [0x02, 0x03]
      mask: [0xff, 0x0f]
    state_open:
      offset: 2
      data: [0x01]
    state_closed:
      offset: 2
      data: [0x00]˚
    command_open:
      data: [0x02, 0x03, 0x01]
      ack: [0x02, 0x13, 0x01]
    command_close:
      data: [0x02, 0x03, 0x00]
      ack: [0x02, 0x13, 0x00]
```
### Configuration variables
- state (Optional, state): 
- state_open (Optional, state): 
- state_closed (Optional, state): 
- state_position (Optional, lambda):
  - float = (uint8_t* data, uint16_t len)
- command_open (Optional, command or lambda): 
  - command = (void)
- command_close (Optional, command or lambda): 
  - command = (void)
- command_stop (Optional, command or lambda): 
  - command = (void)
- command_update (Optional, command or lambda): 
  - command = (void)
<hr/>
-->

The end.