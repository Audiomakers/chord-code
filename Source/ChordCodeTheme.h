#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace ChordCodeTheme {
constexpr int designWidth = 1536;
constexpr int designHeight = 875;

constexpr juce::uint32 background = 0xff07080a;
constexpr juce::uint32 backgroundTop = 0xff0e1014;
constexpr juce::uint32 surface = 0xff0d0f12;
constexpr juce::uint32 surfaceRaised = 0xff15181d;
constexpr juce::uint32 surfaceHover = 0xff1a1e24;
constexpr juce::uint32 surfaceGroup = 0xff0b0d10;
constexpr juce::uint32 surfaceSelected = 0xff1a0f08;
constexpr juce::uint32 border = 0xff272b32;
constexpr juce::uint32 borderQuiet = 0xff1a1d22;
constexpr juce::uint32 borderStrong = 0xff3a404a;
constexpr juce::uint32 divider = 0xff1c1f24;
constexpr juce::uint32 orange = 0xffff5a0a;
constexpr juce::uint32 orangeBrand = 0xffff7a12;
constexpr juce::uint32 orangeLight = 0xffff8c3a;
constexpr juce::uint32 amber = 0xffffb23d;
constexpr juce::uint32 textPrimary = 0xfff5f6f8;
constexpr juce::uint32 textSecondary = 0xff8b929c;
constexpr juce::uint32 textQuiet = 0xff555c66;
constexpr juce::uint32 waveform = 0xff3b414a;

constexpr float radiusWindow = 18.0f;
constexpr float radiusControl = 10.0f;
constexpr float radiusCard = 14.0f;
constexpr float radiusPanel = 14.0f;

juce::Font font(float height, bool bold = false);
juce::Font labelFont(float height = 13.0f);
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
}
