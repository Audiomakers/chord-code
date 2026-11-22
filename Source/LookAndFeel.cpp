#include "LookAndFeel.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include "ChordCodeTheme.h"

using namespace ChordCodeTheme;

ProLookAndFeel::ProLookAndFeel() {
    setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(background));
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(surface));
    setColour(juce::ComboBox::textColourId, juce::Colour(textPrimary));
    setColour(juce::ComboBox::outlineColourId, juce::Colour(border));
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(surface));
    setColour(juce::PopupMenu::textColourId, juce::Colour(textPrimary));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(orange));
    setColour(juce::PopupMenu::highlightedTextColourId, juce::Colour(orangeLight));
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(surface));
    setColour(juce::Slider::textBoxTextColourId, juce::Colour(textPrimary));
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(border));
}

// Audio Makers UI (same rules as Melody Code, in Chord Code's design units): radius 13, one icon set,
// SemiBold labels, a single hero button (GERAR).
namespace {
AMUI::Icon iconFor(const juce::String& id, const juce::String& text) {
    if (id == "play") return text == "PARAR" ? AMUI::Icon::stop : AMUI::Icon::play;
    if (id == "variation") return AMUI::Icon::variation;
    if (id == "random") return AMUI::Icon::dice;
    if (id == "generate") return AMUI::Icon::sparkle;
    if (id == "history") return AMUI::Icon::history;
    if (id == "drag") return AMUI::Icon::midi;
    if (id == "dragWav") return AMUI::Icon::wav;
    if (id == "save") return AMUI::Icon::save;
    return AMUI::Icon::none;
}
}

void ProLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) {
    const auto id = b.getComponentID();
    auto r = b.getLocalBounds().toFloat().reduced(0.75f);
    if (id == "prev" || id == "next") {   // ghost arrows inside a field
        if (over || down) { g.setColour(juce::Colour(border).withAlpha(down ? 1.0f : 0.7f)); g.fillRoundedRectangle(r, 10.0f); }
        return;
    }
    if (id == "generate") {
        juce::Path p; p.addRoundedRectangle(r, radiusControl);
        if (b.isEnabled()) drawGlow(g, p, juce::Colour(orange).withAlpha(over ? 0.42f : 0.28f), 26);
        g.setGradientFill(accentGradient(r));
        g.fillPath(p);
        if (over || down) {
            g.setColour(down ? juce::Colours::black.withAlpha(0.14f) : juce::Colours::white.withAlpha(0.08f));
            g.fillPath(p);
        }
        return;
    }
    g.setColour(juce::Colour(down ? surface : over ? surfaceHover : surfaceRaised));
    g.fillRoundedRectangle(r, radiusControl);
    g.setColour(over ? juce::Colour(orange).withAlpha(0.6f) : juce::Colour(border));
    g.drawRoundedRectangle(r, radiusControl, 1.0f);
}

void ProLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& b, bool over, bool) {
    const auto id = b.getComponentID();
    if (id == "prev" || id == "next") {
        AMUI::drawIcon(g, id == "prev" ? AMUI::Icon::left : AMUI::Icon::right,
                       juce::Rectangle<float>(22.0f, 22.0f).withCentre(b.getLocalBounds().toFloat().getCentre()),
                       over ? juce::Colour(orange) : juce::Colour(textSecondary));
        return;
    }
    if (id == "settings") {
        AMUI::drawIcon(g, AMUI::Icon::gear, juce::Rectangle<float>(26.0f, 26.0f).withCentre(b.getLocalBounds().toFloat().getCentre()),
                       over ? juce::Colour(orange) : juce::Colour(textSecondary));
        if ((bool) b.getProperties().getWithDefault("updateDot", false))
            drawUpdateDot(g, { (float) b.getWidth() - 3.0f, 3.0f });
        return;
    }
    const bool primary = id == "generate";
    const auto text = b.getButtonText();
    const auto icon = iconFor(id, text);
    auto f = primary ? semi(20.5f).withExtraKerningFactor(AMUI::kerningHero) : buttonFont(17.5f);
    const float iconSize = 26.0f, gap = 10.0f;
    const auto r = b.getLocalBounds().toFloat();
    float tw = juce::GlyphArrangement::getStringWidth(f, text);
    const float room = r.getWidth() - 28.0f - (icon == AMUI::Icon::none ? 0.0f : iconSize + gap);
    if (tw > room) { f = buttonFont(16.0f); tw = juce::GlyphArrangement::getStringWidth(f, text); }
    const auto textColour = primary ? juce::Colour(0xff1a0a02) : juce::Colour(textPrimary);
    g.setFont(f);
    if (icon == AMUI::Icon::none || tw > room) {
        g.setColour(textColour);
        g.drawFittedText(text, b.getLocalBounds().reduced(10, 0), juce::Justification::centred, 1, 0.9f);
        return;
    }
    const float x0 = std::round(r.getCentreX() - (iconSize + gap + tw) * 0.5f);
    AMUI::drawIcon(g, icon, { x0, r.getCentreY() - iconSize * 0.5f, iconSize, iconSize },
                   primary ? textColour : over ? juce::Colour(orange) : juce::Colour(textSecondary));
    g.setColour(textColour);
    g.drawText(text, juce::Rectangle<float>(x0 + iconSize + gap, r.getY(), tw + 4.0f, r.getHeight()), juce::Justification::centredLeft);
}

