/*
 * SensorDesc.c
 *
 *  Created on: Nov 18, 2016
 *      Author: Tarolrr
 */

#include "SensorDesc.h"

bool AddSensor(uint16_t address){
/*	uint8_t idx;
	for(idx = 0; idx < SensorMaxNumber; idx++){
		if(sensorDesc[idx].address == 0){
			sensorDesc[idx].address = address;
			return true;
		}
	}*/
	return false;
}

bool DeleteSensor(uint16_t address){
	/*uint8_t idx;
	for(idx = 0; idx < SensorMaxNumber; idx++){
		if(sensorDesc[idx].address == address){
			sensorDesc[idx].address = 0;
			return true;
		}
	}*/
	return false;
}

