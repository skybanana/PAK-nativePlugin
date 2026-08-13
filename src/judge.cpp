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
const double CHORD_COVERAGE_THRESHOLD = 0.65;
const double CHORD_TONE_PRESENCE_RATIO = 0.15;
const int CHORD_HARMONICS = 6;

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

bool matchesChord(PluginState *state,
                  const PendingJudgment &pending,
                  const ChartParser::ChartNote &note) {
    // Measures how strongly the captured spectrum supports every target chord pitch class.
    unsigned int sampleCount = (unsigned int)pending.chordSamples.size();
    if (sampleCount > CHORD_FFT_SIZE)
        sampleCount = CHORD_FFT_SIZE;

    for (unsigned int i = 0; i < CHORD_FFT_SIZE; i++)
        fvec_set_sample(state->chordInput, 0.0f, i);

    const double pi = 3.14159265358979323846;
    for (unsigned int i = 0; i < sampleCount; i++) {
        double window = sampleCount > 1
                            ? 0.5 - 0.5 * std::cos(2.0 * pi * i / (sampleCount - 1))
                            : 1.0;
        fvec_set_sample(state->chordInput, pending.chordSamples[i] * (float)window, i);
    }
    aubio_fft_do(state->chordFft, state->chordInput, state->chordSpectrum);

    bool expectedPitchClasses[12] = {};
    int minimumMidi = 127;
    int maximumMidi = 0;
    for (int midi : note.chordMidis) {
        expectedPitchClasses[midi % 12] = true;
        if (midi < minimumMidi)
            minimumMidi = midi;
        if (midi > maximumMidi)
            maximumMidi = midi;
    }

    double chroma[12] = {};
    for (int midi = minimumMidi - 12; midi <= maximumMidi + 12; midi++) {
        double fundamental = 440.0 * std::pow(2.0, (midi - 69) / 12.0);
        double salience = 0.0;

        for (int harmonic = 1; harmonic <= CHORD_HARMONICS; harmonic++) {
            double frequency = fundamental * harmonic;
            if (frequency >= state->sampleRate * 0.5)
                break;

            int centerBin = (int)std::round(frequency * CHORD_FFT_SIZE / state->sampleRate);
            for (int offset = -2; offset <= 2; offset++) {
                int bin = centerBin + offset;
                if (bin < 1 || bin >= (int)state->chordSpectrum->length)
                    continue;

                double magnitude = cvec_norm_get_sample(state->chordSpectrum, bin);
                salience += magnitude * magnitude / harmonic;
            }
        }

        chroma[midi % 12] += salience;
    }

    double totalEnergy = 0.0;
    double targetEnergy = 0.0;
    double strongestPitchClass = 0.0;
    for (int pitchClass = 0; pitchClass < 12; pitchClass++) {
        totalEnergy += chroma[pitchClass];
        if (expectedPitchClasses[pitchClass])
            targetEnergy += chroma[pitchClass];
        if (chroma[pitchClass] > strongestPitchClass)
            strongestPitchClass = chroma[pitchClass];
    }

    if (totalEnergy == 0.0)
        return false;
    if (targetEnergy / totalEnergy < CHORD_COVERAGE_THRESHOLD)
        return false;

    for (int pitchClass = 0; pitchClass < 12; pitchClass++) {
        if (expectedPitchClasses[pitchClass] &&
            chroma[pitchClass] < strongestPitchClass * CHORD_TONE_PRESENCE_RATIO) {
            return false;
        }
    }
    return true;
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
        bool pitchMatched = false;
        int detectedMidi = 0;
        if (note.interpretation == "chord") {
            pitchMatched = matchesChord(state, pending, note);
        } else {
            bool hasPitch = selectPitchObservation(state, pending, &selected);
            detectedMidi = hasPitch ? selected.midi : 0;
            pitchMatched = std::abs(detectedMidi - note.midi) <= PITCH_TOLERANCE;
        }
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
            double settleMs = note.interpretation == "chord" ? CHORD_SETTLE_MS : PITCH_SETTLE_MS;
            state->pendingJudgments.push_back({noteIndex,
                                               onsetChartTimeMs,
                                               onsetAudioTimeMs,
                                               errorMs,
                                               onsetChartTimeMs + settleMs,
                                               {}});
            noteIndex++;
            state->nextNoteIndex.store(noteIndex);
        }
    }

    for (PendingJudgment &pending : state->pendingJudgments) {
        const ChartParser::ChartNote &note = state->chart.notes[pending.noteIndex];
        if (note.interpretation != "chord")
            continue;

        for (unsigned int i = 0; i < block->frames; i++)
            pending.chordSamples.push_back(fvec_get_sample(state->input, i));
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
