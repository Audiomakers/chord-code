#include "LookAndFeel.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include "ChordCodeTheme.h"
#include "FigmaIcons.h"

using namespace ChordCodeTheme;

namespace {
std::unique_ptr<juce::Drawable> drawableFromSvg(const char* source) {
    if (auto xml = juce::parseXML(source)) return juce::Drawable::createFromSVG(*xml);
    return {};
}

void drawSvg(juce::Graphics& g, juce::Drawable* drawable, juce::Rectangle<float> bounds) {
    if (drawable != nullptr)
        drawable->drawWithin(g, bounds, juce::RectanglePlacement::centred, 1.0f);
}
}

ProLookAndFeel::ProLookAndFeel() {
    settingsIcon = drawableFromSvg(FigmaIcons::settings);
    playIcon = drawableFromSvg(FigmaIcons::play_action);
    variationIcon = drawableFromSvg(FigmaIcons::variation_action);
    randomIcon = drawableFromSvg(FigmaIcons::random_action);
    dragIcon = drawableFromSvg(FigmaIcons::drag_action);
    saveIcon = drawableFromSvg(FigmaIcons::save_action);
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

void ProLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) {
    const auto id = b.getComponentID();
    auto r = b.getLocalBounds().toFloat().reduced(0.75f);
    if (id == "settings") {
        if (over || down) {
            g.setColour(juce::Colour(surfaceHover).withAlpha(down ? 1.0f : 0.85f));
            g.fillRoundedRectangle(r, 10.0f);
        }
        return;
    }
    if (id == "generate") {
        g.setGradientFill(accentGradient(r));
        g.fillRoundedRectangle(r, 14.0f);
        // Glossy top sheen.
        g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.28f), 0.0f, r.getY(),
                                               juce::Colours::white.withAlpha(0.0f), 0.0f, r.getCentreY(), false));
        g.fillRoundedRectangle(r.reduced(2.0f).withHeight(r.getHeight() * 0.5f), 12.0f);
        if (over || down) {
            g.setColour(down ? juce::Colours::black.withAlpha(0.10f) : juce::Colours::white.withAlpha(0.10f));
            g.fillRoundedRectangle(r, 14.0f);
        }
        g.setColour(juce::Colours::white.withAlpha(0.22f));
        g.drawRoundedRectangle(r.reduced(0.5f), 14.0f, 1.0f);
        return;
    }
    const auto top = down ? juce::Colour(surface) : over ? juce::Colour(surfaceHover) : juce::Colour(surfaceRaised);
    g.setGradientFill(raisedGradient(r, top, juce::Colour(surface)));
    g.fillRoundedRectangle(r, 12.0f);
    if (over) {
        g.setGradientFill(juce::ColourGradient(juce::Colour(orange).withAlpha(0.10f), 0.0f, r.getBottom(),
                                               juce::Colour(orange).withAlpha(0.0f), 0.0f, r.getY(), false));
        g.fillRoundedRectangle(r, 12.0f);
    }
    g.setColour(over ? juce::Colour(orange).withAlpha(0.75f) : juce::Colour(border));
    g.drawRoundedRectangle(r, 12.0f, 1.0f);
}

void ProLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& b, bool, bool) {
    const auto id = b.getComponentID();
    if (id == "settings") {
        drawSvg(g, settingsIcon.get(), b.getLocalBounds().toFloat().reduced(1.0f));
        if ((bool) b.getProperties().getWithDefault("updateDot", false))
            drawUpdateDot(g, { (float) b.getWidth() - 5.0f, 5.0f });
        return;
    }
    const bool primary = id == "generate";
    const auto colour = primary ? juce::Colour(0xff160904) : juce::Colour(textPrimary);
    g.setColour(colour);
    g.setFont(primary ? font(26.0f, true).withExtraKerningFactor(0.14f) : labelFont(16.5f));
    auto textArea = b.getLocalBounds().toFloat();
    const bool icon = id == "play" || id == "variation" || id == "random" || id == "history" || id == "drag" || id == "dragWav" || id == "save";
    if (icon) textArea = textArea.withTrimmedLeft(34.0f);
    g.drawFittedText(b.getButtonText(), textArea.toNearestInt().reduced(8, 0), juce::Justification::centred, 1);
    if (!icon) return;
    auto iconArea = juce::Rectangle<float>(16.0f, ((float) b.getHeight() - 28.0f) * 0.5f, 28.0f, 28.0f);
    if (id == "play") {
        if (b.getButtonText() == "PARAR") {
            g.setColour(colour);
            g.fillRoundedRectangle(iconArea.getX() + 6.0f, iconArea.getY() + 5.0f, 5.0f, 18.0f, 1.0f);
            g.fillRoundedRectangle(iconArea.getX() + 17.0f, iconArea.getY() + 5.0f, 5.0f, 18.0f, 1.0f);
        } else drawSvg(g, playIcon.get(), iconArea);
    } else if (id == "variation") drawSvg(g, variationIcon.get(), iconArea);
    else if (id == "random") drawSvg(g, randomIcon.get(), iconArea.withSizeKeepingCentre(24.0f, 24.0f));
    else if (id == "history") drawHistoryIcon(g, iconArea, colour);
    else if (id == "drag" || id == "dragWav") drawSvg(g, dragIcon.get(), iconArea);
    else if (id == "save") drawSvg(g, saveIcon.get(), iconArea);
}

