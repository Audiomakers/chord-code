#include "DrumMachine.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <BinaryData.h>
#include <cmath>
#include <map>
#include <string>

namespace {
// Steps: X accent, x hit, o ghost, r roll (two 32nds), . rest.
struct Line {
    const char* sample;   // resource in Assets/Drums, nullptr = unused
    const char* steps;
    float gain;
    float pan;
    bool bass;            // 808: tuned to the chord's bass note
};
struct Pattern {
    const char* name;
    float swing;          // delay of the off-beat 16ths, as a fraction of a step
    Line lines[DrumMachine::maxLines];
};

const Pattern patterns[DrumMachine::patternCount] {
    { "DESLIGADA", 0.0f, {} },
    // Half-time trap: clap on 3, rolling hats, 808 on the kick.
    { "TRAP", 0.0f, {
        { "kick_brute_wav", "X.....x...x.....", 0.95f, 0.0f, false },
        { "clap_beefy_wav", "........X.......", 0.70f, 0.0f, false },
        { "hat_shook_wav",  "x.x.x.x.x.x.rrx.", 0.34f, 0.15f, false },
        { "open_808_wav",   "..............x.", 0.22f, -0.15f, false },
        { "bass_808_wav",   "X.....x...x.....", 0.62f, 0.0f, true } } },
    // Tamborzao: 3+3+2 kick twice per bar, clap on 2 and 4, tambor filling the gaps, heavy 808.
    { "FUNK BR", 0.0f, {
        { "kick_knock_wav", "X..x..x.X..x..x.", 0.95f, 0.0f, false },
        { "clap_1_wav",     "....X.......X...", 0.72f, 0.0f, false },
        { "conga_wav",      "..o..x.x..o..x.x", 0.50f, 0.25f, false },
        { "rim_808_wav",    ".......x.......x", 0.30f, -0.25f, false },
        { "bass_808_wav",   "X..x..x.X..x..x.", 0.55f, 0.0f, true } } },
    // Drill: sliding-feel kick, late snare, triplet-ish hats.
    { "DRILL", 0.0f, {
        { "kick_hammer_wav", "X......x..x.....", 0.95f, 0.0f, false },
        { "snare_racks_wav", "........X.....o.", 0.70f, 0.0f, false },
        { "hat_tight_wav",   "x..x..x.x..x.x..", 0.34f, 0.15f, false },
        { "rim_808_wav",     "..x.........x...", 0.28f, -0.2f, false },
        { "bass_808_wav",    "X......x..x.....", 0.60f, 0.0f, true } } },
    { "BOOM BAP", 0.22f, {
        { "kick_8_wav",      "X.......X.x.....", 0.95f, 0.0f, false },
        { "snare_vinyl_wav", "....X.......X...", 0.78f, 0.0f, false },
        { "hat_1_wav",       "x.x.x.x.x.x.x.x.", 0.32f, 0.15f, false },
        { "perc_wood_wav",   "...............o", 0.30f, -0.2f, false } } },
    { "LO-FI", 0.28f, {
        { "kick_dash_wav",  "X......x..x.....", 0.92f, 0.0f, false },
        { "snare_lofi_wav", "....x.......x...", 0.68f, 0.0f, false },
        { "hat_soft_wav",   "x.oxx.oxx.oxx.ox", 0.30f, 0.15f, false },
        { "rim_kiss_wav",   "...........o....", 0.32f, -0.2f, false } } },
    { "HOUSE", 0.0f, {
        { "kick_house_wav", "X...X...X...X...", 0.95f, 0.0f, false },
        { "clap_01_wav",    "....X.......X...", 0.62f, 0.0f, false },
        { "hat_tight_wav",  "o.o.o.o.o.o.o.o.", 0.22f, 0.2f, false },
        { "open_1_wav",     "..x...x...x...x.", 0.26f, -0.15f, false } } },
    // Dembow.
    { "REGGAETON", 0.0f, {
        { "kick_red_wav",      "X...X...X...X...", 0.95f, 0.0f, false },
        { "snare_snaprim_wav", "...x..x....x..x.", 0.62f, 0.0f, false },
        { "hat_tight_wav",     "x.x.x.x.x.x.x.x.", 0.26f, 0.15f, false } } },
    { "AFROBEATS", 0.08f, {
        { "kick_dash_wav", "X.....x...x.....", 0.92f, 0.0f, false },
        { "clap_1_wav",    "....x.......x...", 0.55f, 0.0f, false },
        { "rim_10_wav",    "..x..x..x...x.x.", 0.40f, -0.2f, false },
        { "conga_wav",     "......x.x.....x.", 0.45f, 0.25f, false },
        { "perc_skit_wav", "oxoxoxoxoxoxoxox", 0.22f, 0.1f, false } } },
    { "PHONK", 0.0f, {
        { "kick_red_wav",  "X......x..x.....", 0.95f, 0.0f, false },
        { "clap_bold_wav", "....X.......X...", 0.70f, 0.0f, false },
        { "hat_pish_wav",  "xoxoxoxoxoxoxoxo", 0.28f, 0.15f, false },
        { "bass_808_wav",  "X......x..x.....", 0.55f, 0.0f, true } } },
};

float level(char c) {
    switch (c) { case 'X': return 1.0f; case 'x': return 0.8f; case 'o': return 0.45f; case 'r': return 0.6f; default: return 0.0f; }
}

// Every one-shot, decoded once (mono, 44.1 kHz).
const std::vector<float>* sampleFor(const char* resource) {
    static const std::map<std::string, std::vector<float>> cache = [] {
        std::map<std::string, std::vector<float>> m;
        juce::WavAudioFormat wav;
        for (const auto& p : patterns)
            for (const auto& line : p.lines) {
                if (line.sample == nullptr || m.count(line.sample)) continue;
                int size = 0;
                const char* data = BinaryData::getNamedResource(line.sample, size);
                if (data == nullptr) continue;
                std::unique_ptr<juce::AudioFormatReader> reader(
                    wav.createReaderFor(new juce::MemoryInputStream(data, (size_t) size, false), true));
                if (!reader) continue;
                juce::AudioBuffer<float> buf(1, (int) reader->lengthInSamples);
                reader->read(&buf, 0, buf.getNumSamples(), 0, true, false);
                m[line.sample].assign(buf.getReadPointer(0), buf.getReadPointer(0) + buf.getNumSamples());
            }
        return m;
    }();
    if (resource == nullptr) return nullptr;
    auto it = cache.find(resource);
    return it == cache.end() ? nullptr : &it->second;
}
constexpr double sampleRateOfAssets = 44100.0;
}

