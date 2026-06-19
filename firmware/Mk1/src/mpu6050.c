#include "mpu6050.h"
#include "app_config.h"
#include "board_nucleo_f401re.h"

#define MPU6050_REG_GYRO_CONFIG (0x1Bu)
#define MPU6050_REG_ACCEL_CONFIG (0x1Cu)
#define MPU6050_REG_ACCEL_XOUT_H (0x3Bu)
#define MPU6050_REG_PWR_MGMT_1 (0x6Bu)
#define MPU6050_REG_WHO_AM_I (0x75u)

#define MPU6050_EXPECTED_WHO_AM_I (0x68u)

static HAL_StatusTypeDef mpu6050_write_register(uint8_t reg, uint8_t value);
static int16_t combine_i16(uint8_t high, uint8_t low);

static HAL_StatusTypeDef mpu6050_write_register(uint8_t reg, uint8_t value)
{
    HAL_StatusTypeDef status = board_i2c_mem_write(MPU6050_I2C_ADDR_HAL,
                                                   reg,
                                                   I2C_MEMADD_SIZE_8BIT,
                                                   &value,
                                                   1u,
                                                   I2C_TIMEOUT_MS);

    if (status != HAL_OK)
    {
        board_i2c1_recover();

        status = board_i2c_mem_write(MPU6050_I2C_ADDR_HAL,
                                     reg,
                                     I2C_MEMADD_SIZE_8BIT,
                                     &value,
                                     1u,
                                     I2C_TIMEOUT_MS);
    }

    return status;
}

SensorStatus mpu6050_read_whoami(uint8_t *whoami)
{
    HAL_StatusTypeDef status = board_i2c_mem_read(MPU6050_I2C_ADDR_HAL,
                                                  MPU6050_REG_WHO_AM_I,
                                                  I2C_MEMADD_SIZE_8BIT,
                                                  whoami,
                                                  1u,
                                                  I2C_TIMEOUT_MS);

    if (status != HAL_OK)
    {
        board_i2c1_recover();

        board_delay_ms(SENSOR_RETRY_DELAY_MS);

        status = board_i2c_mem_read(MPU6050_I2C_ADDR_HAL,
                                    MPU6050_REG_WHO_AM_I,
                                    I2C_MEMADD_SIZE_8BIT,
                                    whoami,
                                    1u,
                                    I2C_TIMEOUT_MS);
    }

    if (status != HAL_OK)
    {
        *whoami = 0u;
        return SENSOR_MISSING;
    }

    if (*whoami != MPU6050_EXPECTED_WHO_AM_I)
    {
        return SENSOR_BAD_ID;
    }

    return SENSOR_OK;
}

SensorStatus mpu6050_init_sensor(void)
{
    uint8_t whoami = 0u;
    SensorStatus status = mpu6050_read_whoami(&whoami);

    if (status != SENSOR_OK)
    {
        return status;
    }

    /* Wake sensor: clear SLEEP bit in PWR_MGMT_1. */
    if (mpu6050_write_register(MPU6050_REG_PWR_MGMT_1, 0x00u) != HAL_OK)
    {
        return SENSOR_MISSING;
    }

    board_delay_ms(SENSOR_WAKE_DELAY_MS);

    /* +/-250 deg/s gyro, +/-2g accel. Keep first telemetry raw. */
    if (mpu6050_write_register(MPU6050_REG_GYRO_CONFIG, 0x00u) != HAL_OK)
    {
        return SENSOR_MISSING;
    }

    if (mpu6050_write_register(MPU6050_REG_ACCEL_CONFIG, 0x00u) != HAL_OK)
    {
        return SENSOR_MISSING;
    }

    return SENSOR_OK;
}

static int16_t combine_i16(uint8_t high, uint8_t low)
{
    return (int16_t)(((uint16_t)high << 8) | (uint16_t)low);
}

SensorStatus mpu6050_read_sample(ImuSample *sample)
{
    uint8_t data[14] = {0};

    HAL_StatusTypeDef status = board_i2c_mem_read(MPU6050_I2C_ADDR_HAL,
                                                  MPU6050_REG_ACCEL_XOUT_H,
                                                  I2C_MEMADD_SIZE_8BIT,
                                                  data,
                                                  sizeof(data),
                                                  I2C_TIMEOUT_MS);

    if (status != HAL_OK)
    {
        board_i2c1_recover();

        status = board_i2c_mem_read(MPU6050_I2C_ADDR_HAL,
                                    MPU6050_REG_ACCEL_XOUT_H,
                                    I2C_MEMADD_SIZE_8BIT,
                                    data,
                                    sizeof(data),
                                    I2C_TIMEOUT_MS);
    }

    if (status != HAL_OK)
    {
        return SENSOR_MISSING;
    }

    sample->ax = combine_i16(data[0], data[1]);
    sample->ay = combine_i16(data[2], data[3]);
    sample->az = combine_i16(data[4], data[5]);
    sample->temp = combine_i16(data[6], data[7]);
    sample->gx = combine_i16(data[8], data[9]);
    sample->gy = combine_i16(data[10], data[11]);
    sample->gz = combine_i16(data[12], data[13]);

    return SENSOR_OK;
}
