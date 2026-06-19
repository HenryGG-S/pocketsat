#ifndef MPU6050_H
#define MPU6050_H

#include "app_types.h"
#include <stdint.h>

#define MPU6050_I2C_ADDR_7BIT (0x68u)
#define MPU6050_I2C_ADDR_HAL (MPU6050_I2C_ADDR_7BIT << 1)

SensorStatus mpu6050_read_whoami(uint8_t *whoami);
SensorStatus mpu6050_init_sensor(void);
SensorStatus mpu6050_read_sample(ImuSample *sample);

#endif
