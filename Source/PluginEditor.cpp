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
    g.setFont(labelFont(13.5f));
    g.drawText(utf(text), x, y, width, 20, juce::Justification::centredLeft);
}

juce::String romanNumeral(const ccp::Chord& chord) {
    static const char* numerals[] { "I", "II", "III", "IV", "V", "VI", "VII" };
    auto numeral = juce::String(numerals[(size_t) juce::jlimit(0, 6, chord.degree)]);
    auto suffix = utf(chord.name.c_str()).substring(1);
    if (suffix.startsWithChar('#') || suffix.startsWithChar('b')) suffix = suffix.substring(1);
    if (suffix.startsWith("dim") || suffix.startsWith(utf("°")))
        return numeral.toLowerCase() + utf("°");
    if (suffix.startsWithChar('m') && !suffix.startsWith("maj"))
        return numeral.toLowerCase();
    return numeral;
}

juce::String chordNotes(const ccp::Chord& chord) {
    static const char* names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    juce::StringArray notes;
    for (int i = 0; i < chord.noteCount; ++i) {
        const auto name = juce::String(names[(size_t) ((chord.notes[(size_t) i] % 12 + 12) % 12)]);
        if (!notes.contains(name)) notes.add(name);
    }
    return notes.joinIntoString(utf("  ·  "));
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
            auto r = getLocalBounds().toFloat().reduced(4.0f, 2.0f);
            g.setColour(juce::Colour(orange).withAlpha(0.14f));
            g.fillRoundedRectangle(r, 6.0f);
            g.setColour(juce::Colour(orange));
            g.fillRoundedRectangle(r.withWidth(3.0f).reduced(0.0f, 5.0f), 1.5f);
        }
        g.setColour(highlighted ? juce::Colour(orangeLight) : juce::Colour(textPrimary));
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

juce::Rectangle<float> SegmentedControl::itemBounds(float index) const {
    const auto track = getLocalBounds().toFloat().reduced(4.0f);
    const float itemWidth = track.getWidth() / (float) juce::jmax(1, items.size());
    return { track.getX() + index * itemWidth, track.getY(), itemWidth, track.getHeight() };
}

int SegmentedControl::indexAt(juce::Point<float> p) const {
    if (items.isEmpty() || !getLocalBounds().toFloat().contains(p)) return -1;
    const auto track = getLocalBounds().toFloat().reduced(4.0f);
    return juce::jlimit(0, items.size() - 1, (int) ((p.x - track.getX()) / (track.getWidth() / (float) items.size())));
}

void SegmentedControl::setSelected(int value) {
    selected = value;
    if (indicator < 0.0f || !isShowing()) indicator = (float) value;
    else startTimerHz(60);
    repaint();
}

void SegmentedControl::timerCallback() {
    indicator += ((float) selected - indicator) * 0.28f;
    if (std::abs(indicator - (float) selected) < 0.004f) { indicator = (float) selected; stopTimer(); }
    repaint();
}

void SegmentedControl::paint(juce::Graphics& g) {
    if (items.isEmpty()) return;
    auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(juce::Colour(0xff08090b));
    g.fillRoundedRectangle(bounds, 11.0f);
    g.setColour(juce::Colour(borderQuiet));
    g.drawRoundedRectangle(bounds, 11.0f, 1.0f);

    if (hovered >= 0 && hovered != selected) {
        g.setColour(juce::Colour(surfaceHover));
        g.fillRoundedRectangle(itemBounds((float) hovered).reduced(1.0f), 8.0f);
    }

    // Sliding pill marks the active option and glides between choices.
    const auto pill = itemBounds(juce::jmax(0.0f, indicator));
    g.setGradientFill(accentGradient(pill));
    g.fillRoundedRectangle(pill, 8.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.22f), 0.0f, pill.getY(),
                                           juce::Colours::white.withAlpha(0.0f), 0.0f, pill.getCentreY(), false));
    g.fillRoundedRectangle(pill.reduced(1.5f).withHeight(pill.getHeight() * 0.5f), 7.0f);

    g.setFont(labelFont(13.5f));
    for (int i = 0; i < items.size(); ++i) {
        const float closeness = juce::jlimit(0.0f, 1.0f, 1.0f - std::abs(indicator - (float) i));
        const auto idle = juce::Colour(i == hovered ? textPrimary : textSecondary);
        g.setColour(idle.interpolatedWith(juce::Colour(0xff160904), closeness));
        g.drawFittedText(items[i], itemBounds((float) i).toNearestInt().reduced(6, 0), juce::Justification::centred, 1);
    }
}