void ProLookAndFeel::drawComboBox(juce::Graphics& g, int w, int h, bool down, int, int, int, int, juce::ComboBox& box) {
    auto r = juce::Rectangle<float>(0.75f, 0.75f, (float) w - 1.5f, (float) h - 1.5f);
    const bool over = box.isMouseOver(true);
    const bool open = box.isPopupActive() || down;
    g.setGradientFill(raisedGradient(r, juce::Colour(over ? surfaceHover : surfaceRaised), juce::Colour(surface)));
    g.fillRoundedRectangle(r, radiusControl);
    g.setColour(open ? juce::Colour(orange) : over ? juce::Colour(orange).withAlpha(0.6f) : juce::Colour(border));
    g.drawRoundedRectangle(r, radiusControl, open ? 1.4f : 1.0f);

    auto chip = juce::Rectangle<float>((float) w - 42.0f, ((float) h - 30.0f) * 0.5f, 30.0f, 30.0f);
    g.setColour(open || over ? juce::Colour(orange).withAlpha(0.16f) : juce::Colour(0xff1d2127));
    g.fillRoundedRectangle(chip, 8.0f);
    g.setColour(open || over ? juce::Colour(orangeLight) : juce::Colour(textSecondary));
    const auto c = chip.getCentre();
    juce::Path arrow;
    const float dir = open ? -1.0f : 1.0f;
    arrow.startNewSubPath(c.x - 6.0f, c.y - 3.0f * dir);
    arrow.lineTo(c.x, c.y + 3.0f * dir);
    arrow.lineTo(c.x + 6.0f, c.y - 3.0f * dir);
    g.strokePath(arrow, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
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
    auto track = juce::Rectangle<float>(left, trackY - 3.0f, right - left, 6.0f);
    g.setColour(juce::Colour(0xff1f232a));
    g.fillRoundedRectangle(track, 3.0f);
    auto filled = track.withWidth(juce::jmax(6.0f, pos - left));
    g.setGradientFill(juce::ColourGradient(juce::Colour(orange), left, trackY, juce::Colour(amber), right, trackY, false));
    g.fillRoundedRectangle(filled, 3.0f);

    const bool over = slider.isMouseOverOrDragging();
    const float knob = over ? 24.0f : 22.0f;
    auto thumb = juce::Rectangle<float>(knob, knob).withCentre({ pos, trackY });
    juce::Path thumbPath;
    thumbPath.addEllipse(thumb);
    drawGlow(g, thumbPath, juce::Colour(orange).withAlpha(over ? 0.75f : 0.5f), 14);
    g.setColour(juce::Colour(textPrimary));
    g.fillEllipse(thumb);
    g.setColour(juce::Colour(orange));
    g.fillEllipse(thumb.reduced(knob * 0.30f));
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
    g.setFont(font(15.0f, true));
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

juce::Font ProLookAndFeel::getComboBoxFont(juce::ComboBox&) { return font(22.0f, true); }
juce::Font ProLookAndFeel::getPopupMenuFont() { return font(16.0f); }

void ProLookAndFeel::getIdealPopupMenuItemSize(const juce::String& text, bool separator,
                                               int standardHeight, int& width, int& height) {
    if (separator) { width = 50; height = 10; return; }
    const auto popupFont = getPopupMenuFont();
    height = standardHeight > 0 ? juce::jmax(standardHeight, 30) : juce::roundToInt(popupFont.getHeight() * 1.35f);
    width = juce::GlyphArrangement::getStringWidthInt(popupFont, text) + height * 2;
}
