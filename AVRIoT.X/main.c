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

#define TOPIC_SENSORSTATES 0
#define TOPIC_TEMPSENSORCONFIG 1
#define TOPIC_LIGHTSENSORCONFIG 2

static char mqttSubscribeTopic[SUBSCRIBE_TOPIC_SIZE];
static char mqttPublishTopic[3][PUBLISH_TOPIC_SIZE];
static char json[3][PAYLOAD_SIZE];
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
				sprintf(mqttPublishTopic[TOPIC_TEMPSENSORCONFIG], "homeassistant/sensor/%s_temp/config", eeprom->mqttCID); // Can optimize this a lot if never changing CID
				len = sprintf(json[TOPIC_TEMPSENSORCONFIG],
				"{\"dev_cla\": \"temperature\", \"name\": \"AVR IoT Temperature\", \"stat_t\": \"homeassistant/sensor/%s/state\", \"unit_of_meas\": \"°°C\", \"val_tpl\": \"{{ value_json.temp }}\" }", eeprom->mqttCID);
				for(uint8_t i=0;i<len;i++) // TODO: How do I fix the problem with ° == 0xC2B0 in a better way than this?
				{ 
					if (json[TOPIC_TEMPSENSORCONFIG][i] == '°')
						{
							json[TOPIC_TEMPSENSORCONFIG][i] = 0xc2; //
							json[TOPIC_TEMPSENSORCONFIG][i+1] = 0xb0;
							break;
						}
				}
				CLOUD_publishData((uint8_t*)mqttPublishTopic[TOPIC_TEMPSENSORCONFIG] ,(uint8_t*)json[TOPIC_TEMPSENSORCONFIG], len, flags);
				debug_printInfo("%s: %s", mqttPublishTopic[TOPIC_TEMPSENSORCONFIG],json[TOPIC_TEMPSENSORCONFIG]);
			} else {
				sprintf(mqttPublishTopic[TOPIC_LIGHTSENSORCONFIG], "homeassistant/sensor/%s_light/config", eeprom->mqttCID); // Can optimize this a lot if never changing CID
				len = sprintf(json[TOPIC_LIGHTSENSORCONFIG],
				"{\"dev_cla\": \"illuminance\", \"name\": \"AVR IoT Light\", \"stat_t\": \"homeassistant/sensor/%s/state\", \"unit_of_meas\": \"lx\", \"val_tpl\": \"{{ value_json.light }}\" }", eeprom->mqttCID);
				CLOUD_publishData((uint8_t*)mqttPublishTopic[TOPIC_LIGHTSENSORCONFIG] ,(uint8_t*)json[TOPIC_LIGHTSENSORCONFIG], len, flags);
				debug_printInfo("%s: %s", mqttPublishTopic[TOPIC_LIGHTSENSORCONFIG],json[TOPIC_LIGHTSENSORCONFIG]);
			}
			discover--;

		}  else {
			debug_printIoTAppMsg("Application: Sending Sensor Data");
		
			rawTemperature = SENSORS_getTempValue();
			light = SENSORS_getLightValue();

			sprintf(mqttPublishTopic[TOPIC_SENSORSTATES], "homeassistant/sensor/%s/state", eeprom->mqttCID); // Can optimize this a lot if never changing CID
			len = sprintf(json[TOPIC_SENSORSTATES],"{\"light\":%d,\"temp\":%d.%02d}", light,rawTemperature/100,abs(rawTemperature)%100);
			CLOUD_publishData((uint8_t*)mqttPublishTopic[TOPIC_SENSORSTATES] ,(uint8_t*)json[TOPIC_SENSORSTATES], len, (mqttHeaderFlags){.All= 0});
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
