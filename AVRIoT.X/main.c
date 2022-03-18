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
#define TOPIC_HA_LIGHT_ADDCID(extra_id)					HA_TOPIC_ADDCID("light") extra_id
#define TOPIC_HA_LIGHT_STATE_ADDCID(extra_id)			HA_TOPIC_ADDCID("light") extra_id "/state"
#define TOPIC_HA_LIGHT_COMMAND_ADDCID(extra_id)			HA_TOPIC_ADDCID("light") extra_id "/set"
#define TOPIC_HA_LIGHT_CONFIG_ADDCID(extra_id)			HA_TOPIC_ADDCID("light") extra_id "/config"

#define CONFIG_HA(content)								"{" content "}"
#define CONFIG_HA_PREFIX_ADDCID(type)					"\"~\": \"" HA_TOPIC_ADDCID(type) "\""
#define CONFIG_HA_ID_ADDCID(name)						"\"uniq_id\":\"" name "%s\",\"obj_id\":\"" name "%s\""
#define CONFIG_HA_STATE(extra_id)						"\"stat_t\":\"~" extra_id "/state\""
#define CONFIG_HA_COMMAND(extra_id)						"\"cmd_t\":\"~" extra_id "/set\""
#define CONFIG_HA_ICON_MDI(name)						"\"icon\":\"mdi:" name "\""
#define NEXTCFG 										","

// simple string payload with the format state,brightness,r-g-b,h-s (e.g., on,255,255-255-255,360-100),
#define CONFIG_HA_LIGHT_TEMPLATES						"\"schema\":\"template\"" \
														NEXTCFG "\"cmd_on_tpl\":\"on,{{ brightness|d }},{{ red|d }}-{{ green|d }}-{{ blue|d }},{{ hue|d }}-{{ sat|d }}\"" \
														NEXTCFG "\"cmd_off_tpl\":\"off\"" \
														NEXTCFG "\"stat_tpl\":\"{{ value.split(',')[0] }}\"" \
														NEXTCFG "\"bri_tpl\":\"{{ value.split(',')[1] }}\"" \
														NEXTCFG "\"r_tpl\":\"{{ value.split(',')[2].split('-')[0] }}\"" \
														NEXTCFG "\"g_tpl\":\"{{ value.split(',')[2].split('-')[1] }}\"" \
														NEXTCFG "\"b_tpl\":\"{{ value.split(',')[2].split('-')[2] }}\""


#define CONFIG_HA_DEVICE(content)						"\"dev\":{"content"}"
#define CONFIG_HA_DEVICE_IDENTIFIERS(content)			"\"ids\":"content
#define CONFIG_HA_DEVICE_NAME(name)						"\"name\":\""name"\""
#define CONFIG_HA_DEVICE_MANUFACTURER(name)				"\"mf\":\""name"\""
#define CONFIG_HA_DEVICE_MODEL(name)					"\"mdl\":\""name"\""
#define CONFIG_HA_DEVICE_SW_VERSION(version)			"\"sw\":\""version"\""

// Is it enough to send this with one of the config topics? or a completely separate one?
#define CONFIG_HA_THIS_DEVICE							CONFIG_HA_DEVICE( CONFIG_HA_DEVICE_IDENTIFIERS("[\"light_%s\"]") \
														NEXTCFG CONFIG_HA_DEVICE_NAME("AVR-IoT") \
														NEXTCFG CONFIG_HA_DEVICE_MANUFACTURER("Microchip Technology") \
														NEXTCFG CONFIG_HA_DEVICE_MODEL("AVR-IoT Neopixel Light") \
														NEXTCFG CONFIG_HA_DEVICE_SW_VERSION("7.0.0"))

static char mqttSubscribeTopic[NUM_TOPICS_SUBSCRIBE][SUBSCRIBE_TOPIC_SIZE];
static char mqttPublishTopic[PUBLISH_TOPIC_SIZE];
static char json[PAYLOAD_SIZE];
static uint8_t discover = 1;

typedef struct lights_struct {
	uint8_t on;
	uint8_t brightness;
	uint8_t red;
	uint8_t green;
	uint8_t blue;
	uint16_t hue;
	uint8_t saturation;
} lights_t;

static lights_t lights = {
	.on = 1,
	.brightness = 255,
	.red = 255,
	.green = 255,
	.blue =	255,
	.hue = 360,
	.saturation = 100
};

int main(void)
{
	application_init();

	while (1)
	{ 
		runScheduler();  
		if (!shared_networking_params.haveAPConnection)
		{
			discover = 1;
		}
	}
   
	return 0;
}

// This will get called every CFG_SEND_INTERVAL while we have a valid Cloud connection
void sendToCloud(void)
{
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

			if (discover == 0) {
				sprintf(mqttPublishTopic, TOPIC_HA_LIGHT_CONFIG_ADDCID("_light"), eeprom->mqttCID); // Can optimize this a lot if never changing CID
				len = sprintf(json, CONFIG_HA(CONFIG_HA_PREFIX_ADDCID("light") // %s = eeprom->mqttCID
										NEXTCFG CONFIG_HA_DEVICE_NAME("AVR-IoT Neopixel Light") 
										NEXTCFG CONFIG_HA_ID_ADDCID("light_") // 2x %s = eeprom->mqttCID
										NEXTCFG CONFIG_HA_STATE() 
										NEXTCFG CONFIG_HA_COMMAND()
										NEXTCFG CONFIG_HA_LIGHT_TEMPLATES
										NEXTCFG CONFIG_HA_THIS_DEVICE), // %s = eeprom->mqttCID
									eeprom->mqttCID,
									eeprom->mqttCID,
									eeprom->mqttCID,
									eeprom->mqttCID);
			}

		}  else {
			debug_printIoTAppMsg("Application: Sending State");

			sprintf(mqttPublishTopic, TOPIC_HA_LIGHT_STATE_ADDCID(), eeprom->mqttCID); // Can optimize this a lot if never changing CID
//			len = sprintf(json,"on,255,255-255-255,360-100");
			if (lights.on)
			{
				len = sprintf(json,"on,%u,%u-%u-%u,%u-%u",
									 lights.brightness, 
									 lights.red, 
									 lights.green, 
									 lights.blue, 
									 lights.hue, 
									 lights.saturation);
			} else {
				len = sprintf(json, "off");
			}
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
	sprintf(mqttSubscribeTopic[0], TOPIC_HA_LIGHT_COMMAND_ADDCID(), eeprom->mqttCID); // Can optimize this a lot if never changing CID
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
