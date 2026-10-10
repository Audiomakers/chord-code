#include "PluginEditor.h"
#include "ChordCodeTheme.h"
#include "Core/MidiFile.h"
#include "FigmaIcons.h"
#include <cmath>

using namespace ChordCodeTheme;

namespace {
constexpr float editorScale = 960.0f / (float) designWidth;
constexpr int cardPageWidth = 1324;
juce::String utf(const char* text) { return juce::String::fromUTF8(text); }

std::unique_ptr<juce::Drawable> drawableFromSvg(const char* source) {
    if (auto xml = juce::parseXML(source)) return juce::Drawable::createFromSVG(*xml);
    return {};
}

void drawSvg(juce::Graphics& g, juce::Drawable* drawable, juce::Rectangle<float> bounds) {
    if (drawable != nullptr)
        drawable->drawWithin(g, bounds, juce::RectanglePlacement::centred, 1.0f);
}

template <typename Enum, typename Fn>
void addNames(juce::ComboBox& box, Fn fn) {
    for (int i = 0; i < (int) Enum::count; ++i)
        box.addItem(utf(fn((Enum) i)), i + 1);
}

void label(juce::Graphics& g, const char* text, int x, int y, int width) {
    g.setColour(juce::Colour(textSecondary));
    g.setFont(font(16.0f, true));
    g.drawText(utf(text), x, y, width, 20, juce::Justification::centredLeft);
}

juce::String chordFileStem(const ccp::Progression& progression) {
    juce::StringArray names;
    for (int i = 0; i < progression.chordCount; ++i) {
        auto name = utf(progression.chords[(size_t) i].name.c_str());
        for (auto invalid : juce::String("/\\:*?\"<>| ")) name = name.replaceCharacter(invalid, '_');
        names.add(name);
    }
    return names.joinIntoString("-").substring(0, 120);
}

class ChordChoiceMenuItem final : public juce::PopupMenu::CustomComponent {
public:
    ChordChoiceMenuItem(juce::String text, std::function<void()> onHover)
        : juce::PopupMenu::CustomComponent(true), title(std::move(text)), hoverAction(std::move(onHover)) {}

    void getIdealSize(int& width, int& height) override { width = 230; height = 32; }

    void paint(juce::Graphics& g) override {
        if (highlighted) {
            g.setColour(juce::Colour(orange));
            g.fillRect(getLocalBounds());
        }
        g.setColour(highlighted ? juce::Colour(background) : juce::Colour(textPrimary));
        g.setFont(font(15.0f, true));
        g.drawText(title, getLocalBounds().reduced(12, 0), juce::Justification::centredLeft);
    }

    void mouseEnter(const juce::MouseEvent&) override {
        highlighted = true;
        repaint();
        if (hoverAction) hoverAction();
    }
    void mouseExit(const juce::MouseEvent&) override { highlighted = false; repaint(); }

private:
    juce::String title;
    std::function<void()> hoverAction;
    bool highlighted {};
};
}

int SegmentedControl::indexAt(juce::Point<float> p) const {
    if (items.isEmpty() || !getLocalBounds().toFloat().contains(p)) return -1;
    const float itemWidth = ((float) getWidth() - 24.0f) / (float) items.size();
    return juce::jlimit(0, items.size() - 1, (int) (p.x / (itemWidth + 8.0f)));
}

void SegmentedControl::paint(juce::Graphics& g) {
    if (items.isEmpty()) return;
    const float itemWidth = ((float) getWidth() - 24.0f) / (float) items.size();
    for (int i = 0; i < items.size(); ++i) {
        auto r = juce::Rectangle<float>((float) i * (itemWidth + 8.0f), 0.0f, itemWidth, (float) getHeight());
        const bool active = i == selected;
        g.setColour(active ? juce::Colour(orange) : juce::Colour(surface));
        g.fillRoundedRectangle(r, 7.0f);
        g.setColour(active ? juce::Colour(orange)
                           : (i == hovered ? juce::Colour(orange) : juce::Colour(border)));
        g.drawRoundedRectangle(r.reduced(0.5f), 7.0f, 1.0f);
        g.setColour(active ? juce::Colour(background) : juce::Colour(textPrimary));
        g.setFont(font(15.0f, true));
        g.drawFittedText(items[i], r.toNearestInt().reduced(8, 0), juce::Justification::centred, 1);
    }
}

