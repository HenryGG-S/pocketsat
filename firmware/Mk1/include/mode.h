#ifndef MODE_H
#define MODE_H

#include "app_types.h"
#include <stdint.h>

const char *mode_to_string(SystemMode mode);

uint8_t mode_transition_allowed(SystemMode from, SystemMode to);

void request_mode_change(AppState *app, SystemMode requested, uint32_t cmd_seq);

#endif