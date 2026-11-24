// Controls attached to parameters by index (ml::P): knob, choice, toggle,
// panel, plus the performance widgets (Forecast slider, Freeze, snapshot pads,
// hand looper, ghost timeline).
#pragma once
#include "Theme.h"
#include "../PluginProcessor.h"

namespace ui
{
using APVTS = juce::AudioProcessorValueTreeState;

class Knob : public juce::Component, public juce::SettableTooltipClient
{
public:
    Knob (PluginProcessor&, int param, const juce::String& label, juce::Colour colour);
    void resized() override;
    void paint (juce::Graphics&) override;
    void update();
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    void mouseDown (const juce::MouseEvent&) override;
    void setLabel (const juce::String& l) { label = l; repaint(); }
    void setLabelFont (float h) { labelH = h; }
    std::function<void()> onRename;   // right-click "Rename" (macros)
    juce::Slider slider;
    int param;
private:
    PluginProcessor& proc;
    juce::String label;
    float labelH = 10.5f, lastMod = 0;
    std::unique_ptr<APVTS::SliderAttachment> att;
};

class Choice : public juce::Component
{
public:
    Choice (PluginProcessor&, int param, const juce::String& caption = {});
    void resized() override;
    void paint (juce::Graphics&) override;
    juce::ComboBox box;
private:
    juce::String caption;
    std::unique_ptr<APVTS::ComboBoxAttachment> att;
};

class Toggle : public juce::Component
{
public:
    Toggle (PluginProcessor&, int param, const juce::String& text, juce::Colour colour);
    void resized() override { button.setBounds (getLocalBounds()); }
    juce::ToggleButton button;
private:
    std::unique_ptr<APVTS::ButtonAttachment> att;
};

class Panel : public juce::Component
{
public:
    Panel (const juce::String& t, juce::Colour c) : title (t), colour (c) { setInterceptsMouseClicks (false, true); }
    void paint (juce::Graphics&) override;
    juce::Rectangle<int> content() const { return getLocalBounds().reduced (10, 6).withTrimmedTop (20); }
    juce::String title;
    juce::Colour colour;
};

// big Clear -> Storm slider
class ForecastSlider : public juce::Component
{
public:
    explicit ForecastSlider (PluginProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override { slider.setBounds (getLocalBounds()); }
    juce::Slider slider;
private:
    PluginProcessor& proc;
    std::unique_ptr<APVTS::SliderAttachment> att;
};

// click = latch, hold = momentary
class FreezeButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit FreezeButton (PluginProcessor& p) : proc (p) { setTooltip ("Freeze the memory. Click = latch, hold = momentary (Space in the scene)."); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    PluginProcessor& proc;
    juce::uint32 downAt = 0;
};

class SnapshotPads : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit SnapshotPads (PluginProcessor& p) : proc (p)
    { setTooltip ("Snapshots: click an empty pad to save, click a saved pad to morph to it. Shift-click = save over, Alt-click = clear."); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    juce::Rectangle<float> pad (int i) const;
    PluginProcessor& proc;
};

class HandLooper : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit HandLooper (PluginProcessor& p) : proc (p)
    { setTooltip ("Records your hand in the fog for 1-8 bars and loops it. Touch the fog while it loops to overdub."); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    juce::Rectangle<float> recArea() const { return getLocalBounds().toFloat().withWidth (getWidth() * 0.62f).reduced (1); }
    juce::Rectangle<float> clearArea() const { return getLocalBounds().toFloat().withTrimmedLeft (getWidth() * 0.64f).reduced (1); }
    PluginProcessor& proc;
};

class Timeline : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit Timeline (PluginProcessor& p) : proc (p)
    { setTooltip ("Song bars (needs Ableton's transport). Click a bar to bookmark it; Ghosts bring bookmarked moments back later."); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void update();
private:
    int firstBar() const;
    PluginProcessor& proc;
    int lastFlash = 0; float flash = 0;
    static constexpr int visibleBars = 64;
};

template <typename T>
inline void row (juce::Rectangle<int> r, std::initializer_list<T*> items, int gap = 4)
{
    const int n = (int) items.size();
    if (n == 0) return;
    const int w = (r.getWidth() - gap * (n - 1)) / n;
    for (auto* c : items) { if (c) c->setBounds (r.removeFromLeft (w)); r.removeFromLeft (gap); }
}
} // namespace ui