void SegmentedControl::mouseMove(const juce::MouseEvent& e) { const int h = indexAt(e.position); if (h != hovered) { hovered = h; repaint(); } }
void SegmentedControl::mouseExit(const juce::MouseEvent&) { hovered = -1; repaint(); }
void SegmentedControl::mouseUp(const juce::MouseEvent& e) {
    const int i = indexAt(e.position);
    if (i >= 0 && i != selected) { selected = i; repaint(); if (onSelect) onSelect(i); }
}

ChordCardComponent::ChordCardComponent()
    : variationIcon(drawableFromSvg(FigmaIcons::variation_card)),
      playIcon(drawableFromSvg(FigmaIcons::play_card)) {}

juce::Rectangle<float> ChordCardComponent::playArea() const { return { 276.0f, 16.0f, 24.0f, 24.0f }; }
juce::Rectangle<float> ChordCardComponent::variationArea() const { return { 211.0f, 18.0f, 22.0f, 22.0f }; }
juce::Rectangle<float> ChordCardComponent::lockArea() const { return { 242.0f, 16.0f, 24.0f, 24.0f }; }

void ChordCardComponent::paint(juce::Graphics& g) {
    auto r = getLocalBounds().toFloat().reduced(0.8f);
    const bool active = available && playing;
    auto fill = !available ? juce::Colour(0xff030405) : (active ? juce::Colour(surfaceSelected) : juce::Colour(surface));
    if (available && hover && !active) fill = juce::Colour(0xff07090a);
    g.setColour(fill);
    g.fillRoundedRectangle(r, radiusCard);
    if (active) {
        // Soft inner halo stays inside the card's viewport clipping bounds.
        for (int spread = 7; spread >= 1; --spread) {
            g.setColour(juce::Colour(orange).withAlpha(0.018f * (float) (8 - spread)));
            g.drawRoundedRectangle(r.reduced((float) spread * 0.5f), radiusCard, (float) spread);
        }
    }
    g.setColour(available && (active || hover) ? juce::Colour(orange) : juce::Colour(border).withAlpha(available ? 1.0f : 0.42f));
    g.drawRoundedRectangle(r, radiusCard, active ? 1.6f : 1.0f);

    g.setColour(active ? juce::Colour(orange) : juce::Colour(textQuiet).withAlpha(available ? 1.0f : 0.42f));
    g.setFont(font(18.0f, true));
    g.drawText(juce::String(slot + 1).paddedLeft('0', 2), 17, 16, 45, 24, juce::Justification::centredLeft);

    if (available) {
        drawSvg(g, variationIcon.get(), variationArea());
        if (locked) {
            auto lockBackground = lockArea().expanded(4.0f);
            g.setColour(juce::Colour(orange).withAlpha(0.14f));
            g.fillRoundedRectangle(lockBackground, 6.0f);
        }
        drawLockIcon(g, lockArea(), locked ? juce::Colour(orange) : juce::Colour(textSecondary), locked);
    }
    if (active) drawSvg(g, playIcon.get(), playArea());

    g.setColour(juce::Colour(textPrimary).withAlpha(available ? 1.0f : 0.25f));
    g.setFont(font(52.0f, true));
    g.drawFittedText(available ? utf(chord.name.c_str()) : juce::String::fromUTF8("—"), 16, 82, getWidth() - 32, 68, juce::Justification::centred, 1, 0.82f);

    static constexpr float heights[] { 16.0f, 28.0f, 42.0f, 50.0f, 36.0f, 20.0f };
    const float startX = 107.0f;
    g.setColour(active ? juce::Colour(orange) : juce::Colour(waveform).withAlpha(available ? 1.0f : 0.20f));
    for (int i = 0; i < 6; ++i) {
        float height = heights[i];
        if (playing) {
            const float pulse = 0.62f + 0.38f * std::sin(playbackPhase + (float) i * 0.92f);
            height = juce::jlimit(12.0f, 54.0f, heights[i] * (0.72f + pulse * 0.55f));
        }
        const float top = 202.0f - height * 0.5f;
        g.fillRoundedRectangle(startX + (float) i * 16.0f, top, 7.0f, height, 3.5f);
    }
}

