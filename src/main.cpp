#include "main.h"

#include <cstring>
#include <thread>

#include "RtAudio.h"
#include "audioQueue.h"
#include "dsp.h"
#include "eventQueue.h"
#include "input.h"
#include "judge.h"
#include "song.h"

RtAudio *g_adac = nullptr;
PluginState g_state = {};
static std::thread g_judgeThread;

extern "C" PLUGIN_API const char *GetPluginVersion(void) {
    // Returns the version of the loaded native plugin.
    return "0.3.2";
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
    double chartTimeMs = sessionStreamTime * 1000.0 - COUNTDOWN_MS;
    double chartTimeScale = 1.0;
    int sessionMode = state->sessionMode.load();
    if (sessionMode == SessionMode_SlowPractice) {
        chartTimeMs = state->lastChartTimeMs.load();
        chartTimeScale = state->practiceSpeed.load();
        state->lastChartTimeMs.store(chartTimeMs +
                                     nBufferFrames * 1000.0 / state->sampleRate * chartTimeScale);
    } else if (sessionMode == SessionMode_FingeringPractice) {
        int noteIndex = state->nextNoteIndex.load();
        chartTimeMs = noteIndex < (int)state->chart.notes.size()
                          ? state->chart.notes[noteIndex].startMs
                          : state->chart.durationMs;
        chartTimeScale = 0.0;
        state->lastChartTimeMs.store(chartTimeMs);
    } else {
        state->lastChartTimeMs.store(chartTimeMs);
    }

    if (!pushAudioBlock(&state->audioQueue,
                        input,
                        nBufferFrames,
                        sampleCount,
                        sessionStreamTime,
                        chartTimeMs,
                        chartTimeScale))
        state->droppedAudioBlocks.fetch_add(1);
    processMonitorDsp(state, output, input, nBufferFrames, sessionStreamTime);
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

extern "C" PLUGIN_API int LoadChart(const char *chartPath) {
    // Loads a chart JSON file through ChartParser.
    if (!ChartParser::loadChart(chartPath, g_state.chart))
        return -1;
    if (!loadSong(&g_state, chartPath))
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
    g_state.lastChartTimeMs.store(-COUNTDOWN_MS);
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

extern "C" PLUGIN_API int StartSlowPracticeSession(void) {
    // Starts chart judgment without mixing the chart song into the monitor output.
    if (g_adac == nullptr || g_adac->isStreamOpen() == false)
        return -1;

    ResetSessionTime();
    g_state.sessionMode.store(SessionMode_SlowPractice);
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

extern "C" PLUGIN_API void SetPracticeSpeed(float speed) {
    // Sets the chart-clock multiplier used by the running slow-practice session.
    if (speed < 0.25f)
        speed = 0.25f;
    if (speed > 1.25f)
        speed = 1.25f;
    g_state.practiceSpeed.store(speed);
}

extern "C" PLUGIN_API int StartFingeringPracticeSession(void) {
    // Starts note-by-note judgment without song playback or timing-based misses.
    if (g_adac == nullptr || g_adac->isStreamOpen() == false)
        return -1;

    ResetSessionTime();
    g_state.sessionMode.store(SessionMode_FingeringPractice);
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

extern "C" PLUGIN_API void SetSongVolume(float volume) {
    // Updates the chart-song volume without changing the instrument monitor gain.
    g_state.songVolume.store(volume);
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
    outStats->chartTimeMs = g_state.lastChartTimeMs.load();
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
    outInfo->songTimeMs = g_state.lastChartTimeMs.load();
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
    shutdownSongDecoder();
    aubio_cleanup();
}
