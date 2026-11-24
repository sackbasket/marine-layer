#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/Engine.h"

class PluginProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // presets, dice, undo
    void loadFactoryPreset (int index);
    bool loadPresetFile (const juce::File&);
    bool savePresetFile (const juce::File&, const juce::String& name);
    static juce::File userPresetFolder();
    void rollDice (bool wild);
    bool undo();
    bool canUndo() const { return ! undoStack.empty(); }
    void pushUndo();

    // snapshots (6 pads)
    static constexpr int numSnapshots = 6;
    bool snapshotSet (int i) const { return snaps[(size_t) i].set; }
    void saveSnapshot (int i);
    void recallSnapshot (int i);
    void clearSnapshot (int i) { snaps[(size_t) i].set = false; }
    std::atomic<int> activeSnapshot { -1 };
    float morphProgress() const { return morphing ? (float) morphT : 1.0f; }

    // emitters (also stored in the set)
    int addEmitter (float x, float y);
    void moveEmitter (int i, float x, float y);
    void removeEmitter (int i);

    juce::AudioProcessorValueTreeState apvts;
    ml::Engine engine;
    std::atomic<float> uiScale { 1.0f };
    std::atomic<bool> advancedView { false };
    juce::String presetName { "Marine Layer" };
    juce::StringArray macroNames;
    std::atomic<int> presetVersion { 0 };
    int currentProgram = 0;
    std::atomic<float> modNorm[ml::numParams] {};   // live modulation per parameter (0..1 of its range)

    float value (int p) const { return params[(size_t) p]->load(); }
    juce::RangedAudioParameter* param (int p) const { return paramObjs[(size_t) p]; }
    void setParam (int p, float plainValue);
    void setParamGesture (int p, float plainValue);   // begin/end change gesture around it
    void applyValues (const float* plain, bool includeFreeze);

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void timerCallback() override;
    juce::ValueTree makeStateTree (bool forPreset, const juce::String& name);
    void applyStateTree (const juce::ValueTree&, bool fromPreset);
    void computeModulation();
    static bool snapshotSkips (int p);

    std::vector<std::atomic<float>*> params;
    std::vector<juce::RangedAudioParameter*> paramObjs;
    std::vector<juce::ValueTree> undoStack;
    float base[ml::numParams] {}, eff[ml::numParams] {};
    juce::AudioBuffer<float> monoTmp;

    struct Snap { bool set = false; std::vector<float> v; bool em[4] {}; float ex[4] {}, ey[4] {}; };
    std::array<Snap, numSnapshots> snaps;
    bool morphing = false; double morphT = 0, morphDur = 0; std::vector<float> morphFrom, morphTo;
    juce::uint32 morphLastMs = 0;
    std::atomic<double> lastBpm { 120.0 }, lastBarBeats { 4.0 };
    int diceCount = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