void ChordCardComponent::mouseUp(const juce::MouseEvent& e) {
    if (e.mods.isPopupMenu()) return;
    if (!available) return;
    if (variationArea().contains(e.position)) { if (onRegenerate) onRegenerate(); return; }
    if (lockArea().contains(e.position)) { if (onLock) onLock(); return; }
    if ((selected || playing) && playArea().contains(e.position)) { if (onPlay) onPlay(); return; }
    if (onSelect) onSelect();
    if (onPlay) onPlay();
}

void ChordCardComponent::mouseDown(const juce::MouseEvent& e) {
    if (available && e.mods.isPopupMenu() && onShowChordChoices)
        onShowChordChoices(slot);
}

void ContinueCard::paintButton(juce::Graphics& g, bool over, bool down) {
    auto r = getLocalBounds().toFloat().reduced(0.8f);
    g.setColour(juce::Colour(over || down ? 0xff060809 : 0xff030405));
    g.fillRoundedRectangle(r, radiusCard);
    juce::Path outline, dashed;
    outline.addRoundedRectangle(r, radiusCard);
    const float pattern[] { 6.0f, 5.0f };
    juce::PathStrokeType(1.0f).createDashedStroke(dashed, outline, pattern, 2);
    g.setColour(over ? juce::Colour(orange) : juce::Colour(border).withAlpha(0.76f));
    g.fillPath(dashed);
    g.setColour(juce::Colour(textPrimary));
    g.setFont(font(46.0f));
    g.drawText(backMode ? juce::String::fromUTF8("−") : "+", 0, 58, getWidth(), 62, juce::Justification::centred);
    g.setFont(font(16.0f, true));
    g.drawFittedText(backMode ? "VOLTAR A 4" : "CONTINUAR", 8, 130, getWidth() - 16, 28,
                     juce::Justification::centred, 1);
    g.setColour(juce::Colour(textSecondary));
    g.setFont(font(12.0f, true));
    g.drawFittedText(backMode ? "REMOVER 4" : "+4 ACORDES", 8, 157, getWidth() - 16, 22,
                     juce::Justification::centred, 1);
}

