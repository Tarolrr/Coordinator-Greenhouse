/*
 * SensorDesc.h
 *
 *  Created on: Nov 18, 2016
 *      Author: Tarolrr
 */

#ifndef SENSORDESC_H_
#define SENSORDESC_H_

#include <stdint.h>
#include <stdbool.h>

#define SensorMaxNumber 10

struct{
	uint16_t address;
	float threshold[2];
}typedef SensorDesc;

//bool AddSensor(uint16_t address);
//bool DeleteSensor(uint16_t address);

#endif /* SENSORDESC_H_ */
