#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace ChordCodeTheme {
constexpr int designWidth = 1536;
constexpr int designHeight = 875;

constexpr juce::uint32 background = 0xff020304;
constexpr juce::uint32 surface = 0xff050607;
constexpr juce::uint32 surfaceGroup = 0xff060708;
constexpr juce::uint32 surfaceSelected = 0xff0e0b08;
constexpr juce::uint32 border = 0xff3d454a;
constexpr juce::uint32 borderQuiet = 0xff1f2426;
constexpr juce::uint32 divider = 0xff292e31;
constexpr juce::uint32 orange = 0xffff5706;
constexpr juce::uint32 orangeBrand = 0xffff7a12;
constexpr juce::uint32 textPrimary = 0xfff7f7f7;
constexpr juce::uint32 textSecondary = 0xff80878c;
constexpr juce::uint32 textQuiet = 0xff52595e;
constexpr juce::uint32 waveform = 0xff575c5e;

constexpr float radiusWindow = 18.0f;
constexpr float radiusControl = 8.0f;
constexpr float radiusCard = 10.0f;

juce::Font font(float height, bool bold = false);
void drawPlayIcon(juce::Graphics&, juce::Rectangle<float>, juce::Colour);
void drawVariationIcon(juce::Graphics&, juce::Rectangle<float>, juce::Colour);
void drawLockIcon(juce::Graphics&, juce::Rectangle<float>, juce::Colour, bool locked);
void drawRandomIcon(juce::Graphics&, juce::Rectangle<float>, juce::Colour);
void drawHistoryIcon(juce::Graphics&, juce::Rectangle<float>, juce::Colour);
void drawMidiIcon(juce::Graphics&, juce::Rectangle<float>, juce::Colour);
void drawSaveIcon(juce::Graphics&, juce::Rectangle<float>, juce::Colour);
void drawSettingsIcon(juce::Graphics&, juce::Rectangle<float>, juce::Colour);
}
