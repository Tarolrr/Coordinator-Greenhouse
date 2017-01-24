/*
 * main.h
 *
 *  Created on: Oct 15, 2016
 *      Author: Tarolrr
 */

#ifndef MAIN_H_
#define MAIN_H_

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "pin_description.h"
//#include "SensorDesc.h"
//#include "ActuatorDesc.h"

#include "ChannelDesc.h"

#include "NeowayM590_Driver.h"

const uint64_t GUID[] = {
	0x848B04992EC34F28,
	0x95DB3576EFA594A0
};


//constant for LoRa driver

const uint8_t NetworkID = 0x01;
const uint16_t LoRaAddress = 0x0000;

#define LoRaDeviceType_Sensor		0x0000
#define LoRaDeviceType_Relay		0x8000
#define LoRaRelayType_Cold			0x0000
#define LoRaRelayType_Hot			0x4000


enum{
	SensorState_Normal			= 0x00,
	SensorState_Low				= 0x01,
	SensorState_High			= 0x02,
	SensorState_TooMuch			= 0x04
}typedef SensorState;


/*enum{
	ChannelState_Normal			= 0x00,
	ChannelState_Low			= 0x01,
	ChannelState_High			= 0x02,
	ChannelState_LowHighError	= 0x03,	//set if some sensors show too low and too high value simultaneously
	ChannelState_TooLongError	= 0x04,	//set if controlled parameter doesn't return to specified boundaries in fixed amount of time
	ChannelState_TooMuchError	= 0x08,	//set if controlled parameter value is out of boundaries more than specified amount
}typedef ChannelState;*/

enum{
	ChannelFlag_ColdOn			= 0x01,
	ChannelFlag_ColdOff			= 0x02,
	ChannelFlag_HotOn			= 0x04,
	ChannelFlag_HotOff			= 0x08,
	ChannelFlag_Low				= 0x10,
	ChannelFlag_High			= 0x20,
	ChannelFlag_LowHighError	= 0x30,	//set if some sensors show too low and too high value simultaneously
	ChannelFlag_TooLongError	= 0x40,	//set if controlled parameter doesn't return to specified boundaries in fixed amount of time
	ChannelFlag_TooMuchError	= 0x80,	//set if controlled parameter value is out of boundaries more than specified amount
}typedef ChannelFlag;

#define GSMModemPass 				"12345678"

#define ChannelCount 				1
#define SensorCount					4
#define RelayCount					2
#define DefaultThresholdTempLow 	17
#define DefaultThresholdTempHigh 	28
#define ThresholdWindow				1
#define MaximumWorkingTimeInMinutes 10	//TODO need some normal names!!!
#define ConnectTimeoutInMinutes		12	//TODO need some normal names!!!
#define StatusSMSPeriodInMinutes	20	//TODO need some normal names!!!
#define MinimumRxTimeout 200
#define LCDUpdatePeriodMs			2000
#define RetryCount 5

const uint16_t AddrSensors[SensorCount] = {
	1|LoRaDeviceType_Sensor,
	2|LoRaDeviceType_Sensor,
	3|LoRaDeviceType_Sensor,
	4|LoRaDeviceType_Sensor,
};



// MSB of relay address should be 1, MSB-1 of relay address is for relay type (1 - "hot" relay, 0 - "cold" relay)
const uint16_t AddrRelays[RelayCount] = {
	1|LoRaDeviceType_Relay|LoRaRelayType_Cold,
	1|LoRaDeviceType_Relay|LoRaRelayType_Hot,
};

/*bool ConnectionTable[SensorMaxNumber][ActuatorMaxNumber];
SensorDesc sensorDesc[SensorMaxNumber];
ActuatorDesc actuatorDesc[ActuatorMaxNumber];

ChannelDesc channels[ChannelNumber];*/

float ThresholdTempLow, ThresholdTempHigh;

#endif /* MAIN_H_ */