void SegmentedControl::mouseMove(const juce::MouseEvent& e) { const int h = indexAt(e.position); if (h != hovered) { hovered = h; repaint(); } }
void SegmentedControl::mouseExit(const juce::MouseEvent&) { hovered = -1; repaint(); }
void SegmentedControl::mouseUp(const juce::MouseEvent& e) {
    const int i = indexAt(e.position);
    if (i >= 0 && i != selected) { setSelected(i); if (onSelect) onSelect(i); }
}

ChordCardComponent::ChordCardComponent()
    : variationIcon(drawableFromSvg(FigmaIcons::variation_card)),
      playIcon(drawableFromSvg(FigmaIcons::play_card)) {}

juce::Rectangle<float> ChordCardComponent::body() const { return getLocalBounds().toFloat().reduced(glowMargin); }
juce::Rectangle<float> ChordCardComponent::playArea() const { auto b = body(); return { b.getRight() - 40.0f, b.getY() + 16.0f, 24.0f, 24.0f }; }
juce::Rectangle<float> ChordCardComponent::variationArea() const { auto b = body(); return { b.getRight() - 105.0f, b.getY() + 18.0f, 22.0f, 22.0f }; }
juce::Rectangle<float> ChordCardComponent::lockArea() const { auto b = body(); return { b.getRight() - 74.0f, b.getY() + 16.0f, 24.0f, 24.0f }; }

void ChordCardComponent::advanceAnimation() {
    const float hoverTarget = available && hover ? 1.0f : 0.0f;
    const float activeTarget = available && playing ? 1.0f : 0.0f;
    const auto ease = [](float& value, float target, float speed) {
        if (std::abs(value - target) < 0.005f) { const bool changed = value != target; value = target; return changed; }
        value += (target - value) * speed;
        return true;
    };
    const bool changed = ease(hoverLevel, hoverTarget, 0.30f) | ease(activeLevel, activeTarget, 0.22f);
    if (changed) repaint();
}

