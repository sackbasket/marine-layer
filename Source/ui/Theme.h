// Marine Layer look (shared dusk palette with LA): deep violet night, sunset accents,
// sepia for old memories, cyan for frost / wind.
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>

namespace ui
{
namespace col
{
    const juce::Colour bg      { 0xff0c0a12 };
    const juce::Colour panel   { 0xff14111c };
    const juce::Colour panelHi { 0xff1c1827 };
    const juce::Colour line    { 0xff2a2438 };
    const juce::Colour text    { 0xffd8d2e4 };
    const juce::Colour dim     { 0xff7b7292 };
    const juce::Colour bright  { 0xfff7f3ff };
    const juce::Colour sun     { 0xffff8a3d };   // sunset orange (main accent)
    const juce::Colour pink    { 0xffff4d8d };
    const juce::Colour violet  { 0xff9b6bff };
    const juce::Colour cyan    { 0xff3fd6e0 };
    const juce::Colour gold    { 0xffffc857 };
    const juce::Colour smog    { 0xffb59a7a };   // dusty brown-grey
    const juce::Colour sepia   { 0xffc8a27a };   // old memory
    const juce::Colour fog     { 0xffcfc6e6 };   // particles
    const juce::Colour accent  = sun;
    const juce::Colour accent2 = pink;
    const juce::Colour warn    { 0xffff4a4a };
    inline juce::Colour body (int i) { const juce::Colour c[] = { sun, pink, cyan, gold }; return c[i & 3]; }
}

juce::Font font (float height, bool bold = false);
inline juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float min, float max, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return font (12.0f); }
    juce::Font getPopupMenuFont() override { return font (13.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return font (12.0f, true); }
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
};
} // namespace ui
