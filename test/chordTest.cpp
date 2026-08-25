#include <algorithm>
#include <cstdint>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

#include "RtAudio.h"

extern "C" {
#include <aubio/aubio.h>
}

typedef int16_t MY_TYPE;
#define FORMAT RTAUDIO_SINT16

const unsigned int CHORD_FFT_SIZE = 16384;
const double CHORD_SETTLE_MS = 160.0;
const double CHORD_COVERAGE_THRESHOLD = 0.65;
const double CHORD_TONE_PRESENCE_RATIO = 0.15;
const int CHORD_HARMONICS = 6;

struct ChordTestState {
    unsigned int channels;
    unsigned int sampleRate;
    unsigned int bufferBytes;
    fvec_t *input;
    fvec_t *onset;
    fvec_t *chordInput;
    cvec_t *chordSpectrum;
    aubio_onset_t *onsetDetector;
    aubio_fft_t *chordFft;
    bool collecting;
    double deadlineMs;
    std::vector<float> chordSamples;
};

void usage(void) {
    // Prints command-line usage for the input-triggered Am chord test.
    std::cout << "\nuseage: chordTest N fs <iDevice> <oDevice> <iChannelOffset> <oChannelOffset>\n";
    exit(0);
}

bool matchesAmChord(ChordTestState *state) {
    // Applies the plugin chord spectrum algorithm to the captured Am input.
    unsigned int sampleCount = std::min((unsigned int)state->chordSamples.size(), CHORD_FFT_SIZE);
    for (unsigned int i = 0; i < CHORD_FFT_SIZE; i++)
        fvec_set_sample(state->chordInput, 0.0f, i);

    const double pi = 3.14159265358979323846;
    for (unsigned int i = 0; i < sampleCount; i++) {
        double window = sampleCount > 1
                            ? 0.5 - 0.5 * std::cos(2.0 * pi * i / (sampleCount - 1))
                            : 1.0;
        fvec_set_sample(state->chordInput, state->chordSamples[i] * (float)window, i);
    }
    aubio_fft_do(state->chordFft, state->chordInput, state->chordSpectrum);

    const int amMidis[] = {45, 52, 57, 60, 64};
    bool expectedPitchClasses[12] = {};
    for (int midi : amMidis)
        expectedPitchClasses[midi % 12] = true;

    double chroma[12] = {};
    for (int midi = 33; midi <= 76; midi++) {
        double fundamental = 440.0 * std::pow(2.0, (midi - 69) / 12.0);
        double salience = 0.0;
        for (int harmonic = 1; harmonic <= CHORD_HARMONICS; harmonic++) {
            double frequency = fundamental * harmonic;
            if (frequency >= state->sampleRate * 0.5)
                break;

            int centerBin = (int)std::round(frequency * CHORD_FFT_SIZE / state->sampleRate);
            for (int offset = -2; offset <= 2; offset++) {
                int bin = centerBin + offset;
                if (bin < 1 || bin >= (int)state->chordSpectrum->length)
                    continue;
                double magnitude = cvec_norm_get_sample(state->chordSpectrum, bin);
                salience += magnitude * magnitude / harmonic;
            }
        }
        chroma[midi % 12] += salience;
    }

    double totalEnergy = 0.0;
    double targetEnergy = 0.0;
    double strongestPitchClass = 0.0;
    for (int pitchClass = 0; pitchClass < 12; pitchClass++) {
        totalEnergy += chroma[pitchClass];
        if (expectedPitchClasses[pitchClass])
            targetEnergy += chroma[pitchClass];
        strongestPitchClass = std::max(strongestPitchClass, chroma[pitchClass]);
    }

    if (totalEnergy == 0.0 || targetEnergy / totalEnergy < CHORD_COVERAGE_THRESHOLD)
        return false;

    for (int pitchClass = 0; pitchClass < 12; pitchClass++) {
        if (expectedPitchClasses[pitchClass] &&
            chroma[pitchClass] < strongestPitchClass * CHORD_TONE_PRESENCE_RATIO)
            return false;
    }
    return true;
}