ChordCodeProEditor::ChordCodeProEditor(ChordCodeProProcessor& p) : AudioProcessorEditor(&p), pro(p) {
    setLookAndFeel(&laf);
    setOpaque(true);
    addAndMakeVisible(content);
    content.setInterceptsMouseClicks(false, true);
    setSize(juce::roundToInt((float) designWidth * editorScale), juce::roundToInt((float) designHeight * editorScale));
    setResizable(false, false);

    const char* notes[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    for (int i = 0; i < 12; ++i) key.addItem(notes[i], i + 1);
    addNames<ccp::ScaleType>(scale, ccp::scaleName);
    addNames<ccp::Style>(style, ccp::styleName);
    addNames<ccp::Mood>(mood, ccp::moodName);
    addNames<ccp::Complexity>(complexity, ccp::complexityName);
    addNames<ccp::Tension>(tension, ccp::tensionName);
    addNames<ccp::Timbre>(timbre, ccp::timbreName);
    addNames<ccp::Voicing>(voicingParameter, ccp::voicingName);
    rhythmParameter.addItemList({ "RETO", "HUMANIZE", "STRUM", "ARPEJO" }, 1);
    length.addItemList({ "1/4", "2/4", "4/4" }, 1);

    std::array<juce::ComboBox*, 10> boxes { &key, &scale, &style, &mood, &complexity, &tension, &timbre,
                                            &voicingParameter, &rhythmParameter, &length };
    const char* ids[] { "key", "scale", "style", "mood", "complexity", "tension", "timbre", "voicing", "rhythm", "length" };
    for (size_t i = 0; i < boxes.size(); ++i) {
        boxes[i]->setJustificationType(juce::Justification::centredLeft);
        comboAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.apvts, ids[i], *boxes[i]);
        if (i < 7 || i == 9) content.addAndMakeVisible(*boxes[i]);
    }

    voicing.setItems({ utf("PADRÃO"), "FECHADO", "ABERTO", "INVERTER" });
    rhythm.setItems({ "RETO", "HUMANIZE", "STRUM", "ARPEJO" });
    content.addAndMakeVisible(voicing);
    content.addAndMakeVisible(rhythm);
    voicing.onSelect = [this](int i) { voicingParameter.setSelectedItemIndex(i, juce::sendNotificationSync); };
    rhythm.onSelect = [this](int i) { rhythmParameter.setSelectedItemIndex(i, juce::sendNotificationSync); };

    velocity.setSliderStyle(juce::Slider::LinearHorizontal);
    velocity.setTextBoxStyle(juce::Slider::TextBoxRight, false, 88, 48);
    velocity.setRange(0, 100, 1);
    velocity.setNumDecimalPlacesToDisplay(0);
    velocity.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(surface));
    velocity.setColour(juce::Slider::textBoxTextColourId, juce::Colour(textPrimary));
    velocity.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(border));
    content.addAndMakeVisible(velocity);
    velocityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "velocity", velocity);

    key.onChange = [this] { if (!ignore) { pro.setKey(key.getSelectedItemIndex()); refresh(); } };
    for (auto* box : { &scale, &style, &mood, &complexity, &tension, &voicingParameter })
        box->onChange = [this] { if (!ignore) { pro.rebuildForSettings(); refresh(); } };
    timbre.onChange = [this] { pro.stopPreview(); };

    content.addAndMakeVisible(cardsViewport);
    cardsViewport.setInterceptsMouseClicks(false, true);
    for (int i = 0; i < 8; ++i) {
        cardsViewport.addAndMakeVisible(cards[(size_t) i]);
        cards[(size_t) i].setSlot(i);
        cards[(size_t) i].onSelect = [this, i] { selectCard(i); };
        cards[(size_t) i].onPlay = [this, i] { pro.audition(i); };
        cards[(size_t) i].onRegenerate = [this, i] { pro.regenerateChord(i); refresh(); };
        cards[(size_t) i].onLock = [this, i] { pro.toggleLock(i); refresh(); };
        cards[(size_t) i].onShowChordChoices = [this](int slot) { showChordChoices(slot); };
    }
    content.addAndMakeVisible(continueCard);

    for (auto* button : { (juce::Button*) &settings, (juce::Button*) &play, (juce::Button*) &variation,
                          (juce::Button*) &random, (juce::Button*) &generate, (juce::Button*) &history,
                          (juce::Button*) &drag, (juce::Button*) &save })
        content.addAndMakeVisible(*button);
    settings.setComponentID("settings"); settings.setTitle("Configurações");
    play.setComponentID("play"); variation.setComponentID("variation"); random.setComponentID("random");
    generate.setComponentID("generate"); history.setComponentID("history"); drag.setComponentID("drag"); save.setComponentID("save");
    drag.setButtonText("ARRASTAR MIDI");

    settings.onClick = [this] { showSettings(); };
    play.onClick = [this] { if (pro.activeChord() >= 0) pro.stopPreview(); else pro.playSequence(); };
    variation.onClick = [this] { pro.variation(); refresh(); };
    random.onClick = [this] {
        ignore = true;
        pro.randomize();
        ignore = false;
        updateFromSettings();
        refresh();
    };
    generate.onClick = [this] { pro.generate(); refresh(); };
    history.onClick = [this] { showHistory(); };
    continueCard.onClick = [this] {
        if (std::abs(slidePosition - slideTarget) > 0.01f) return;
        if (pageOffset == 4) {
            pendingChordCount = 4;
            animateToPage(0);
        } else {
            pro.continueToEight();
            refresh();
            animateToPage(4);
        }
    };
    drag.dragAction = [this] { exportDrag(); };
    save.onClick = [this] { saveMidi(); };

    refresh();
    startTimerHz(30);
}

