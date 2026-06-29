#pragma once

#include "ChartParser.h"
#include "main.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

extern "C" {
#include <aubio/aubio.h>
}

typedef int16_t MY_TYPE;

constexpr double COUNTDOWN_SECONDS = 5.0;
constexpr double COUNTDOWN_MS = COUNTDOWN_SECONDS * 1000.0;
constexpr double PITCH_SETTLE_MS = 80.0;

struct AudioBlock {
    double streamTime;
    unsigned int frames;
    std::vector<MY_TYPE> samples;
};

struct AudioSpscQueue {
    std::vector<AudioBlock> blocks;
    std::atomic<unsigned int> readIndex;
    std::atomic<unsigned int> writeIndex;
};

struct JudgeEventQueue {
    std::vector<JudgeEvent> events;
    unsigned int readIndex;
    unsigned int writeIndex;
    std::mutex mutex;
};

struct PitchObservation {
    double chartTimeMs;
    int midi;
};

struct PendingJudgment {
    int noteIndex;
    double onsetChartTimeMs;
    double onsetAudioTimeMs;
    double errorMs;
    double deadlineChartTimeMs;
};

struct PluginState {
    ChartParser::Chart chart;
    unsigned int channels;
    unsigned int sampleRate;
    unsigned int bufferFrames;
    AudioSpscQueue audioQueue;
    JudgeEventQueue judgeQueue;
    fvec_t *input;
    fvec_t *pitch;
    fvec_t *onset;
    aubio_pitch_t *pitchDetector;
    aubio_onset_t *onsetDetector;
    std::vector<float> lpfState;
    std::atomic<float> inputGain;
    std::atomic<float> outputGain;
    std::atomic<float> lpfAlpha;
    std::atomic<bool> stopRequested;
    std::atomic<bool> gameStarted;
    std::atomic<bool> summaryFinished;
    std::atomic<unsigned int> droppedAudioBlocks;
    std::atomic<unsigned int> droppedJudgeEvents;
    std::atomic<double> lastStreamTime;
    std::atomic<int> nextNoteIndex;
    std::vector<PitchObservation> pitchObservations;
    std::vector<PendingJudgment> pendingJudgments;
    int lastDetectedMidi;
};