int inoutChordTest(void *outputBuffer,
                   void *inputBuffer,
                   unsigned int nBufferFrames,
                   double streamTime,
                   RtAudioStreamStatus,
                   void *data) {
    // Waits for a played onset, then captures it and reports whether it is Am.
    ChordTestState *state = (ChordTestState *)data;
    memcpy(outputBuffer, inputBuffer, state->bufferBytes);

    MY_TYPE *samples = (MY_TYPE *)inputBuffer;
    for (unsigned int i = 0; i < nBufferFrames; i++)
        fvec_set_sample(state->input, (smpl_t)samples[i * state->channels] / 32768.0f, i);

    double currentMs = streamTime * 1000.0;
    if (!state->collecting) {
        aubio_onset_do(state->onsetDetector, state->input, state->onset);
        if (fvec_get_sample(state->onset, 0) != 0.0f) {
            state->collecting = true;
            state->deadlineMs = currentMs + CHORD_SETTLE_MS;
            state->chordSamples.clear();
            std::cout << "\nInput detected. Checking Am..." << std::flush;
        }
    }

    if (state->collecting) {
        for (unsigned int i = 0; i < nBufferFrames; i++)
            state->chordSamples.push_back(fvec_get_sample(state->input, i));

        if (currentMs >= state->deadlineMs) {
            std::cout << (matchesAmChord(state) ? " Am: matched\n" : " Am: not matched\n")
                      << "Waiting for input..." << std::flush;
            state->collecting = false;
        }
    }
    return 0;
}

int main(int argc, char *argv[]) {
    // Starts the live input test and waits for the user to end it.
    unsigned int channels, fs, oDevice = 0, iDevice = 0, iOffset = 0, oOffset = 0;
    if (argc < 3 || argc > 7)
        usage();

    channels = (unsigned int)atoi(argv[1]);
    fs = (unsigned int)atoi(argv[2]);
    if (argc > 3) iDevice = (unsigned int)atoi(argv[3]);
    if (argc > 4) oDevice = (unsigned int)atoi(argv[4]);
    if (argc > 5) iOffset = (unsigned int)atoi(argv[5]);
    if (argc > 6) oOffset = (unsigned int)atoi(argv[6]);

    RtAudio adac;
    std::vector<unsigned int> deviceIds = adac.getDeviceIds();
    unsigned int bufferFrames = 128;
    RtAudio::StreamParameters iParams, oParams;
    iParams.deviceId = iDevice == 0 ? adac.getDefaultInputDevice() : deviceIds[iDevice];
    iParams.nChannels = channels;
    iParams.firstChannel = iOffset;
    oParams.deviceId = oDevice == 0 ? adac.getDefaultOutputDevice() : deviceIds[oDevice];
    oParams.nChannels = channels;
    oParams.firstChannel = oOffset;

    ChordTestState state = {};
    state.channels = channels;
    state.sampleRate = fs;
    state.bufferBytes = bufferFrames * channels * sizeof(MY_TYPE);
    state.input = new_fvec(bufferFrames);
    state.onset = new_fvec(1);
    state.chordInput = new_fvec(CHORD_FFT_SIZE);
    state.chordSpectrum = new_cvec(CHORD_FFT_SIZE);
    state.onsetDetector = new_aubio_onset("default", 1024, bufferFrames, fs);
    state.chordFft = new_aubio_fft(CHORD_FFT_SIZE);
    aubio_onset_set_threshold(state.onsetDetector, 0.3f);

    RtAudio::StreamOptions options;
    adac.openStream(&oParams, &iParams, FORMAT, fs, &bufferFrames, &inoutChordTest, &state, &options);
    state.bufferBytes = bufferFrames * channels * sizeof(MY_TYPE);
    adac.startStream();

    std::cout << "Waiting for Am... "
                 "press <enter> to quit."
              << std::flush;
    std::cin.get();

    adac.stopStream();
    adac.closeStream();
    del_aubio_fft(state.chordFft);
    del_aubio_onset(state.onsetDetector);
    del_cvec(state.chordSpectrum);
    del_fvec(state.chordInput);
    del_fvec(state.onset);
    del_fvec(state.input);
    aubio_cleanup();
    return 0;
}
