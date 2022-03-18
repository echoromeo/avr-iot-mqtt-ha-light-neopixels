/*
    \file   mqtt_config.h

    \brief  MQTT Configuration File

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

#ifndef MQTT_CONFIG_H
#define	MQTT_CONFIG_H


#include <stdint.h>


/********************MQTT Client configurations***********************/
#define SUBSCRIBE_TOPIC_SIZE        100     //Defines the topic length that is supported when we process a published packet from the cloud
#define PUBLISH_TOPIC_SIZE          80      //Defines the topic length that is supported when we send a publish packet to the cloud
#define PAYLOAD_SIZE                768     //Defines the payload size that is supported when we process a packet
#define MQTT_CID_LENGTH				41
#define NUM_TOPICS_SUBSCRIBE        1                       //Defines number of topics which must(?) be subscribed
#define NUM_TOPICS_UNSUBSCRIBE      NUM_TOPICS_SUBSCRIBE	// The MQTT client can unsubscribe only from those topics to which it has already subscribed

// MCC generated parameters
#define CFG_MQTT_PORT 1883
#define CFG_MQTT_HOSTURL "192.168.1.10"
#define CFG_MQTT_SERVERIPv4_HEX
#define CFG_MQTT_CONN_TIMEOUT 60
#define CFG_MQTT_TXBUFFER_SIZE (PUBLISH_TOPIC_SIZE+PAYLOAD_SIZE+2)
#define CFG_MQTT_RXBUFFER_SIZE (SUBSCRIBE_TOPIC_SIZE+PAYLOAD_SIZE+2)
#define CFG_MQTT_USERNAME "mchpUser"
#define CFG_MQTT_PASSWORD "microchip"
#define CFG_MQTT_CID "" // If empty it will be filled with the ECC608 serial
#define CFG_QOS 0
#define TCPIP_BSD 1

/********************MQTT Client configurations*(END)***********************/


/********************Timeout Driver for MQTT definitions***********************/
#ifdef TCPIP_BSD
#include "../drivers/timeout.h"
#define timerstruct_t                   timerStruct_t
#define htons(a)                        (uint16_t)((((uint16_t) (a)) << 8) | (((uint16_t) (a)) >> 8))
#define ntohs(a)                        (uint16_t)((((uint16_t) (a)) << 8) | (((uint16_t) (a)) >> 8)) // Socket.h remapped to htons

// Timeout is calculated on the basis of clock frequency.
// This macro needs to be changed in accordance with the clock frequency.
#define SECONDS (uint32_t)720 // I do not understand this number. the RTC frequency is 1k, so it should be 1024???
#endif /* TCPIP_BSD */

#ifdef TCPIP_LITE
#define absolutetime_t                  uint32_t
#define timerstruct_t                   timerStruct_t

// Timeout is calculated on the basis of clock frequency.
// This macro needs to be changed in accordance with the clock frequency.
#define SECONDS (uint32_t)720 // I do not understand this number. the RTC frequency is 1k, so it should be 1024???
#endif /* TCPIP_LITE */

/*******************Timeout Driver for MQTT definitions*(END)******************/


#endif	/* MQTT_CONFIG_H */
