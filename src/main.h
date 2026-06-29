#pragma once

#if defined(_WIN32)
#define PLUGIN_API __declspec(dllexport)
#else
#define PLUGIN_API
#endif

enum JudgeResult {
    JudgeResult_Perfect = 0,
    JudgeResult_Good = 1,
    JudgeResult_Bad = 2,
    JudgeResult_Miss = 3,
};

struct JudgeEvent {
    int noteIndex;
    int result;
    // Plugin audio stream time when this judgment was made.
    double judgedAudioTimeMs;
    // Chart time when this judgment was made.
    double judgedChartTimeMs;
    float errorMs;
    int detectedMidi;
    int targetMidi;
    int stringNumber;
    int fret;
    int startMs;
    char noteName[16];
};

struct AudioStats {
    // Raw RtAudio stream time in seconds.
    double streamTime;
    // Plugin audio stream time in milliseconds.
    double audioTimeMs;
    // Chart time in milliseconds, derived from audioTimeMs - countdownMs.
    double chartTimeMs;
    // Pre-song countdown offset in milliseconds.
    double countdownMs;
    int streamLatency;
    unsigned int bufferFrames;
    unsigned int droppedAudioBlocks;
    unsigned int droppedJudgeEvents;
    int totalNotes;
    int nextNoteIndex;
    int isRunning;
    int isFinished;
};

#ifdef __cplusplus
extern "C" {
#endif

// Initializes the audio stream and plugin state.
PLUGIN_API int Initialize(unsigned int channels,
                          unsigned int sampleRate,
                          unsigned int inputDevice,
                          unsigned int outputDevice,
                          unsigned int inputOffset,
                          unsigned int outputOffset);

// Loads a chart JSON file for the next session.
PLUGIN_API int LoadChart(const char *chartPath);

// Starts audio capture, monitor DSP, and judgment processing.
PLUGIN_API int StartSession(void);

// Stops the current session.
PLUGIN_API void StopSession(void);

// Updates monitor DSP gain and low-pass parameters.
PLUGIN_API void SetDSPParams(float inputGain, float outputGain, float lpfAlpha);

// Polls one pending judge event.
PLUGIN_API int PollJudgeEvent(JudgeEvent *outEvent);

// Copies current audio and judgment progress stats.
PLUGIN_API int GetAudioStats(AudioStats *outStats);

// Releases all plugin resources.
PLUGIN_API void Shutdown(void);

#ifdef __cplusplus
}
#endif
