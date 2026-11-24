// Advanced view: tabs Grains, Weather, Listen/Key, Memory/Ghosts, Character,
// Gravity, Matrix. Pages are built from parameter lists.
#pragma once
#include "Controls.h"

namespace ui
{
class ParamPage : public juce::Component
{
public:
    // knobs flow in a row on the right; choices and toggles stack on the left
    ParamPage (PluginProcessor&, std::initializer_list<int> params, juce::Colour, const juce::String& note = {});
    void resized() override;
    void paint (juce::Graphics&) override;
    void update();
protected:
    PluginProcessor& proc;
    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<juce::Component>> side;
    juce::String note;
};

class GravityPage : public juce::Component
{
public:
    explicit GravityPage (PluginProcessor&);
    void resized() override;
    void paint (juce::Graphics&) override;
    void update();
private:
    PluginProcessor& proc;
    std::vector<std::unique_ptr<Choice>> types;
    std::vector<std::unique_ptr<Knob>> knobs;
    std::unique_ptr<Knob> force;
};

class MatrixPage : public juce::Component
{
public:
    explicit MatrixPage (PluginProcessor&);
    void resized() override;
    void paint (juce::Graphics&) override;
private:
    PluginProcessor& proc;
    std::vector<std::unique_ptr<Choice>> srcs, dsts;
    std::vector<std::unique_ptr<juce::Slider>> amts;
    std::vector<std::unique_ptr<APVTS::SliderAttachment>> atts;
};

class Pages : public juce::Component
{
public:
    explicit Pages (PluginProcessor&);
    void resized() override;
    void paint (juce::Graphics&) override;
    void update();
private:
    void show (int i);
    PluginProcessor& proc;
    juce::OwnedArray<juce::TextButton> tabs;
    std::vector<std::unique_ptr<juce::Component>> pages;
    int current = 0;
};
} // namespace ui
