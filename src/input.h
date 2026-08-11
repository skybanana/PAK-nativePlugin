#pragma once

#include "main.h"

#include <vector>

struct PluginState;

struct PendingGuitarInput {
    double onsetAudioTimeMs;
    double deadlineAudioTimeMs;
};

int pollGuitarInputEvent(PluginState *state, GuitarInputEvent *outEvent);
void processGuitarInputBlock(PluginState *state,
                             bool hasOnset,
                             double onsetAudioTimeMs,
                             double audioTimeMs);
