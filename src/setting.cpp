#include "setting.h"

#include <cstring>
#include <vector>

#include "RtAudio.h"
#include "audioQueue.h"
#include "eventQueue.h"
#include "song.h"

#define FORMAT RTAUDIO_SINT16

int inoutRhythmGame(void *outputBuffer,
                    void *inputBuffer,
                    unsigned int nBufferFrames,
                    double streamTime,
                    RtAudioStreamStatus status,
                    void *data);

static int initializeAudioDriver(RtAudio::Api api,
                                 unsigned int channels,
                                 unsigned int sampleRate,
                                 unsigned int inputDeviceId,
                                 unsigned int outputDeviceId,
                                 unsigned int inputOffset,
                                 unsigned int outputOffset) {
    // Initializes the selected driver, device, and input/output channel offsets.
    Shutdown();

    g_adac = new RtAudio(api);
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
    g_state.songVolume.store(1.0f);
    g_state.lpfAlpha.store(0.2f);
    g_state.stopRequested.store(true);
    g_state.requestedSessionMode.store(SessionMode_GuitarInput);
    g_state.sessionMode.store(SessionMode_None);
    g_state.sessionStreamTimeOffset.store(0.0);
    g_state.sessionClockStarted.store(false);
    g_state.lastChartTimeMs.store(-COUNTDOWN_MS);
    g_state.practiceSpeed.store(1.0f);
    g_state.lpfState.assign(channels, 0.0f);
    g_state.songSamples.clear();
    g_state.songChannels = 0;
    g_state.songFrames = 0;
    g_state.pitchObservations.clear();
    g_state.pendingGuitarInputs.clear();
    g_state.pendingJudgments.clear();

    RtAudio::StreamParameters iParams, oParams;
    iParams.nChannels = channels;
    iParams.firstChannel = inputOffset;
    oParams.nChannels = channels;
    oParams.firstChannel = outputOffset;
    iParams.deviceId = inputDeviceId == 0 ? g_adac->getDefaultInputDevice() : inputDeviceId;
    oParams.deviceId = outputDeviceId == 0 ? g_adac->getDefaultOutputDevice() : outputDeviceId;

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
    if (!initializeSongDecoder()) {
        Shutdown();
        return -1;
    }
    return 0;
}

extern "C" PLUGIN_API int Initialize(unsigned int channels,
                                      unsigned int sampleRate,
                                      unsigned int inputDevice,
                                      unsigned int outputDevice,
                                      unsigned int inputOffset,
                                      unsigned int outputOffset) {
    // Initializes through the default driver using the legacy device-list indexes.
    RtAudio audio;
    std::vector<unsigned int> deviceIds = audio.getDeviceIds();
    unsigned int inputDeviceId = inputDevice == 0 ? 0 : deviceIds[inputDevice];
    unsigned int outputDeviceId = outputDevice == 0 ? 0 : deviceIds[outputDevice];
    return initializeAudioDriver(RtAudio::UNSPECIFIED,
                                 channels,
                                 sampleRate,
                                 inputDeviceId,
                                 outputDeviceId,
                                 inputOffset,
                                 outputOffset);
}

extern "C" PLUGIN_API unsigned int GetAudioDriverCount(void) {
    // Returns the number of compiled RtAudio APIs available for selection.
    std::vector<RtAudio::Api> apis;
    RtAudio::getCompiledApi(apis);
    return (unsigned int)apis.size();
}

extern "C" PLUGIN_API int GetAudioDriverInfo(unsigned int driverIndex,
                                              AudioDriverInfo *outInfo) {
    // Copies the API identifier and names for one compiled audio driver.
    std::vector<RtAudio::Api> apis;
    RtAudio::getCompiledApi(apis);
    if (driverIndex >= apis.size())
        return -1;

    *outInfo = {};
    outInfo->api = (unsigned int)apis[driverIndex];
    std::strncpy(outInfo->name,
                 RtAudio::getApiName(apis[driverIndex]).c_str(),
                 sizeof(outInfo->name) - 1);
    std::strncpy(outInfo->displayName,
                 RtAudio::getApiDisplayName(apis[driverIndex]).c_str(),
                 sizeof(outInfo->displayName) - 1);
    return 0;
}

extern "C" PLUGIN_API unsigned int GetAudioDeviceCount(unsigned int api) {
    // Returns devices currently discoverable through the selected audio driver.
    RtAudio audio((RtAudio::Api)api);
    return audio.getDeviceCount();
}

extern "C" PLUGIN_API int GetAudioDeviceInfo(unsigned int api,
                                              unsigned int deviceIndex,
                                              AudioDeviceInfo *outInfo) {
    // Copies device IDs, names, and available input/output channel counts.
    RtAudio audio((RtAudio::Api)api);
    std::vector<unsigned int> deviceIds = audio.getDeviceIds();
    if (deviceIndex >= deviceIds.size())
        return -1;

    RtAudio::DeviceInfo device = audio.getDeviceInfo(deviceIds[deviceIndex]);
    *outInfo = {};
    outInfo->id = device.ID;
    std::strncpy(outInfo->name, device.name.c_str(), sizeof(outInfo->name) - 1);
    outInfo->inputChannels = device.inputChannels;
    outInfo->outputChannels = device.outputChannels;
    outInfo->duplexChannels = device.duplexChannels;
    outInfo->isDefaultInput = device.isDefaultInput ? 1 : 0;
    outInfo->isDefaultOutput = device.isDefaultOutput ? 1 : 0;
    outInfo->preferredSampleRate = device.preferredSampleRate;
    return 0;
}

extern "C" PLUGIN_API int InitializeWithAudioDriver(unsigned int api,
                                                      unsigned int channels,
                                                      unsigned int sampleRate,
                                                      unsigned int inputDeviceId,
                                                      unsigned int outputDeviceId,
                                                      unsigned int inputOffset,
                                                      unsigned int outputOffset) {
    // Initializes through the selected driver using IDs returned by GetAudioDeviceInfo.
    return initializeAudioDriver((RtAudio::Api)api,
                                 channels,
                                 sampleRate,
                                 inputDeviceId,
                                 outputDeviceId,
                                 inputOffset,
                                 outputOffset);
}
