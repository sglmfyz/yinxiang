#ifndef __SENSORS_H__
#define __SENSORS_H__

#include "common.h"
#include "i2c.h"

#define MAX_VOC_VALUE 100

int Sensors_Init();
int Sensors_Add_Temp(int channel, I2C_T *i2c1);
int Sensor_Start();


#endif
