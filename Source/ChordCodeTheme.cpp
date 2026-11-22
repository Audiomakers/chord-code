#include "ChordCodeTheme.h"
#include "EmbeddedFonts.h"
#include <cmath>

namespace ChordCodeTheme {
namespace {
juce::Typeface::Ptr regularTypeface() {
    static auto face = juce::Typeface::createSystemTypefaceFor(EmbeddedFonts::interRegular,
                                                                EmbeddedFonts::interRegularSize);
    return face;
}

juce::Typeface::Ptr boldTypeface() {
    static auto face = juce::Typeface::createSystemTypefaceFor(EmbeddedFonts::interBold,
                                                                EmbeddedFonts::interBoldSize);
    return face;
}
}

juce::Font font(float height, bool bold) {
    return juce::Font(juce::FontOptions(bold ? boldTypeface() : regularTypeface()).withHeight(height));
}

juce::Font semi(float height) {
    static auto face = juce::Typeface::createSystemTypefaceFor(EmbeddedFonts::interSemiBold, EmbeddedFonts::interSemiBoldSize);
    return juce::Font(juce::FontOptions(face).withHeight(height));
}

juce::Font labelFont(float height) {
    return semi(height).withExtraKerningFactor(AMUI::kerningLabel);
}

juce::Font buttonFont(float height) {
    return semi(height).withExtraKerningFactor(AMUI::kerningButton);
}

juce::ColourGradient accentGradient(juce::Rectangle<float> r) {
    juce::ColourGradient gradient(juce::Colour(orange), r.getX(), r.getBottom(),
                                  juce::Colour(orangeLight), r.getRight(), r.getY(), false);
    gradient.addColour(0.85, juce::Colour(amber).interpolatedWith(juce::Colour(orangeLight), 0.55f));
    return gradient;
}

juce::ColourGradient raisedGradient(juce::Rectangle<float> r, juce::Colour top, juce::Colour bottom) {
    return juce::ColourGradient(top, 0.0f, r.getY(), bottom, 0.0f, r.getBottom(), false);
}

void drawGlow(juce::Graphics& g, const juce::Path& shape, juce::Colour colour, int radius) {
    juce::DropShadow(colour, radius, {}).drawForPath(g, shape);
}

void drawRaisedPanel(juce::Graphics& g, juce::Rectangle<float> r, float radius, juce::Colour outline) {
    g.setGradientFill(raisedGradient(r, juce::Colour(surfaceRaised), juce::Colour(surface)));
    g.fillRoundedRectangle(r, radius);
    // A faint top highlight gives panels a lifted, glassy edge.
    g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.07f), 0.0f, r.getY(),
                                           juce::Colours::transparentWhite, 0.0f, r.getY() + r.getHeight() * 0.5f, false));
    g.drawRoundedRectangle(r.reduced(0.5f), radius, 1.0f);
    g.setColour(outline);
    g.drawRoundedRectangle(r, radius, 1.0f);
}

void drawPlayIcon(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c) {
    juce::Path p;
    p.addTriangle(r.getX() + r.getWidth() * 0.31f, r.getY() + r.getHeight() * 0.20f,
                  r.getX() + r.getWidth() * 0.31f, r.getBottom() - r.getHeight() * 0.20f,
                  r.getRight() - r.getWidth() * 0.22f, r.getCentreY());
    g.setColour(c);
    g.fillPath(p);
}

void drawVariationIcon(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c) {
    const auto centre = r.getCentre();
    const float radius = juce::jmin(r.getWidth(), r.getHeight()) * 0.34f;
    g.setColour(c);
    juce::Path upper;
    upper.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, -2.35f, 0.72f, true);
    g.strokePath(upper, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::Path lower;
    lower.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, 0.80f, 3.85f, true);
    g.strokePath(lower, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::Path arrows;
    arrows.addTriangle(centre.x + radius * 0.84f, centre.y - radius * 0.64f,
                       centre.x + radius * 1.22f, centre.y - radius * 0.02f,
                       centre.x + radius * 0.48f, centre.y - radius * 0.04f);
    arrows.addTriangle(centre.x - radius * 0.84f, centre.y + radius * 0.64f,
                       centre.x - radius * 1.22f, centre.y + radius * 0.02f,
                       centre.x - radius * 0.48f, centre.y + radius * 0.04f);
    g.fillPath(arrows);
}

