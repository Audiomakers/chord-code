#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Core/MidiFile.h"
#include "Core/Music.h"
#include "PreviewSynth.h"
#include "DrumMachine.h"

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
    // Cadeados das opções: TOM, ESCALA, ESTILO, VIBE, COMPLEXIDADE, TENSÃO keep their value on ALEATÓRIO (and on GERAR for AUTOMÁTICO).
    static constexpr int optionCount = 6;
    void toggleOptionLock(int);
    bool isOptionLocked(int) const;
    bool isKeyAutomatic() const;
    bool isScaleAutomatic() const;
    int automaticKey() const { return resolvedKey.load(); }
    ccp::ScaleType automaticScale() const { return (ccp::ScaleType) resolvedScale.load(); }
    // Renders the progression as REPRODUZIR plays it (timbre, rhythm, duration, volume, tempo)
    // to a 24-bit stereo 44.1 kHz WAV that loops seamlessly: the loop is rendered twice and
    // the second pass is kept, so the last chord's release wraps onto the start.
    bool renderWav(juce::MemoryBlock& destination);
    void playSequence();
    void stopPreview();
    // REPRODUZIR loops until PARAR; anything that changes the chords starts it again from the first chord.
    bool isSequencePlaying() const { return sequenceRunning.load(); }
    void restartIfPlaying() { if (sequenceRunning.load()) playSequence(); }
    void audition(int);
    int activeChord() const { return active.load(); }
    double hostBpm() const;
    void toggleLock(int);
    bool isLocked(int) const;
    bool restoreHistory(int);

    // MIDI keyboard. In the chord modes each key plays a whole chord of the current key and
    // scale: "do tom" uses the note you press (snapped to the scale), "teclas brancas" maps
    // C D E F G A B to degrees I-VII so any key and scale can be played on white keys only.
    enum class KeyMode { notes, scaleChords, whiteKeys, count };
    KeyMode keyMode() const { return (KeyMode) (int) *apvts.getRawParameterValue("keyMode"); }
    // Arms a slot: the next chord played on the MIDI keyboard replaces that slot and locks it.
    void armCapture(int slot);
    int armedSlot() const { return captureSlot.load(); }
    // Called from the editor's timer; applies a finished capture. Returns true if it changed the progression.
    bool applyPendingCapture();
    // Exact played notes -> chord with a detected name (exposed for tests).
    static ccp::Chord chordFromNotes(const ccp::GenerationSettings&, std::vector<int> notes);
    int historySize() const;
    juce::String historyLabel(int) const;

private:
    struct HistoryEntry {
        ccp::Progression progression;
        int rhythm {};
        int length {};
        int velocity { 100 };
        int groove {};
    };
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    ccp::GenerationSettings settingsFromParams() const;
    std::array<bool, ccp::maxChords> lockSnapshot() const;
    void publish(const ccp::Progression&, bool addHistory = true);
    void persistProgression();
    bool restoreProgression();
    void scheduleChord(juce::MidiBuffer&, const ccp::Chord&, int chordIndex,
                       int64_t chordStart, int64_t chordDuration,
                       int64_t blockStart, int64_t blockEnd, ccp::MidiRhythm, double sampleRate);
    void setPreviewTimbreIfNeeded();
    void handleKeyboard(const juce::MidiBuffer& incoming, juce::MidiBuffer& out, int numSamples);
    void refreshKeyboardChords();
    int64_t chordStartSamples(const ccp::MidiOptions&, int index, double rate) const;
    int64_t chordLengthSamples(const ccp::MidiOptions&, int index, double rate) const;
    void setChoice(const char*, int);
    void rollAutomatic(bool force);
    std::atomic<int> resolvedKey { 9 }, resolvedScale { 1 };
    std::array<std::atomic<bool>, optionCount> optionLocks {};

    ccp::ProgressionGenerator generator;
    std::array<ccp::Progression, 2> slots {};
    std::atomic<int> activeSlot { 0 };
    std::vector<HistoryEntry> history;
    std::array<std::atomic<bool>, ccp::maxChords> locks {};
    PreviewSynth synth;
    DrumMachine drums;
    double sr { 44100.0 };
    float outputGain { 0.68f };
    std::atomic<int> command { 0 }, active { -1 };
    std::atomic<bool> sequenceRunning { false };
    std::atomic<int> previewDegree { 0 }, previewKind { 0 }, previewRoot { 0 }, previewQuality { -1 };
    ccp::Chord customPreviewChord;
    bool customAudition {};
    int64_t playheadSamples {};
    int64_t releaseSamples {};
    int playing { -1 };
    bool auditionMode {};
    int currentTimbre { -1 };

    // Keyboard chord mode (audio thread only).
    std::array<std::array<int, ccp::maxNotes>, 7> keyboardChords {};
    std::array<int, 7> keyboardChordSizes {};
    uint64_t keyboardSettingsHash { ~0ull };
    std::array<std::array<int, ccp::maxNotes>, 128> heldChord {};
    std::array<int, 128> heldChordSize {}, soundingCount {};
    int lastKeyMode { 0 };

    // Chord capture: the audio thread collects, the message thread applies.
    std::atomic<int> captureSlot { -1 };
    juce::SpinLock captureLock;
    std::array<int, 16> captureNotes {};
    int captureCount {}, captureDegree { -1 }, captureHeld {};
    bool captureReady {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChordCodeProProcessor)
};
