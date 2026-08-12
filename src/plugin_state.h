#pragma once

#include "audioQueue.h"
#include "ChartParser.h"
#include "eventQueue.h"
#include "input.h"
#include "judge.h"
#include "main.h"

#include <atomic>
#include <vector>

extern "C" {
#include <aubio/aubio.h>
}

constexpr double COUNTDOWN_SECONDS = 5.0;
constexpr double COUNTDOWN_MS = COUNTDOWN_SECONDS * 1000.0;
constexpr double PITCH_SETTLE_MS = 80.0;
constexpr double CHORD_SETTLE_MS = 160.0;
constexpr unsigned int CHORD_FFT_SIZE = 16384;

enum SessionMode {
    SessionMode_None = 0,
    SessionMode_GuitarInput = 1,
    SessionMode_Judge = 2,
};

struct PitchObservation {
    double audioTimeMs;
    double chartTimeMs;
    int midi;
};

struct PluginState {
    ChartParser::Chart chart;
    unsigned int channels;
    unsigned int sampleRate;
    unsigned int bufferFrames;
    AudioSpscQueue audioQueue;
    PluginEventQueue eventQueue;
    fvec_t *input;
    fvec_t *pitch;
    fvec_t *onset;
    fvec_t *chordInput;
    cvec_t *chordSpectrum;
    aubio_pitch_t *pitchDetector;
    aubio_onset_t *onsetDetector;
    aubio_fft_t *chordFft;
    std::vector<float> lpfState;
    std::atomic<float> inputGain;
    std::atomic<float> outputGain;
    std::atomic<float> lpfAlpha;
    std::atomic<bool> stopRequested;
    std::atomic<bool> gameStarted;
    std::atomic<bool> summaryFinished;
    std::atomic<int> requestedSessionMode;
    std::atomic<int> sessionMode;
    std::atomic<unsigned int> droppedAudioBlocks;
    std::atomic<unsigned int> droppedJudgeEvents;
    std::atomic<double> lastStreamTime;
    std::atomic<double> sessionStreamTimeOffset;
    std::atomic<bool> sessionClockStarted;
    std::atomic<int> nextNoteIndex;
    std::vector<PitchObservation> pitchObservations;
    std::vector<PendingGuitarInput> pendingGuitarInputs;
    std::vector<PendingJudgment> pendingJudgments;
    int lastDetectedMidi;
};