void drawLockIcon(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, bool locked) {
    g.setColour(c);
    auto body = juce::Rectangle<float>(r.getX() + r.getWidth() * 0.19f, r.getY() + r.getHeight() * 0.45f,
                                       r.getWidth() * 0.62f, r.getHeight() * 0.43f);
    g.drawRoundedRectangle(body, 2.2f, 1.8f);
    juce::Path shackle;
    shackle.startNewSubPath(r.getX() + r.getWidth() * 0.34f, body.getY());
    shackle.lineTo(r.getX() + r.getWidth() * 0.34f, r.getY() + r.getHeight() * 0.34f);
    shackle.cubicTo(r.getX() + r.getWidth() * 0.34f, r.getY() + r.getHeight() * 0.12f,
                    r.getX() + r.getWidth() * 0.66f, r.getY() + r.getHeight() * 0.12f,
                    r.getX() + r.getWidth() * 0.66f, r.getY() + r.getHeight() * 0.34f);
    shackle.lineTo(r.getX() + r.getWidth() * 0.66f, body.getY());
    g.strokePath(shackle, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (locked)
        g.fillEllipse(r.getCentreX() - 1.6f, body.getCentreY() - 1.6f, 3.2f, 5.0f);
}

void drawRandomIcon(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c) {
    g.setColour(c);
    juce::Path top, bottom;
    top.startNewSubPath(r.getX() + 1.0f, r.getY() + r.getHeight() * 0.28f);
    top.cubicTo(r.getCentreX() - 2.0f, r.getY() + r.getHeight() * 0.28f,
                r.getCentreX() + 1.0f, r.getBottom() - r.getHeight() * 0.28f,
                r.getRight() - 4.0f, r.getBottom() - r.getHeight() * 0.28f);
    bottom.startNewSubPath(r.getX() + 1.0f, r.getBottom() - r.getHeight() * 0.28f);
    bottom.cubicTo(r.getCentreX() - 2.0f, r.getBottom() - r.getHeight() * 0.28f,
                   r.getCentreX() + 1.0f, r.getY() + r.getHeight() * 0.28f,
                   r.getRight() - 4.0f, r.getY() + r.getHeight() * 0.28f);
    g.strokePath(top, juce::PathStrokeType(1.8f));
    g.strokePath(bottom, juce::PathStrokeType(1.8f));
    juce::Path arrows;
    arrows.addTriangle(r.getRight() - 5.0f, r.getY() + 1.0f,
                       r.getRight(), r.getY() + r.getHeight() * 0.28f,
                       r.getRight() - 5.0f, r.getY() + r.getHeight() * 0.50f);
    arrows.addTriangle(r.getRight() - 5.0f, r.getBottom() - 1.0f,
                       r.getRight(), r.getBottom() - r.getHeight() * 0.28f,
                       r.getRight() - 5.0f, r.getBottom() - r.getHeight() * 0.50f);
    g.fillPath(arrows);
}

void drawHistoryIcon(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c) {
    g.setColour(c);
    g.drawEllipse(r.reduced(2.0f), 1.7f);
    g.drawLine(r.getCentreX(), r.getY() + 5.0f, r.getCentreX(), r.getCentreY(), 1.7f);
    g.drawLine(r.getCentreX(), r.getCentreY(), r.getRight() - 5.0f, r.getCentreY() + 3.0f, 1.7f);
}

void drawMidiIcon(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c) {
    g.setColour(c);
    g.drawRoundedRectangle(r.reduced(1.0f), 2.0f, 1.6f);
    const float keyY = r.getY() + r.getHeight() * 0.57f;
    for (int i = 1; i < 5; ++i)
        g.drawVerticalLine((int) std::round(r.getX() + r.getWidth() * (float) i / 5.0f), keyY, r.getBottom() - 1.0f);
    for (float x : { 0.29f, 0.49f, 0.69f })
        g.fillRect(r.getX() + r.getWidth() * x, keyY, 2.5f, r.getHeight() * 0.22f);
}

void drawSaveIcon(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c) {
    g.setColour(c);
    const float x = r.getCentreX();
    g.drawLine(x, r.getY() + 1.0f, x, r.getBottom() - 6.0f, 1.8f);
    g.drawLine(x, r.getBottom() - 6.0f, x - 5.0f, r.getBottom() - 11.0f, 1.8f);
    g.drawLine(x, r.getBottom() - 6.0f, x + 5.0f, r.getBottom() - 11.0f, 1.8f);
    g.drawLine(r.getX() + 2.0f, r.getBottom() - 1.0f, r.getRight() - 2.0f, r.getBottom() - 1.0f, 1.8f);
}

void drawSettingsIcon(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c) {
    g.setColour(c);
    const auto centre = r.getCentre();
    const float outer = juce::jmin(r.getWidth(), r.getHeight()) * 0.38f;
    juce::Path gear;
    for (int i = 0; i < 16; ++i) {
        const float a = juce::MathConstants<float>::twoPi * (float) i / 16.0f;
        const float radius = (i % 2 == 0) ? outer : outer * 0.77f;
        const auto point = centre + juce::Point<float>(std::cos(a), std::sin(a)) * radius;
        if (i == 0) gear.startNewSubPath(point); else gear.lineTo(point);
    }
    gear.closeSubPath();
    g.strokePath(gear, juce::PathStrokeType(1.6f));
    g.drawEllipse(juce::Rectangle<float>(7.0f, 7.0f).withCentre(centre), 1.6f);
}
}
