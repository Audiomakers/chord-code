#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "LookAndFeel.h"
#include "PluginProcessor.h"
#include "UpdateChecker.h"

class MidiDragButton final : public juce::TextButton {
public:
    std::function<void()> dragAction;
    void mouseDown(const juce::MouseEvent& e) override { started = false; juce::TextButton::mouseDown(e); }
    void mouseDrag(const juce::MouseEvent& e) override {
        if (!started && e.getDistanceFromDragStart() > 5 && dragAction) { started = true; dragAction(); }
        else if (!started) juce::TextButton::mouseDrag(e);
    }
    void mouseUp(const juce::MouseEvent& e) override { juce::TextButton::mouseUp(e); started = false; }
private:
    bool started {};
};

class SegmentedControl final : public juce::Component {
public:
    void setItems(juce::StringArray values) { items = std::move(values); repaint(); }
    void setSelected(int value) { selected = value; repaint(); }
    std::function<void(int)> onSelect;
    void paint(juce::Graphics&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
private:
    int indexAt(juce::Point<float>) const;
    juce::StringArray items;
    int selected {}, hovered { -1 };
};

class ChordCardComponent final : public juce::Component {
public:
    ChordCardComponent();
    void setSlot(int value) { slot = value; repaint(); }
    void setChord(const ccp::Chord& value) { chord = value; repaint(); }
    void setLocked(bool value) { locked = value; repaint(); }
    void setSelected(bool value) { selected = value; repaint(); }
    void setPlaying(bool value) { if (playing != value) { playing = value; repaint(); } }
    void setAvailable(bool value) { if (available != value) { available = value; repaint(); } }
    void setPlaybackPhase(float value) { playbackPhase = value; if (playing) repaint(); }
    std::function<void()> onSelect, onRegenerate, onLock, onPlay;
    std::function<void(int)> onShowChordChoices;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseEnter(const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit(const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseUp(const juce::MouseEvent&) override;
private:
    juce::Rectangle<float> playArea() const;
    juce::Rectangle<float> variationArea() const;
    juce::Rectangle<float> lockArea() const;
    int slot {};
    bool locked {}, selected {}, playing {}, hover {}, available { true };
    float playbackPhase {};
    ccp::Chord chord;
    std::unique_ptr<juce::Drawable> variationIcon, playIcon;
};

class ContinueCard final : public juce::Button {
public:
    ContinueCard() : juce::Button("Continuar") {}
    void setBackMode(bool value) { backMode = value; repaint(); }
    void paintButton(juce::Graphics&, bool, bool) override;
private:
    bool backMode {};
};

class ChordCodeProEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit ChordCodeProEditor(ChordCodeProProcessor&);
    ~ChordCodeProEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refresh();
    void selectCard(int);
    void exportDrag();
    void saveMidi();
    void showHistory();
    void showChordChoices(int);
    void showSettings();
    void checkForUpdates();
    void showUpdateResult(UpdateChecker::Result);
    void updateFromSettings();
    void animateToPage(int);
    void layoutChordPages();
    void updateLengthLabels(int);

    ChordCodeProProcessor& pro;
    ProLookAndFeel laf;
    juce::Component content;
    juce::Component cardsViewport;
    juce::ComboBox key, scale, style, mood, complexity, tension, timbre, voicingParameter, rhythmParameter, length;
    SegmentedControl voicing, rhythm;
    juce::Slider velocity;
    std::array<ChordCardComponent, 8> cards;
    ContinueCard continueCard;
    juce::TextButton settings, play { "REPRODUZIR" }, variation { juce::String::fromUTF8("VARIAÇÃO") },
        random { juce::String::fromUTF8("ALEATÓRIO") }, generate { "GERAR" },
        history { juce::String::fromUTF8("HISTÓRICO") }, save { "SALVAR MIDI" };
    MidiDragButton drag;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>, 10> comboAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> velocityAttachment;
    std::unique_ptr<juce::FileChooser> chooser;
    UpdateChecker updateChecker;
    int pageOffset {}, selectedSlot { -1 }, lastPlaying { -2 };
    float slidePosition {}, slideTarget {}, animationPhase {};
    bool ignore {};
    int pendingChordCount {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChordCodeProEditor)
};
