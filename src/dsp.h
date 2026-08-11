#pragma once

#include "plugin_state.h"

void processMonitorDsp(PluginState *state,
                       MY_TYPE *output,
                       MY_TYPE *input,
                       unsigned int nBufferFrames);
