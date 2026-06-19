#ifndef APP_H
#define APP_H

#include "app_types.h"
#include <stdint.h>

void app_init(AppState *app);
void app_tick(AppState *app);

#endif