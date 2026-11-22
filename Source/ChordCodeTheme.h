#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "AMUI.h"

namespace ChordCodeTheme {
constexpr int designWidth = 1536;
constexpr int designHeight = 875;

constexpr juce::uint32 background = 0xff0b0d11;
constexpr juce::uint32 backgroundTop = 0xff0e1116;
constexpr juce::uint32 surface = 0xff12151b;
constexpr juce::uint32 surfaceRaised = 0xff181c24;
constexpr juce::uint32 surfaceHover = 0xff1f242e;
constexpr juce::uint32 surfaceGroup = 0xff0e1015;
constexpr juce::uint32 surfaceSelected = 0xff1a0f08;
constexpr juce::uint32 border = 0xff262b36;
constexpr juce::uint32 borderQuiet = 0xff1c2028;
constexpr juce::uint32 borderStrong = 0xff343a47;
constexpr juce::uint32 divider = 0xff1f232b;
constexpr juce::uint32 orange = 0xffff5a0a;
constexpr juce::uint32 orangeBrand = 0xffff7a12;
constexpr juce::uint32 orangeLight = 0xffff8c3a;
constexpr juce::uint32 amber = 0xffffb23d;
constexpr juce::uint32 updateYellow = 0xffffd23a;
constexpr juce::uint32 textPrimary = 0xfff2f4f8;
constexpr juce::uint32 textSecondary = 0xff9aa3b2;
constexpr juce::uint32 textQuiet = 0xff5f6878;
constexpr juce::uint32 waveform = 0xff343a47;

constexpr float radiusWindow = 18.0f;
constexpr float radiusControl = 13.0f;
constexpr float radiusCard = 18.0f;
constexpr float radiusPanel = 18.0f;

juce::Font font(float height, bool bold = false);
juce::Font labelFont(float height = 13.0f);     // upper-case overline label (SemiBold, +8%)
juce::Font semi(float height);                   // SemiBold: control values, buttons, chips
juce::Font buttonFont(float height = 17.5f);     // upper-case button text (SemiBold, +4%)
// Audio Makers UI sizes are set in real pixels for Melody Code; Chord Code's design canvas (1536 wide)
// opens at about 73%, so its numbers are the shared ones times this factor.
constexpr float designScale = 1.37f;
juce::ColourGradient accentGradient(juce::Rectangle<float>);
juce::ColourGradient raisedGradient(juce::Rectangle<float>, juce::Colour top, juce::Colour bottom);
void drawGlow(juce::Graphics&, const juce::Path&, juce::Colour, int radius);
void drawRaisedPanel(juce::Graphics&, juce::Rectangle<float>, float radius, juce::Colour outline);
void drawPlayIcon(juce::Graphics&, juce::Rectangle<float>, juce::Colour);
void drawVariationIcon(juce::Graphics&, juce::Rectangle<float>, juce::Colour);
void drawLockIcon(juce::Graphics&, juce::Rectangle<float>, juce::Colour, bool locked);
void drawRandomIcon(juce::Graphics&, juce::Rectangle<float>, juce::Colour);
void drawHistoryIcon(juce::Graphics&, juce::Rectangle<float>, juce::Colour);
void drawMidiIcon(juce::Graphics&, juce::Rectangle<float>, juce::Colour);
void drawSaveIcon(juce::Graphics&, juce::Rectangle<float>, juce::Colour);
void drawSettingsIcon(juce::Graphics&, juce::Rectangle<float>, juce::Colour);

// Pulsing yellow dot that marks an available update (settings button and its panel row).
inline void drawUpdateDot(juce::Graphics& g, juce::Point<float> centre) {
    const float pulse = 0.5f + 0.5f * std::sin((float) juce::Time::getMillisecondCounter() * 0.006f);
    const auto yellow = juce::Colour(updateYellow);
    g.setColour(yellow.withAlpha(0.12f + 0.22f * pulse));
    g.fillEllipse(juce::Rectangle<float>(20.0f, 20.0f).withCentre(centre));
    g.setColour(juce::Colour(background));
    g.fillEllipse(juce::Rectangle<float>(13.0f, 13.0f).withCentre(centre));
    g.setColour(yellow.withAlpha(0.45f + 0.55f * pulse));
    g.fillEllipse(juce::Rectangle<float>(9.0f, 9.0f).withCentre(centre));
}
}