void ChordCardComponent::paint(juce::Graphics& g) {
    const auto r = body();
    const bool active = available && playing;
    const float glow = juce::jmax(activeLevel, selected && available ? 0.35f : 0.0f);
    juce::Path shape;
    shape.addRoundedRectangle(r, radiusCard);

    if (glow > 0.01f)
        drawGlow(g, shape, juce::Colour(orange).withAlpha(0.55f * glow), 18);

    const float alpha = available ? 1.0f : 0.45f;
    const auto top = juce::Colour(surfaceRaised).interpolatedWith(juce::Colour(surfaceHover), hoverLevel);
    g.setGradientFill(raisedGradient(r, top.withMultipliedAlpha(alpha), juce::Colour(surface).withMultipliedAlpha(alpha)));
    g.fillPath(shape);

    if (activeLevel > 0.01f) {
        juce::ColourGradient warm(juce::Colour(orange).withAlpha(0.26f * activeLevel), r.getCentreX(), r.getBottom(),
                                  juce::Colour(orange).withAlpha(0.0f), r.getCentreX(), r.getY() + r.getHeight() * 0.15f, true);
        g.setGradientFill(warm);
        g.fillPath(shape);
    }

    // Top edge highlight.
    g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.07f + 0.05f * hoverLevel), 0.0f, r.getY(),
                                           juce::Colours::transparentWhite, 0.0f, r.getY() + 60.0f, false));
    g.strokePath(shape, juce::PathStrokeType(1.0f), juce::AffineTransform::translation(0.0f, 0.5f));

    if (glow > 0.01f) {
        auto outline = accentGradient(r);
        for (int i = 0; i < outline.getNumColours(); ++i)
            outline.setColour(i, outline.getColour(i).withAlpha(glow));
        g.setGradientFill(outline);
        g.strokePath(shape, juce::PathStrokeType(1.0f + 0.8f * glow));
    } else {
        g.setColour(juce::Colour(border).interpolatedWith(juce::Colour(borderStrong), hoverLevel).withMultipliedAlpha(available ? 1.0f : 0.5f));
        g.strokePath(shape, juce::PathStrokeType(1.0f));
    }

    // Slot badge.
    auto badge = juce::Rectangle<float>(r.getX() + 16.0f, r.getY() + 16.0f, 36.0f, 24.0f);
    g.setColour(active ? juce::Colour(orange).withAlpha(0.18f) : juce::Colour(0xff1d2127).withMultipliedAlpha(alpha));
    g.fillRoundedRectangle(badge, 7.0f);
    g.setColour(active ? juce::Colour(orangeLight) : juce::Colour(textSecondary).withMultipliedAlpha(alpha));
    g.setFont(font(13.0f, true));
    g.drawText(juce::String(slot + 1).paddedLeft('0', 2), badge, juce::Justification::centred);

    if (available) {
        drawSvg(g, variationIcon.get(), variationArea());
        if (locked) {
            g.setColour(juce::Colour(orange).withAlpha(0.16f));
            g.fillRoundedRectangle(lockArea().expanded(4.0f), 7.0f);
        }
        drawLockIcon(g, lockArea(), locked ? juce::Colour(orange) : juce::Colour(textSecondary), locked);
        if (activeLevel > 0.01f) {
            g.setOpacity(activeLevel);
            drawSvg(g, playIcon.get(), playArea());
            g.setOpacity(1.0f);
        }
    }

    // Chord name, gently lifting when hovered or playing.
    const float lift = 3.0f * juce::jmax(hoverLevel, activeLevel);
    g.setColour(juce::Colour(textPrimary).withAlpha(available ? 1.0f : 0.25f));
    g.setFont(font(54.0f, true));
    g.drawFittedText(available ? utf(chord.name.c_str()) : juce::String::fromUTF8("—"),
                     (int) r.getX() + 16, (int) (r.getY() + 62.0f - lift), (int) r.getWidth() - 32, 68,
                     juce::Justification::centred, 1, 0.82f);

    if (available) {
        const auto numeral = romanNumeral(chord);
        g.setFont(font(14.0f, true));
        const float numeralWidth = (float) juce::GlyphArrangement::getStringWidthInt(font(14.0f, true), numeral) + 16.0f;
        const auto notes = chordNotes(chord);
        const auto notesFont = font(13.0f, false);
        const float notesWidth = (float) juce::GlyphArrangement::getStringWidthInt(notesFont, notes);
        const float rowWidth = numeralWidth + 10.0f + notesWidth;
        auto row = juce::Rectangle<float>(rowWidth, 22.0f).withCentre({ r.getCentreX(), r.getY() + 146.0f - lift * 0.5f });
        auto pill = row.removeFromLeft(numeralWidth);
        g.setColour(juce::Colour(orange).withAlpha(0.12f + 0.10f * activeLevel));
        g.fillRoundedRectangle(pill, 11.0f);
        g.setColour(juce::Colour(orangeLight));
        g.drawText(numeral, pill, juce::Justification::centred);
        row.removeFromLeft(10.0f);
        g.setColour(juce::Colour(textSecondary));
        g.setFont(notesFont);
        g.drawText(notes, row, juce::Justification::centredLeft);
    }

    // Equalizer: idle bars stay calm, playback makes them dance.
    static constexpr float heights[] { 10.0f, 18.0f, 28.0f, 40.0f, 48.0f, 40.0f, 28.0f, 18.0f, 10.0f };
    constexpr int barCount = 9;
    constexpr float barWidth = 6.0f, gap = 8.0f;
    const float startX = r.getCentreX() - ((float) barCount * barWidth + (float) (barCount - 1) * gap) * 0.5f;
    const float centreY = r.getY() + 200.0f;
    const auto idle = juce::Colour(waveform).interpolatedWith(juce::Colour(0xff59606b), hoverLevel).withMultipliedAlpha(available ? 1.0f : 0.35f);
    for (int i = 0; i < barCount; ++i) {
        float height = heights[i];
        if (activeLevel > 0.01f) {
            const float pulse = 0.5f + 0.5f * std::sin(playbackPhase * 1.15f + (float) i * 0.85f)
                                     * std::cos(playbackPhase * 0.55f - (float) i * 0.4f);
            const float animated = juce::jlimit(8.0f, 56.0f, heights[i] * (0.55f + pulse * 0.85f));
            height = juce::jmap(activeLevel, height, animated);
        }
        auto bar = juce::Rectangle<float>(startX + (float) i * (barWidth + gap), centreY - height * 0.5f, barWidth, height);
        if (activeLevel > 0.01f) {
            juce::ColourGradient fill(juce::Colour(amber), 0.0f, bar.getY(), juce::Colour(orange), 0.0f, bar.getBottom(), false);
            fill.multiplyOpacity(activeLevel);
            g.setColour(idle.withMultipliedAlpha(1.0f - activeLevel));
            g.fillRoundedRectangle(bar, barWidth * 0.5f);
            g.setGradientFill(fill);
        } else {
            g.setColour(idle);
        }
        g.fillRoundedRectangle(bar, barWidth * 0.5f);
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
    g.setColour(juce::Colour(over || down ? surfaceRaised : surfaceGroup));
    g.fillRoundedRectangle(r, radiusCard);
    juce::Path outline, dashed;
    outline.addRoundedRectangle(r, radiusCard);
    const float pattern[] { 6.0f, 6.0f };
    juce::PathStrokeType(1.2f).createDashedStroke(dashed, outline, pattern, 2);
    g.setColour(over ? juce::Colour(orange).withAlpha(0.8f) : juce::Colour(borderStrong).withAlpha(0.8f));
    g.fillPath(dashed);

    auto circle = juce::Rectangle<float>(52.0f, 52.0f).withCentre({ r.getCentreX(), 96.0f });
    if (over) {
        g.setGradientFill(accentGradient(circle));
        g.fillEllipse(circle);
    } else {
        g.setColour(juce::Colour(0xff1d2127));
        g.fillEllipse(circle);
        g.setColour(juce::Colour(borderStrong));
        g.drawEllipse(circle, 1.0f);
    }
    g.setColour(over ? juce::Colour(0xff160904) : juce::Colour(textPrimary));
    const auto c = circle.getCentre();
    g.drawLine(c.x - 9.0f, c.y, c.x + 9.0f, c.y, 2.4f);
    if (!backMode) g.drawLine(c.x, c.y - 9.0f, c.x, c.y + 9.0f, 2.4f);

    g.setColour(juce::Colour(textPrimary));
    g.setFont(labelFont(14.0f));
    g.drawFittedText(backMode ? "VOLTAR A 4" : "CONTINUAR", 8, 138, getWidth() - 16, 24,
                     juce::Justification::centred, 1);
    g.setColour(juce::Colour(textSecondary));
    g.setFont(labelFont(11.5f));
    g.drawFittedText(backMode ? "REMOVER 4" : "+4 ACORDES", 8, 162, getWidth() - 16, 20,
                     juce::Justification::centred, 1);
}