ChordCodeProEditor::~ChordCodeProEditor() {
    stopTimer();
    updateChecker.cancelAndWait();
    chooser.reset();
    for (auto& card : cards) { card.onSelect = {}; card.onPlay = {}; card.onRegenerate = {}; card.onLock = {}; card.onShowChordChoices = {}; }
    pro.stopPreview();
    setLookAndFeel(nullptr);
}

void ChordCodeProEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(background));
    g.addTransform(juce::AffineTransform::scale(editorScale));
    auto window = juce::Rectangle<float>(0.0f, 0.0f, (float) designWidth, (float) designHeight).reduced(0.75f);
    g.setColour(juce::Colour(border));
    g.drawRoundedRectangle(window, radiusWindow, 1.5f);

    const auto logoFont = font(42.0f, true);
    g.setFont(logoFont);
    g.setColour(juce::Colour(textPrimary));
    g.drawText("CHORD", 32, 20, 160, 50, juce::Justification::centredLeft);
    g.setColour(juce::Colour(orange));
    const int codeX = 32 + juce::GlyphArrangement::getStringWidthInt(logoFont, "CHORD") + 8;
    g.drawText("CODE", codeX, 20, 170, 50, juce::Justification::centredLeft);
    g.setColour(juce::Colour(textSecondary));
    g.setFont(font(16.0f));
    g.drawText("v" JucePlugin_VersionString, 1372, 30, 76, 36, juce::Justification::centredRight);

    g.setColour(juce::Colour(divider));
    g.drawLine(30.5f, 90.5f, 1500.5f, 90.5f, 1.0f);

    label(g, "TOM", 34, 116, 150); label(g, "ESCALA", 396, 116, 150);
    label(g, "ESTILO", 768, 116, 150); label(g, "VIBE", 1130, 116, 150);
    label(g, "COMPLEXIDADE", 34, 218, 180); label(g, "TENSÃO", 504, 218, 150); label(g, "TIMBRE", 964, 218, 150);

    const auto drawGroup = [&g](juce::Rectangle<float> bounds) {
        g.setColour(juce::Colour(surfaceGroup)); g.fillRoundedRectangle(bounds, 10.0f);
        g.setColour(juce::Colour(borderQuiet)); g.drawRoundedRectangle(bounds, 10.0f, 1.0f);
    };
    drawGroup({ 34.5f, 608.5f, 460.0f, 118.0f });
    drawGroup({ 504.5f, 608.5f, 450.0f, 118.0f });
    drawGroup({ 964.5f, 608.5f, 536.0f, 118.0f });
    label(g, "FORMA DO ACORDE", 55, 625, 220); label(g, "RITMO", 525, 625, 180);
    label(g, "DURAÇÃO", 981, 625, 180); label(g, "VOLUME", 1193, 625, 160);

}

void ChordCodeProEditor::resized() {
    content.setBounds(0, 0, designWidth, designHeight);
    content.setTransform(juce::AffineTransform::scale(editorScale));
    settings.setBounds(1465, 29, 38, 38);
    key.setBounds(34, 140, 320, 50); scale.setBounds(396, 140, 330, 50);
    style.setBounds(768, 140, 330, 50); mood.setBounds(1130, 140, 370, 50);
    complexity.setBounds(34, 242, 460, 50); tension.setBounds(504, 242, 450, 50); timbre.setBounds(964, 242, 536, 50);

    cardsViewport.setBounds(34, 332, 1288, 252);
    layoutChordPages();
    continueCard.setBounds(1358, 332, 142, 252);

    voicing.setBounds(53, 657, 416, 46);
    rhythm.setBounds(523, 657, 404, 46);
    length.setBounds(981, 655, 188, 48);
    velocity.setBounds(1193, 647, 274, 64);

    play.setBounds(34, 758, 168, 84); variation.setBounds(212, 758, 174, 84);
    random.setBounds(396, 758, 176, 84); generate.setBounds(587, 757, 340, 84);
    history.setBounds(942, 758, 168, 84); drag.setBounds(1120, 758, 190, 84); save.setBounds(1322, 758, 178, 84);
}

