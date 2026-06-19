#ifndef APP_TYPES_H
#define APP_TYPES_H

#include <stdint.h>

typedef enum
{
    MODE_BOOT = 0,
    MODE_SAFE,
    MODE_NOMINAL,
    MODE_PAYLOAD
} SystemMode;

typedef enum
{
    SENSOR_OK = 0,
    SENSOR_MISSING,
    SENSOR_BAD_ID
} SensorStatus;

typedef struct
{
    int16_t ax;
    int16_t ay;
    int16_t az;
    int16_t temp;
    int16_t gx;
    int16_t gy;
    int16_t gz;
} ImuSample;

typedef struct
{
    uint32_t telemetry_seq;
    uint32_t event_seq;
    uint32_t command_seq;
} TelemetryState;

typedef struct
{
    uint32_t window_start_tick;
    uint8_t count;
} CommandRateState;

typedef struct
{
    ImuSample latest_imu;
    SensorStatus latest_sensor_status;
    uint8_t latest_whoami;
    uint32_t latest_imu_tick;
    uint32_t latest_sensor_tick;
    uint8_t imu_valid;
    uint8_t sensor_status_valid;
} SensorRuntime;

typedef enum
{
    LCD_PAGE_STATUS = 0,
    LCD_PAGE_IMU
} LcdPage;

typedef struct
{
    uint32_t last_app_tick_ms;
    uint32_t last_health_ms;
    uint32_t last_sensor_check_ms;
    uint32_t last_imu_sample_ms;

    uint8_t health_task_seen;
    uint8_t sensor_task_seen;
    uint8_t imu_task_seen;
} HealthRuntime;

typedef enum
{
    FAULT_NONE = 0,
    FAULT_SENSOR_MISSING,
    FAULT_OVERTEMP,
    FAULT_COMMAND_STORM,
    FAULT_APP_STALL
} SystemFault;

typedef struct
{
    SystemMode mode;
    SystemFault fault;

    HealthRuntime health;

    TelemetryState telemetry;
    CommandRateState command_rate;
    SensorRuntime sensor;

    LcdPage lcd_page;
} AppState;

#endif