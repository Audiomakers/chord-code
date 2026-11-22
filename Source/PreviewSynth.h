#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Core/Music.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include "Synth.h"

// Preview instrument. Since 1.4 every timbre is a patch of the Audio Makers synth engine shared with Melody Code
// (Sets.cpp: layered oscillators, FM, additive, plucked strings and the CC0 sampled piano) with its own effects
// (chorus, drive, tape, shimmer, delay, reverb). The chords arrive as MIDI note-ons and note-offs.
class PreviewSynth {
public:
    PreviewSynth();
    void prepare(double sampleRate, int maximumBlockSize = 512);
    void render(juce::AudioBuffer<float>&, juce::MidiBuffer&);
    void allNotesOff(bool allowTailOff = true);
    bool isActive() const;
    void setTimbre(ccp::Timbre);
    void setTempo(double bpm) { synth.setTempo(bpm); }
private:
    Synth synth;
    double sr { 44100.0 };
    ccp::Timbre current { ccp::Timbre::piano };
};
