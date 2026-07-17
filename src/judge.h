#pragma once

#include "audioQueue.h"
#include "main.h"

#include <mutex>
#include <vector>

struct PluginState;

struct JudgeEventQueue {
    std::vector<JudgeEvent> events;
    unsigned int readIndex;
    unsigned int writeIndex;
    std::mutex mutex;
};

struct PendingJudgment {
    int noteIndex;
    double onsetChartTimeMs;
    double onsetAudioTimeMs;
    double errorMs;
    double deadlineChartTimeMs;
};

void prepareJudgeQueue(JudgeEventQueue *queue, unsigned int eventCount);
int pollJudgeEvent(PluginState *state, JudgeEvent *outEvent);
void processJudgmentBlock(PluginState *state, AudioBlock *block);
void judgeThreadMain(PluginState *state);