void ChordCodeProEditor::timerCallback() {
    const int now = pro.activeChord();
    animationPhase += 0.28f;
    if (animationPhase > juce::MathConstants<float>::twoPi)
        animationPhase -= juce::MathConstants<float>::twoPi;
    for (int i = 0; i < 8; ++i) {
        cards[(size_t) i].setPlaying(i == now);
        cards[(size_t) i].setPlaybackPhase(animationPhase);
    }
    if (now != lastPlaying) {
        lastPlaying = now;
        play.setButtonText(now >= 0 ? "PARAR" : "REPRODUZIR");
    }

    if (std::abs(slidePosition - slideTarget) > 0.001f) {
        slidePosition += (slideTarget - slidePosition) * 0.24f;
        if (std::abs(slidePosition - slideTarget) < 0.008f)
            slidePosition = slideTarget;
        layoutChordPages();
        if (slidePosition == 0.0f && pendingChordCount != 0) {
            const int count = pendingChordCount;
            pendingChordCount = 0;
            pro.setChordCount(count);
            refresh();
        }
    }
}

void ChordCodeProEditor::animateToPage(int offset) {
    pageOffset = offset == 4 ? 4 : 0;
    slideTarget = pageOffset == 4 ? 1.0f : 0.0f;
    continueCard.setBackMode(pageOffset == 4);
    if (pro.getProgression().chordCount > 4)
        drag.setButtonText("ARRASTAR MIDI");
}

void ChordCodeProEditor::layoutChordPages() {
    static constexpr int xPositions[] { 0, 326, 652, 978 };
    const int travel = juce::roundToInt(slidePosition * (float) cardPageWidth);
    for (int i = 0; i < 8; ++i) {
        const int page = i / 4;
        const int column = i % 4;
        cards[(size_t) i].setBounds(xPositions[column] + page * cardPageWidth - travel, 0, 310, 252);
    }
}

void ChordCodeProEditor::updateLengthLabels(int chordCount) {
    const int selected = length.getSelectedItemIndex();
    const bool oldIgnore = ignore;
    ignore = true;
    length.clear(juce::dontSendNotification);
    length.addItemList(chordCount > 4 ? juce::StringArray { "2/8", "4/8", "8/8" }
                                      : juce::StringArray { "1/4", "2/4", "4/4" }, 1);
    length.setSelectedItemIndex(juce::jlimit(0, 2, selected), juce::dontSendNotification);
    ignore = oldIgnore;
}

void ChordCodeProEditor::selectCard(int slot) {
    selectedSlot = selectedSlot == slot ? -1 : slot;
    for (int i = 0; i < 8; ++i) cards[(size_t) i].setSelected(i == selectedSlot);
}

void ChordCodeProEditor::refresh() {
    const auto progression = pro.getProgression();
    if (pageOffset == 4 && progression.chordCount <= 4) pageOffset = 0;
    for (int slot = 0; slot < 8; ++slot) {
        cards[(size_t) slot].setSlot(slot);
        cards[(size_t) slot].setVisible(slot < 4 || progression.chordCount > 4);
        cards[(size_t) slot].setAvailable(slot < progression.chordCount);
        if (slot < progression.chordCount) cards[(size_t) slot].setChord(progression.chords[(size_t) slot]);
        cards[(size_t) slot].setLocked(pro.isLocked(slot));
        cards[(size_t) slot].setSelected(slot == selectedSlot);
        cards[(size_t) slot].setPlaying(slot == pro.activeChord());
    }
    continueCard.setBackMode(pageOffset == 4);
    slideTarget = pageOffset == 4 ? 1.0f : 0.0f;
    continueCard.setVisible(true);
    updateLengthLabels(progression.chordCount);
    drag.setButtonText("ARRASTAR MIDI");
    voicing.setSelected(voicingParameter.getSelectedItemIndex());
    rhythm.setSelected(rhythmParameter.getSelectedItemIndex());
    repaint();
}

