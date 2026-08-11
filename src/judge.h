#pragma once

#include "audioQueue.h"
#include "main.h"

#include <vector>

struct PluginState;

struct PendingJudgment {
    int noteIndex;
    double onsetChartTimeMs;
    double onsetAudioTimeMs;
    double errorMs;
    double deadlineChartTimeMs;
};

int pollJudgeEvent(PluginState *state, JudgeEvent *outEvent);
void processJudgmentBlock(PluginState *state, AudioBlock *block);
void judgeThreadMain(PluginState *state);
