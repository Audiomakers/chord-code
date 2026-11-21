#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>

// Reference beat played under REPRODUZIR. Everything is synthesised (no samples), one bar
// of 16 steps per pattern, kept quiet enough to sit under the chords. Never exported.
class DrumMachine {
public:
    static constexpr int patternCount = 10;          // index 0 = off
    static const char* patternName(int index);       // "DESLIGADA", "TRAP", ...

    void prepare(double sampleRate);
    void setPattern(int index) { pattern = juce::jlimit(0, patternCount - 1, index); }
    int getPattern() const { return pattern; }
    // Adds the beat for the timeline span [position, position + count) into buffer at offset.
    // position is in samples from the start of the loop; triggers only happen when sequencing is true.
    void render(juce::AudioBuffer<float>& buffer, int offset, int numSamples, int64_t position, double bpm, bool sequencing, float gain);
    void stop() { for (auto& v : voices) v.active = false; }

private:
    enum Instrument { kick, snare, clap, hat, openHat, rim, shaker, instrumentCount };
    struct Voice {
        bool active {};
        double age {}, phase {};
        float velocity {}, hp {}, lpState {}, prevNoise {};
    };
    void trigger(int instrument, float velocity);
    float renderVoice(int instrument, Voice&);
    float noise() { return random.nextFloat() * 2.0f - 1.0f; }

    double sr { 44100.0 };
    int pattern {};
    std::array<Voice, instrumentCount> voices {};
    juce::Random random { 1234 };
};
