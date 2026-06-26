#include "RhythmGame_0.2v.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

#ifdef _WIN32
#include <conio.h>
#endif

void prepareAudioQueue(AudioSpscQueue *queue, unsigned int blockCount, unsigned int sampleCount) {
    // Prepares fixed SPSC audio blocks so the callback only copies PCM samples.
    queue->blocks.resize(blockCount);
    for (unsigned int i = 0; i < blockCount; i++) {
        queue->blocks[i].streamTime = 0.0;
        queue->blocks[i].frames = 0;
        queue->blocks[i].samples.assign(sampleCount, 0);
    }
    queue->readIndex.store(0);
    queue->writeIndex.store(0);
}

bool pushAudioBlock(AudioSpscQueue *queue,
                    MY_TYPE *samples,
                    unsigned int frames,
                    unsigned int sampleCount,
                    double streamTime) {
    // Pushes one callback buffer from the audio thread to the judge thread.
    unsigned int write = queue->writeIndex.load(std::memory_order_relaxed);
    unsigned int next = (write + 1) % (unsigned int)queue->blocks.size();
    if (next == queue->readIndex.load(std::memory_order_acquire))
        return false;

    AudioBlock &block = queue->blocks[write];
    block.streamTime = streamTime;
    block.frames = frames;
    memcpy(block.samples.data(), samples, sampleCount * sizeof(MY_TYPE));
    queue->writeIndex.store(next, std::memory_order_release);
    return true;
}

AudioBlock *frontAudioBlock(AudioSpscQueue *queue) {
    // Returns the next readable audio block for the single consumer thread.
    unsigned int read = queue->readIndex.load(std::memory_order_relaxed);
    if (read == queue->writeIndex.load(std::memory_order_acquire))
        return nullptr;
    return &queue->blocks[read];
}

void popAudioBlock(AudioSpscQueue *queue) {
    // Releases the current audio block after the judge thread has processed it.
    unsigned int read = queue->readIndex.load(std::memory_order_relaxed);
    queue->readIndex.store((read + 1) % (unsigned int)queue->blocks.size(),
                           std::memory_order_release);
}

std::string formatSeconds(double seconds) {
    // Formats seconds for the console HUD.
    std::ostringstream text;
    text << std::fixed << std::setprecision(2) << seconds << "s";
    return text.str();
}

std::string makeProgressBar(double streamTime, int durationMs) {
    // Builds a small progress bar for the current chart position.
    const int width = 12;
    double durationSeconds = durationMs / 1000.0;
    double ratio = durationSeconds > 0.0 ? streamTime / durationSeconds : 0.0;
    if (ratio < 0.0)
        ratio = 0.0;
    if (ratio > 1.0)
        ratio = 1.0;

    int filled = (int)std::round(ratio * width);
    std::string bar = "[";
    for (int i = 0; i < width; i++) bar += i < filled ? '#' : '-';
    bar += "]";
    return bar;
}

std::string shortenText(const std::string &text, int maxLength) {
    // Shortens long judgment text so the HUD stays on one console line.
    if ((int)text.size() <= maxLength)
        return text;
    return text.substr(0, maxLength - 3) + "...";
}

std::string makeTimingCue(double remainSeconds) {
    // Shows the last second before a note as four 0.25 second boxes.
    int filled = (int)std::floor((1.0 - remainSeconds) / 0.25) + 1;
    if (remainSeconds > 1.0)
        filled = 0;
    if (filled < 0)
        filled = 0;
    if (filled > 4)
        filled = 4;

    std::string cue;
    for (int i = 0; i < 4; i++)
        cue += i < filled ? "[#]" : "[ ]";
    return cue;
}

const char *judgeTiming(double absErrorMs) {
    // Converts timing error to a rhythm judgment.
    if (absErrorMs <= PERFECT_MS)
        return "Perfect";
    if (absErrorMs <= GOOD_MS)
        return "Good";
    if (absErrorMs <= BAD_MS)
        return "Bad";
    return "Miss";
}

float lpf(float input, float previous, float alpha) {
    // Applies a simple one-pole low-pass filter.
    return previous + alpha * (input - previous);
}

void printHud(RhythmState *state, double streamTime) {
    // Prints the current progress and next note as a single updating line.
    std::cout << "\r" << makeProgressBar(streamTime, state->chart.durationMs) << " "
              << formatSeconds(streamTime) << "/"
              << formatSeconds(state->chart.durationMs / 1000.0);

    if (state->nextNoteIndex < (int)state->chart.notes.size()) {
        const ChartParser::ChartNote &note = state->chart.notes[state->nextNoteIndex];
        double remainSeconds = note.startMs / 1000.0 - streamTime;
        std::cout << " | N " << state->nextNoteIndex + 1 << "/" << state->chart.notes.size() << " S"
                  << note.stringNumber << " F" << note.fret << " " << note.noteName << " "
                  << makeTimingCue(remainSeconds);
    } else {
        std::cout << " | N finished";
    }

    std::cout << " | Last " << shortenText(state->lastResult, 32) << std::flush;
}

