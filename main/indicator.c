#include "indicator.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

static void Indicator_Task(void *param)
{
    Indicator_t *ind = (Indicator_t *)param;

    while (1) {
        switch (ind->state) {
            case INDICATOR_OFF:
                gpio_set_level(ind->gpio_num, 0);
                vTaskDelay(pdMS_TO_TICKS(100));
                break;

            case INDICATOR_ON:
                gpio_set_level(ind->gpio_num, 1);
                vTaskDelay(pdMS_TO_TICKS(100));
                break;

            case INDICATOR_BLINK:
                gpio_set_level(ind->gpio_num, 1);
                vTaskDelay(pdMS_TO_TICKS(500));
                gpio_set_level(ind->gpio_num, 0);
                vTaskDelay(pdMS_TO_TICKS(500));
                break;
        }
    }
}

void Indicator_Init(Indicator_t *indicator, uint8_t gpio_num)
{
    indicator->gpio_num = gpio_num;
    indicator->state = INDICATOR_OFF;

    gpio_reset_pin(gpio_num);
    gpio_set_direction(gpio_num, GPIO_MODE_OUTPUT);
    gpio_set_level(gpio_num, 0);

    xTaskCreate(Indicator_Task, "Indicator_Task", 2048, indicator, 5, NULL);
}

void Indicator_Control(Indicator_t *indicator, Indicator_State_t state)
{
    indicator->state = state;
}
