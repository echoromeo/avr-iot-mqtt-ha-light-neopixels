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

#define HA_TOPIC_ADDCID(type)							"homeassistant/"type"/%s"
#define TOPIC_HA_SENSOR_ADDCID(extra_id)				HA_TOPIC_ADDCID("sensor") extra_id
#define TOPIC_HA_SENSOR_STATE_ADDCID(extra_id)			HA_TOPIC_ADDCID("sensor") extra_id "/state"
#define TOPIC_HA_SENSOR_COMMAND_ADDCID(extra_id)		HA_TOPIC_ADDCID("sensor") extra_id "/set"
#define TOPIC_HA_SENSOR_CONFIG_ADDCID(extra_id)			HA_TOPIC_ADDCID("sensor") extra_id "/config"

#define CONFIG_HA(content)								"{"content"}"
#define CONFIG_HA_PREFIX_ADDCID(type)					"\"~\": \""HA_TOPIC_ADDCID(type)"\""
#define CONFIG_HA_DEVICE_CLASS(type)					"\"dev_cla\":\""type"\""
#define CONFIG_HA_DEVICE_CLASS_TEMP						CONFIG_HA_DEVICE_CLASS("temperature")
#define CONFIG_HA_DEVICE_CLASS_LIGHT					CONFIG_HA_DEVICE_CLASS("illuminance")
#define CONFIG_HA_ID_ADDCID(name)						"\"uniq_id\":\""name"%s\",\"obj_id\":\""name"%s\""
#define CONFIG_HA_STATE(extra_id)						"\"stat_t\":\"~"extra_id"/state\""
#define CONFIG_HA_COMMAND(extra_id)						"\"cmd_t\":\"~"extra_id"/set\""
#define CONFIG_HA_UNIT_TEMP								"\"unit_of_meas\":\"°C\"" // Need utf-8 formatting on the file for ° to work!
#define CONFIG_HA_UNIT_LIGHT							"\"unit_of_meas\":\"lx\""
#define CONFIG_HA_JSON_VALUE(value)						"\"val_tpl\":\"{{value_json."value"}}\""
#define CONFIG_HA_EXPIRE_AFTER(value)					"\"expire_after\":"value
#define CONFIG_HA_ICON_MDI(name)						"\"icon\":\"mdi:"name"\""
#define NEXTCFG 										","

#define CONFIG_HA_DEVICE(content)						"\"dev\":{"content"}"
#define CONFIG_HA_DEVICE_IDENTIFIERS(content)			"\"ids\":"content
#define CONFIG_HA_DEVICE_NAME(name)						"\"name\":\""name"\""
#define CONFIG_HA_DEVICE_MANUFACTURER(name)				"\"mf\":\""name"\""
#define CONFIG_HA_DEVICE_MODEL(name)					"\"mdl\":\""name"\""
#define CONFIG_HA_DEVICE_SW_VERSION(version)			"\"sw\":\""version"\""

// Is it enough to send this with one of the config topics? or a completely separate one?
#define CONFIG_HA_THIS_DEVICE							CONFIG_HA_DEVICE( CONFIG_HA_DEVICE_IDENTIFIERS("[\"temp_%s\", \"light_%s\"]") \
														NEXTCFG CONFIG_HA_DEVICE_NAME("AVR-IoT") \
														NEXTCFG CONFIG_HA_DEVICE_MANUFACTURER("Microchip Technology") \
														NEXTCFG CONFIG_HA_DEVICE_MODEL("AVR-IoT Sensor Node") \
														NEXTCFG CONFIG_HA_DEVICE_SW_VERSION("6.1.0"))

