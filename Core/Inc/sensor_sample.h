/*
 * sensor_sample.h
 *
 *  Created on: Sep 13, 2026
 *      Author: yarma
 */

#ifndef INC_SENSOR_SAMPLE_H_
#define INC_SENSOR_SAMPLE_H_

#include <stdint.h>

typedef struct
{
	uint32_t tick_ms;
	float temp_c;
	float rh_pct;
	int status; // 0 - OK, -1 - I2C, -2 - CRC
	uint8_t source; //0 - normal, 1 - heater
} sensor_sample_t;

#endif /* INC_SENSOR_SAMPLE_H_ */
