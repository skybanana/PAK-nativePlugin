#include "dsp.h"

#include <algorithm>
#include <cmath>

float lpf(float input, float previous, float alpha) {
    // Applies a simple one-pole low-pass filter.
    return previous + alpha * (input - previous);
}

void processMonitorDsp(PluginState *state,
                       MY_TYPE *output,
                       MY_TYPE *input,
                       unsigned int nBufferFrames,
                       double sessionStreamTime) {
    // Mixes the chart song with callback input, then applies the monitor DSP.
    for (unsigned int frame = 0; frame < nBufferFrames; frame++) {
        for (unsigned int channel = 0; channel < state->channels; channel++) {
            unsigned int index = frame * state->channels + channel;
            float sample = (float)input[index] / 32768.0f;

            double songTimeSeconds = sessionStreamTime + (double)frame / state->sampleRate
                                     - COUNTDOWN_SECONDS;
            int sessionMode = state->sessionMode.load();
            if (sessionMode != SessionMode_SlowPractice &&
                sessionMode != SessionMode_FingeringPractice && songTimeSeconds >= 0.0) {
                unsigned long long songFrame =
                    (unsigned long long)(songTimeSeconds * state->sampleRate);
                if (songFrame < state->songFrames) {
                    unsigned int songChannel = state->songChannels == 1 ? 0 : channel % state->songChannels;
                    unsigned long long songIndex = songFrame * state->songChannels + songChannel;
                    sample += (float)state->songSamples[songIndex] / 32768.0f;
                }
            }

            sample *= state->inputGain.load();
            sample = std::tanh(sample);
            sample = lpf(sample, state->lpfState[channel], state->lpfAlpha.load());
            state->lpfState[channel] = sample;
            sample *= state->outputGain.load();

            sample = std::clamp(sample, -1.0f, 1.0f);
            output[index] = (MY_TYPE)(sample * 32767.0f);
        }
    }
}
