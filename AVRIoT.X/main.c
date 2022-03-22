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
#include <math.h>
#include "main.h"
#include "libavr_neopixel_uart/neopixel.h"

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
#define TOPIC_HA_LIGHT_ATTRIBUTES_ADDCID(extra_id)		HA_TOPIC_ADDCID("light") extra_id "/attr"

#define CONFIG_HA(content)								"{" content "}"
#define CONFIG_HA_PREFIX_ADDCID(type)					"\"~\": \"" HA_TOPIC_ADDCID(type) "\""
#define CONFIG_HA_ID_ADDCID(name)						"\"uniq_id\":\"" name "%s\",\"obj_id\":\"" name "%s\""
#define CONFIG_HA_STATE(extra_id)						"\"stat_t\":\"~" extra_id "/state\""
#define CONFIG_HA_COMMAND(extra_id)						"\"cmd_t\":\"~" extra_id "/set\""
#define CONFIG_HA_ATTRIBUTES(extra_id)					"\"json_attr_t\":\"~" extra_id "/attr\""
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
#define CONFIG_HA_THIS_DEVICE							CONFIG_HA_DEVICE( CONFIG_HA_DEVICE_IDENTIFIERS("[\"neopixel_%s\"]") \
														NEXTCFG CONFIG_HA_DEVICE_NAME("AVR-IoT") \
														NEXTCFG CONFIG_HA_DEVICE_MANUFACTURER("Microchip Technology") \
														NEXTCFG CONFIG_HA_DEVICE_MODEL("AVR-IoT Neopixel Light") \
														NEXTCFG CONFIG_HA_DEVICE_SW_VERSION("7.1.0"))

static char mqttSubscribeTopic[NUM_TOPICS_SUBSCRIBE][SUBSCRIBE_TOPIC_SIZE];
static char mqttPublishTopic[PUBLISH_TOPIC_SIZE];
static char json[PAYLOAD_SIZE];
static uint8_t discover = 10;

typedef struct control_struct {
	uint8_t changed;
	uint8_t on;
	uint8_t brightness;
	color_t rgb;
	float hue;
	float saturation;

} control_t;

static volatile control_t control = {
	.changed = 3,
	.on = false,
	.brightness = 20,
	.hue = 360,
	.saturation = 100
};

static volatile color_t lights = {{0}};
static volatile uint16_t prev_num_leds = 0;

static const uint8_t NeoPixelGammaTable_Adafruit[256] = {
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   1,   1,   1,   1,   1,
	1,   1,   1,   1,   1,   1,   2,   2,   2,   2,   2,   2,   2,   2,   3,
	3,   3,   3,   3,   3,   4,   4,   4,   4,   5,   5,   5,   5,   5,   6,
	6,   6,   6,   7,   7,   7,   8,   8,   8,   9,   9,   9,   10,  10,  10,
	11,  11,  11,  12,  12,  13,  13,  13,  14,  14,  15,  15,  16,  16,  17,
	17,  18,  18,  19,  19,  20,  20,  21,  21,  22,  22,  23,  24,  24,  25,
	25,  26,  27,  27,  28,  29,  29,  30,  31,  31,  32,  33,  34,  34,  35,
	36,  37,  38,  38,  39,  40,  41,  42,  42,  43,  44,  45,  46,  47,  48,
	49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,  63,
	64,  65,  66,  68,  69,  70,  71,  72,  73,  75,  76,  77,  78,  80,  81,
	82,  84,  85,  86,  88,  89,  90,  92,  93,  94,  96,  97,  99,  100, 102,
	103, 105, 106, 108, 109, 111, 112, 114, 115, 117, 119, 120, 122, 124, 125,
	127, 129, 130, 132, 134, 136, 137, 139, 141, 143, 145, 146, 148, 150, 152,
	154, 156, 158, 160, 162, 164, 166, 168, 170, 172, 174, 176, 178, 180, 182,
	184, 186, 188, 191, 193, 195, 197, 199, 202, 204, 206, 209, 211, 213, 215,
	218, 220, 223, 225, 227, 230, 232, 235, 237, 240, 242, 245, 247, 250, 252,
255};

void update_led_config(void);
uint24_t NeoPixel_HSV_Adafruit_Float(float ha_hue, float ha_sat, uint8_t val);

