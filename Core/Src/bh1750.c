/*
 * bh1750.c
 *
 *  Created on: Oct 11, 2026
 *      Author: yarma
 */


#include "main.h"
#include "bh1750.h"

extern I2C_HandleTypeDef hi2c1;

#define BH1750_ADDR 			0x23U
#define BH1750_CMD_MEAS_HIGH 	0x20U//high precision measure command
#define BH1750_DELAY_MEAS 		180U //delay after measuring

const char *bh1750_status_str(bh1750_status_t status)
{
	switch(status)
	{
	case BH1750_OK:			return "OK";
	case BH1750_ERR_I2C:	return "I2C";
	case BH1750_ERR_PARAM:	return "PARAM";
	default:				return "UNKNOWN";
	}
}

static bh1750_status_t bh1750_measure(uint8_t cmd, uint32_t wait_ms, float *lux)
{
	uint8_t data[2]; //data package: [lux][lux], no crc

	if (lux == NULL)
		return BH1750_ERR_PARAM;

	if (HAL_I2C_Master_Transmit(&hi2c1, (uint16_t)(BH1750_ADDR << 1), &cmd, 1, 100) != HAL_OK)
		{
			return BH1750_ERR_I2C;
		}

	uint32_t start = HAL_GetTick();
	while ((HAL_GetTick() - start) < wait_ms)
	{
	}

	if (HAL_I2C_Master_Receive(&hi2c1, (uint16_t)(BH1750_ADDR << 1), data, 2, 100) != HAL_OK)
		{
			return BH1750_ERR_I2C;
		}

	uint16_t count = ((uint16_t)data[0] << 8) | data[1];

	*lux = (float)count / 1.2f;

	return BH1750_OK;
}

bh1750_status_t bh1750_read_sample(bh1750_sample_t *out)
{
	if (out == NULL)
		return BH1750_ERR_PARAM;

	out->tick_ms = HAL_GetTick();
	out->status = bh1750_measure(BH1750_CMD_MEAS_HIGH, BH1750_DELAY_MEAS, &out->lux);
	return out->status;
}
