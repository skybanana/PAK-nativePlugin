#include "main.h"

#include <cstring>
#include <thread>
#include <vector>

#include "RtAudio.h"
#include "audioQueue.h"
#include "dsp.h"
#include "eventQueue.h"
#include "input.h"
#include "judge.h"


#define FORMAT RTAUDIO_SINT16

static RtAudio *g_adac = nullptr;
static PluginState g_state = {};
static std::thread g_judgeThread;

extern "C" PLUGIN_API const char *GetPluginVersion(void) {
    // Returns the version of the loaded native plugin.
    return "0.1.0";
}

int inoutRhythmGame(void *outputBuffer,
                    void *inputBuffer,
                    unsigned int nBufferFrames,
                    double streamTime,
                    RtAudioStreamStatus,
                    void *data) {
    // Queues input for judgment, then writes processed monitor audio to output.
    PluginState *state = (PluginState *)data;
    MY_TYPE *input = (MY_TYPE *)inputBuffer;
    MY_TYPE *output = (MY_TYPE *)outputBuffer;
    unsigned int sampleCount = nBufferFrames * state->channels;

    if (!state->sessionClockStarted.exchange(true))
        state->sessionStreamTimeOffset.store(streamTime);
    double sessionStreamTime = streamTime - state->sessionStreamTimeOffset.load();

    state->lastStreamTime.store(sessionStreamTime);
    if (!pushAudioBlock(&state->audioQueue, input, nBufferFrames, sampleCount, sessionStreamTime))
        state->droppedAudioBlocks.fetch_add(1);
    processMonitorDsp(state, output, input, nBufferFrames);
    return 0;
}

void releaseAubio(PluginState *state) {
    // Releases aubio objects owned by the plugin state.
    if (state->pitchDetector)
        del_aubio_pitch(state->pitchDetector);
    if (state->onsetDetector)
        del_aubio_onset(state->onsetDetector);
    if (state->chordFft)
        del_aubio_fft(state->chordFft);
    if (state->pitch)
        del_fvec(state->pitch);
    if (state->onset)
        del_fvec(state->onset);
    if (state->chordSpectrum)
        del_cvec(state->chordSpectrum);
    if (state->chordInput)
        del_fvec(state->chordInput);
    if (state->input)
        del_fvec(state->input);
    state->pitchDetector = nullptr;
    state->onsetDetector = nullptr;
    state->chordFft = nullptr;
    state->pitch = nullptr;
    state->onset = nullptr;
    state->chordSpectrum = nullptr;
    state->chordInput = nullptr;
    state->input = nullptr;
}

extern "C" PLUGIN_API int Initialize(unsigned int channels,
                                     unsigned int sampleRate,
                                     unsigned int inputDevice,
                                     unsigned int outputDevice,
                                     unsigned int inputOffset,
                                     unsigned int outputOffset) {
    // Initializes RtAudio, DSP state, aubio detectors, and fixed queues.
    Shutdown();

    g_adac = new RtAudio();
    g_adac->showWarnings(true);

    g_state.chart = {};
    g_state.channels = channels;
    g_state.sampleRate = sampleRate;
    g_state.bufferFrames = 128;
    g_state.nextNoteIndex.store(0);
    g_state.lastDetectedMidi = -1;
    g_state.input = nullptr;
    g_state.pitch = nullptr;
    g_state.onset = nullptr;
    g_state.chordInput = nullptr;
    g_state.chordSpectrum = nullptr;
    g_state.pitchDetector = nullptr;
    g_state.onsetDetector = nullptr;
    g_state.chordFft = nullptr;
    g_state.inputGain.store(4.0f);
    g_state.outputGain.store(0.5f);
    g_state.lpfAlpha.store(0.2f);
    g_state.stopRequested.store(true);
    g_state.requestedSessionMode.store(SessionMode_GuitarInput);
    g_state.sessionMode.store(SessionMode_None);
    g_state.sessionStreamTimeOffset.store(0.0);
    g_state.sessionClockStarted.store(false);
    g_state.lpfState.assign(channels, 0.0f);
    g_state.pitchObservations.clear();
    g_state.pendingGuitarInputs.clear();
    g_state.pendingJudgments.clear();

    RtAudio::StreamParameters iParams, oParams;
    iParams.nChannels = channels;
    iParams.firstChannel = inputOffset;
    oParams.nChannels = channels;
    oParams.firstChannel = outputOffset;

    if (inputDevice == 0) {
        iParams.deviceId = g_adac->getDefaultInputDevice();
    } else {
        std::vector<unsigned int> deviceIds = g_adac->getDeviceIds();
        iParams.deviceId = deviceIds[inputDevice];
    }

    if (outputDevice == 0) {
        oParams.deviceId = g_adac->getDefaultOutputDevice();
    } else {
        std::vector<unsigned int> deviceIds = g_adac->getDeviceIds();
        oParams.deviceId = deviceIds[outputDevice];
    }

    RtAudio::StreamOptions options;
    if (g_adac->openStream(&oParams,
                           &iParams,
                           FORMAT,
                           sampleRate,
                           &g_state.bufferFrames,
                           &inoutRhythmGame,
                           (void *)&g_state,
                           &options)) {
        Shutdown();
        return -1;
    }

    prepareAudioQueue(&g_state.audioQueue, 8, g_state.bufferFrames * channels);
    preparePluginEventQueue(&g_state.eventQueue, 64);

    g_state.input = new_fvec(g_state.bufferFrames);
    g_state.pitch = new_fvec(1);
    g_state.onset = new_fvec(1);
    g_state.chordInput = new_fvec(CHORD_FFT_SIZE);
    g_state.chordSpectrum = new_cvec(CHORD_FFT_SIZE);
    g_state.pitchDetector = new_aubio_pitch("default", 2048, g_state.bufferFrames, sampleRate);
    g_state.onsetDetector = new_aubio_onset("default", 1024, g_state.bufferFrames, sampleRate);
    g_state.chordFft = new_aubio_fft(CHORD_FFT_SIZE);
    aubio_pitch_set_unit(g_state.pitchDetector, "midi");
    aubio_onset_set_threshold(g_state.onsetDetector, 0.3f);
    return 0;
}

