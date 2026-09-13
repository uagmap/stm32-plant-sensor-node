/*
 * sht40.h
 *
 *  Created on: Sep 13, 2026
 *      Author: yarma
 */

#ifndef INC_SHT40_H_
#define INC_SHT40_H_

int sht40_read_normal(float *temp_c, float *rh_pct);
int sht40_read_heater(float *temp_c, float *rh_pct);

#endif /* INC_SHT40_H_ */