void ProLookAndFeel::drawComboBox(juce::Graphics& g, int w, int h, bool down, int, int, int, int, juce::ComboBox& box) {
    auto r = juce::Rectangle<float>(0.75f, 0.75f, (float) w - 1.5f, (float) h - 1.5f);
    const bool over = box.isMouseOver(true);
    const bool open = box.isPopupActive() || down;
    g.setColour(juce::Colour(over ? surfaceHover : surfaceRaised));
    g.fillRoundedRectangle(r, radiusControl);
    g.setColour(open ? juce::Colour(orange) : over ? juce::Colour(orange).withAlpha(0.6f) : juce::Colour(border));
    g.drawRoundedRectangle(r, radiusControl, 1.0f);
    const float s = 22.0f;
    AMUI::drawIcon(g, AMUI::Icon::chevron, { (float) w - 18.0f - s, ((float) h - s) * 0.5f, s, s },
                   open || over ? juce::Colour(orange) : juce::Colour(textSecondary));
}

void ProLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label) {
    label.setBounds(17, 1, box.getWidth() - 62, box.getHeight() - 2);
    label.setFont(getComboBoxFont(box));
    label.setJustificationType(juce::Justification::centredLeft);
}

void ProLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h, float pos,
                                      float, float, juce::Slider::SliderStyle, juce::Slider& slider) {
    const float trackY = (float) y + (float) h * 0.5f;
    const float left = (float) x + 10.0f;
    const float right = (float) x + (float) w - 10.0f;
    pos = juce::jlimit(left, right, pos);
    auto track = juce::Rectangle<float>(left, trackY - 2.5f, right - left, 5.0f);
    g.setColour(juce::Colour(borderStrong));
    g.fillRoundedRectangle(track, 2.5f);
    auto filled = track.withWidth(juce::jmax(5.0f, pos - left));
    g.setGradientFill(juce::ColourGradient(juce::Colour(orange), left, trackY, juce::Colour(amber), right, trackY, false));
    g.fillRoundedRectangle(filled, 2.5f);

    // same thumb as Melody Code: white dot with a soft accent ring
    const bool over = slider.isMouseOverOrDragging();
    const float knob = 22.0f;
    g.setColour(juce::Colour(orange).withAlpha(over ? 0.45f : 0.32f));
    g.fillEllipse(juce::Rectangle<float>(knob + 8.0f, knob + 8.0f).withCentre({ pos, trackY }));
    g.setColour(juce::Colour(textPrimary));
    g.fillEllipse(juce::Rectangle<float>(knob, knob).withCentre({ pos, trackY }));
}

void ProLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& l) {
    if (l.isBeingEdited()) return;
    g.setColour(l.findColour(juce::Label::textColourId));
    g.setFont(l.getFont());
    g.drawFittedText(l.getText(), l.getLocalBounds().reduced(2), l.getJustificationType(), 1, 0.8f);
}

void ProLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height) {
    g.fillAll(juce::Colour(surface));
    g.setColour(juce::Colour(border));
    g.drawRect(0, 0, width, height, 1);
}

void ProLookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                                       bool separator, bool enabled, bool highlighted, bool ticked,
                                       bool hasSubMenu, const juce::String& text,
                                       const juce::String& shortcut, const juce::Drawable* icon,
                                       const juce::Colour* textColour) {
    if (separator) {
        g.setColour(juce::Colour(borderQuiet));
        g.fillRect(area.reduced(10, 0).withHeight(1).withY(area.getCentreY()));
        return;
    }

    if (highlighted && enabled) {
        auto r = area.toFloat().reduced(4.0f, 2.0f);
        g.setColour(juce::Colour(orange).withAlpha(0.14f));
        g.fillRoundedRectangle(r, 6.0f);
        g.setColour(juce::Colour(orange));
        g.fillRoundedRectangle(r.withWidth(3.0f).reduced(0.0f, 5.0f), 1.5f);
    }

    const auto colour = !enabled ? juce::Colour(textQuiet)
        : highlighted ? juce::Colour(orangeLight)
        : (textColour != nullptr ? *textColour : juce::Colour(textPrimary));
    g.setColour(colour);
    g.setFont(semi(15.0));
    auto textArea = area.reduced(14, 0);
    if (ticked) {
        g.setColour(juce::Colour(orange));
        g.fillEllipse(textArea.removeFromLeft(20).toFloat().withSizeKeepingCentre(7.0f, 7.0f));
        g.setColour(colour);
    }
    if (icon != nullptr)
        icon->drawWithin(g, textArea.removeFromLeft(22).toFloat().reduced(2.0f),
                         juce::RectanglePlacement::centred, 1.0f);
    if (hasSubMenu)
        textArea = textArea.withTrimmedRight(18);
    if (shortcut.isNotEmpty())
        g.drawText(shortcut, textArea.removeFromRight(70), juce::Justification::centredRight);
    g.drawText(text, textArea, juce::Justification::centredLeft);

    if (hasSubMenu) {
        juce::Path arrow;
        arrow.startNewSubPath((float) area.getRight() - 15.0f, (float) area.getCentreY() - 4.0f);
        arrow.lineTo((float) area.getRight() - 11.0f, (float) area.getCentreY());
        arrow.lineTo((float) area.getRight() - 15.0f, (float) area.getCentreY() + 4.0f);
        g.strokePath(arrow, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

juce::PopupMenu::Options ProLookAndFeel::getOptionsForComboBoxPopupMenu(juce::ComboBox& box, juce::Label& label) {
    auto options = LookAndFeel_V4::getOptionsForComboBoxPopupMenu(box, label);
    // Open the list inside the editor so it is drawn at full size instead of
    // inheriting the editor's down-scaled transform.
    if (auto* editor = box.findParentComponentOfClass<juce::AudioProcessorEditor>()) {
        const auto area = editor->getLocalArea(nullptr, box.getScreenBounds());
        options = options.withParentComponent(editor)
                         .withTargetScreenArea(box.getScreenBounds())
                         .withMinimumWidth(area.getWidth())
                         .withStandardItemHeight(30)
                         .withMaximumNumColumns(2)
                         .withPreferredPopupDirection(juce::PopupMenu::Options::PopupDirection::downwards);
    }
    return options;
}

juce::Font ProLookAndFeel::getComboBoxFont(juce::ComboBox&) { return semi(20.5f); }
juce::Font ProLookAndFeel::getPopupMenuFont() { return semi(15.0f); }

void ProLookAndFeel::getIdealPopupMenuItemSize(const juce::String& text, bool separator,
                                               int standardHeight, int& width, int& height) {
    if (separator) { width = 50; height = 10; return; }
    const auto popupFont = getPopupMenuFont();
    height = standardHeight > 0 ? juce::jmax(standardHeight, 30) : juce::roundToInt(popupFont.getHeight() * 1.35f);
    width = juce::GlyphArrangement::getStringWidthInt(popupFont, text) + height * 2;
}

// Hover explanations (same look as the Melody Code): a dark card with an orange border.
namespace {
juce::TextLayout tooltipLayout(const juce::String& text) {
    juce::AttributedString a;
    a.setJustification(juce::Justification::centredLeft);
    a.append(text, ChordCodeTheme::font(13.5f), juce::Colour(ChordCodeTheme::textPrimary));
    juce::TextLayout layout;
    layout.createLayoutWithBalancedLineLengths(a, 300.0f);
    return layout;
}
}

juce::Rectangle<int> ProLookAndFeel::getTooltipBounds(const juce::String& text, juce::Point<int> pos, juce::Rectangle<int> parent) {
    const auto layout = tooltipLayout(text);
    const int w = (int) std::ceil(layout.getWidth()) + 28, h = (int) std::ceil(layout.getHeight()) + 20;
    return juce::Rectangle<int>(pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 12,
                                pos.y > parent.getCentreY() ? pos.y - (h + 8) : pos.y + 18, w, h)
        .constrainedWithin(parent);
}

void ProLookAndFeel::drawTooltip(juce::Graphics& g, const juce::String& text, int w, int h) {
    const auto r = juce::Rectangle<float>(0, 0, (float) w, (float) h);
    g.setColour(juce::Colour(ChordCodeTheme::surfaceRaised));
    g.fillRoundedRectangle(r, 8.0f);
    g.setColour(juce::Colour(ChordCodeTheme::borderStrong));
    g.drawRoundedRectangle(r.reduced(0.5f), 8.0f, 1.0f);
    tooltipLayout(text).draw(g, r.reduced(14.0f, 10.0f));
}