int main(void)
{
	application_init();
	neopixel_init();

	while (1)
	{
		if (control.changed & 1)
		{
			control.changed &= ~1;
			update_led_config();
			neopixel_configure_constant(lights, eeprom->num_leds);
		}
		 
		runScheduler();  
		if (!shared_networking_params.haveAPConnection)
		{
			discover = 10;
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
			#ifdef DEBUG
				flags.retain = 0;
			#else
				flags.retain = 1;
			#endif

			if (discover == 5) {
				debug_printIoTAppMsg("Application: Sending Discover Config %");

				sprintf(mqttPublishTopic, TOPIC_HA_LIGHT_CONFIG_ADDCID("_light"), eeprom->mqttCID); // Can optimize this a lot if never changing CID
				len = sprintf(json, CONFIG_HA(CONFIG_HA_PREFIX_ADDCID("light") // %s = eeprom->mqttCID
										NEXTCFG CONFIG_HA_DEVICE_NAME("AVR-IoT Neopixel Light") 
										NEXTCFG CONFIG_HA_ID_ADDCID("neopixel_") // 2x %s = eeprom->mqttCID
										NEXTCFG CONFIG_HA_STATE() 
										NEXTCFG CONFIG_HA_COMMAND()
										NEXTCFG CONFIG_HA_ATTRIBUTES()
										NEXTCFG CONFIG_HA_LIGHT_TEMPLATES
										NEXTCFG CONFIG_HA_THIS_DEVICE), // %s = eeprom->mqttCID
									eeprom->mqttCID,
									eeprom->mqttCID,
									eeprom->mqttCID,
									eeprom->mqttCID);
			}

		}  else if (control.changed == 2) { //Update neopixels before sending update
			control.changed = 0;
			
			debug_printIoTAppMsg("Application: Sending State");

			sprintf(mqttPublishTopic, TOPIC_HA_LIGHT_STATE_ADDCID(), eeprom->mqttCID); // Can optimize this a lot if never changing CID
			if (control.on)
			{
				len = sprintf(json,"on,%u,%u-%u-%u,-", // Looks like I do not need to send back the floats
									 control.brightness, 
									 control.rgb.r, 
									 control.rgb.g, 
									 control.rgb.b);
			} else {
				len = sprintf(json, "off");
			}
		} else if (eeprom->num_leds != prev_num_leds) { // Update attributes (Number of NeoPixels)
			sprintf(mqttPublishTopic, TOPIC_HA_LIGHT_ATTRIBUTES_ADDCID(), eeprom->mqttCID); // Can optimize this a lot if never changing CID
			len = sprintf(json,"{\"NeoPixels\":%d}", eeprom->num_leds);
			prev_num_leds = eeprom->num_leds;
			control.changed |= 1;
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

	
	if (payload[1] == 'f') // off
	{
		control.on = false;
	} 
	else // on with brightness or RGB (not both at the same time)
	{ 	
		char * next_string;

		//strtok does not handle two delimiters in a row, so checking for "on,," and for "on,,--,-"
		if(payload[3] != ',') {
			next_string = strtok((char *)&payload[3], ",");		
			control.brightness = atoi(next_string);
		} else if(payload[4] != '-') {
			next_string = strtok((char *)&payload[4], "-");
			control.rgb.r = atoi(next_string);
			next_string = strtok(NULL, "-");
			control.rgb.g = atoi(next_string);
			next_string = strtok(NULL, ",");
			control.rgb.b = atoi(next_string);
			next_string = strtok(NULL, "-");
			control.hue = atof(next_string);
			next_string = strtok(NULL, "-");
			control.saturation = atof(next_string);
		}

		control.on = true;
	}
	control.changed = 3;
	
	ledParameterRed.onTime = LED_BLIP;
	ledParameterRed.offTime = LED_BLIP;
	LED_control(&ledParameterRed);
}

void update_led_config(void) {

	if (control.on)
	{
		control.rgb.channel = NeoPixel_HSV_Adafruit_Float(control.hue,control.saturation,255);
		lights.channel = NeoPixel_HSV_Adafruit_Float(control.hue,control.saturation,control.brightness);
		for (uint8_t i = 0; i < NEOPIXEL_LED_BYTES; i++)
		{
			lights.array[i] = NeoPixelGammaTable_Adafruit[lights.array[i]];
		}
	}
	else 
	{
		lights.channel = 0;
	}
}

/*
 * Ported the Adafruit HSV to 0-360 degrees, 0-100 saturation floats. brightness is still 0-255.
 * Original can be found here:
 * https://github.com/adafruit/Adafruit_NeoPixel/blob/2a33fdf9a075121a94f478d24c999602cc4a0dbf/Adafruit_NeoPixel.cpp#L3212
 */
uint24_t NeoPixel_HSV_Adafruit_Float(float ha_hue, float ha_sat, uint8_t val) {

	uint8_t r, g, b;

	// Remap 0-65535 to 0-1529. Pure red is CENTERED on the 64K rollover;
	// 0 is not the start of pure red, but the midpoint...a few values above
	// zero and a few below 65536 all yield pure red (similarly, 32768 is the
	// midpoint, not start, of pure cyan). The 8-bit RGB hexcone (256 values
	// each for red, green, blue) really only allows for 1530 distinct hues
	// (not 1536, more on that below), but the full unsigned 16-bit type was
	// chosen for hue so that one's code can easily handle a contiguous color
	// wheel by allowing hue to roll over in either direction.
	uint16_t hue = round(ha_hue * (1530.0 / 360.0));
	uint8_t sat = round(ha_sat * (255.0/100.0));
	// Because red is centered on the rollover point (the +32768 above,
	// essentially a fixed-point +0.5), the above actually yields 0 to 1530,
	// where 0 and 1530 would yield the same thing. Rather than apply a
	// costly modulo operator, 1530 is handled as a special case below.

	// So you'd think that the color "hexcone" (the thing that ramps from
	// pure red, to pure yellow, to pure green and so forth back to red,
	// yielding six slices), and with each color component having 256
	// possible values (0-255), might have 1536 possible items (6*256),
	// but in reality there's 1530. This is because the last element in
	// each 256-element slice is equal to the first element of the next
	// slice, and keeping those in there this would create small
	// discontinuities in the color wheel. So the last element of each
	// slice is dropped...we regard only elements 0-254, with item 255
	// being picked up as element 0 of the next slice. Like this:
	// Red to not-quite-pure-yellow is:        255,   0, 0 to 255, 254,   0
	// Pure yellow to not-quite-pure-green is: 255, 255, 0 to   1, 255,   0
	// Pure green to not-quite-pure-cyan is:     0, 255, 0 to   0, 255, 254
	// and so forth. Hence, 1530 distinct hues (0 to 1529), and hence why
	// the constants below are not the multiples of 256 you might expect.

	// Convert hue to R,G,B (nested ifs faster than divide+mod+switch):
	if (hue < 510) { // Red to Green-1
		b = 0;
		if (hue < 255) { //   Red to Yellow-1
			r = 255;
			g = hue;       //     g = 0 to 254
			} else {         //   Yellow to Green-1
			r = 510 - hue; //     r = 255 to 1
			g = 255;
		}
		} else if (hue < 1020) { // Green to Blue-1
		r = 0;
		if (hue < 765) { //   Green to Cyan-1
			g = 255;
			b = hue - 510;  //     b = 0 to 254
			} else {          //   Cyan to Blue-1
			g = 1020 - hue; //     g = 255 to 1
			b = 255;
		}
		} else if (hue < 1530) { // Blue to Red-1
		g = 0;
		if (hue < 1275) { //   Blue to Magenta-1
			r = hue - 1020; //     r = 0 to 254
			b = 255;
			} else { //   Magenta to Red-1
			r = 255;
			b = 1530 - hue; //     b = 255 to 1
		}
		} else { // Last 0.5 Red (quicker than % operator)
		r = 255;
		g = b = 0;
	}

	// Apply saturation and value to R,G,B, pack into 32-bit result:
	uint32_t v1 = 1 + val;  // 1 to 256; allows >>8 instead of /255
	uint16_t s1 = 1 + sat;  // 1 to 256; same reason
	uint8_t s2 = 255 - sat; // 255 to 0
	
	color_t output = {.r = (uint32_t)(((((r * s1) >> 8) + s2) * v1) >> 8),
					  .g = (uint32_t)(((((g * s1) >> 8) + s2) * v1) >> 8),
					  .b = (uint32_t)(((((b * s1) >> 8) + s2) * v1) >> 8)};
					  
	return output.channel;
}