# Microchip AVR-IoT WHA (Wireless for Home Assistant) Application

The aim here is to set up the AVR-IoT with an [MQTT Light](https://www.home-assistant.io/integrations/light.mqtt/) using [MQTT Discovery](https://www.home-assistant.io/docs/mqtt/discovery/)

Connect to the COM port of the AVR IoT and use the CLI to configure wifi, mqtt connection and credentials (9600 BAUD).
Any terminal with serial support can be used, I like the [MPLAB Data Visualizer](https://www.microchip.com/en-us/tools-resources/debug/mplab-data-visualizer) a lot, at least if you want to output a lot of raw data from the MCU

![device](images/ha_light.png)

## The debug info looks like this

![debug_info](images/ha_debug_info.png)

FYI, this repo is a fork of [avr-iot-mqtt-ha-sensor-node](https://github.com/echoromeo/avr-iot-mqtt-ha-sensor-node) but since that repo is a fork of [avr-iot-aws-sensor-node-mplab](https://github.com/microchip-pic-avr-solutions/avr-iot-aws-sensor-node-mplab) github did not allow me to fork it.