void printCountdown(double gameTime) {
    // Prints the pre-song countdown as a single updating line.
    int count = (int)std::ceil(-gameTime);
    std::cout << "\rStarting in " << count << "..." << std::flush;
}

void printSummary(RhythmState *state) {
    // Prints every note judgment after the chart is finished.
    std::cout << "\n\nResult\n";
    for (int i = 0; i < (int)state->chart.notes.size(); i++) {
        const ChartParser::ChartNote &note = state->chart.notes[i];
        std::cout << std::setw(2) << i + 1 << ". "
                  << "string " << note.stringNumber << ", fret " << note.fret << ", "
                  << note.noteName << " @ " << formatSeconds(note.startMs / 1000.0) << " -> "
                  << state->noteResults[i] << "\n";
    }
    std::cout << std::flush;
}

void usage(void) {
    // Command-line usage for the rhythm game test.
    std::cout << "\nuseage: RhythmGame_0.2v N fs <iDevice> <oDevice> <iChannelOffset> "
                 "<oChannelOffset> <chartPath>\n";
    std::cout << "    where N = number of channels,\n";
    std::cout << "    fs = the sample rate,\n";
    std::cout << "    iDevice = optional input device index to use (default = 0),\n";
    std::cout << "    oDevice = optional output device index to use (default = 0),\n";
    std::cout << "    iChannelOffset = an optional input channel offset (default = 0),\n";
    std::cout << "    oChannelOffset = optional output channel offset (default = 0),\n";
    std::cout << "    and chartPath = optional chart json path.\n\n";
    exit(0);
}

unsigned int getDeviceIndex(std::vector<std::string> deviceNames, bool isInput) {
    // Lets the user select an audio device when the requested index is invalid.
    unsigned int i;
    std::string keyHit;
    std::cout << '\n';
    for (i = 0; i < deviceNames.size(); i++)
        std::cout << "  Device #" << i << ": " << deviceNames[i] << '\n';
    do {
        if (isInput)
            std::cout << "\nChoose an input device #: ";
        else
            std::cout << "\nChoose an output device #: ";
        std::cin >> i;
    } while (i >= deviceNames.size());
    std::getline(std::cin, keyHit);
    return i;
}

void processMonitorDsp(RhythmState *state,
                       MY_TYPE *output,
                       MY_TYPE *input,
                       unsigned int nBufferFrames) {
    // Processes callback input as input gain > tanh overdrive > LPF > output gain.
    for (unsigned int frame = 0; frame < nBufferFrames; frame++) {
        for (unsigned int channel = 0; channel < state->channels; channel++) {
            unsigned int index = frame * state->channels + channel;
            float sample = (float)input[index] / 32768.0f;

            sample *= state->inputGain;
            sample = std::tanh(sample);
            sample = lpf(sample, state->lpfState[channel], state->lpfAlpha);
            state->lpfState[channel] = sample;
            sample *= state->outputGain;

            sample = std::clamp(sample, -1.0f, 1.0f);
            output[index] = (MY_TYPE)(sample * 32767.0f);
        }
    }
}

