#include "LookAndFeel.h"
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
    setColour(juce::PopupMenu::highlightedTextColourId, juce::Colour(background));
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(surface));
    setColour(juce::Slider::textBoxTextColourId, juce::Colour(textPrimary));
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(border));
}

void ProLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) {
    const auto id = b.getComponentID();
    auto r = b.getLocalBounds().toFloat().reduced(id == "settings" ? 0.0f : 0.75f);
    if (id == "settings") {
        return;
    }
    if (id == "generate") {
        g.setColour(juce::Colour(orange));
        g.fillRoundedRectangle(r, 9.0f);
        if (over || down) {
            g.setColour(juce::Colours::white.withAlpha(down ? 0.15f : 0.08f));
            g.fillRoundedRectangle(r, 9.0f);
        }
        return;
    }
    auto fill = juce::Colour(surface);
    if (over) fill = juce::Colour(0xff080a0b);
    if (down) fill = juce::Colour(0xff0b0d0e);
    g.setColour(fill);
    g.fillRoundedRectangle(r, 9.0f);
    g.setColour(over ? juce::Colour(orange) : juce::Colour(border));
    g.drawRoundedRectangle(r, 9.0f, 1.0f);
}

void ProLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& b, bool, bool) {
    const auto id = b.getComponentID();
    if (id == "settings") {
        drawSvg(g, settingsIcon.get(), b.getLocalBounds().toFloat().reduced(1.0f));
        return;
    }
    const bool primary = id == "generate";
    const auto colour = primary ? juce::Colour(background) : juce::Colour(textPrimary);
    g.setColour(colour);
    g.setFont(font(primary ? 24.0f : 16.0f, true));
    auto textArea = b.getLocalBounds().toFloat();
    const bool icon = id == "play" || id == "variation" || id == "random" || id == "history" || id == "drag" || id == "save";
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
    else if (id == "drag") drawSvg(g, dragIcon.get(), iconArea);
    else if (id == "save") drawSvg(g, saveIcon.get(), iconArea);
}

void ProLookAndFeel::drawComboBox(juce::Graphics& g, int w, int h, bool over, int, int, int, int, juce::ComboBox&) {
    auto r = juce::Rectangle<float>(0.75f, 0.75f, (float) w - 1.5f, (float) h - 1.5f);
    g.setColour(juce::Colour(surface));
    g.fillRoundedRectangle(r, radiusControl);
    g.setColour(over ? juce::Colour(orange) : juce::Colour(border));
    g.drawRoundedRectangle(r, radiusControl, 1.0f);
    g.setColour(juce::Colour(textPrimary));
    juce::Path arrow;
    arrow.startNewSubPath((float) w - 29.0f, (float) h * 0.43f);
    arrow.lineTo((float) w - 21.0f, (float) h * 0.58f);
    arrow.lineTo((float) w - 13.0f, (float) h * 0.43f);
    g.strokePath(arrow, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void ProLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label) {
    label.setBounds(17, 1, box.getWidth() - 54, box.getHeight() - 2);
    label.setFont(getComboBoxFont(box));
    label.setJustificationType(juce::Justification::centredLeft);
}

void ProLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h, float pos,
                                      float, float, juce::Slider::SliderStyle, juce::Slider&) {
    const float trackY = (float) y + (float) h * 0.5f;
    const float left = (float) x + 1.0f;
    const float right = (float) x + (float) w - 1.0f;
    g.setColour(juce::Colour(0xff252a2d));
    g.fillRoundedRectangle(left, trackY - 3.5f, right - left, 7.0f, 3.5f);
    g.setColour(juce::Colour(orange));
    g.fillRoundedRectangle(left, trackY - 3.5f, juce::jmax(0.0f, pos - left), 7.0f, 3.5f);
    g.fillEllipse(pos - 11.5f, trackY - 11.5f, 23.0f, 23.0f);
}

void ProLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& l) {
    if (l.isBeingEdited()) return;
    g.setColour(l.findColour(juce::Label::textColourId));
    g.setFont(l.getFont());
    g.drawFittedText(l.getText(), l.getLocalBounds().reduced(2), l.getJustificationType(), 1, 0.8f);
}

void ProLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height) {
    g.fillAll(juce::Colour(surface));
    g.setColour(juce::Colour(borderQuiet));
    g.drawRect(0, 0, width, height, 1);
}

void ProLookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                                       bool separator, bool enabled, bool highlighted, bool ticked,
                                       bool hasSubMenu, const juce::String& text,
                                       const juce::String& shortcut, const juce::Drawable* icon,
                                       const juce::Colour* textColour) {
    if (separator) {
        g.setColour(juce::Colour(borderQuiet));
        g.fillRect(area.withHeight(1).withY(area.getCentreY()));
        return;
    }

    if (highlighted && enabled) {
        g.setColour(juce::Colour(orange));
        g.fillRect(area);
    }

    const auto colour = !enabled ? juce::Colour(textQuiet)
        : highlighted ? juce::Colour(background)
        : (textColour != nullptr ? *textColour : juce::Colour(textPrimary));
    g.setColour(colour);
    g.setFont(font(15.0f, true));
    auto textArea = area.reduced(12, 0);
    if (ticked) {
        g.drawText(juce::CharPointer_UTF8("✓"), textArea.removeFromLeft(20), juce::Justification::centredLeft);
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
        arrow.addTriangle((float) area.getRight() - 13.0f, (float) area.getCentreY() - 4.0f,
                          (float) area.getRight() - 13.0f, (float) area.getCentreY() + 4.0f,
                          (float) area.getRight() - 8.0f, (float) area.getCentreY());
        g.fillPath(arrow);
    }
}

juce::Font ProLookAndFeel::getComboBoxFont(juce::ComboBox&) { return font(22.0f, true); }
juce::Font ProLookAndFeel::getPopupMenuFont() { return font(16.0f); }

void ProLookAndFeel::getIdealPopupMenuItemSize(const juce::String& text, bool separator,
                                               int standardHeight, int& width, int& height) {
    if (separator) { width = 50; height = 10; return; }
    const auto popupFont = getPopupMenuFont();
    height = standardHeight > 0 ? juce::jmax(standardHeight, 34) : juce::roundToInt(popupFont.getHeight() * 1.35f);
    width = juce::GlyphArrangement::getStringWidthInt(popupFont, text) + height * 2;
}
