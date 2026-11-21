#include "DrumMachine.h"
#include <cmath>

namespace {
// One bar of 16th notes per instrument: X accent, x hit, o ghost, r roll (two 32nds), . rest.
struct Pattern {
    const char* name;
    float swing;   // delay of the off-beat 16ths, as a fraction of a step
    const char* lines[7];   // kick, snare, clap, hat, open hat, rim, shaker
};

const Pattern patterns[DrumMachine::patternCount] {
    { "DESLIGADA", 0.0f, { "................", "................", "................", "................", "................", "................", "................" } },
    { "TRAP",      0.0f, { "X.....x...x.....", "................", "........X.......", "x.x.x.x.x.x.rrx.", "................", "................", "................" } },
    { "FUNK BR",   0.0f, { "X..x....X..x....", "................", "....X.......X...", "x.x.x.x.x.x.x.x.", "................", "..x..x....x..x..", "................" } },
    { "DRILL",     0.0f, { "X.........x.....", "............X...", "....X...........", "x..x..x.x.r..x..", "................", "................", "................" } },
    { "BOOM BAP",  0.22f, { "X......x.x......", "....X.......X...", "................", "x.x.x.x.x.x.x.x.", "................", "...............o", "................" } },
    { "LO-FI",     0.28f, { "X......x..x.....", "....x.......x...", "................", "x.oxx.oxx.oxx.ox", "................", "................", "................" } },
    { "HOUSE",     0.0f, { "X...X...X...X...", "................", "....x.......x...", "x.x.x.x.x.x.x.x.", "..x...x...x...x.", "................", "................" } },
    { "REGGAETON", 0.0f, { "X...X...X...X...", "...x..x....x..x.", "................", "x.x.x.x.x.x.x.x.", "................", "................", "................" } },
    { "AFROBEATS", 0.08f, { "X.....x...x.....", "................", "....x.......x...", "................", "................", "...x..x....x..x.", "oxoxoxoxoxoxoxox" } },
    { "PHONK",     0.0f, { "X......x..x.....", "................", "....X.......X...", "xoxoxoxoxoxoxoxo", "................", "................", "................" } },
};

float level(char c) {
    switch (c) { case 'X': return 1.0f; case 'x': return 0.78f; case 'o': return 0.42f; case 'r': return 0.55f; default: return 0.0f; }
}
}

const char* DrumMachine::patternName(int index) { return patterns[juce::jlimit(0, patternCount - 1, index)].name; }

void DrumMachine::prepare(double sampleRate) {
    sr = sampleRate > 1.0 ? sampleRate : 44100.0;
    stop();
}

void DrumMachine::trigger(int instrument, float velocity) {
    auto& v = voices[(size_t) instrument];
    v.active = true;
    v.age = 0.0;
    v.phase = 0.0;
    v.velocity = velocity;
    v.hp = v.lpState = v.prevNoise = 0.0f;
    if (instrument == hat) voices[(size_t) openHat].active = false;   // closed hat chokes the open one
}

float DrumMachine::renderVoice(int instrument, Voice& v) {
    const double t = v.age;
    v.age += 1.0 / sr;
    float out = 0.0f;
    switch (instrument) {
        case kick: {
            const double freq = 46.0 + 110.0 * std::exp(-t / 0.035);
            v.phase += juce::MathConstants<double>::twoPi * freq / sr;
            out = (float) std::sin(v.phase) * (float) std::exp(-t / 0.32) + 0.35f * noise() * (float) std::exp(-t / 0.003);
            out = std::tanh(1.6f * out) * 0.95f;
            if (t > 1.2) v.active = false;
            break;
        }
        case snare: {
            v.phase += juce::MathConstants<double>::twoPi * 185.0 / sr;
            const float n = noise();
            const float bright = n - v.prevNoise; v.prevNoise = n;
            out = 0.45f * (float) std::sin(v.phase) * (float) std::exp(-t / 0.06) + 0.55f * bright * (float) std::exp(-t / 0.13);
            if (t > 0.6) v.active = false;
            break;
        }
        case clap: {
            // Three quick bursts then a short tail, high-passed.
            const double ms = t * 1000.0;
            float env = 0.0f;
            for (int i = 0; i < 3; ++i) { const double d = ms - i * 9.0; if (d >= 0.0) env = juce::jmax(env, (float) std::exp(-d / 3.0)); }
            if (ms > 18.0) env = juce::jmax(env, 0.7f * (float) std::exp(-(ms - 18.0) / 120.0));
            const float n = noise();
            const float bright = n - v.prevNoise; v.prevNoise = n;
            v.lpState += 0.55f * (bright - v.lpState);
            out = 0.9f * v.lpState * env;
            if (t > 0.6) v.active = false;
            break;
        }
        case hat:
        case openHat: {
            const float n = noise();
            const float bright = n - v.prevNoise; v.prevNoise = n;
            const double decay = instrument == hat ? 0.032 : 0.22;
            out = 0.32f * bright * (float) std::exp(-t / decay);
            if (t > decay * 8.0) v.active = false;
            break;
        }
        case rim: {
            v.phase += juce::MathConstants<double>::twoPi * 820.0 / sr;
            out = 0.4f * (float) std::sin(v.phase) * (float) std::exp(-t / 0.025) + 0.15f * noise() * (float) std::exp(-t / 0.006);
            if (t > 0.2) v.active = false;
            break;
        }
        case shaker: {
            const float n = noise();
            const float bright = n - v.prevNoise; v.prevNoise = n;
            const float env = (float) (t < 0.012 ? t / 0.012 : std::exp(-(t - 0.012) / 0.045));
            out = 0.2f * bright * env;
            if (t > 0.3) v.active = false;
            break;
        }
        default: break;
    }
    return out * v.velocity;
}

void DrumMachine::render(juce::AudioBuffer<float>& buffer, int offset, int numSamples, int64_t position, double bpm, bool sequencing, float gain) {
    const auto& p = patterns[(size_t) pattern];
    const double step = sr * 60.0 / juce::jlimit(20.0, 400.0, bpm) / 4.0;
    const int channels = juce::jmin(2, buffer.getNumChannels());
    for (int i = 0; i < numSamples; ++i) {
        if (sequencing && pattern > 0) {
            // Triggers whose (swung) time falls on this sample.
            const int64_t now = position + i;
            const int64_t bar = (int64_t) std::floor((double) now / (step * 16.0));
            const double inBar = (double) now - (double) bar * step * 16.0;
            for (int s = 0; s < 16; ++s) {
                const double start = step * s + (s % 2 == 1 ? p.swing * step : 0.0);
                for (int sub = 0; sub < 2; ++sub) {
                    const double at = start + sub * step * 0.5;
                    if (inBar < at || inBar >= at + 1.0) continue;
                    for (int inst = 0; inst < (int) instrumentCount; ++inst) {
                        const char c = p.lines[inst][s];
                        if (c == '.' || (sub == 1 && c != 'r')) continue;
                        trigger(inst, level(c));
                    }
                }
            }
        }
        float mix = 0.0f;
        for (int inst = 0; inst < (int) voices.size(); ++inst)
            if (voices[(size_t) inst].active) mix += renderVoice(inst, voices[(size_t) inst]);
        mix *= gain;
        for (int c = 0; c < channels; ++c) buffer.addSample(c, offset + i, mix);
    }
}
