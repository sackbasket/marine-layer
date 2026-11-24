#pragma once
#include "PluginProcessor.h"
#include "ui/Theme.h"
#include "ui/Controls.h"
#include "ui/Scene.h"
#include "ui/Pages.h"

class PluginEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick() { timerCallback(); }   // used by Tests/Snapshot.cpp
    void setAdvanced (bool);

    static constexpr int baseW = 1100, baseH = 720;

private:
    void timerCallback() override;
    void applyScale (float);
    void showPresetMenu();
    void stepPreset (int delta);
    void savePreset();
    void renameMacro (int i);
    void refreshMacroLabels();

    PluginProcessor& proc;
    ui::LookAndFeel lnf;
    juce::Component content;
    ui::Scene scene;
    ui::Timeline timeline;
    ui::SnapshotPads snaps;
    ui::Choice morphBox;
    ui::HandLooper looper;
    ui::Choice loopBox;
    ui::FreezeButton freezeBtn;
    ui::ForecastSlider forecastSl;
    std::vector<std::unique_ptr<ui::Knob>> macros;
    std::unique_ptr<ui::Knob> mixKnob, outKnob;
    ui::Pages pages;

    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, presetBtn, diceBtn { "DICE" }, undoBtn { "UNDO" }, saveBtn { "SAVE" };
    juce::TextButton simpleBtn { "SIMPLE" }, advBtn { "ADVANCED" };
    juce::ComboBox sizeBox;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::TooltipWindow tooltips { this, 700 };
    int seenPresetVersion = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
