#ifndef INDICATOR_H
#define INDICATOR_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    INDICATOR_OFF = 0,
    INDICATOR_ON,
    INDICATOR_BLINK
} Indicator_State_t;

typedef struct {
    uint8_t gpio_num;
    Indicator_State_t state;
} Indicator_t;

void Indicator_Init(Indicator_t *indicator, uint8_t gpio_num);

void Indicator_Control(Indicator_t *indicator, Indicator_State_t state);

#endif 