extern "C" PLUGIN_API int LoadChart(const char *chartPath) {
    // Loads a chart JSON file through ChartParser.
    if (!ChartParser::loadChart(chartPath, g_state.chart))
        return -1;

    g_state.requestedSessionMode.store(SessionMode_Judge);
    ResetSessionTime();
    return 0;
}

extern "C" PLUGIN_API void ResetSessionTime(void) {
    // Resets the session-relative clock and judgment progress.
    g_state.nextNoteIndex.store(0);
    g_state.lastDetectedMidi = -1;
    g_state.gameStarted.store(false);
    g_state.summaryFinished.store(false);
    g_state.sessionMode.store(SessionMode_None);
    g_state.lastStreamTime.store(0.0);
    g_state.sessionStreamTimeOffset.store(0.0);
    g_state.sessionClockStarted.store(false);
    g_state.audioQueue.readIndex.store(0);
    g_state.audioQueue.writeIndex.store(0);
    g_state.pitchObservations.clear();
    g_state.pendingGuitarInputs.clear();
    g_state.pendingJudgments.clear();
    if (g_state.onsetDetector)
        aubio_onset_reset(g_state.onsetDetector);
    preparePluginEventQueue(&g_state.eventQueue, 64);
}

extern "C" PLUGIN_API int StartSession(void) {
    // Starts the audio stream and the plugin-owned judge thread.
    if (g_adac == nullptr)
        return -1;
    if (g_adac->isStreamOpen() == false)
        return -1;

    ResetSessionTime();
    g_state.sessionMode.store(g_state.requestedSessionMode.load());
    g_state.stopRequested.store(false);
    g_state.droppedAudioBlocks.store(0);
    g_state.droppedJudgeEvents.store(0);

    g_judgeThread = std::thread(judgeThreadMain, &g_state);
    if (g_adac->startStream()) {
        StopSession();
        return -1;
    }
    return 0;
}

extern "C" PLUGIN_API void StopSession(void) {
    // Stops the audio stream and joins the judge thread.
    g_state.stopRequested.store(true);
    if (g_adac != nullptr && g_adac->isStreamRunning())
        g_adac->stopStream();
    if (g_judgeThread.joinable())
        g_judgeThread.join();
}

extern "C" PLUGIN_API void SetDSPParams(float inputGain, float outputGain, float lpfAlpha) {
    // Updates realtime monitor DSP parameters.
    g_state.inputGain.store(inputGain);
    g_state.outputGain.store(outputGain);
    g_state.lpfAlpha.store(lpfAlpha);
}

extern "C" PLUGIN_API int PollJudgeEvent(JudgeEvent *outEvent) {
    // Polls one pending judge event through the judge module.
    return pollJudgeEvent(&g_state, outEvent);
}

extern "C" PLUGIN_API int PollGuitarInputEvent(GuitarInputEvent *outEvent) {
    // Polls one pending guitar input event through the judge module.
    return pollGuitarInputEvent(&g_state, outEvent);
}

extern "C" PLUGIN_API int GetAudioStats(AudioStats *outStats) {
    // Copies current stream and judge progress stats for Unity.
    if (g_adac == nullptr)
        return -1;

    *outStats = {};
    outStats->streamTime = g_state.lastStreamTime.load();
    outStats->audioTimeMs = outStats->streamTime * 1000.0;
    outStats->chartTimeMs = outStats->audioTimeMs - COUNTDOWN_MS;
    outStats->countdownMs = COUNTDOWN_MS;
    outStats->streamLatency = g_adac->isStreamOpen() ? g_adac->getStreamLatency() : 0;
    outStats->bufferFrames = g_state.bufferFrames;
    outStats->droppedAudioBlocks = g_state.droppedAudioBlocks.load();
    outStats->droppedJudgeEvents = g_state.droppedJudgeEvents.load();
    outStats->totalNotes = (int)g_state.chart.notes.size();
    outStats->nextNoteIndex = g_state.nextNoteIndex.load();
    outStats->isRunning = g_adac->isStreamRunning() ? 1 : 0;
    outStats->isFinished = g_state.summaryFinished.load() ? 1 : 0;
    return 0;
}

extern "C" PLUGIN_API int GetSongSyncInfo(SongSyncInfo *outInfo) {
    // Returns the chart song and its position on the plugin session clock.
    if (g_adac == nullptr || g_state.chart.audioFile.empty())
        return -1;

    *outInfo = {};
    std::strncpy(
        outInfo->audioFile, g_state.chart.audioFile.c_str(), sizeof(outInfo->audioFile) - 1);
    outInfo->audioOffsetMs = g_state.chart.audioOffsetMs;
    outInfo->durationMs = g_state.chart.durationMs;
    outInfo->songTimeMs = g_state.lastStreamTime.load() * 1000.0 - COUNTDOWN_MS;
    return 0;
}

extern "C" PLUGIN_API void Shutdown(void) {
    // Releases the stream, judge thread, and aubio resources.
    StopSession();
    if (g_adac != nullptr) {
        if (g_adac->isStreamOpen())
            g_adac->closeStream();
        delete g_adac;
        g_adac = nullptr;
    }
    releaseAubio(&g_state);
    aubio_cleanup();
}
