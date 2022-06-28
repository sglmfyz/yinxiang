#ifndef __ADC_H__
#define __ADC_H__

#include "common.h"

int ADC_Buffer_Init(adc_type* hadc, int avg_num, int channel_num);
uint16_t ADC_Get(adc_type* hadc, uint8_t cid);
void ADC_value(adc_type* hadc);

int ADC_Get_Buffer_Size(adc_type* hadc);
void *ADC_Get_Buffer(adc_type* hadc);



#endif