const char* DrumMachine::patternName(int index) { return patterns[juce::jlimit(0, patternCount - 1, index)].name; }

DrumMachine::DrumMachine() { sampleFor(nullptr); }   // decode on the message thread, not on the first beat

void DrumMachine::prepare(double sampleRate) {
    sr = sampleRate > 1.0 ? sampleRate : 44100.0;
    stop();
}

void DrumMachine::setBassNotes(const int64_t* starts, const int* pitchClasses, int count) {
    bassCount = juce::jlimit(0, (int) bassStarts.size(), count);
    for (int i = 0; i < bassCount; ++i) { bassStarts[(size_t) i] = starts[i]; bassNotes[(size_t) i] = pitchClasses[i]; }
}

int DrumMachine::bassPitchClassAt(int64_t position) const {
    int pc = bassCount > 0 ? bassNotes[0] : 0;
    for (int i = 0; i < bassCount; ++i)
        if (bassStarts[(size_t) i] <= position) pc = bassNotes[(size_t) i];
    return pc;
}

void DrumMachine::trigger(int line, float velocity, int64_t at) {
    const auto& l = patterns[(size_t) pattern].lines[line];
    const auto* sample = sampleFor(l.sample);
    if (sample == nullptr || sample->empty()) return;
    // The line's previous hit fades out over 4 ms instead of clicking off.
    auto& a = voices[(size_t) line * 2];
    auto& b = voices[(size_t) line * 2 + 1];
    if (a.active) { b = a; b.fadeStep = b.fade / (float) juce::jmax(1.0, 0.004 * sr); }
    a = {};
    a.active = true;
    a.sample = sample;
    double ratio = 1.0;
    if (l.bass) {   // the 808 one-shot is a C: shift to the bass note, never more than half an octave
        int semis = ((bassPitchClassAt(at) % 12) + 12) % 12;
        if (semis > 6) semis -= 12;
        ratio = std::pow(2.0, semis / 12.0);
    }
    a.inc = ratio * sampleRateOfAssets / sr;
    a.gain = l.gain * velocity;
    a.pan = l.pan;
}

void DrumMachine::render(juce::AudioBuffer<float>& buffer, int offset, int numSamples, int64_t position, double bpm, bool sequencing, float gain) {
    const auto& p = patterns[(size_t) pattern];
    const double step = sr * 60.0 / juce::jlimit(20.0, 400.0, bpm) / 4.0;
    const int channels = juce::jmin(2, buffer.getNumChannels());
    for (int i = 0; i < numSamples; ++i) {
        const int64_t now = position + i;
        if (sequencing && pattern > 0) {
            // Triggers whose (swung) time falls on this sample.
            const int64_t bar = (int64_t) std::floor((double) now / (step * 16.0));
            const double inBar = (double) now - (double) bar * step * 16.0;
            for (int s = 0; s < 16; ++s) {
                const double start = step * s + (s % 2 == 1 ? p.swing * step : 0.0);
                for (int sub = 0; sub < 2; ++sub) {
                    const double at = start + sub * step * 0.5;
                    if (inBar < at || inBar >= at + 1.0) continue;
                    for (int line = 0; line < maxLines; ++line) {
                        if (p.lines[line].sample == nullptr) continue;
                        const char c = p.lines[line].steps[s];
                        if (c == '.' || (sub == 1 && c != 'r')) continue;
                        trigger(line, level(c), now);
                    }
                }
            }
        }
        float left = 0.0f, right = 0.0f;
        for (auto& v : voices) {
            if (!v.active) continue;
            const auto& s = *v.sample;
            const int i0 = (int) v.pos;
            if (i0 + 1 >= (int) s.size()) { v.active = false; continue; }
            const float frac = (float) (v.pos - i0);
            float x = (s[(size_t) i0] + frac * (s[(size_t) i0 + 1] - s[(size_t) i0])) * v.gain;
            if (v.fadeStep > 0.0f) {
                x *= v.fade;
                v.fade -= v.fadeStep;
                if (v.fade <= 0.0f) { v.active = false; continue; }
            }
            v.pos += v.inc;
            left += x * (1.0f - juce::jmax(0.0f, v.pan));
            right += x * (1.0f + juce::jmin(0.0f, v.pan));
        }
        if (channels == 1) { buffer.addSample(0, offset + i, 0.5f * (left + right) * gain); continue; }
        buffer.addSample(0, offset + i, left * gain);
        buffer.addSample(1, offset + i, right * gain);
    }
}
