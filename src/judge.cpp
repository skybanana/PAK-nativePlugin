#include "judge.h"

#include "eventQueue.h"
#include "input.h"
#include "plugin_state.h"

#include <chrono>
#include <cmath>
#include <cstring>
#include <mutex>
#include <thread>

const double PERFECT_MS = 60.0;
const double GOOD_MS = 140.0;
const double BAD_MS = 240.0;
const int PITCH_TOLERANCE = 0;

void pushJudgeEvent(PluginState *state, const JudgeEvent &event) {
    // Pushes one note judgment for Unity to poll later.
    PluginEvent output = {};
    output.type = PluginEvent_Judge;
    output.judge = event;
    if (!pushPluginEvent(&state->eventQueue, output))
        state->droppedJudgeEvents.fetch_add(1);
}

int pollJudgeEvent(PluginState *state, JudgeEvent *outEvent) {
    // Pops one pending judge event for the Unity-side polling API.
    PluginEvent event = {};
    if (!pollPluginEvent(&state->eventQueue, PluginEvent_Judge, &event))
        return 0;

    *outEvent = event.judge;
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

void prunePitchObservations(PluginState *state, double chartTimeMs) {
    // Keeps only recent pitch observations needed by pending judgments.
    while (!state->pitchObservations.empty() &&
           state->pitchObservations.front().chartTimeMs < chartTimeMs - 500.0) {
        state->pitchObservations.erase(state->pitchObservations.begin());
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

    if (state->sessionMode.load() == SessionMode_GuitarInput) {
        processGuitarInputBlock(state, hasOnset, onsetAudioTimeMs, audioTimeMs);
        return;
    }

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
