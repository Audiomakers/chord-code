#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>

// Reference beat played under REPRODUZIR: one bar of 16 steps per pattern, played with
// one-shots from the AMKR Formula 30 pack (Assets/Drums). The 808 line follows the bass
// note of the chord that is playing. Kept under the chords and never exported.
class DrumMachine {
public:
    static constexpr int patternCount = 10;          // index 0 = off
    static constexpr int maxLines = 6;
    static const char* patternName(int index);       // "DESLIGADA", "TRAP", ...

    DrumMachine();
    void prepare(double sampleRate);
    void setPattern(int index) { pattern = juce::jlimit(0, patternCount - 1, index); }
    int getPattern() const { return pattern; }
    // Bass notes for the 808: pitch class of each chord and the timeline sample it starts at.
    void setBassNotes(const int64_t* starts, const int* pitchClasses, int count);
    // Adds the beat for the timeline span [position, position + count) into buffer at offset.
    // position is in samples from the start of the loop; triggers only happen when sequencing is true.
    void render(juce::AudioBuffer<float>& buffer, int offset, int numSamples, int64_t position, double bpm, bool sequencing, float gain);
    void stop() { for (auto& v : voices) v.active = false; }

private:
    struct Voice {
        bool active {};
        const std::vector<float>* sample {};
        double pos {}, inc {};
        float gain {}, fade { 1.0f }, fadeStep {};
        float pan {};
    };
    void trigger(int line, float velocity, int64_t at);
    int bassPitchClassAt(int64_t position) const;

    double sr { 44100.0 };
    int pattern {};
    std::array<Voice, maxLines * 2> voices {};   // two per line: the new hit and the one fading out
    std::array<int64_t, 8> bassStarts {};
    std::array<int, 8> bassNotes {};
    int bassCount {};
};
