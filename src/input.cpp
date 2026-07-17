#include "input.h"

#include "plugin_state.h"

#include <cmath>
#include <mutex>

void prepareGuitarInputQueue(GuitarInputEventQueue *queue, unsigned int eventCount) {
    // Prepares a fixed event ring for guitar-control input events.
    std::lock_guard<std::mutex> lock(queue->mutex);
    queue->events.assign(eventCount, {});
    queue->readIndex = 0;
    queue->writeIndex = 0;
}

void pushGuitarInputEvent(PluginState *state, const GuitarInputEvent &event) {
    // Pushes one detected guitar input for Unity to poll later.
    GuitarInputEventQueue *queue = &state->guitarInputQueue;
    std::lock_guard<std::mutex> lock(queue->mutex);
    unsigned int next = (queue->writeIndex + 1) % (unsigned int)queue->events.size();
    if (next == queue->readIndex)
        return;

    queue->events[queue->writeIndex] = event;
    queue->writeIndex = next;
}

int pollGuitarInputEvent(PluginState *state, GuitarInputEvent *outEvent) {
    // Pops one pending guitar input event for the Unity-side polling API.
    GuitarInputEventQueue *queue = &state->guitarInputQueue;
    std::lock_guard<std::mutex> lock(queue->mutex);
    if (queue->readIndex == queue->writeIndex)
        return 0;

    *outEvent = queue->events[queue->readIndex];
    queue->readIndex = (queue->readIndex + 1) % (unsigned int)queue->events.size();
    return 1;
}

bool selectGuitarPitchObservation(PluginState *state,
                                  const PendingGuitarInput &pending,
                                  PitchObservation *selected) {
    // Selects the non-zero pitch closest to the guitar input settle deadline.
    bool found = false;
    double bestTimeDistance = 1000000.0;

    for (const PitchObservation &observation : state->pitchObservations) {
        if (observation.audioTimeMs < pending.onsetAudioTimeMs)
            continue;
        if (observation.audioTimeMs > pending.deadlineAudioTimeMs)
            continue;
        if (observation.midi == 0)
            continue;

        double timeDistance = std::abs(observation.audioTimeMs - pending.deadlineAudioTimeMs);
        if (!found || timeDistance < bestTimeDistance) {
            *selected = observation;
            bestTimeDistance = timeDistance;
            found = true;
        }
    }

    return found;
}

void finalizePendingGuitarInputs(PluginState *state, double audioTimeMs) {
    // Emits guitar input events after their pitch settle window has closed.
    size_t index = 0;
    while (index < state->pendingGuitarInputs.size()) {
        PendingGuitarInput pending = state->pendingGuitarInputs[index];
        if (audioTimeMs < pending.deadlineAudioTimeMs) {
            index++;
            continue;
        }

        PitchObservation selected = {};
        if (selectGuitarPitchObservation(state, pending, &selected)) {
            GuitarInputEvent event = {};
            event.midi = selected.midi;
            event.audioTimeMs = pending.onsetAudioTimeMs;
            pushGuitarInputEvent(state, event);
        }

        state->pendingGuitarInputs.erase(state->pendingGuitarInputs.begin() + index);
    }
}
