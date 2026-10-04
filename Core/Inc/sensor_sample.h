/*
 * sensor_sample.h
 *
 *  Created on: Sep 13, 2026
 *      Author: yarma
 */

#ifndef INC_SENSOR_SAMPLE_H_
#define INC_SENSOR_SAMPLE_H_

#include <stdint.h>

typedef enum
{
	SHT40_OK = 0,
	SHT40_ERR_I2C = -1,
	SHT40_ERR_CRC = -2,
	SHT40_ERR_PARAM = -3
} sht40_status_t;

typedef struct
{
	uint32_t tick_ms;
	float temp_c;
	float rh_pct;
	sht40_status_t status;
	uint8_t source; //0 - normal, 1 - heater
} sensor_sample_t;

const char *sht40_status_str(sht40_status_t status);

#endif /* INC_SENSOR_SAMPLE_H_ */
