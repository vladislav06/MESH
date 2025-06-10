//
// Created by vm on 25.10.6.
//

#include "actuatorImpl.h"
#include "macros.h"

#include "hw.h"

#define CH(X) if(ch==X)
#define QUALIFIER(X) ||X==hw_id()
#define NODE(...) if(0 FOR_EACH(QUALIFIER,__VA_ARGS__) )


void actuator_impl_init() {
    NODE(0x4f4d) {
        GPIO_InitTypeDef GPIO_InitStruct = {0};
        GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_3 | GPIO_PIN_5 | GPIO_PIN_7;
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
        GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_2 | GPIO_PIN_4 | GPIO_PIN_6;
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    }
}

#define MAP(X, PO, PI)  if ((float)data >= ((1000.f / 8.f) * (float)X)) {     \
                            HAL_GPIO_WritePin(PO,PI,1);   \
                        }else{                           \
                            HAL_GPIO_WritePin(PO,PI,0);   \
                            }

void actuator_process(uint8_t ch, uint16_t data) {
    NODE(0x4f4d) {
        CH(0) {
            //map 0-255 to outputs
            MAP(1, GPIOC, GPIO_PIN_0)
            MAP(2, GPIOC, GPIO_PIN_2)
            MAP(3, GPIOC, GPIO_PIN_4)
            MAP(4, GPIOC, GPIO_PIN_6)
            MAP(5, GPIOB, GPIO_PIN_7)
            MAP(6, GPIOB, GPIO_PIN_5)
            MAP(7, GPIOB, GPIO_PIN_3)
            MAP(8, GPIOB, GPIO_PIN_1)
        }
    }
}