void StatusPill::setLive(bool value, float phase) {
    const float next = value ? 0.5f + 0.5f * std::sin(phase) : 0.0f;
    if (value != live || (value && std::abs(next - pulse) > 0.02f)) { live = value; pulse = next; repaint(); }
}

void StatusPill::paint(juce::Graphics& g) {
    auto r = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(juce::Colour(surfaceRaised));
    g.fillRoundedRectangle(r, r.getHeight() * 0.5f);
    g.setColour(live ? juce::Colour(orange).withAlpha(0.45f) : juce::Colour(border));
    g.drawRoundedRectangle(r, r.getHeight() * 0.5f, 1.0f);
    auto dot = juce::Rectangle<float>(8.0f, 8.0f).withCentre({ r.getX() + 20.0f, r.getCentreY() });
    if (live) {
        g.setColour(juce::Colour(orange).withAlpha(0.35f * pulse));
        g.fillEllipse(dot.expanded(4.0f + 3.0f * pulse));
    }
    g.setColour(live ? juce::Colour(orange) : juce::Colour(textQuiet));
    g.fillEllipse(dot);
    g.setColour(juce::Colour(textSecondary));
    g.setFont(labelFont(12.5f));
    g.drawFittedText(text, r.withTrimmedLeft(34.0f).withTrimmedRight(14.0f).toNearestInt(), juce::Justification::centredLeft, 1);
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
        boxes[i]->setRepaintsOnMouseActivity(true);
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
    velocity.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    velocity.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
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
    content.addAndMakeVisible(status);

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
    const auto full = juce::Rectangle<float>(0.0f, 0.0f, (float) designWidth, (float) designHeight);
    g.setGradientFill(juce::ColourGradient(juce::Colour(backgroundTop), 0.0f, 0.0f,
                                           juce::Colour(background), 0.0f, full.getBottom(), false));
    g.fillRoundedRectangle(full, radiusWindow);

    // Ambient warm light: one glow behind the brand, one rising behind the chord cards.
    g.setGradientFill(juce::ColourGradient(juce::Colour(orange).withAlpha(0.10f), 120.0f, 40.0f,
                                           juce::Colour(orange).withAlpha(0.0f), 620.0f, 300.0f, true));
    g.fillRoundedRectangle(full, radiusWindow);
    g.setGradientFill(juce::ColourGradient(juce::Colour(orange).withAlpha(0.06f), 768.0f, 600.0f,
                                           juce::Colour(orange).withAlpha(0.0f), 768.0f, 180.0f, true));
    g.fillRoundedRectangle(full, radiusWindow);

    // Subtle dot grid texture in the header.
    g.setColour(juce::Colours::white.withAlpha(0.025f));
    for (int y = 18; y < 88; y += 14)
        for (int x = 560; x < 1340; x += 14)
            g.fillRect((float) x, (float) y, 1.5f, 1.5f);

    auto window = full.reduced(0.75f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(borderStrong), 0.0f, 0.0f,
                                           juce::Colour(borderQuiet), 0.0f, full.getBottom(), false));
    g.drawRoundedRectangle(window, radiusWindow, 1.5f);

    // Brand mark: a small equalizer glyph followed by the wordmark.
    auto mark = juce::Rectangle<float>(34.0f, 26.0f, 38.0f, 38.0f);
    g.setGradientFill(accentGradient(mark));
    g.fillRoundedRectangle(mark, 10.0f);
    g.setColour(juce::Colour(0xff160904));
    static constexpr float markBars[] { 10.0f, 20.0f, 14.0f, 24.0f };
    for (int i = 0; i < 4; ++i)
        g.fillRoundedRectangle(mark.getX() + 8.0f + (float) i * 6.5f, mark.getCentreY() - markBars[i] * 0.5f, 3.5f, markBars[i], 1.75f);

    const auto logoFont = font(38.0f, true).withExtraKerningFactor(0.02f);
    g.setFont(logoFont);
    g.setColour(juce::Colour(textPrimary));
    g.drawText("CHORD", 84, 20, 170, 50, juce::Justification::centredLeft);
    const int codeX = 84 + juce::GlyphArrangement::getStringWidthInt(logoFont, "CHORD") + 9;
    const auto codeArea = juce::Rectangle<float>((float) codeX, 20.0f, 130.0f, 50.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(orange), codeArea.getX(), 0.0f,
                                           juce::Colour(amber), codeArea.getRight(), 0.0f, false));
    g.drawText("CODE", codeArea, juce::Justification::centredLeft);

    auto versionPill = juce::Rectangle<float>(1384.0f, 33.0f, 64.0f, 26.0f);
    g.setColour(juce::Colour(surfaceRaised));
    g.fillRoundedRectangle(versionPill, 13.0f);
    g.setColour(juce::Colour(border));
    g.drawRoundedRectangle(versionPill, 13.0f, 1.0f);
    g.setColour(juce::Colour(textSecondary));
    g.setFont(font(13.0f, true));
    g.drawText("v" JucePlugin_VersionString, versionPill, juce::Justification::centred);

    {
        juce::ColourGradient line(juce::Colour(divider).withAlpha(0.0f), 30.0f, 0.0f,
                                  juce::Colour(divider).withAlpha(0.0f), 1506.0f, 0.0f, false);
        line.addColour(0.15, juce::Colour(borderStrong).withAlpha(0.6f));
        line.addColour(0.85, juce::Colour(borderStrong).withAlpha(0.6f));
        g.setGradientFill(line);
        g.fillRect(30.0f, 90.0f, 1476.0f, 1.0f);
    }

    label(g, "TOM", 34, 116, 150); label(g, "ESCALA", 396, 116, 150);
    label(g, "ESTILO", 768, 116, 150); label(g, "VIBE", 1130, 116, 150);
    label(g, "COMPLEXIDADE", 34, 218, 180); label(g, "TENSÃO", 504, 218, 150); label(g, "TIMBRE", 964, 218, 150);

    const auto drawGroup = [&g](juce::Rectangle<float> bounds) {
        drawRaisedPanel(g, bounds, radiusPanel, juce::Colour(borderQuiet));
    };
    drawGroup({ 34.5f, 608.5f, 460.0f, 118.0f });
    drawGroup({ 504.5f, 608.5f, 450.0f, 118.0f });
    drawGroup({ 964.5f, 608.5f, 536.0f, 118.0f });
    label(g, "FORMA DO ACORDE", 55, 625, 220); label(g, "RITMO", 525, 625, 180);
    label(g, "DURAÇÃO", 981, 625, 180); label(g, "VOLUME", 1193, 625, 160);

    // Warm glow under the primary action.
    juce::Path generateShape;
    generateShape.addRoundedRectangle(generate.getBounds().toFloat().reduced(6.0f, 4.0f).translated(0.0f, 6.0f), 14.0f);
    drawGlow(g, generateShape, juce::Colour(orange).withAlpha(0.55f), 26);
}

