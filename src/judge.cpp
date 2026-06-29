#include "judge.h"

#include <chrono>
#include <cmath>
#include <cstring>
#include <mutex>
#include <thread>

const double PERFECT_MS = 60.0;
const double GOOD_MS = 140.0;
const double BAD_MS = 240.0;
const int PITCH_TOLERANCE = 0;

void prepareAudioQueue(AudioSpscQueue *queue, unsigned int blockCount, unsigned int sampleCount) {
    // Prepares fixed audio blocks for callback-to-judge transfer.
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
    // Pushes one input buffer from the audio callback to the judge thread.
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
    // Returns the next readable audio block for the single judge thread.
    unsigned int read = queue->readIndex.load(std::memory_order_relaxed);
    if (read == queue->writeIndex.load(std::memory_order_acquire))
        return nullptr;
    return &queue->blocks[read];
}

void popAudioBlock(AudioSpscQueue *queue) {
    // Releases the current audio block after judgment work is complete.
    unsigned int read = queue->readIndex.load(std::memory_order_relaxed);
    queue->readIndex.store((read + 1) % (unsigned int)queue->blocks.size(),
                           std::memory_order_release);
}

void prepareJudgeQueue(JudgeEventQueue *queue, unsigned int eventCount) {
    // Prepares a fixed event ring for the plugin-to-Unity polling API.
    std::lock_guard<std::mutex> lock(queue->mutex);
    queue->events.assign(eventCount, {});
    queue->readIndex = 0;
    queue->writeIndex = 0;
}

void pushJudgeEvent(PluginState *state, const JudgeEvent &event) {
    // Pushes one note judgment for Unity to poll later.
    JudgeEventQueue *queue = &state->judgeQueue;
    std::lock_guard<std::mutex> lock(queue->mutex);
    unsigned int next = (queue->writeIndex + 1) % (unsigned int)queue->events.size();
    if (next == queue->readIndex) {
        state->droppedJudgeEvents.fetch_add(1);
        return;
    }
    queue->events[queue->writeIndex] = event;
    queue->writeIndex = next;
}

int pollJudgeEvent(PluginState *state, JudgeEvent *outEvent) {
    // Pops one pending judge event for the Unity-side polling API.
    JudgeEventQueue *queue = &state->judgeQueue;
    std::lock_guard<std::mutex> lock(queue->mutex);
    if (queue->readIndex == queue->writeIndex)
        return 0;

    *outEvent = queue->events[queue->readIndex];
    queue->readIndex = (queue->readIndex + 1) % (unsigned int)queue->events.size();
    return 1;
}

int resultToCode(const char *result) {
    // Converts the timing text into a compact C ABI result code.
    if (strcmp(result, "Perfect") == 0)
        return JudgeResult_Perfect;
    if (strcmp(result, "Good") == 0)
        return JudgeResult_Good;
    if (strcmp(result, "Bad") == 0)
        return JudgeResult_Bad;
    return JudgeResult_Miss;
}

const char *judgeTiming(double absErrorMs) {
    // Converts timing error to the rhythm judgment window.
    if (absErrorMs <= PERFECT_MS)
        return "Perfect";
    if (absErrorMs <= GOOD_MS)
        return "Good";
    if (absErrorMs <= BAD_MS)
        return "Bad";
    return "Miss";
}

void fillJudgeEvent(JudgeEvent *event,
                    int noteIndex,
                    int result,
                    float errorMs,
                    double judgedAudioTimeMs,
                    double judgedChartTimeMs,
                    int detectedMidi,
                    const ChartParser::ChartNote &note) {
    // Copies chart note data into a fixed-size Unity polling event.
    *event = {};
    event->noteIndex = noteIndex;
    event->result = result;
    event->errorMs = errorMs;
    event->judgedAudioTimeMs = judgedAudioTimeMs;
    event->judgedChartTimeMs = judgedChartTimeMs;
    event->detectedMidi = detectedMidi;
    event->targetMidi = note.midi;
    event->stringNumber = note.stringNumber;
    event->fret = note.fret;
    event->startMs = note.startMs;
    strncpy(event->noteName, note.noteName.c_str(), sizeof(event->noteName) - 1);
}

void processJudgmentBlock(PluginState *state, AudioBlock *block) {
    // Judges one captured audio block and emits note events.
    double audioTimeMs = block->streamTime * 1000.0;
    double chartTimeMs = audioTimeMs - COUNTDOWN_MS;
    if (chartTimeMs < 0.0)
        return;

    if (!state->gameStarted.load())
        state->gameStarted.store(true);

    for (unsigned int i = 0; i < block->frames; i++) {
        smpl_t sample = (smpl_t)block->samples[i * state->channels] / 32768.0f;
        fvec_set_sample(state->input, sample, i);
    }

    aubio_pitch_do(state->pitchDetector, state->input, state->pitch);
    state->lastDetectedMidi = (int)std::round(fvec_get_sample(state->pitch, 0));

    aubio_onset_do(state->onsetDetector, state->input, state->onset);
    int noteIndex = state->nextNoteIndex.load();
    if (fvec_get_sample(state->onset, 0) != 0.0f &&
        noteIndex < (int)state->chart.notes.size()) {
        double onsetChartTimeMs = aubio_onset_get_last_s(state->onsetDetector) * 1000.0;
        double onsetAudioTimeMs = onsetChartTimeMs + COUNTDOWN_MS;
        const ChartParser::ChartNote &note = state->chart.notes[noteIndex];
        double errorMs = onsetChartTimeMs - note.startMs;
        double absErrorMs = std::abs(errorMs);
        const char *timingResult = judgeTiming(absErrorMs);
        bool pitchMatched = std::abs(state->lastDetectedMidi - note.midi) <= PITCH_TOLERANCE;

        if (absErrorMs <= BAD_MS) {
            JudgeEvent event;
            int result = pitchMatched ? resultToCode(timingResult) : JudgeResult_Miss;
            fillJudgeEvent(&event,
                           noteIndex,
                           result,
                           (float)errorMs,
                           onsetAudioTimeMs,
                           onsetChartTimeMs,
                           state->lastDetectedMidi,
                           note);
            pushJudgeEvent(state, event);
            noteIndex++;
            state->nextNoteIndex.store(noteIndex);
        }
    }

    noteIndex = state->nextNoteIndex.load();
    while (noteIndex < (int)state->chart.notes.size()) {
        const ChartParser::ChartNote &note = state->chart.notes[noteIndex];
        if (chartTimeMs <= note.startMs + BAD_MS)
            break;

        JudgeEvent event;
        fillJudgeEvent(&event,
                       noteIndex,
                       JudgeResult_Miss,
                       (float)(chartTimeMs - note.startMs),
                       audioTimeMs,
                       chartTimeMs,
                       state->lastDetectedMidi,
                       note);
        pushJudgeEvent(state, event);
        noteIndex++;
        state->nextNoteIndex.store(noteIndex);
    }

    if (state->nextNoteIndex.load() >= (int)state->chart.notes.size())
        state->summaryFinished.store(true);
}

void judgeThreadMain(PluginState *state) {
    // Consumes audio blocks and runs aubio judgment away from the audio callback.
    while (!state->stopRequested.load()) {
        AudioBlock *block = frontAudioBlock(&state->audioQueue);
        if (!block) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        processJudgmentBlock(state, block);
        popAudioBlock(&state->audioQueue);
    }
}
