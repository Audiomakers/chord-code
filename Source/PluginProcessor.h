#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Core/MidiFile.h"
#include "Core/Music.h"
#include "PreviewSynth.h"

class ChordCodeProProcessor : public juce::AudioProcessor {
public:
    ChordCodeProProcessor();
    ~ChordCodeProProcessor() override;

    void prepareToPlay(double, int) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    ccp::Progression getProgression() const;
    ccp::MidiOptions midiOptions() const;
    void generate();
    void variation();
    void continueToEight();
    void reduceToFour();
    void setChordCount(int);
    void regenerateChord(int);
    std::vector<ccp::Chord> chordChoices(int position) const;
    std::vector<ccp::Chord> allChordChoices(int position) const;
    void chooseChord(int position, const ccp::Chord& chord);
    void auditionChord(int position, const ccp::Chord& chord);
    void setKey(int);
    void rebuildForSettings();
    void randomize();
    void playSequence();
    void stopPreview();
    void audition(int);
    int activeChord() const { return active.load(); }
    double hostBpm() const;
    void toggleLock(int);
    bool isLocked(int) const;
    bool restoreHistory(int);
    int historySize() const;
    juce::String historyLabel(int) const;

private:
    struct HistoryEntry {
        ccp::Progression progression;
        int rhythm {};
        int length {};
        int velocity { 100 };
    };
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    ccp::GenerationSettings settingsFromParams() const;
    std::array<bool, ccp::maxChords> lockSnapshot() const;
    void publish(const ccp::Progression&, bool addHistory = true);
    void persistProgression();
    bool restoreProgression();
    void scheduleChord(juce::MidiBuffer&, const ccp::Chord&, int chordIndex,
                       int64_t chordStart, int64_t chordDuration,
                       int64_t blockStart, int64_t blockEnd, ccp::MidiRhythm);
    void setPreviewTimbreIfNeeded();
    void setChoice(const char*, int);

    ccp::ProgressionGenerator generator;
    std::array<ccp::Progression, 2> slots {};
    std::atomic<int> activeSlot { 0 };
    std::vector<HistoryEntry> history;
    std::array<std::atomic<bool>, ccp::maxChords> locks {};
    PreviewSynth synth;
    double sr { 44100.0 };
    float outputGain { 0.68f };
    std::atomic<int> command { 0 }, active { -1 };
    std::atomic<int> previewDegree { 0 }, previewKind { 0 }, previewRoot { 0 }, previewQuality { -1 };
    ccp::Chord customPreviewChord;
    bool customAudition {};
    int64_t playheadSamples {};
    int64_t releaseSamples {};
    int playing { -1 };
    bool auditionMode {};
    int currentTimbre { -1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChordCodeProProcessor)
};