void ChordCodeProEditor::resized() {
    content.setBounds(0, 0, designWidth, designHeight);
    content.setTransform(juce::AffineTransform::scale(editorScale));
    settings.setBounds(1465, 29, 38, 38);
    key.setBounds(34, 140, 320, 50); scale.setBounds(396, 140, 330, 50);
    style.setBounds(768, 140, 330, 50); mood.setBounds(1130, 140, 370, 50);
    complexity.setBounds(34, 242, 460, 50); tension.setBounds(504, 242, 450, 50); timbre.setBounds(964, 242, 536, 50);

    const int margin = (int) ChordCardComponent::glowMargin;
    cardsViewport.setBounds(34 - margin, 332 - margin, 1288 + margin * 2, 252 + margin * 2);
    status.setBounds(1060, 32, 310, 28);
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
        cards[(size_t) i].advanceAnimation();
    }
    status.setLive(now >= 0, animationPhase);
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
        const int margin = (int) ChordCardComponent::glowMargin;
        cards[(size_t) i].setBounds(xPositions[column] + page * cardPageWidth - travel, 0, 310 + margin * 2, 252 + margin * 2);
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
    updateStatus();
    repaint();
}

void ChordCodeProEditor::updateStatus() {
    const auto progression = pro.getProgression();
    status.setText(key.getText() + " " + scale.getText() + utf("  ·  ") + juce::String(progression.chordCount)
                   + utf(" ACORDES  ·  ") + juce::String(juce::roundToInt(pro.hostBpm())) + " BPM");
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