void processJudgmentBlock(RhythmState *state, AudioBlock *block) {
    // Judges one audio block that was captured by the audio callback.
    double gameTime = block->streamTime - COUNTDOWN_SECONDS;

    if (gameTime < 0.0) {
        if (block->streamTime >= state->nextPrintTime) {
            printCountdown(gameTime);
            state->nextPrintTime += 0.05;
        }
        return;
    }

    if (!state->gameStarted) {
        state->gameStarted = true;
        state->nextPrintTime = block->streamTime;
        printHud(state, 0.0);
    }

    for (unsigned int i = 0; i < block->frames; i++) {
        smpl_t sample = (smpl_t)block->samples[i * state->channels] / 32768.0f;
        fvec_set_sample(state->input, sample, i);
    }

    aubio_pitch_do(state->pitchDetector, state->input, state->pitch);
    state->lastDetectedMidi = (int)std::round(fvec_get_sample(state->pitch, 0));

    aubio_onset_do(state->onsetDetector, state->input, state->onset);
    if (fvec_get_sample(state->onset, 0) != 0.0f &&
        state->nextNoteIndex < (int)state->chart.notes.size()) {
        double onsetMs = aubio_onset_get_last_s(state->onsetDetector) * 1000.0;
        const ChartParser::ChartNote &note = state->chart.notes[state->nextNoteIndex];
        double errorMs = onsetMs - note.startMs;
        double absErrorMs = std::abs(errorMs);
        const char *result = judgeTiming(absErrorMs);
        bool pitchMatched = std::abs(state->lastDetectedMidi - note.midi) <= PITCH_TOLERANCE;

        std::ostringstream resultText;
        if (absErrorMs <= BAD_MS && pitchMatched) {
            resultText << result << " (" << std::showpos << std::fixed << std::setprecision(1)
                       << errorMs << std::noshowpos << " ms, " << note.noteName << ")";
            state->noteResults[state->nextNoteIndex] = resultText.str();
            state->nextNoteIndex++;
        } else if (absErrorMs <= BAD_MS) {
            resultText << "Miss (wrong note: MIDI " << state->lastDetectedMidi << ", target "
                       << note.noteName << ")";
            state->noteResults[state->nextNoteIndex] = resultText.str();
            state->nextNoteIndex++;
        } else {
            resultText << "Miss (" << std::showpos << std::fixed << std::setprecision(1) << errorMs
                       << std::noshowpos << " ms)";
        }

        state->lastResult = resultText.str();
        printHud(state, gameTime);
    }

    while (state->nextNoteIndex < (int)state->chart.notes.size()) {
        const ChartParser::ChartNote &note = state->chart.notes[state->nextNoteIndex];
        if (gameTime * 1000.0 <= note.startMs + BAD_MS)
            break;

        std::ostringstream resultText;
        resultText << "Miss (no input: " << note.noteName << ")";
        state->lastResult = resultText.str();
        state->noteResults[state->nextNoteIndex] = resultText.str();
        state->nextNoteIndex++;
        printHud(state, gameTime);
    }

    if (state->nextNoteIndex >= (int)state->chart.notes.size() && !state->summaryPrinted) {
        state->summaryPrinted = true;
        printHud(state, gameTime);
        printSummary(state);
        return;
    }

    if (!state->summaryPrinted && block->streamTime >= state->nextPrintTime) {
        printHud(state, gameTime);
        state->nextPrintTime += 0.05;
    }
}

void handleCli(RhythmState *state) {
    // Checks whether the user pressed enter without blocking judgment work.
#ifdef _WIN32
    if (_kbhit()) {
        int input = _getch();
        if (input == '\r' || input == '\n')
            state->quitRequested.store(true);
    }
#else
    if (std::cin.rdbuf()->in_avail() > 0) {
        char input;
        std::cin.get(input);
        state->quitRequested.store(true);
    }
#endif
}

void judgeThreadMain(RhythmState *state) {
    // Consumes audio blocks, runs aubio judgment, and handles CLI exit.
    while (!state->quitRequested.load()) {
        handleCli(state);

        AudioBlock *block = frontAudioBlock(&state->audioQueue);
        if (!block) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        processJudgmentBlock(state, block);
        popAudioBlock(&state->audioQueue);
    }
}

int inoutRhythmGame(void *outputBuffer,
                    void *inputBuffer,
                    unsigned int nBufferFrames,
                    double streamTime,
                    RtAudioStreamStatus status,
                    void *data) {
    // Sends input to the SPSC queue first, then runs DSP for live monitoring.
    if (status)
        std::cout << "Stream over/underflow detected." << std::endl;

    RhythmState *state = (RhythmState *)data;
    MY_TYPE *input = (MY_TYPE *)inputBuffer;
    MY_TYPE *output = (MY_TYPE *)outputBuffer;
    unsigned int sampleCount = nBufferFrames * state->channels;

    pushAudioBlock(&state->audioQueue, input, nBufferFrames, sampleCount, streamTime);
    processMonitorDsp(state, output, input, nBufferFrames);
    return 0;
}

void printConnectedDevices(RtAudio &adac, unsigned int inputDeviceId, unsigned int outputDeviceId) {
    // Prints the selected input and output device names.
    RtAudio::DeviceInfo inputInfo = adac.getDeviceInfo(inputDeviceId);
    RtAudio::DeviceInfo outputInfo = adac.getDeviceInfo(outputDeviceId);

    std::cout << "\nInput Device  : " << inputInfo.name << std::endl;
    std::cout << "Output Device : " << outputInfo.name << std::endl;
}

