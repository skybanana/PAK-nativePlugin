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
    // Plugin audio stream time for the judged onset.
    double judgedAudioTimeMs;
    // Chart time for the judged onset.
    double judgedChartTimeMs;
    float errorMs;
    int detectedMidi;
    int targetMidi;
    int stringNumber;
    int fret;
    int startMs;
    char noteName[16];
};

struct GuitarInputEvent {
    int midi;
    // Plugin audio stream time for the detected guitar onset.
    double audioTimeMs;
};

struct AudioStats {
    // Session-relative stream time in seconds.
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
    int isPaused;
};

struct SongSyncInfo {
    // Audio path declared by the loaded chart.
    char audioFile[260];
    // Chart audio offset used when converting ticks to chart time.
    int audioOffsetMs;
    // Declared song duration.
    int durationMs;
    // Playback position relative to the beginning of the song audio.
    // A negative value means the pre-song countdown is still in progress.
    double songTimeMs;
};

struct AudioDriverInfo {
    // RtAudio API value used by InitializeWithAudioDriver.
    unsigned int api;
    char name[64];
    char displayName[64];
};

struct AsioDriverInfo {
    // Registered ASIO driver name used for direct ASIO selection.
    char name[256];
};

struct AudioDeviceInfo {
    // RtAudio device ID used by InitializeWithAudioDriver.
    unsigned int id;
    char name[256];
    unsigned int inputChannels;
    unsigned int outputChannels;
    unsigned int duplexChannels;
    int isDefaultInput;
    int isDefaultOutput;
    unsigned int preferredSampleRate;
};

#ifdef __cplusplus
extern "C" {
#endif

// Returns the native plugin version string.
PLUGIN_API const char *GetPluginVersion(void);

// Initializes the audio stream and plugin state.
PLUGIN_API int Initialize(unsigned int channels,
                          unsigned int sampleRate,
                          unsigned int inputDevice,
                          unsigned int outputDevice,
                          unsigned int inputOffset,
                          unsigned int outputOffset);

// Returns the number of audio driver APIs compiled into the plugin.
PLUGIN_API unsigned int GetAudioDriverCount(void);

// Copies one compiled audio driver API by its zero-based list index.
PLUGIN_API int GetAudioDriverInfo(unsigned int driverIndex, AudioDriverInfo *outInfo);

// Returns registered ASIO driver names without opening any ASIO driver.
PLUGIN_API unsigned int GetAsioDriverCount(void);

// Copies one registered ASIO driver name by its zero-based list index.
PLUGIN_API int GetAsioDriverInfo(unsigned int driverIndex, AsioDriverInfo *outInfo);

// Opens one selected ASIO driver to read its available channel counts.
PLUGIN_API int GetAsioDriverDeviceInfo(const char *driverName, AudioDeviceInfo *outInfo);

// Returns the number of devices currently visible through an audio driver API.
PLUGIN_API unsigned int GetAudioDeviceCount(unsigned int api);

// Copies one device by its zero-based list index. inputChannels is the selectable input-channel count.
PLUGIN_API int GetAudioDeviceInfo(unsigned int api,
                                  unsigned int deviceIndex,
                                  AudioDeviceInfo *outInfo);

// Initializes a stream through a selected audio driver and RtAudio device IDs.
PLUGIN_API int InitializeWithAudioDriver(unsigned int api,
                                         unsigned int channels,
                                         unsigned int sampleRate,
                                         unsigned int inputDeviceId,
                                         unsigned int outputDeviceId,
                                         unsigned int inputOffset,
                                         unsigned int outputOffset);

// Loads a chart JSON file for the next session.
PLUGIN_API int LoadChart(const char *chartPath);

// Resets the current session clock and judgment progress.
PLUGIN_API void ResetSessionTime(void);

// Starts audio capture, monitor DSP, and judgment processing.
PLUGIN_API int StartSession(void);

// Starts a no-song practice session whose chart clock is controlled by SetPracticeSpeed.
PLUGIN_API int StartSlowPracticeSession(void);

// Sets the slow-practice chart speed. Supported range is 0.25 to 1.25.
PLUGIN_API void SetPracticeSpeed(float speed);

// Starts a no-song practice session that waits for a correct input before advancing.
PLUGIN_API int StartFingeringPracticeSession(void);

// Stops the current session.
PLUGIN_API void StopSession(void);

// Pauses the current session without resetting its progress.
PLUGIN_API void PauseSession(void);

// Resumes a session paused by PauseSession.
PLUGIN_API void ResumeSession(void);

// Restarts the current session mode from the beginning.
PLUGIN_API int RestartSession(void);

// Updates monitor DSP gain and low-pass parameters.
PLUGIN_API void SetDSPParams(float inputGain, float outputGain, float lpfAlpha);

// Sets the chart-song mix volume independently from monitor DSP gain.
PLUGIN_API void SetSongVolume(float volume);

// Polls one pending judge event.
PLUGIN_API int PollJudgeEvent(JudgeEvent *outEvent);

// Polls one pending guitar input event.
PLUGIN_API int PollGuitarInputEvent(GuitarInputEvent *outEvent);

// Copies current audio and judgment progress stats.
PLUGIN_API int GetAudioStats(AudioStats *outStats);

// Copies the loaded song path and the playback time synchronized to the session clock.
PLUGIN_API int GetSongSyncInfo(SongSyncInfo *outInfo);

// Releases all plugin resources.
PLUGIN_API void Shutdown(void);

#ifdef __cplusplus
}
#endif
