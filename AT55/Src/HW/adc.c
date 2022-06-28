#include "adc.h"
#include "heap.h"

#define MAX_ADC_NUM 1

typedef struct {
    adc_type *hadc;
    uint16_t *value;
    int avg_num;
    int channel_num;
} adc_info_t;

static adc_info_t *adc_info;



int ADC_Buffer_Init(adc_type* hadc, int avg_num, int channel_num)
{
    int i;
    adc_info_t *info;

    zmalloc(adc_info, sizeof(adc_info_t) * MAX_ADC_NUM);

    for (i=0; i<MAX_ADC_NUM; i++) {
        if (adc_info[i].hadc == NULL) {
            adc_info[i].hadc = hadc;
            break;
        }
    }

    i_assert(i < MAX_ADC_NUM);
    info = &adc_info[i];
    info->avg_num = avg_num;
    info->channel_num = channel_num;
    zmalloc(info->value, sizeof(uint16_t) * avg_num * channel_num);
    
    return 0;
}

static adc_info_t *_get_adc_info(adc_type* hadc)
{
    int i;

    for (i=0; i<MAX_ADC_NUM; i++) {
        if (adc_info[i].hadc == hadc) {
            return &adc_info[i];   //看看怎么获取的
        }
    }

    return NULL;
}


int ADC_Get_Buffer_Size(adc_type* hadc) 
{
    adc_info_t *adc;

    adc = _get_adc_info(hadc);
    if (adc != NULL) {
        return sizeof(uint16_t) * adc->avg_num * adc->channel_num;
    } else {
        return 0;
    }
}

void *ADC_Get_Buffer(adc_type* hadc)    //这是获取adc的地址？用在了dma的
{
    adc_info_t *adc;

    adc = _get_adc_info(hadc);
    if (adc != NULL) {
        return adc->value;
    } else {
        return NULL;
    }
}

uint16_t ADC_Get(adc_type* hadc, uint8_t cid)
{
    adc_info_t *info;
    int i;
    uint32_t v = 0;

    info = _get_adc_info(hadc);
    i_assert(info);

    for (i=0; i<info->avg_num; i++) {
        v += info->value[(info->channel_num*i) + cid];  //读平均值，读取avg_num次，channel设置为开启转换通道数减一，从cid位置开始。
        //printf("value register get=%d ,i=%d		\n", info->value[(info->channel_num*i) + cid],i);
    }
	printf("adc_get value = %d     \n",v/info->avg_num);
    return v/info->avg_num;
}
void ADC_value(adc_type* hadc)
{
adc_info_t *info;
int i=0;
uint32_t v = 0;

info = _get_adc_info(hadc);
i_assert(info);
int repeat_time=info->avg_num/info->channel_num;
      for(int a=0;a<repeat_time;a++)
      {  	for(int b=0;b<=info->channel_num;b++)
     {
        printf("voc channel %d get=%d in %d	time	\n",b+1 ,info->value[(info->channel_num*i) ],a+1);
		i++;
    }
}
}
#if 0
#define BASE_VOLTAGE 3300
#define VREFINT_CAL *((volatile uint16_t *)(uint32_t)0x1FFFF7BA) 

uint16_t ADC_Get(ADC_HandleTypeDef* hadc, uint8_t cid)
{
    uint16_t raw;
    uint16_t ref, v, v1;
    adc_info_t *info;

    info = _get_adc_info(hadc);

    if (!info) {
        return 0;
    }
    ref = _get_raw(hadc, info->channel_num-1);
    raw = _get_raw(hadc, cid);
    v1 = VREFINT_CAL;

    if (ref) {
        v = raw * v1 / ref;
    } else {
        v = raw;
    }
    return v;
}
#endif


