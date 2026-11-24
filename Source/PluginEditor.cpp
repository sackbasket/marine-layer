#include "PluginEditor.h"
#include "dsp/Presets.h"

using namespace ml;

namespace
{
const float kScales[] = { 0.8f, 0.9f, 1.0f, 1.15f, 1.3f };
}

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p), proc (p), scene (p), timeline (p), snaps (p), morphBox (p, morph, "Morph"),
      looper (p), loopBox (p, loopLen), freezeBtn (p), forecastSl (p), pages (p)
{
    setLookAndFeel (&lnf);
    setWantsKeyboardFocus (false);
    addAndMakeVisible (content);
    for (auto* c : std::initializer_list<juce::Component*> { &scene, &timeline, &snaps, &morphBox, &looper, &loopBox, &freezeBtn, &forecastSl,
                                                             &prevBtn, &nextBtn, &presetBtn, &diceBtn, &undoBtn, &saveBtn, &simpleBtn, &advBtn, &sizeBox })
        content.addAndMakeVisible (c);
    content.addChildComponent (pages);

    const juce::Colour mc[] = { ui::col::fog, ui::col::cyan, ui::col::pink, ui::col::sepia, ui::col::violet, ui::col::gold };
    for (int i = 0; i < numMacros; ++i)
    {
        macros.push_back (std::make_unique<ui::Knob> (proc, m1 + i, proc.macroNames[i], mc[i]));
        macros.back()->setLabelFont (11.5f);
        macros.back()->onRename = [this, i] { renameMacro (i); };
        macros.back()->setTooltip ("Macro " + juce::String (i + 1) + " (right-click to rename; routing in Advanced > Matrix)");
        content.addAndMakeVisible (*macros.back());
    }
    mixKnob = std::make_unique<ui::Knob> (proc, mix, "Mix", ui::col::sun);
    outKnob = std::make_unique<ui::Knob> (proc, output, "Output", ui::col::dim.brighter (0.4f));
    mixKnob->setLabelFont (11.5f); outKnob->setLabelFont (11.5f);
    content.addAndMakeVisible (*mixKnob); content.addAndMakeVisible (*outKnob);

    prevBtn.onClick = [this] { stepPreset (-1); };
    nextBtn.onClick = [this] { stepPreset (1); };
    presetBtn.onClick = [this] { showPresetMenu(); };
    presetBtn.setTooltip ("Presets");
    diceBtn.setTooltip ("Roll a new sound (Alt-click = wild)");
    diceBtn.onClick = [this] { proc.rollDice (juce::ModifierKeys::currentModifiers.isAltDown()); };
    undoBtn.onClick = [this] { proc.undo(); };
    saveBtn.onClick = [this] { savePreset(); };
    simpleBtn.onClick = [this] { setAdvanced (false); };
    advBtn.onClick = [this] { setAdvanced (true); };

    for (int i = 0; i < 5; ++i) sizeBox.addItem ("Size " + juce::String (juce::roundToInt (kScales[i] * 100)) + "%", i + 1);
    int sel = 3;
    for (int i = 0; i < 5; ++i) if (std::abs (kScales[i] - proc.uiScale.load()) < 0.01f) sel = i + 1;
    sizeBox.setSelectedId (sel, juce::dontSendNotification);
    sizeBox.onChange = [this] { applyScale (kScales[sizeBox.getSelectedId() - 1]); };

    setAdvanced (proc.advancedView.load());
    applyScale (proc.uiScale.load());
    startTimerHz (60);
}

PluginEditor::~PluginEditor() { setLookAndFeel (nullptr); }

void PluginEditor::applyScale (float s)
{
    proc.uiScale.store (s);
    content.setTransform (juce::AffineTransform::scale (s));
    setSize ((int) std::round (baseW * s), (int) std::round (baseH * s));
}

