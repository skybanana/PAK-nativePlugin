#pragma once

#include "plugin_state.h"

void prepareAudioQueue(AudioSpscQueue *queue, unsigned int blockCount, unsigned int sampleCount);
bool pushAudioBlock(AudioSpscQueue *queue,
                    MY_TYPE *samples,
                    unsigned int frames,
                    unsigned int sampleCount,
                    double streamTime);
AudioBlock *frontAudioBlock(AudioSpscQueue *queue);
void popAudioBlock(AudioSpscQueue *queue);
void prepareJudgeQueue(JudgeEventQueue *queue, unsigned int eventCount);
int pollJudgeEvent(PluginState *state, JudgeEvent *outEvent);
void processJudgmentBlock(PluginState *state, AudioBlock *block);
void judgeThreadMain(PluginState *state);
