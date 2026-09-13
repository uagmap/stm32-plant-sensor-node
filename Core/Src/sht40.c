/*
 * sht40.c
 *
 *  Created on: Sep 13, 2026
 *      Author: yarma
 */

#include "main.h"
#include "sht40.h"

extern I2C_HandleTypeDef hi2c1;

#define SHT40_ADDR 0x44U
#define SHT40_CMD_MEAS_HIGH 0xFDU //high precision measure command
#define SHT40_CMD_HEATER_LOW 0x15U //heater lowest power + measure
#define SHT40_DELAY_MEAS 10U //delay after measuring
#define SHT40_DELAY_HEATER 120U //delay needed after heater

//checksum
static uint8_t sht40_crc8(const uint8_t *data, uint8_t len)
{
	uint8_t crc = 0xFF;

	for (uint8_t i = 0; i < len; i++)
	{
		crc ^= data[i];
		for (uint8_t bit = 0; bit < 8; bit++)
		{
			if (crc & 0x80U)
			{
				crc = (uint8_t)((crc << 1) ^ 0x31U);
			}
			else
			{
				crc = (uint8_t)(crc << 1);
			}
		}
	}
	return crc;
}

static int sht40_measure(uint8_t cmd, uint32_t wait_ms, float *temp_c, float *rh_pct)
{
	uint8_t data[6]; //data package: [t][t][t_crc][rh][rh][rh_crc]

	if (HAL_I2C_Master_Transmit(&hi2c1, (uint16_t)(SHT40_ADDR << 1), &cmd, 1, 100) != HAL_OK)
	{
		return -1;
	}

	uint32_t start = HAL_GetTick();
	while ((HAL_GetTick() - start) < wait_ms)
	{
	}

	if (HAL_I2C_Master_Receive(&hi2c1, (uint16_t)(SHT40_ADDR << 1), data, 6, 100) != HAL_OK)
	{
		return -1;
	}

	if (sht40_crc8(&data[0], 2) != data[2])
	{
		return -2;   /* temperature CRC bad */
	}
	if (sht40_crc8(&data[3], 2) != data[5])
	{
		return -2;   /* humidity CRC bad */
	}

	uint16_t t_ticks = ((uint16_t)data[0] << 8) | data[1]; //assemble bytes of data into one number
	uint16_t rh_ticks = ((uint16_t)data[3] << 8) | data[4];

	//convert to real data according to datasheet formula
	*temp_c = -45.0f + 175.0f * (float)t_ticks / 65535.0f;
	*rh_pct = -6.0f + 125.0f * (float)rh_ticks / 65535.0f;

	if (*rh_pct > 100.0f) *rh_pct = 100.0f;
	if (*rh_pct < 0.0f) *rh_pct = 0.0f;

	return 0;
}

int sht40_read_normal(float *temp_c, float *rh_pct)
{
	return sht40_measure(SHT40_CMD_MEAS_HIGH, SHT40_DELAY_MEAS, temp_c, rh_pct);
}

int sht40_read_heater(float *temp_c, float *rh_pct)
{
	return sht40_measure(SHT40_CMD_HEATER_LOW, SHT40_DELAY_HEATER, temp_c, rh_pct);
}

int sht40_read_normal_sample(sensor_sample_t *out)
{
	if (out == NULL)
	{
		return -1;
	}

	out->tick_ms = HAL_GetTick();
	out->source = 0U;
	out->status = sht40_measure(SHT40_CMD_MEAS_HIGH, SHT40_DELAY_MEAS, &out->temp_c, &out->rh_pct);
	return out->status;
}

int sht40_read_heater_sample(sensor_sample_t *out)
{
	if (out == NULL)
	{
		return -1;
	}

	out->tick_ms = HAL_GetTick();
	out->source = 1U;
	out->status = sht40_measure(SHT40_CMD_HEATER_LOW, SHT40_DELAY_HEATER, &out->temp_c, &out->rh_pct);
	return out->status;
}
