#include "eventQueue.h"

#include <mutex>

void preparePluginEventQueue(PluginEventQueue *queue, unsigned int eventCount) {
    // Prepares the single plugin-to-client event queue for a session.
    std::lock_guard<std::mutex> lock(queue->mutex);
    queue->events.assign(eventCount, {});
    queue->readIndex = 0;
    queue->writeIndex = 0;
}

bool pushPluginEvent(PluginEventQueue *queue, const PluginEvent &event) {
    // Pushes one output event produced by the analysis thread.
    std::lock_guard<std::mutex> lock(queue->mutex);
    unsigned int next = (queue->writeIndex + 1) % (unsigned int)queue->events.size();
    if (next == queue->readIndex)
        return false;

    queue->events[queue->writeIndex] = event;
    queue->writeIndex = next;
    return true;
}

int pollPluginEvent(PluginEventQueue *queue, int type, PluginEvent *outEvent) {
    // Pops the next output event only when it matches the requested type.
    std::lock_guard<std::mutex> lock(queue->mutex);
    if (queue->readIndex == queue->writeIndex)
        return 0;
    if (queue->events[queue->readIndex].type != type)
        return 0;

    *outEvent = queue->events[queue->readIndex];
    queue->readIndex = (queue->readIndex + 1) % (unsigned int)queue->events.size();
    return 1;
}
