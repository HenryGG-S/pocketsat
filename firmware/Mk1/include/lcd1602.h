#ifndef LCD1602_H
#define LCD1602_H

#include "app_types.h"

void lcd_init(void);
void lcd_clear(void);

void lcd_show_status(SystemMode mode, SystemFault fault);
void lcd_show_imu(const ImuSample *sample);

void lcd_show_status_page(AppState *app);
void lcd_show_imu_page(AppState *app, const ImuSample *sample);
void lcd_refresh(AppState *app);

#endif