void PluginEditor::setAdvanced (bool a)
{
    proc.advancedView = a;
    simpleBtn.setToggleState (! a, juce::dontSendNotification);
    advBtn.setToggleState (a, juce::dontSendNotification);
    pages.setVisible (a);
    for (auto& m : macros) m->setVisible (! a);
    mixKnob->setVisible (true); outKnob->setVisible (true);
    resized();
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (ui::col::bg);
    g.addTransform (juce::AffineTransform::scale (proc.uiScale.load()));
    g.setColour (ui::col::bright);
    g.setFont (ui::font (22.0f, true));
    const juce::String name (PLUGIN_DISPLAY_NAME);
    g.drawText (name, juce::Rectangle<int> (18, 0, 300, 48), juce::Justification::centredLeft);
    const int nameW = juce::GlyphArrangement::getStringWidthInt (ui::font (22.0f, true), name);
    g.setColour (ui::col::dim);
    g.setFont (ui::font (12.0f));
    g.drawText (PLUGIN_TAGLINE, juce::Rectangle<int> (18 + nameW + 12, 0, 200, 48), juce::Justification::centredLeft);
    // sunset stripe
    juce::ColourGradient grad (ui::col::sun, 18, 0, ui::col::violet, 18 + (float) nameW, 0, false);
    grad.addColour (0.5, ui::col::pink);
    g.setGradientFill (grad);
    g.fillRect (18, 36, nameW, 2);
}

void PluginEditor::resized()
{
    content.setBounds (0, 0, baseW, baseH);
    auto r = juce::Rectangle<int> (0, 0, baseW, baseH).reduced (16, 0);
    const bool adv = proc.advancedView.load();

    // header
    auto header = r.removeFromTop (48).withSizeKeepingCentre (r.getWidth(), 28);
    sizeBox.setBounds (header.removeFromRight (104));
    header.removeFromRight (10);
    advBtn.setBounds (header.removeFromRight (84));
    simpleBtn.setBounds (header.removeFromRight (70));
    auto pb = header.withSizeKeepingCentre (430, 28).translated (110, 0);
    prevBtn.setBounds (pb.removeFromLeft (28)); pb.removeFromLeft (4);
    saveBtn.setBounds (pb.removeFromRight (54)); pb.removeFromRight (4);
    undoBtn.setBounds (pb.removeFromRight (56)); pb.removeFromRight (4);
    diceBtn.setBounds (pb.removeFromRight (54)); pb.removeFromRight (4);
    nextBtn.setBounds (pb.removeFromRight (28)); pb.removeFromRight (4);
    presetBtn.setBounds (pb);

    // bottom up
    r.removeFromBottom (12);
    auto bottom = r.removeFromBottom (adv ? 214 : 112);
    r.removeFromBottom (6);
    forecastSl.setBounds (r.removeFromBottom (44));
    r.removeFromBottom (6);
    auto perf = r.removeFromBottom (40);
    r.removeFromBottom (6);
    timeline.setBounds (r.removeFromBottom (20));
    r.removeFromBottom (4);
    scene.setBounds (r);

    freezeBtn.setBounds (perf.removeFromRight (180));
    perf.removeFromRight (16);
    loopBox.setBounds (perf.removeFromRight (86).reduced (0, 6));
    perf.removeFromRight (6);
    looper.setBounds (perf.removeFromRight (200).reduced (0, 4));
    perf.removeFromRight (20);
    morphBox.setBounds (perf.removeFromRight (180).reduced (0, 6));
    perf.removeFromRight (12);
    snaps.setBounds (perf.reduced (0, 3));

    if (adv)
    {
        auto right = bottom.removeFromRight (96);
        mixKnob->setBounds (right.removeFromTop (right.getHeight() / 2));
        outKnob->setBounds (right);
        bottom.removeFromRight (8);
        pages.setBounds (bottom);
    }
    else
    {
        auto right = bottom.removeFromRight (200);
        mixKnob->setBounds (right.removeFromLeft (100));
        outKnob->setBounds (right);
        bottom.removeFromRight (20);
        const int kw = bottom.getWidth() / numMacros;
        for (auto& m : macros) m->setBounds (bottom.removeFromLeft (kw).reduced (6, 0));
    }
}

