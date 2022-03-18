#ifndef IOT_SENSOR_NODE_CONFIG_H
#define IOT_SENSOR_NODE_CONFIG_H

#define CFG_SEND_INTERVAL 1

#define CFG_TIMEOUT 5000

#define CFG_ENABLE_CLI (1)

#ifdef DEBUG
	#define CFG_DEBUG_PRINT (4)
	
	#define ENABLE_DEBUG_IOT_APP_MSGS (1)
#else
	#define CFG_DEBUG_PRINT (0)
	
	#define ENABLE_DEBUG_IOT_APP_MSGS (0)
#endif

#define CFG_NTP_SERVER    ("*.pool.ntp.org")

#endif // IOT_SENSOR_NODE_CONFIG_H
