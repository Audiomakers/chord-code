#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "LookAndFeel.h"
#include "PluginProcessor.h"
#include "UpdateChecker.h"
#include "License.h"
#include "ModalPanels.h"

class MidiDragButton final : public juce::TextButton {
public:
    std::function<void()> dragAction;
    // Plain click (no drag): fallback for hosts/setups where an external drag can't start.
    std::function<void()> clickAction;
    void mouseDown(const juce::MouseEvent& e) override { started = false; juce::TextButton::mouseDown(e); }
    void mouseDrag(const juce::MouseEvent& e) override {
        if (!started && e.getDistanceFromDragStart() > 5 && dragAction) { started = true; dragAction(); }
        else if (!started) juce::TextButton::mouseDrag(e);
    }
    void mouseUp(const juce::MouseEvent& e) override { juce::TextButton::mouseUp(e); started = false; }
protected:
    void clicked() override { if (!started && clickAction) clickAction(); }
private:
    bool started {};
};

class SegmentedControl final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer {
public:
    void setItems(juce::StringArray values) { items = std::move(values); repaint(); }
    void setSelected(int value);
    std::function<void(int)> onSelect;
    void paint(juce::Graphics&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
private:
    void timerCallback() override;
    juce::Rectangle<float> itemBounds(float index) const;
    int indexAt(juce::Point<float>) const;
    juce::StringArray items;
    int selected {}, hovered { -1 };
    float indicator { -1.0f };
};

class ChordCardComponent final : public juce::Component, public juce::TooltipClient {
public:
    static constexpr float glowMargin = 10.0f;
    ChordCardComponent();
    void setSlot(int value) { slot = value; repaint(); }
    void setChord(const ccp::Chord& value) { chord = value; repaint(); }
    void setKeyRoot(int value) { if (keyRoot != value) { keyRoot = value; repaint(); } }
    void setLocked(bool value) { locked = value; repaint(); }
    void setSelected(bool value) { selected = value; repaint(); }
    void setPlaying(bool value) { if (playing != value) { playing = value; repaint(); } }
    void setAvailable(bool value) { if (available != value) { available = value; repaint(); } }
    // Waiting for a chord from the MIDI keyboard.
    void setArmed(bool value) { if (armed != value) { armed = value; repaint(); } }
    // Mini piano with the chord's notes in place of the equalizer.
    void setShowPiano(bool value) { if (showPiano != value) { showPiano = value; repaint(); } }
    void setPlaybackPhase(float value) { playbackPhase = value; if (playing) repaint(); }
    // Eases hover and playback highlights; called from the editor's animation timer.
    void advanceAnimation();
    std::function<void()> onSelect, onRegenerate, onLock, onPlay, onRecord;
    std::function<void(int)> onShowChordChoices;
    void paint(juce::Graphics&) override;
    bool hitTest(int x, int y) override { return body().contains((float) x, (float) y); }
    void mouseDown(const juce::MouseEvent&) override;
    void mouseEnter(const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit(const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseUp(const juce::MouseEvent&) override;
    juce::String getTooltip() override;   // depends on the part under the mouse
private:
    juce::Rectangle<float> body() const;
    juce::Rectangle<float> playArea() const;
    juce::Rectangle<float> variationArea() const;
    juce::Rectangle<float> lockArea() const;
    juce::Rectangle<float> recordArea() const;
    int slot {}, keyRoot {};
    bool locked {}, selected {}, playing {}, hover {}, available { true }, armed {}, showPiano {};
    void paintPiano(juce::Graphics&, juce::Rectangle<float>) const;
    float playbackPhase {}, hoverLevel {}, activeLevel {};
    ccp::Chord chord;
};

class ContinueCard final : public juce::Button {
public:
    ContinueCard() : juce::Button("Continuar") {}
    void setBackMode(bool value) { backMode = value; repaint(); }
    void paintButton(juce::Graphics&, bool, bool) override;
private:
    bool backMode {};
};

class StatusPill final : public juce::Component {
public:
    void setText(const juce::String& value) { if (text != value) { text = value; repaint(); } }
    void setLive(bool value, float phase);
    void paint(juce::Graphics&) override;
private:
    juce::String text;
    bool live {};
    float pulse {};
};

// Header pill with a dropdown (MIDI keyboard mode, reference beat). Orange when something is on.
class HeaderPill final : public juce::Component, public juce::SettableTooltipClient {
public:
    enum class Icon { keyboard, drum, piano };
    explicit HeaderPill(Icon i) : icon(i) {}
    void setState(const juce::String& value, bool isOn) { if (text != value || on != isOn) { text = value; on = isOn; repaint(); } }
    std::function<void()> onClick;
    void paint(juce::Graphics&) override;
    void mouseEnter(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override { repaint(); }
    void mouseUp(const juce::MouseEvent& e) override { if (onClick && getLocalBounds().contains(e.getPosition())) onClick(); }
private:
    Icon icon;
    juce::String text;
    bool on {};
};

class ChordCodeProEditor final : public juce::AudioProcessorEditor, private juce::Timer, private juce::ChangeListener {
public:
    explicit ChordCodeProEditor(ChordCodeProProcessor&);
    ~ChordCodeProEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refresh();
    void selectCard(int);
    enum class ExportKind { midi, wav };
    bool exportBytes(ExportKind, std::vector<uint8_t>&);
    void exportDrag(ExportKind);
    void revealForDrag(ExportKind);
    void showSaveMenu();
    void saveFile(ExportKind);
    void showMidiProblem(const juce::String& title, const juce::String& message);
    juce::File writeDragFile(ExportKind, juce::String& error);
    float uiScale() const;
    void applyWindowScale(float scaleOfDefault);
    float panelScaleFor(const juce::Component& panel) const;
    void showHistory();
    void showChordChoices(int);
    void showSettings();
    void checkForUpdates();
    void showUpdateResult(UpdateChecker::Result);
    void presentPanel(std::unique_ptr<juce::Component>, bool dismissable);
    void checkLicense();
    void showLicense(LicensePanel::Mode);
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void updateFromSettings();
    void animateToPage(int);
    void layoutChordPages();
    void updateLengthLabels(int);
    void updateStatus();

    ChordCodeProProcessor& pro;
    ProLookAndFeel laf;
    juce::Component content;
    juce::Component cardsViewport;
    juce::ComboBox key, scale, style, mood, complexity, tension, timbre, voicingParameter, rhythmParameter, length, groove;
    HeaderPill keyModePill { HeaderPill::Icon::keyboard }, beatPill { HeaderPill::Icon::drum }, pianoPill { HeaderPill::Icon::piano };
    bool showPiano { true };
    void setShowPiano(bool);
    void showKeyModeMenu();
    void showBeatMenu();
    void updatePills();
    SegmentedControl voicing, rhythm;
    juce::Slider velocity;
    std::array<ChordCardComponent, 8> cards;
    ContinueCard continueCard;
    StatusPill status;
    juce::TextButton timbrePrev, timbreNext;   // step through the timbres, inside the TIMBRE field
    juce::TextButton settings, play { "OUVIR" }, variation { juce::String::fromUTF8("VARIAÇÃO") },
        random { juce::String::fromUTF8("ALEATÓRIO") }, generate { "GERAR" },
        history { juce::String::fromUTF8("HISTÓRICO") }, save { "SALVAR" };
    MidiDragButton drag, dragWav;
    juce::ComponentBoundsConstrainer constrainer;
    juce::TooltipWindow tooltips { this, 650 };
    void setTooltips();
    void updateTimbreTooltip();
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>, 10> comboAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> velocityAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> grooveAttachment;
    std::unique_ptr<juce::FileChooser> chooser;
    UpdateChecker updateChecker, backgroundUpdateCheck;
    bool updateAvailable {};
    void setUpdateAvailable(bool);
    juce::SharedResourcePointer<LicenseManager> license;
    ModalOverlay overlay;
    std::shared_ptr<bool> showAllChords;
    int licenseTicks {};
    int pageOffset {}, selectedSlot { -1 }, lastPlaying { -2 };
    float slidePosition {}, slideTarget {}, animationPhase {};
    bool ignore {};
    int pendingChordCount {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChordCodeProEditor)
};