void PluginEditor::timerCallback()
{
    scene.update();
    timeline.update();
    snaps.repaint();
    looper.repaint();
    freezeBtn.repaint();
    forecastSl.repaint();
    for (auto& m : macros) m->update();
    mixKnob->update(); outKnob->update();
    if (proc.advancedView.load()) pages.update();
    undoBtn.setEnabled (proc.canUndo());
    if (seenPresetVersion != proc.presetVersion.load())
    {
        seenPresetVersion = proc.presetVersion.load();
        presetBtn.setButtonText (proc.presetName);
        refreshMacroLabels();
        if (proc.advancedView.load() != advBtn.getToggleState()) setAdvanced (proc.advancedView.load());
    }
}

void PluginEditor::refreshMacroLabels()
{
    for (int i = 0; i < numMacros; ++i) macros[(size_t) i]->setLabel (proc.macroNames[i]);
}

void PluginEditor::stepPreset (int d)
{
    const int n = (int) factoryPresets().size();
    proc.loadFactoryPreset (((proc.currentProgram + d) % n + n) % n);
}

void PluginEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    const auto& ps = factoryPresets();
    juce::StringArray cats;
    for (auto& p : ps) cats.addIfNotAlreadyThere (p.category);
    for (auto& c : cats)
    {
        juce::PopupMenu sub;
        for (int i = 0; i < (int) ps.size(); ++i)
            if (c == ps[(size_t) i].category) sub.addItem (i + 1, ps[(size_t) i].name, true, i == proc.currentProgram && proc.presetName == ps[(size_t) i].name);
        menu.addSubMenu (c, sub);
    }
    auto files = PluginProcessor::userPresetFolder().findChildFiles (juce::File::findFiles, false, "*.mlpreset");
    files.sort();
    juce::PopupMenu user;
    for (int i = 0; i < files.size(); ++i) user.addItem (1000 + i, files[i].getFileNameWithoutExtension());
    if (files.isEmpty()) user.addItem (-1, "(none yet: use SAVE)", false);
    menu.addSeparator();
    menu.addSubMenu ("User", user);
    menu.addItem (900, "Open preset folder");
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (presetBtn), [this, files] (int r)
    {
        if (r >= 1 && r <= (int) factoryPresets().size()) proc.loadFactoryPreset (r - 1);
        else if (r >= 1000 && r - 1000 < files.size()) proc.loadPresetFile (files[r - 1000]);
        else if (r == 900) PluginProcessor::userPresetFolder().startAsProcess();
    });
}

void PluginEditor::savePreset()
{
    chooser = std::make_unique<juce::FileChooser> ("Save preset", PluginProcessor::userPresetFolder().getChildFile (proc.presetName + ".mlpreset"), "*.mlpreset");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (f == juce::File()) return;
        if (! f.hasFileExtension ("mlpreset")) f = f.withFileExtension ("mlpreset");
        proc.savePresetFile (f, f.getFileNameWithoutExtension());
    });
}

void PluginEditor::renameMacro (int i)
{
    auto* ed = new juce::TextEditor();
    ed->setText (proc.macroNames[i]);
    ed->setSize (140, 24);
    ed->setFont (ui::font (13.0f));
    ed->selectAll();
    auto* k = macros[(size_t) i].get();
    ed->onReturnKey = [this, ed, i]
    {
        proc.macroNames.set (i, ed->getText().substring (0, 14));
        refreshMacroLabels();
        if (auto* box = ed->findParentComponentOfClass<juce::CallOutBox>()) box->dismiss();
    };
    juce::CallOutBox::launchAsynchronously (std::unique_ptr<juce::Component> (ed), k->getScreenBounds(), nullptr);
    ed->grabKeyboardFocus();
}