void ChordCodeProEditor::updateFromSettings() {
    ignore = true;
    for (auto* combo : { &key, &scale, &style, &mood, &complexity, &tension, &timbre, &voicingParameter, &rhythmParameter, &length })
        combo->repaint();
    voicing.setSelected(voicingParameter.getSelectedItemIndex());
    rhythm.setSelected(rhythmParameter.getSelectedItemIndex());
    ignore = false;
}

void ChordCodeProEditor::exportDrag() {
    const auto progression = pro.getProgression();
    auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("Audio Makers").getChildFile("Chord Code");
    dir.createDirectory();
    for (const auto& old : dir.findChildFiles(juce::File::findFiles, false, "*.mid"))
        if (old.getLastModificationTime() < juce::Time::getCurrentTime() - juce::RelativeTime::days(3)) old.deleteFile();
    auto file = dir.getNonexistentChildFile(chordFileStem(progression), ".mid", false);
    if (ccp::MidiFile::write(progression, file.getFullPathName().toStdString(), pro.midiOptions()))
        juce::DragAndDropContainer::performExternalDragDropOfFiles({ file.getFullPathName() }, true, this);
}

void ChordCodeProEditor::saveMidi() {
    const auto progression = pro.getProgression();
    chooser = std::make_unique<juce::FileChooser>(utf("Salvar acordes MIDI"),
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile(chordFileStem(progression) + ".mid"), "*.mid");
    juce::Component::SafePointer<ChordCodeProEditor> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [safe](const juce::FileChooser& fc) {
            if (safe == nullptr) return;
            const auto file = fc.getResult();
            if (file == juce::File {}) return;
            const auto path = file.withFileExtension("mid").getFullPathName().toStdString();
            ccp::MidiFile::write(safe->pro.getProgression(), path, safe->pro.midiOptions());
        });
}

void ChordCodeProEditor::showHistory() {
    juce::PopupMenu menu;
    menu.setLookAndFeel(&laf);
    if (pro.historySize() == 0) menu.addItem(100, utf("Nenhuma geração anterior"), false);
    for (int i = 0; i < pro.historySize(); ++i)
        menu.addItem(i + 1, juce::String(i + 1) + ".  " + pro.historyLabel(i));
    juce::Component::SafePointer<ChordCodeProEditor> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(history), [safe](int result) {
        if (safe == nullptr || result <= 0 || result > 10) return;
        safe->ignore = true;
        const bool restored = safe->pro.restoreHistory(result - 1);
        safe->ignore = false;
        if (restored) { safe->updateFromSettings(); safe->refresh(); }
    });
}

void ChordCodeProEditor::showChordChoices(int slot) {
    const auto choices = pro.chordChoices(slot);
    if (choices.empty()) return;

    juce::PopupMenu menu;
    menu.setLookAndFeel(&laf);
    juce::Component::SafePointer<ChordCodeProEditor> safe(this);
    for (size_t i = 0; i < choices.size(); ++i) {
        const auto choice = choices[i];
        auto item = std::make_unique<ChordChoiceMenuItem>(
            utf(choice.name.c_str()), [safe, slot, choice] {
                if (safe != nullptr) safe->pro.auditionChord(slot, choice);
            });
        menu.addCustomItem((int) i + 1, std::move(item), nullptr, utf(choice.name.c_str()));
    }

    menu.showMenuAsync(juce::PopupMenu::Options().withParentComponent(this)
                                                   .withTargetComponent(this)
                                                   .withTargetScreenArea(cards[(size_t) slot].getScreenBounds())
                                                   .withMaximumNumColumns(2),
        [safe, slot, choices](int result) {
            if (safe == nullptr) return;
            safe->pro.stopPreview();
            if (result > 0 && result <= (int) choices.size()) {
                safe->pro.chooseChord(slot, choices[(size_t) (result - 1)]);
                safe->refresh();
            }
        });
}

