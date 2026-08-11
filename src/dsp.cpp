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
                       unsigned int nBufferFrames) {
    // Processes callback input as input gain > tanh overdrive > LPF > output gain.
    for (unsigned int frame = 0; frame < nBufferFrames; frame++) {
        for (unsigned int channel = 0; channel < state->channels; channel++) {
            unsigned int index = frame * state->channels + channel;
            float sample = (float)input[index] / 32768.0f;

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
