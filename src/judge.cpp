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

void prepareGuitarInputQueue(GuitarInputEventQueue *queue, unsigned int eventCount) {
    // Prepares a fixed event ring for guitar-control input events.
    std::lock_guard<std::mutex> lock(queue->mutex);
    queue->events.assign(eventCount, {});
    queue->readIndex = 0;
    queue->writeIndex = 0;
}

void pushGuitarInputEvent(PluginState *state, const GuitarInputEvent &event) {
    // Pushes one detected guitar input for Unity to poll later.
    GuitarInputEventQueue *queue = &state->guitarInputQueue;
    std::lock_guard<std::mutex> lock(queue->mutex);
    unsigned int next = (queue->writeIndex + 1) % (unsigned int)queue->events.size();
    if (next == queue->readIndex)
        return;

    queue->events[queue->writeIndex] = event;
    queue->writeIndex = next;
}

int pollGuitarInputEvent(PluginState *state, GuitarInputEvent *outEvent) {
    // Pops one pending guitar input event for the Unity-side polling API.
    GuitarInputEventQueue *queue = &state->guitarInputQueue;
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
                    const ChartParser::ChartNote &note);

bool selectPitchObservation(PluginState *state,
                            const PendingJudgment &pending,
                            PitchObservation *selected) {
    // Selects the non-zero pitch closest to the delayed judgment time.
    bool found = false;
    double bestTimeDistance = 1000000.0;

    for (const PitchObservation &observation : state->pitchObservations) {
        if (observation.chartTimeMs < pending.onsetChartTimeMs)
            continue;
        if (observation.chartTimeMs > pending.deadlineChartTimeMs)
            continue;
        if (observation.midi == 0)
            continue;

        double timeDistance = std::abs(observation.chartTimeMs - pending.deadlineChartTimeMs);
        if (!found || timeDistance < bestTimeDistance) {
            *selected = observation;
            bestTimeDistance = timeDistance;
            found = true;
        }
    }

    return found;
}

bool selectGuitarPitchObservation(PluginState *state,
                                  const PendingGuitarInput &pending,
                                  PitchObservation *selected) {
    // Selects the non-zero pitch closest to the guitar input settle deadline.
    bool found = false;
    double bestTimeDistance = 1000000.0;

    for (const PitchObservation &observation : state->pitchObservations) {
        if (observation.audioTimeMs < pending.onsetAudioTimeMs)
            continue;
        if (observation.audioTimeMs > pending.deadlineAudioTimeMs)
            continue;
        if (observation.midi == 0)
            continue;

        double timeDistance = std::abs(observation.audioTimeMs - pending.deadlineAudioTimeMs);
        if (!found || timeDistance < bestTimeDistance) {
            *selected = observation;
            bestTimeDistance = timeDistance;
            found = true;
        }
    }

    return found;
}

void prunePitchObservations(PluginState *state, double chartTimeMs) {
    // Keeps only recent pitch observations needed by pending judgments.
    while (!state->pitchObservations.empty() &&
           state->pitchObservations.front().chartTimeMs < chartTimeMs - 500.0) {
        state->pitchObservations.erase(state->pitchObservations.begin());
    }
}

void finalizePendingGuitarInputs(PluginState *state, double audioTimeMs) {
    // Emits guitar input events after their pitch settle window has closed.
    size_t index = 0;
    while (index < state->pendingGuitarInputs.size()) {
        PendingGuitarInput pending = state->pendingGuitarInputs[index];
        if (audioTimeMs < pending.deadlineAudioTimeMs) {
            index++;
            continue;
        }

        PitchObservation selected = {};
        if (selectGuitarPitchObservation(state, pending, &selected)) {
            GuitarInputEvent event = {};
            event.midi = selected.midi;
            event.audioTimeMs = pending.onsetAudioTimeMs;
            pushGuitarInputEvent(state, event);
        }

        state->pendingGuitarInputs.erase(state->pendingGuitarInputs.begin() + index);
    }
}

void finalizePendingJudgments(PluginState *state, double chartTimeMs) {
    // Emits delayed judge events after their pitch window has closed.
    size_t index = 0;
    while (index < state->pendingJudgments.size()) {
        PendingJudgment pending = state->pendingJudgments[index];
        if (chartTimeMs < pending.deadlineChartTimeMs) {
            index++;
            continue;
        }

        const ChartParser::ChartNote &note = state->chart.notes[pending.noteIndex];
        PitchObservation selected = {};
        bool hasPitch = selectPitchObservation(state, pending, &selected);
        int detectedMidi = hasPitch ? selected.midi : 0;
        bool pitchMatched = std::abs(detectedMidi - note.midi) <= PITCH_TOLERANCE;
        const char *timingResult = judgeTiming(std::abs(pending.errorMs));

        JudgeEvent event;
        int result = pitchMatched ? resultToCode(timingResult) : JudgeResult_Miss;
        fillJudgeEvent(&event,
                       pending.noteIndex,
                       result,
                       (float)pending.errorMs,
                       pending.onsetAudioTimeMs,
                       pending.onsetChartTimeMs,
                       detectedMidi,
                       note);
        pushJudgeEvent(state, event);
        state->pendingJudgments.erase(state->pendingJudgments.begin() + index);
    }
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

    for (unsigned int i = 0; i < block->frames; i++) {
        smpl_t sample = (smpl_t)block->samples[i * state->channels] / 32768.0f;
        fvec_set_sample(state->input, sample, i);
    }

    aubio_pitch_do(state->pitchDetector, state->input, state->pitch);
    state->lastDetectedMidi = (int)std::round(fvec_get_sample(state->pitch, 0));
    state->pitchObservations.push_back({audioTimeMs, chartTimeMs, state->lastDetectedMidi});
    prunePitchObservations(state, chartTimeMs);

    aubio_onset_do(state->onsetDetector, state->input, state->onset);
    bool hasOnset = fvec_get_sample(state->onset, 0) != 0.0f;
    double onsetAudioTimeMs = aubio_onset_get_last_s(state->onsetDetector) * 1000.0;
    double onsetChartTimeMs = onsetAudioTimeMs - COUNTDOWN_MS;
    if (hasOnset) {
        state->pendingGuitarInputs.push_back({onsetAudioTimeMs,
                                              onsetAudioTimeMs + PITCH_SETTLE_MS});
    }
    finalizePendingGuitarInputs(state, audioTimeMs);

    if (chartTimeMs < 0.0)
        return;

    if (!state->gameStarted.load())
        state->gameStarted.store(true);

    int noteIndex = state->nextNoteIndex.load();
    if (hasOnset && noteIndex < (int)state->chart.notes.size()) {
        const ChartParser::ChartNote &note = state->chart.notes[noteIndex];
        double errorMs = onsetChartTimeMs - note.startMs;
        double absErrorMs = std::abs(errorMs);

        if (absErrorMs <= BAD_MS) {
            state->pendingJudgments.push_back({noteIndex,
                                               onsetChartTimeMs,
                                               onsetAudioTimeMs,
                                               errorMs,
                                               onsetChartTimeMs + PITCH_SETTLE_MS});
            noteIndex++;
            state->nextNoteIndex.store(noteIndex);
        }
    }

    finalizePendingJudgments(state, chartTimeMs);

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

    if (state->nextNoteIndex.load() >= (int)state->chart.notes.size() &&
        state->pendingJudgments.empty())
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
