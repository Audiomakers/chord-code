#include "PreviewSynth.h"

namespace {
Synth::Fx fx(float rev, float time, float chorus = 0, float delay = 0, float lp = 12000, float hp = 30, float drive = 0, float tape = 0,
             float shimmer = 0, float damp = 0.4f, float pre = 15, float beats = 0.75f, float feedback = 0.35f) {
    Synth::Fx f;
    f.reverbMix = rev; f.reverbTime = time; f.chorus = chorus; f.delayMix = delay; f.lowpass = lp; f.highpass = hp; f.drive = drive;
    f.tape = tape; f.octaveUp = shimmer; f.reverbDamp = damp; f.preDelayMs = pre; f.delayBeats = beats; f.delayFeedback = feedback;
    return f;
}
struct TimbrePatch { const char* patch; float gainDb; Synth::Fx effects; };
// Same order as ccp::Timbre. Gains measured (AudioTests loudness) so every timbre plays generated progressions
// at about -19 dB RMS at full volume, and percussive attacks hit the -1 dBFS limiter by 2.5 dB at most.
const TimbrePatch& timbrePatch(ccp::Timbre t) {
    static const TimbrePatch table[] {
        { "piano_chords",      1.5f, fx(0.20f, 1.6f) },
        { "piano_cinema",      0.2f, fx(0.38f, 3.2f, 0.1f, 0.0f, 9000, 30, 0, 0, 0.1f, 0.3f, 30) },
        { "piano_lofi_chords", 3.4f, fx(0.22f, 1.8f, 0.1f, 0.08f, 4200, 140, 0.3f, 0.7f, 0, 0.6f, 12, 0.5f, 0.3f) },
        { "rhodes",            1.9f, fx(0.22f, 1.8f, 0.35f, 0.06f, 9000) },
        { "rhodes_suitcase",   1.7f, fx(0.2f, 1.6f, 0.2f, 0.08f, 7500, 40, 0.15f) },
        { "wurli_chords",      -0.3f, fx(0.2f, 1.4f, 0.25f, 0.06f, 8000, 40, 0.2f) },
        { "fm_ep_80s",         2.6f, fx(0.28f, 2.2f, 0.65f, 0.1f, 11000) },
        { "vibes_chords",      4.5f, fx(0.32f, 2.4f, 0.1f, 0.05f, 10000) },
        { "m1_organ",          5.8f, fx(0.18f, 1.4f, 0.1f, 0.15f, 10000, 50) },
        { "church_organ",     -4.1f, fx(0.48f, 5.0f, 0.1f, 0.0f, 9000, 30, 0, 0, 0, 0.3f, 40) },
        { "strings_ens",      1.6f, fx(0.42f, 3.5f, 0.15f, 0.0f, 10000, 40, 0, 0, 0, 0.35f, 30) },
        { "orchestra",        -2.6f, fx(0.45f, 3.8f, 0.1f, 0.0f, 10000, 40, 0, 0, 0, 0.35f, 35) },
        { "brass_section",    -3.2f, fx(0.28f, 2.2f, 0.1f, 0.0f, 10000, 50, 0.1f) },
        { "dark_pad_body",    -2.1f, fx(0.38f, 3.5f, 0.25f, 0.14f, 2800, 40, 0.3f, 0, 0, 0.7f, 20, 1.0f, 0.35f) },
        { "juno_pad_wide",    -3.8f, fx(0.32f, 2.8f, 0.85f, 0.1f, 9000) },
        { "atmos_pad",        0.1f, fx(0.55f, 6.5f, 0.35f, 0.3f, 8000, 60, 0, 0, 0.2f, 0.35f, 45, 0.75f, 0.5f) },
        { "synthwave_poly",    -1.0f, fx(0.35f, 2.8f, 0.6f, 0.18f, 9000, 40, 0, 0, 0, 0.4f, 20, 0.75f, 0.4f) },
        { "supersaw_big",      2.1f, fx(0.35f, 3.0f, 0.1f, 0.2f, 12000, 80, 0, 0, 0, 0.35f, 20, 0.75f, 0.4f) },
        { "saw_stab_fat",      6.3f, fx(0.28f, 2.0f, 0.1f, 0.25f, 12000, 80, 0.1f, 0, 0, 0.4f, 10, 0.75f, 0.4f) },
    };
    static_assert(std::size(table) == (size_t) ccp::Timbre::count, "one patch per timbre");
    return table[(size_t) juce::jlimit(0, (int) ccp::Timbre::count - 1, (int) t)];
}
}

PreviewSynth::PreviewSynth() { Sets::noFold = true; }

void PreviewSynth::prepare(double sampleRate, int) {
    sr = sampleRate > 1.0 ? sampleRate : 44100.0;
    synth.prepare(sr);
    synth.setKit(0);
    synth.setLevels(0.8f, 0.8f, 0.8f);
    setTimbre(current);
}

void PreviewSynth::setTimbre(ccp::Timbre t) {
    current = t;
    const auto& p = timbrePatch(t);
    synth.allNotesOff();
    synth.setChordPatch(p.patch, p.gainDb);
    synth.setFx(p.effects);
}

void PreviewSynth::allNotesOff(bool) { synth.allNotesOff(); }

bool PreviewSynth::isActive() const { return synth.activeVoices() > 0; }

void PreviewSynth::render(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    const int n = buffer.getNumSamples();
    float* l = buffer.getWritePointer(0);
    float* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;
    juce::HeapBlock<float> scratch;
    if (r == nullptr) { scratch.allocate((size_t) n, true); r = scratch.get(); }
    int pos = 0;
    const int held = (int) (sr * 60.0);   // the note-off ends it
    for (const auto meta : midi) {
        const int at = juce::jlimit(0, n, meta.samplePosition);
        if (at > pos) { synth.render(l + pos, r + pos, at - pos); pos = at; }
        const auto m = meta.getMessage();
        if (m.isNoteOn()) synth.noteOn(m.getNoteNumber(), m.getVelocity(), held, false, 2);
        else if (m.isNoteOff()) synth.noteOff(m.getNoteNumber(), 2);
        else if (m.isAllNotesOff() || m.isAllSoundOff()) synth.allNotesOff();
    }
    if (pos < n) synth.render(l + pos, r + pos, n - pos);
}
