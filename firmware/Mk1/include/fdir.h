#ifndef FDIR_H
#define FDIR_H

#include "app_types.h"
#include <stdint.h>

const char *fault_to_string(SystemFault fault);

void raise_fault(AppState *app, SystemFault fault);
void clear_faults(AppState *app, uint32_t cmd_seq);

uint8_t command_rate_note(CommandRateState *rate);

#endif