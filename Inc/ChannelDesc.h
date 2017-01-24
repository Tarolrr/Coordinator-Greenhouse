/*
 * ChannelDesc.h
 *
 *  Created on: Nov 26, 2016
 *      Author: Tarolrr
 */

#ifndef CHANNELDESC_H_
#define CHANNELDESC_H_

#include "SensorDesc.h"
#include "ActuatorDesc.h"

struct{
	SensorDesc sensors[SensorMaxNumber];
	ActuatorDesc actuators[ActuatorMaxNumber];
	float threshold[2];
} typedef ChannelDesc;

#endif /* CHANNELDESC_H_ */