void ChordCodeProEditor::showSettings() {
    const bool synced = *pro.apvts.getRawParameterValue("bpmSync") > 0.5f;
    const int currentBpm = (int) *pro.apvts.getRawParameterValue("manualBpm");
    juce::PopupMenu bpmMenu;
    for (int bpm : { 80, 90, 100, 110, 120, 128, 140, 160 })
        bpmMenu.addItem(1000 + bpm, juce::String(bpm) + " BPM", true, currentBpm == bpm);
    juce::PopupMenu menu;
    menu.setLookAndFeel(&laf);
    menu.addItem(1, utf("Sincronizar BPM com o projeto"), true, synced);
    menu.addSubMenu("BPM manual", bpmMenu);
    menu.addSeparator();
    menu.addItem(2, updateChecker.isChecking() ? utf("Verificando atualizações...") : utf("Verificar atualizações"),
                 !updateChecker.isChecking());
    menu.addItem(3, "By Audio Makers", false);
    juce::Component::SafePointer<ChordCodeProEditor> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withParentComponent(this)
                                                   .withTargetComponent(this)
                                                   .withTargetScreenArea(settings.getScreenBounds())
                                                   .withStandardItemHeight(30), [safe, synced](int result) {
        if (safe == nullptr || result == 0) return;
        if (result == 1) {
            if (auto* param = safe->pro.apvts.getParameter("bpmSync"))
                param->setValueNotifyingHost(param->convertTo0to1(synced ? 0.0f : 1.0f));
        } else if (result >= 1040 && result <= 1240) {
            if (auto* sync = safe->pro.apvts.getParameter("bpmSync")) sync->setValueNotifyingHost(0.0f);
            if (auto* bpm = safe->pro.apvts.getParameter("manualBpm"))
                bpm->setValueNotifyingHost(bpm->convertTo0to1((float) (result - 1000)));
        } else if (result == 2) {
            safe->checkForUpdates();
        }
    });
}

void ChordCodeProEditor::checkForUpdates() {
    if (updateChecker.isChecking()) return;
    juce::Component::SafePointer<ChordCodeProEditor> safe(this);
    updateChecker.start(JucePlugin_VersionString, [safe](UpdateChecker::Result result) mutable {
        if (safe != nullptr) safe->showUpdateResult(std::move(result));
    });
}

void ChordCodeProEditor::showUpdateResult(UpdateChecker::Result result) {
    if (!result.succeeded) {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
            "CHORD CODE PRO", result.error, "OK", this);
        return;
    }

    if (!result.updateAvailable) {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
            "CHORD CODE PRO", utf("Você já está usando a versão mais recente.\n\nVersão instalada: ")
                + result.installedVersion, "OK", this);
        return;
    }

    auto message = utf("Uma nova atualização está disponível.\n\nVersão instalada: ") + result.installedVersion
        + utf("\nNova versão: ") + result.latestVersion;
    if (result.releaseNotes.isNotEmpty()) message += "\n\n" + result.releaseNotes;
    const auto options = juce::MessageBoxOptions()
        .withIconType(juce::MessageBoxIconType::InfoIcon)
        .withTitle(utf("Nova atualização disponível"))
        .withMessage(message)
        .withButton(utf("IR PARA DOWNLOAD"))
        .withButton(utf("AGORA NÃO"))
        .withAssociatedComponent(this);
    juce::Component::SafePointer<ChordCodeProEditor> safe(this);
    juce::AlertWindow::showAsync(options, [safe, url = result.downloadUrl](int button) {
        if (safe != nullptr && button == 1) juce::URL(url).launchInDefaultBrowser();
    });
}
