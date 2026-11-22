#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class ProLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    ProLookAndFeel();
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool, bool) override;
    void drawComboBox(juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;
    void drawLinearSlider(juce::Graphics&, int, int, int, int, float, float, float,
                          juce::Slider::SliderStyle, juce::Slider&) override;
    void drawLabel(juce::Graphics&, juce::Label&) override;
    void drawPopupMenuBackground(juce::Graphics&, int, int) override;
    void drawPopupMenuItem(juce::Graphics&, const juce::Rectangle<int>&, bool, bool, bool,
                           bool, bool, const juce::String&, const juce::String&,
                           const juce::Drawable*, const juce::Colour*) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::PopupMenu::Options getOptionsForComboBoxPopupMenu(juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override;
    void getIdealPopupMenuItemSize(const juce::String&, bool, int, int&, int&) override;
    int getPopupMenuBorderSize() override { return 1; }
    juce::Rectangle<int> getTooltipBounds(const juce::String&, juce::Point<int>, juce::Rectangle<int>) override;
    void drawTooltip(juce::Graphics&, const juce::String&, int, int) override;
};
