/*
    (c) 2018 Microchip Technology Inc. and its subsidiaries. 
    
    Subject to your compliance with these terms, you may use Microchip software and any 
    derivatives exclusively with Microchip products. It is your responsibility to comply with third party 
    license terms applicable to your use of third party software (including open source software) that 
    may accompany Microchip software.
    
    THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER 
    EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY 
    IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS 
    FOR A PARTICULAR PURPOSE.
    
    IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE, 
    INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND 
    WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP 
    HAS BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO 
    THE FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL 
    CLAIMS IN ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT 
    OF FEES, IF ANY, THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS 
    SOFTWARE.
*/

#include <xc.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include "main.h"

#include "mcc_generated_files/config/IoT_Sensor_Node_config.h"
#include "mcc_generated_files/config/conf_winc.h"
#include "mcc_generated_files/config/mqtt_config.h"
#include "mcc_generated_files/config/cloud_config.h"

#include "mcc_generated_files/application_manager.h"
#include "mcc_generated_files/led.h"
#include "mcc_generated_files/debug_print.h"

#include "mcc_generated_files/sensors_handling.h"
#include "mcc_generated_files/credentials_storage/credentials_storage.h"
#include "mcc_generated_files/time_service.h"
#include "mcc_generated_files/cloud/cloud_service.h"
#include "mcc_generated_files/cloud/mqtt_service.h"
#include "mcc_generated_files/mqtt/mqtt_core/mqtt_core.h"

#define TOPIC_HA_SENSOR_CONFIG_VARIABLE(name)	"homeassistant/sensor/%s"name"/config"
#define TOPIC_HA_SENSOR_STATE_VARIABLE			"homeassistant/sensor/%s/state"

#define CONFIG_HA_DEVICE_CLASS_TEMP				"\"dev_cla\": \"temperature\""
#define CONFIG_HA_DEVICE_CLASS_LIGHT			"\"dev_cla\": \"illuminance\""
#define CONFIG_HA_NAME(name)					"\"name\": \""name"\""
#define CONFIG_HA_NAME_VARIABLE					STRING_HA_NAME("%s")
#define CONFIG_HA_SENSOR_STATE_VARIABLE			"\"stat_t\": \""TOPIC_HA_SENSOR_STATE_VARIABLE"\""
#define CONFIG_HA_UNIT_TEMP						"\"unit_of_meas\": \"°C\"" // ° does not work properly!
#define CONFIG_HA_UNIT_LIGHT					"\"unit_of_meas\": \"lx\""
#define CONFIG_HA_JSON_VALUE(value)				"\"val_tpl\": \"{{ value_json."value" }}\""

static char mqttSubscribeTopic[SUBSCRIBE_TOPIC_SIZE];
static char mqttPublishTopic[PUBLISH_TOPIC_SIZE];
static char json[PAYLOAD_SIZE];
static uint8_t discover = 2;

int main(void)
{
	application_init();

	while (1)
	{ 
		runScheduler();  
		if (!shared_networking_params.haveAPConnection)
		{
			discover = 2;
		}
	}
   
	return 0;
}

// This will get called every CFG_SEND_INTERVAL while we have a valid Cloud connection
void sendToCloud(void)
{
	int rawTemperature = 0;
	int light = 0;
	int len = 0;

	if (shared_networking_params.haveAPConnection) // Do we really need this one?
	{
		if (discover)
		{
			debug_printIoTAppMsg("Application: Sending Discover Config %");
			mqttHeaderFlags flags = {.retain = 1};

			if (discover == 2) {
				sprintf(mqttPublishTopic, TOPIC_HA_SENSOR_CONFIG_VARIABLE("_temp"), eeprom->mqttCID); // Can optimize this a lot if never changing CID
				len = sprintf(json, "{" CONFIG_HA_DEVICE_CLASS_TEMP ", "
										CONFIG_HA_NAME("AVR IoT Temperature") ", "
										CONFIG_HA_SENSOR_STATE_VARIABLE ", " // %s = eeprom->mqttCID
										CONFIG_HA_UNIT_TEMP ", "
										CONFIG_HA_JSON_VALUE("temp") " }", 
										eeprom->mqttCID);
				CLOUD_publishData((uint8_t*)mqttPublishTopic ,(uint8_t*)json, len, flags);
				debug_printInfo("%s: %s", mqttPublishTopic,json);
			} else {
				sprintf(mqttPublishTopic, TOPIC_HA_SENSOR_CONFIG_VARIABLE("_light"), eeprom->mqttCID); // Can optimize this a lot if never changing CID
				len = sprintf(json, "{" CONFIG_HA_DEVICE_CLASS_LIGHT ", "
										CONFIG_HA_NAME("AVR IoT Light") ", "
										CONFIG_HA_SENSOR_STATE_VARIABLE ", " // %s = eeprom->mqttCID
										CONFIG_HA_UNIT_LIGHT ", "
										CONFIG_HA_JSON_VALUE("light") " }",
										eeprom->mqttCID);
				CLOUD_publishData((uint8_t*)mqttPublishTopic ,(uint8_t*)json, len, flags);
				debug_printInfo("%s: %s", mqttPublishTopic,json);
			}
			discover--;

		}  else {
			debug_printIoTAppMsg("Application: Sending Sensor Data");
		
			rawTemperature = SENSORS_getTempValue();
			light = SENSORS_getLightValue();

			sprintf(mqttPublishTopic, TOPIC_HA_SENSOR_STATE_VARIABLE, eeprom->mqttCID); // Can optimize this a lot if never changing CID
			len = sprintf(json,"{\"light\":%d,\"temp\":%d.%02d}", light,rawTemperature/100,abs(rawTemperature)%100);
			CLOUD_publishData((uint8_t*)mqttPublishTopic ,(uint8_t*)json, len, (mqttHeaderFlags){.All= 0});
		}

		ledParameterYellow.onTime = LED_BLIP;
		ledParameterYellow.offTime = LED_BLIP;
		LED_control(&ledParameterYellow);
	}
}

void subscribeToCloud(void)
{
	sprintf(mqttSubscribeTopic, "%s/test", eeprom->mqttCID); // Can optimize this a lot if never changing CID
	CLOUD_registerSubscription((uint8_t*)mqttSubscribeTopic,receivedFromCloud);
}

//This handles messages published from the MQTT server when subscribed
void receivedFromCloud(uint8_t *topic, uint8_t *payload)
{
	debug_printIoTAppMsg("Application: Received Data");
	LED_test();
	
	// TODO: this is where to handle stuff..

	debug_printIoTAppMsg("topic: %s", topic);
	debug_printIoTAppMsg("payload: %s", payload);
	
	ledParameterRed.onTime = LED_BLIP;
	ledParameterRed.offTime = LED_BLIP;
	LED_control(&ledParameterRed);
}
