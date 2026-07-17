#pragma once

#include "main.h"

#include <mutex>
#include <vector>

struct PluginState;

struct GuitarInputEventQueue {
    std::vector<GuitarInputEvent> events;
    unsigned int readIndex;
    unsigned int writeIndex;
    std::mutex mutex;
};

struct PendingGuitarInput {
    double onsetAudioTimeMs;
    double deadlineAudioTimeMs;
};

void prepareGuitarInputQueue(GuitarInputEventQueue *queue, unsigned int eventCount);
int pollGuitarInputEvent(PluginState *state, GuitarInputEvent *outEvent);
void finalizePendingGuitarInputs(PluginState *state, double audioTimeMs);
