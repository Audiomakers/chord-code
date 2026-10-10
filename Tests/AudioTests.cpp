#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

static void choice(ChordCodeProProcessor& processor, const char* id, int value) {
    auto* parameter = processor.apvts.getParameter(id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1((float) value));
}

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI initialise;
    try {
        constexpr int blockSize = 128;
        juce::AudioBuffer<float> audio(2, blockSize);
        juce::MidiBuffer midi;
        for (double rate : { 44100.0, 48000.0, 96000.0 }) {
            for (int timbre = 0; timbre < (int) ccp::Timbre::count; ++timbre) {
                std::cout << "Transition: " << rate << " Hz, timbre " << timbre << std::endl;
                PreviewSynth synth;
                synth.prepare(rate, blockSize);
                synth.setTimbre((ccp::Timbre) timbre);
                midi.clear();
                for (int note : { 60, 64, 67 }) midi.addEvent(juce::MidiMessage::noteOn(1, note, (juce::uint8) 100), 0);
                for (int block = 0; block < 87; ++block) {
                    audio.clear(); synth.render(audio, midi); midi.clear();
                }
                const float previous = audio.getSample(0, blockSize - 1);
                synth.allNotesOff();
                for (int note : { 62, 65, 69 }) midi.addEvent(juce::MidiMessage::noteOn(1, note, (juce::uint8) 100), 0);
                audio.clear(); synth.render(audio, midi); midi.clear();
                require(std::abs(audio.getSample(0, 0) - previous) < 0.035f, "Discontinuity at chord switch");
                synth.allNotesOff();
                for (int block = 0; block < (int) (rate * 8.0 / blockSize); ++block) {
                    audio.clear(); synth.render(audio, midi);
                    for (int i = 0; i < blockSize; ++i)
                        require(std::isfinite(audio.getSample(0, i)), "Non-finite audio");
                }
                require(audio.getMagnitude(0, blockSize) < 0.000001f, "Audio did not settle to silence");
            }
        }
        for (int timbre = 0; timbre < (int) ccp::Timbre::count; ++timbre) {
            ChordCodeProProcessor processor;
            processor.prepareToPlay(48000, blockSize);
            choice(processor, "timbre", timbre);
            processor.audition(2);
            for (int block = 0; block < 100; ++block) processor.processBlock(audio, midi);
            require(processor.activeChord() == 2, "Card cleared before the release finished");
            for (int block = 0; block < 600; ++block) processor.processBlock(audio, midi);
            require(processor.activeChord() == -1, "Card remained active after silence");
            processor.audition(0);
            for (int block = 0; block < 70; ++block) processor.processBlock(audio, midi);
            processor.audition(1); processor.processBlock(audio, midi);
            require(processor.activeChord() == 1, "New chord did not become active");
            processor.stopPreview(); processor.processBlock(audio, midi);
            require(processor.activeChord() == 1, "Stop discarded release highlight");
            for (int block = 0; block < 600; ++block) processor.processBlock(audio, midi);
            require(processor.activeChord() == -1, "Stopped preview stuck active");
        }
        ChordCodeProProcessor processor;
        processor.prepareToPlay(48000, blockSize);
        for (int count : { 4, 8 }) {
            processor.setChordCount(count);
            const auto progression = processor.getProgression();
            const auto options = processor.midiOptions();
            const auto bytes = ccp::MidiFile::bytes(progression, options);
            juce::MemoryInputStream stream(bytes.data(), bytes.size(), false);
            juce::MidiFile file;
            require(file.readFrom(stream), "Invalid MIDI export");
            int expected = 0, noteOns = 0;
            for (int i = 0; i < count; ++i) expected += progression.chords[(size_t) i].noteCount;
            for (int track = 0; track < file.getNumTracks(); ++track)
                for (auto* event : *file.getTrack(track)) if (event->message.isNoteOn()) ++noteOns;
            require(noteOns == expected, "MIDI export omitted chords");
            processor.playSequence();
            for (int block = 0; block < (count * 48000 / blockSize) + 600; ++block) processor.processBlock(audio, midi);
            require(processor.activeChord() == -1, "Sequence did not stop");
        }
        processor.toggleLock(0);
        juce::MemoryBlock state; processor.getStateInformation(state);
        ChordCodeProProcessor restored;
        restored.setStateInformation(state.getData(), (int) state.getSize());
        require(state.getSize() > 0, "Session state was not serialized");
        require(processor.getName() == "Chord Code", "Incorrect product name");
        if (argc > 1) {
            juce::File folder = juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]);
            folder.createDirectory();
            auto save = [&](juce::Component& component, const char* name) {
                auto image = component.createComponentSnapshot(component.getLocalBounds(), true, 2.0f);
                juce::FileOutputStream output(folder.getChildFile(name));
                require(output.openedOk() && juce::PNGImageFormat().writeImageToStream(image, output), "Snapshot failed");
            };
            ChordCardComponent card;
            card.setSize(310, 252); card.setChord(processor.getProgression().chords[0]); card.setPlaying(true);
            save(card, "active-card.png");
            auto editor = std::unique_ptr<juce::AudioProcessorEditor>(processor.createEditor());
            save(*editor, "interface.png");
        }
        std::cout << "Audio transitions, release highlights, 4/8 chord MIDI and state tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
    return 0;
}
