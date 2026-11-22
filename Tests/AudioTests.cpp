#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "LookAndFeel.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <set>

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
        if (argc > 1 && juce::String(argv[1]) == "loudness") {
            // per timbre and complexity: RMS (dB) of generated progressions and the deepest limiter gain reduction
            ChordCodeProProcessor processor;
            processor.prepareToPlay(44100.0, blockSize);
            for (int timbre = 0; timbre < (int) ccp::Timbre::count; ++timbre) {
                std::cout << ccp::timbreName((ccp::Timbre) timbre);
                for (int complexity = 0; complexity < 4; ++complexity) {
                    choice(processor, "complexity", complexity);
                    double sum = 0; long n = 0; float worst = 1.0f;
                    for (int round = 0; round < 6; ++round) {
                        processor.generate();
                        const auto prog = processor.getProgression();
                        PreviewSynth synth; synth.prepare(44100.0, blockSize); synth.setTimbre((ccp::Timbre) timbre);
                        midi.clear();
                        for (int c = 0; c < prog.chordCount; ++c) {
                            const auto& chord = prog.chords[(size_t) c];
                            for (int k = 0; k < chord.noteCount; ++k) midi.addEvent(juce::MidiMessage::noteOn(1, chord.notes[(size_t) k], (juce::uint8) 127), 0);
                            for (int block = 0; block < 44100 / blockSize; ++block) {
                                audio.clear(); synth.render(audio, midi); midi.clear();
                                for (int i = 0; i < blockSize; ++i) { const double x = audio.getSample(0, i); sum += x * x; ++n; }
                            }
                            for (int k = 0; k < chord.noteCount; ++k) midi.addEvent(juce::MidiMessage::noteOff(1, chord.notes[(size_t) k]), 0);
                        }
                        worst = std::min(worst, synth.takeMinLimiterGain());
                    }
                    std::cout << "  |  " << juce::String(juce::Decibels::gainToDecibels(std::sqrt(sum / (double) n)), 1)
                              << " dB rms, lim " << juce::String(juce::Decibels::gainToDecibels(worst), 1);
                }
                std::cout << std::endl;
            }
            return 0;
        }
        if (argc > 1 && juce::String(argv[1]) == "levels") {
            // RMS of a C major triad over its first second, before normalisation (to tune normalisationGain).
            for (int timbre = 0; timbre < (int) ccp::Timbre::count; ++timbre) {
                PreviewSynth synth; synth.prepare(44100.0, blockSize); synth.setTimbre((ccp::Timbre) timbre);
                midi.clear();
                for (int note : { 60, 64, 67 }) midi.addEvent(juce::MidiMessage::noteOn(1, note, (juce::uint8) 88), 0);
                double sum = 0; int n = 0;
                for (int block = 0; block < 44100 / blockSize; ++block) {
                    audio.clear(); synth.render(audio, midi); midi.clear();
                    for (int i = 0; i < blockSize; ++i) { const double x = audio.getSample(0, i); sum += x * x; ++n; }
                }
                std::cout << ccp::timbreName((ccp::Timbre) timbre) << " rms " << std::sqrt(sum / n) << std::endl;
            }
            return 0;
        }
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
                float naturalStep = 0.0f;   // bright timbres move this much between samples on their own
                for (int i = 1; i < blockSize; ++i) naturalStep = juce::jmax(naturalStep, std::abs(audio.getSample(0, i) - audio.getSample(0, i - 1)));
                synth.allNotesOff();
                for (int note : { 62, 65, 69 }) midi.addEvent(juce::MidiMessage::noteOn(1, note, (juce::uint8) 100), 0);
                audio.clear(); synth.render(audio, midi); midi.clear();
                require(std::abs(audio.getSample(0, 0) - previous) < juce::jmax(0.035f, naturalStep * 1.5f), "Discontinuity at chord switch");
                synth.allNotesOff();
                // long reverbs and echoes (PAD ATMOSFERA, ÓRGÃO) ring for a while
                for (int block = 0; block < (int) (rate * 20.0 / blockSize); ++block) {
                    audio.clear(); synth.render(audio, midi);
                    for (int i = 0; i < blockSize; ++i)
                        require(std::isfinite(audio.getSample(0, i)), "Non-finite audio");
                }
                if (audio.getMagnitude(0, blockSize) >= 0.00001f) std::cout << "  tail " << audio.getMagnitude(0, blockSize) << std::endl;
                require(audio.getMagnitude(0, blockSize) < 0.00001f, "Audio did not settle to silence");
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
            require(processor.activeChord() >= 0 && processor.isSequencePlaying(), "Sequence should loop until stopped");
            // a new progression restarts the loop from the first chord
            processor.generate(); processor.processBlock(audio, midi);
            require(processor.activeChord() == 0 && processor.isSequencePlaying(), "New chords should restart the loop");
            processor.stopPreview();
            for (int block = 0; block < 600; ++block) processor.processBlock(audio, midi);
            require(processor.activeChord() == -1 && !processor.isSequencePlaying(), "Sequence did not stop");
        }
        // WAV export: valid 24-bit stereo file, exactly one loop long, normalised, every timbre and rhythm.
        processor.setChordCount(4);
        for (int timbre = 0; timbre < (int) ccp::Timbre::count; ++timbre) {
            for (int rhythm = 0; rhythm < 4; ++rhythm) {
                choice(processor, "timbre", timbre);
                choice(processor, "rhythm", rhythm);
                juce::MemoryBlock wavData;
                require(processor.renderWav(wavData), "WAV render failed");
                juce::WavAudioFormat format;
                std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(new juce::MemoryInputStream(wavData, false), true));
                require(reader != nullptr, "WAV unreadable");
                require(reader->numChannels == 2 && reader->bitsPerSample == 24 && (int) reader->sampleRate == 44100, "WAV format");
                const auto options = processor.midiOptions();
                const auto expected = (juce::int64) std::llround(44100.0 * 60.0 / options.bpm * options.beatsPerChord) * 4;
                require(reader->lengthInSamples == expected, "WAV is not exactly one loop long");
                juce::AudioBuffer<float> wavAudio(2, (int) reader->lengthInSamples);
                reader->read(&wavAudio, 0, wavAudio.getNumSamples(), 0, true, true);
                const float peak = wavAudio.getMagnitude(0, wavAudio.getNumSamples());
                require(peak > 0.85f && peak < 0.9f, "WAV not normalised to -1 dBFS");
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < wavAudio.getNumSamples(); ++i)
                        require(std::isfinite(wavAudio.getSample(c, i)), "Non-finite WAV sample");
            }
        }
        choice(processor, "timbre", 0);
        choice(processor, "rhythm", 0);

        // Groove: LONGO-CURTO keeps the loop length but alternates 1.5x / 0.5x.
        {
            auto options = processor.midiOptions();
            options.beatsPerChord = 2.0;
            options.groove = ccp::Groove::longShort;
            require(std::abs(ccp::chordLengthBeats(options, 0) - 3.0) < 1e-9 && std::abs(ccp::chordLengthBeats(options, 1) - 1.0) < 1e-9, "Groove lengths");
            require(std::abs(ccp::chordStartBeats(options, 2) - 4.0) < 1e-9 && std::abs(ccp::chordStartBeats(options, 4) - 8.0) < 1e-9, "Groove keeps the bar");
            const auto bytes = ccp::MidiFile::bytes(processor.getProgression(), options);
            juce::MemoryInputStream stream(bytes.data(), bytes.size(), false);
            juce::MidiFile file;
            require(file.readFrom(stream), "Groove MIDI invalid");
            std::set<int> starts;
            for (auto* event : *file.getTrack(0)) if (event->message.isNoteOn()) starts.insert((int) event->message.getTimeStamp());
            require(starts.count(0) && starts.count(3 * 960) && starts.count(4 * 960) && starts.count(7 * 960), "Groove MIDI timing");
        }

        // Chord detection from played notes.
        {
            ccp::GenerationSettings s; s.key = 0; s.scale = ccp::ScaleType::major;
            auto named = [&](std::vector<int> notes) { return ChordCodeProProcessor::chordFromNotes(s, notes).name; };
            require(named({ 60, 64, 67 }) == "C", "detect C");
            require(named({ 57, 60, 64 }) == "Am", "detect Am");
            require(named({ 64, 67, 72 }) == "C/E", "detect C/E");
            require(named({ 55, 59, 62, 65 }) == "G7", "detect G7");
            require(named({ 62, 65, 69, 72 }) == "Dm7", "detect Dm7");
            require(named({ 60, 64, 67, 71 }) == "Cmaj7", "detect Cmaj7");
            require(named({ 69 }) == "Am", "single note -> scale chord");
            require(ChordCodeProProcessor::chordFromNotes(s, { 60, 64, 67, 71, 74, 77 }).noteCount <= 5, "max five notes");
        }

        // MIDI keyboard: chord modes turn one key into a chord; capture fills and locks a slot.
        {
            ChordCodeProProcessor keys;
            keys.prepareToPlay(48000, blockSize);
            choice(keys, "key", 0); choice(keys, "scale", 0); choice(keys, "complexity", 0);
            choice(keys, "keyMode", 1);
            juce::MidiBuffer in;
            in.addEvent(juce::MidiMessage::noteOn(1, 62, (juce::uint8) 100), 0);   // D -> Dm
            audio.clear(); keys.processBlock(audio, in);
            for (int block = 0; block < 40; ++block) { juce::MidiBuffer none; keys.processBlock(audio, none); }
            require(audio.getMagnitude(0, blockSize) > 0.001f, "Chord mode is silent");

            keys.armCapture(1);
            require(keys.armedSlot() == 1, "Slot not armed");
            juce::MidiBuffer press;
            press.addEvent(juce::MidiMessage::noteOn(1, 67, (juce::uint8) 100), 0);  // G -> G
            keys.processBlock(audio, press);
            require(keys.applyPendingCapture(), "Capture not applied (chord mode)");
            require(keys.getProgression().chords[1].name == "G" && keys.isLocked(1), "Captured chord wrong or not locked");
            // The recorded chord sits in the octave that was played, an octave apart for keys an octave apart.
            for (int octave : { 0, 1, -1 }) {
                keys.armCapture(1);
                juce::MidiBuffer up;
                up.addEvent(juce::MidiMessage::noteOn(1, 67 + 12 * octave, (juce::uint8) 100), 0);
                keys.processBlock(audio, up);
                require(keys.applyPendingCapture(), "Capture not applied (octave)");
                const auto& c = keys.getProgression().chords[1];
                require(std::abs(c.notes[0] - (67 + 12 * octave)) <= 6, "Captured chord in another octave");
                juce::MidiBuffer upOff; upOff.addEvent(juce::MidiMessage::noteOff(1, 67 + 12 * octave), 0);
                keys.processBlock(audio, upOff);
            }

            choice(keys, "keyMode", 0);
            keys.armCapture(2);
            juce::MidiBuffer chordOn, chordOff;
            for (int n : { 57, 60, 64, 67 }) chordOn.addEvent(juce::MidiMessage::noteOn(1, n, (juce::uint8) 90), 0);
            keys.processBlock(audio, chordOn);
            require(!keys.applyPendingCapture(), "Capture finished before release");
            for (int n : { 57, 60, 64, 67 }) chordOff.addEvent(juce::MidiMessage::noteOff(1, n), 0);
            keys.processBlock(audio, chordOff);
            require(keys.applyPendingCapture(), "Capture not applied (notes mode)");
            require(keys.getProgression().chords[2].name == "Am7", "Played chord not detected as Am7");
            require(keys.getProgression().chords[2].notes[0] == 57, "Played voicing not kept");
            keys.armCapture(3); keys.armCapture(3);
            require(keys.armedSlot() == -1, "Second click should disarm");
        }

        // Reference beat: every pattern makes sound, stays finite, loops the preview.
        for (int beat = 1; beat < DrumMachine::patternCount; ++beat) {
            ChordCodeProProcessor beatProcessor;
            beatProcessor.prepareToPlay(48000, blockSize);
            choice(beatProcessor, "beat", beat);
            beatProcessor.playSequence();
            float loudest = 0.0f;
            const int blocks = 4 * 2 * 48000 / blockSize + 200;   // longer than one pass of the loop
            for (int block = 0; block < blocks; ++block) {
                beatProcessor.processBlock(audio, midi);
                for (int i = 0; i < blockSize; ++i) require(std::isfinite(audio.getSample(0, i)), "Beat produced NaN");
                loudest = juce::jmax(loudest, audio.getMagnitude(0, blockSize));
            }
            require(loudest > 0.05f && loudest < 1.5f, "Beat level");
            require(beatProcessor.activeChord() >= 0, "Preview with beat should loop");
            beatProcessor.stopPreview(); beatProcessor.processBlock(audio, midi);
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
            card.setPlaying(false); card.setArmed(true);
            save(card, "armed-card.png");
            {   // Hover explanation, as the timbre box shows it.
                ProLookAndFeel look;
                const juce::String text = juce::String::fromUTF8("RHODES: piano elétrico com tremolo, quente e macio (o mesmo do Melody Code). Ótimo para R&B, lo-fi, neo soul e trap melódico.");
                const auto bounds = look.getTooltipBounds(text, { 10, 10 }, { 0, 0, 800, 400 });
                juce::Image image(juce::Image::ARGB, bounds.getWidth() * 2, bounds.getHeight() * 2, true);
                juce::Graphics g(image);
                g.addTransform(juce::AffineTransform::scale(2.0f));
                look.drawTooltip(g, text, bounds.getWidth(), bounds.getHeight());
                juce::FileOutputStream output(folder.getChildFile("tooltip.png"));
                require(output.openedOk() && juce::PNGImageFormat().writeImageToStream(image, output), "Tooltip snapshot failed");
            }
            auto editor = std::unique_ptr<juce::AudioProcessorEditor>(processor.createEditor());
            save(*editor, "interface.png");
            require(editor->isResizable(), "Editor is not resizable");
            {   // 8 chords (scroll bar), TOM and ESCALA on AUTOMÁTICO, a locked option, MUITO RICO
                ChordCodeProProcessor eight;
                eight.prepareToPlay(48000, blockSize);
                choice(eight, "key", 12); choice(eight, "scale", (int) ccp::ScaleType::count);
                choice(eight, "complexity", (int) ccp::Complexity::full);
                eight.toggleOptionLock(2);
                eight.generate();
                eight.setChordCount(8);
                auto ed8 = std::unique_ptr<juce::AudioProcessorEditor>(eight.createEditor());
                save(*ed8, "interface-8.png");
            }
            for (int width : { 840, 1680 }) {
                editor->setSize(width, juce::roundToInt(width * 875.0 / 1536.0));
                save(*editor, (juce::String("interface-") + juce::String(width) + ".png").toRawUTF8());
            }
            {
                SettingsPanel::Model model;
                model.isSynced = [] { return false; };
                model.manualBpm = [] { return 120; };
                model.hostBpm = [] { return 120; };
                model.setSynced = [](bool) {};
                model.setManualBpm = [](int) {};
                model.licenseEmail = [] { return juce::String("cliente@exemplo.com"); };
                model.updateAvailable = [] { return false; };
                model.windowScale = [] { return 1.0f; };
                SettingsPanel panel(model);
                save(panel, "settings.png");
            }
            if (argc > 2) {
                juce::MemoryBlock wavData;
                require(processor.renderWav(wavData), "WAV render failed");
                folder.getChildFile("progressao.wav").replaceWithData(wavData.getData(), wavData.getSize());
                // Preview with each reference beat, two passes of the loop.
                for (int beat = 1; beat < DrumMachine::patternCount; ++beat) {
                    ChordCodeProProcessor demo;
                    demo.prepareToPlay(44100, blockSize);
                    choice(demo, "beat", beat);
                    choice(demo, "bpmSync", 0);
                    choice(demo, "manualBpm", beat == 2 ? 130 : beat == 6 ? 124 : beat == 9 ? 130 : beat == 1 || beat == 3 ? 140 : 90);
                    choice(demo, "length", 2);
                    demo.playSequence();
                    const auto opts = demo.midiOptions();
                    const int total = (int) (44100.0 * 60.0 / opts.bpm * 16.0 * 2.0);
                    juce::AudioBuffer<float> out(2, total);
                    juce::MidiBuffer none;
                    for (int pos = 0; pos + blockSize <= total; pos += blockSize) {
                        demo.processBlock(audio, none);
                        for (int c = 0; c < 2; ++c) out.copyFrom(c, pos, audio, c, 0, blockSize);
                    }
                    auto file = folder.getChildFile(juce::String("batida-") + juce::String(DrumMachine::patternName(beat)).replace(" ", "-").toLowerCase() + ".wav");
                    file.deleteFile();
                    juce::WavAudioFormat format;
                    std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(file.createOutputStream().release(), 44100, 2, 16, {}, 0));
                    if (writer) writer->writeFromAudioSampleBuffer(out, 0, total);
                }
            }
        }
        std::cout << "Audio transitions, release highlights, 4/8 chord MIDI and state tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
    return 0;
}