static char mqttSubscribeTopic[NUM_TOPICS_SUBSCRIBE][SUBSCRIBE_TOPIC_SIZE];
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
	mqttHeaderFlags flags = {.All = 0};

	if (shared_networking_params.haveAPConnection) // Do we really need this one?
	{
		if (discover)
		{
			discover--;
			debug_printIoTAppMsg("Application: Sending Discover Config %");
			#ifdef DEBUG
				flags.retain = 0;
			#else
				flags.retain = 1;
			#endif

			if (discover == 1) {
				sprintf(mqttPublishTopic, TOPIC_HA_SENSOR_CONFIG_ADDCID("_temp"), eeprom->mqttCID); // Can optimize this a lot if never changing CID
				len = sprintf(json, CONFIG_HA(CONFIG_HA_PREFIX_ADDCID("sensor") // %s = eeprom->mqttCID
										NEXTCFG CONFIG_HA_DEVICE_CLASS_TEMP
										NEXTCFG CONFIG_HA_DEVICE_NAME("AVR-IoT Temperature")
										NEXTCFG CONFIG_HA_ID_ADDCID("temp_") // 2x %s = eeprom->mqttCID
										NEXTCFG CONFIG_HA_EXPIRE_AFTER("300")
										NEXTCFG CONFIG_HA_ICON_MDI("thermometer-lines")
										NEXTCFG CONFIG_HA_STATE()
										NEXTCFG CONFIG_HA_UNIT_TEMP
										NEXTCFG CONFIG_HA_JSON_VALUE("temp")
										NEXTCFG CONFIG_HA_THIS_DEVICE), // 2x %s = eeprom->mqttCID
									eeprom->mqttCID,
									eeprom->mqttCID,
									eeprom->mqttCID,
									eeprom->mqttCID,
									eeprom->mqttCID);
			} else if (discover == 0) {
				sprintf(mqttPublishTopic, TOPIC_HA_SENSOR_CONFIG_ADDCID("_light"), eeprom->mqttCID); // Can optimize this a lot if never changing CID
				len = sprintf(json, CONFIG_HA(CONFIG_HA_PREFIX_ADDCID("sensor") // %s = eeprom->mqttCID
										NEXTCFG CONFIG_HA_DEVICE_CLASS_LIGHT 
										NEXTCFG CONFIG_HA_DEVICE_NAME("AVR-IoT Light") 
										NEXTCFG CONFIG_HA_ID_ADDCID("light_") // 2x %s = eeprom->mqttCID
										NEXTCFG CONFIG_HA_ICON_MDI("sun-wireless-outline")
										NEXTCFG CONFIG_HA_EXPIRE_AFTER("30")
										NEXTCFG CONFIG_HA_STATE() 
										NEXTCFG CONFIG_HA_UNIT_LIGHT 
										NEXTCFG CONFIG_HA_JSON_VALUE("light")
										NEXTCFG CONFIG_HA_THIS_DEVICE), // 2x %s = eeprom->mqttCID
									eeprom->mqttCID,
									eeprom->mqttCID,
									eeprom->mqttCID,
									eeprom->mqttCID,
									eeprom->mqttCID);
			}

		}  else {
			debug_printIoTAppMsg("Application: Sending Sensor Data");
		
			rawTemperature = SENSORS_getTempValue();
			light = SENSORS_getLightValue();

			sprintf(mqttPublishTopic, TOPIC_HA_SENSOR_STATE_ADDCID(), eeprom->mqttCID); // Can optimize this a lot if never changing CID
			len = sprintf(json,"{\"light\":%d,\"temp\":%d.%02d}", light,rawTemperature/100,abs(rawTemperature)%100);
		}

		if (len) {
			CLOUD_publishData((uint8_t*)mqttPublishTopic ,(uint8_t*)json, len, flags);
			debug_printInfo("%s: %s (%d)", mqttPublishTopic, json, len);			
		}

		ledParameterYellow.onTime = LED_BLIP;
		ledParameterYellow.offTime = LED_BLIP;
		LED_control(&ledParameterYellow);
	}
}

void subscribeToCloud(void)
{
	sprintf(mqttSubscribeTopic[0], TOPIC_HA_SENSOR_COMMAND_ADDCID("_temp"), eeprom->mqttCID); // Can optimize this a lot if never changing CID
	CLOUD_registerSubscription((uint8_t*)mqttSubscribeTopic[0],receivedFromCloud);
	sprintf(mqttSubscribeTopic[0], TOPIC_HA_SENSOR_COMMAND_ADDCID("_light"), eeprom->mqttCID); // Can optimize this a lot if never changing CID
	CLOUD_registerSubscription((uint8_t*)mqttSubscribeTopic[0],receivedFromCloud);
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
