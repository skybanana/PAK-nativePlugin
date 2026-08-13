#pragma once

#include <string>
#include <vector>

namespace ChartParser {

struct ChartNote {
    std::string interpretation;
    int startTick;
    int durationTick;
    int stringNumber;
    int fret;
    int finger;
    int startMs;
    int durationMs;
    int midi;
    std::string noteName;
    std::string technique;
    std::string chordId;
    std::string strumTechnique;
    std::vector<int> chordMidis;
};

struct Chart {
    std::string schemaVersion;
    std::string songId;
    std::string title;
    std::string artist;
    double bpm;
    int resolution;
    std::string timeSignature;
    int durationMs;
    std::string audioFile;
    int audioOffsetMs;
    std::string trackId;
    std::string instrument;
    std::string difficulty;
    std::vector<std::string> tuning;
    std::vector<ChartNote> notes;
};

bool loadChart(const std::string &path, Chart &chart);

}