int main(int argc, char *argv[]) {
    unsigned int channels, fs, oDevice = 0, iDevice = 0, iOffset = 0, oOffset = 0;
    std::string chartPath = "assets/charts/simple_chromatic_001.json";
    std::thread judgeThread;

    // Minimal command-line checking.
    if (argc < 3 || argc > 8)
        usage();

    channels = (unsigned int)atoi(argv[1]);
    fs = (unsigned int)atoi(argv[2]);
    if (argc > 3)
        iDevice = (unsigned int)atoi(argv[3]);
    if (argc > 4)
        oDevice = (unsigned int)atoi(argv[4]);
    if (argc > 5)
        iOffset = (unsigned int)atoi(argv[5]);
    if (argc > 6)
        oOffset = (unsigned int)atoi(argv[6]);
    if (argc > 7)
        chartPath = argv[7];

    RhythmState state = {};
    state.audioQueue.readIndex.store(0);
    state.audioQueue.writeIndex.store(0);
    state.quitRequested.store(false);

    if (!ChartParser::loadChart(chartPath, state.chart)) {
        std::cout << "Failed to load chart: " << chartPath << std::endl;
        return 1;
    }

    RtAudio adac;
    std::vector<unsigned int> deviceIds = adac.getDeviceIds();
    if (deviceIds.size() < 1) {
        std::cout << "\nNo audio devices found!\n";
        exit(1);
    }

    adac.showWarnings(true);

    unsigned int bufferFrames = 128;
    RtAudio::StreamParameters iParams, oParams;
    iParams.nChannels = channels;
    iParams.firstChannel = iOffset;
    oParams.nChannels = channels;
    oParams.firstChannel = oOffset;

    if (iDevice == 0)
        iParams.deviceId = adac.getDefaultInputDevice();
    else {
        if (iDevice >= deviceIds.size())
            iDevice = getDeviceIndex(adac.getDeviceNames(), true);
        iParams.deviceId = deviceIds[iDevice];
    }
    if (oDevice == 0)
        oParams.deviceId = adac.getDefaultOutputDevice();
    else {
        if (oDevice >= deviceIds.size())
            oDevice = getDeviceIndex(adac.getDeviceNames());
        oParams.deviceId = deviceIds[oDevice];
    }
    printConnectedDevices(adac, iParams.deviceId, oParams.deviceId);

    RtAudio::StreamOptions options;
    state.channels = channels;
    state.bufferBytes = bufferFrames * channels * sizeof(MY_TYPE);
    state.lastResult = "Waiting";
    state.nextPrintTime = 0.05;
    state.nextNoteIndex = 0;
    state.lastDetectedMidi = -1;
    state.gameStarted = false;
    state.summaryPrinted = false;
    state.noteResults.assign(state.chart.notes.size(), "Not judged");
    state.inputGain = 4.0f;
    state.outputGain = 0.5f;
    state.lpfAlpha = 0.2f;
    state.lpfState.assign(channels, 0.0f);

    if (adac.openStream(&oParams,
                        &iParams,
                        FORMAT,
                        fs,
                        &bufferFrames,
                        &inoutRhythmGame,
                        (void *)&state,
                        &options)) {
        goto cleanup;
    }

    if (adac.isStreamOpen() == false)
        goto cleanup;

    state.bufferBytes = bufferFrames * channels * sizeof(MY_TYPE);
    prepareAudioQueue(&state.audioQueue, 8, bufferFrames * channels);
    state.input = new_fvec(bufferFrames);
    state.pitch = new_fvec(1);
    state.onset = new_fvec(1);
    state.pitchDetector = new_aubio_pitch("default", 2048, bufferFrames, fs);
    state.onsetDetector = new_aubio_onset("default", 1024, bufferFrames, fs);
    aubio_pitch_set_unit(state.pitchDetector, "midi");
    aubio_onset_set_threshold(state.onsetDetector, 0.3f);

    std::cout << "\nLoaded chart: " << state.chart.title << " / " << state.chart.difficulty << "\n";
    std::cout << "Stream latency = " << adac.getStreamLatency() << " frames" << std::endl;
    std::cout << "Running ... press <enter> to quit (buffer frames = " << bufferFrames << ").\n";

    judgeThread = std::thread(judgeThreadMain, &state);

    if (adac.startStream())
        goto cleanup;

    while (!state.quitRequested.load())
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

cleanup:
    state.quitRequested.store(true);
    if (adac.isStreamRunning())
        adac.stopStream();
    if (adac.isStreamOpen())
        adac.closeStream();
    if (judgeThread.joinable())
        judgeThread.join();
    if (state.pitchDetector)
        del_aubio_pitch(state.pitchDetector);
    if (state.onsetDetector)
        del_aubio_onset(state.onsetDetector);
    if (state.pitch)
        del_fvec(state.pitch);
    if (state.onset)
        del_fvec(state.onset);
    if (state.input)
        del_fvec(state.input);
    aubio_cleanup();

    return 0;
}
