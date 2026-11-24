#include "Controls.h"

namespace ui
{
using namespace ml;

// ---------------------------------------------------------------------------- Knob
Knob::Knob (PluginProcessor& p, int prm, const juce::String& l, juce::Colour c)
    : param (prm), proc (p), label (l)
{
    const auto& info = paramTable()[prm];
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setColour (juce::Slider::rotarySliderFillColourId, c);
    slider.getProperties().set ("bipolar", info.bipolar);
    slider.getProperties().set ("hasMod", true);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.22f, juce::MathConstants<float>::pi * 2.78f, true);
    slider.setMouseDragSensitivity (200);
    slider.setPopupDisplayEnabled (false, false, nullptr);
    addAndMakeVisible (slider);
    slider.addMouseListener (this, false);
    att = std::make_unique<APVTS::SliderAttachment> (proc.apvts, info.id, slider);
    if (auto* po = proc.param (prm)) slider.setDoubleClickReturnValue (true, po->convertFrom0to1 (po->getDefaultValue()));
    slider.onValueChange = [this] { repaint(); };
    slider.onDragEnd = [this] { repaint(); };
    setTooltip (info.name);
    slider.setTooltip (info.name);
}

void Knob::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() && onRename)
    {
        juce::PopupMenu m;
        m.addItem (1, "Rename...");
        m.showMenuAsync (juce::PopupMenu::Options(), [this] (int r) { if (r == 1 && onRename) onRename(); });
    }
}

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromBottom ((int) (labelH + 5));
    const int s = std::min (r.getWidth(), r.getHeight());
    slider.setBounds (r.withSizeKeepingCentre (s, s));
}

void Knob::update()
{
    const float m = proc.modNorm[param].load (std::memory_order_relaxed);
    if (std::abs (m - lastMod) > 0.002f)
    {
        lastMod = m;
        slider.getProperties().set ("mod", m);
        slider.repaint();
    }
}

void Knob::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().removeFromBottom ((int) (labelH + 5));
    g.setFont (font (labelH, true));
    if (slider.isMouseOverOrDragging())
    {
        auto* prm = proc.param (param);
        g.setColour (col::bright);
        g.drawText (prm ? prm->getCurrentValueAsText() : juce::String(), r, juce::Justification::centred);
    }
    else
    {
        g.setColour (col::dim.brighter (0.2f));
        g.drawText (label.toUpperCase(), r, juce::Justification::centred);
    }
}

// ---------------------------------------------------------------------------- Choice / Toggle / Panel
Choice::Choice (PluginProcessor& p, int prm, const juce::String& cap) : caption (cap)
{
    const auto& info = paramTable()[prm];
    for (int i = 0; info.choices && info.choices[i]; ++i) box.addItem (info.choices[i], i + 1);
    addAndMakeVisible (box);
    att = std::make_unique<APVTS::ComboBoxAttachment> (p.apvts, info.id, box);
    box.setTooltip (info.name);
}

void Choice::resized()
{
    auto r = getLocalBounds();
    if (caption.isNotEmpty()) r.removeFromLeft (std::min (74, r.getWidth() / 3));
    box.setBounds (r);
}

void Choice::paint (juce::Graphics& g)
{
    if (caption.isEmpty()) return;
    g.setColour (col::dim.brighter (0.2f));
    g.setFont (font (10.5f, true));
    g.drawText (caption.toUpperCase(), getLocalBounds().removeFromLeft (std::min (74, getWidth() / 3)), juce::Justification::centredLeft);
}

Toggle::Toggle (PluginProcessor& p, int prm, const juce::String& text, juce::Colour c)
{
    button.setButtonText (text);
    button.setColour (juce::ToggleButton::tickColourId, c);
    addAndMakeVisible (button);
    att = std::make_unique<APVTS::ButtonAttachment> (p.apvts, paramTable()[prm].id, button);
    button.setTooltip (paramTable()[prm].name);
}

void Panel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (col::panel);
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (col::line);
    g.drawRoundedRectangle (r, 7.0f, 1.0f);
    g.setColour (colour);
    g.fillRoundedRectangle (juce::Rectangle<float> (10, 9, 14, 3), 1.5f);
    g.setColour (col::text);
    g.setFont (font (11.5f, true));
    g.drawText (title.toUpperCase(), juce::Rectangle<float> (30, 2, r.getWidth() - 40, 18), juce::Justification::centredLeft);
}

// ---------------------------------------------------------------------------- Forecast
namespace
{
struct ForecastLnf : public LookAndFeel
{
    void drawLinearSlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider::SliderStyle, juce::Slider&) override {}
};
ForecastLnf& forecastLnf() { static ForecastLnf l; return l; }
}

ForecastSlider::ForecastSlider (PluginProcessor& p) : proc (p)
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setLookAndFeel (&forecastLnf());
    slider.setSliderSnapsToMousePosition (true);
    slider.setTooltip ("Forecast: one move from Clear through Haze, Fog and Drizzle to Storm. Automate it to tell a story across the song.");
    addAndMakeVisible (slider);
    att = std::make_unique<APVTS::SliderAttachment> (proc.apvts, paramTable()[forecast].id, slider);
    slider.onValueChange = [this] { repaint(); };
}

void ForecastSlider::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    auto track = r.reduced (16.0f, 0).withSizeKeepingCentre (r.getWidth() - 32.0f, 10.0f).translated (0, 6);
    const juce::Colour stops[] = { juce::Colour (0xff7fd4ff), juce::Colour (0xffe8b98a), juce::Colour (0xffa99cc4),
                                   juce::Colour (0xff4f7fc9), juce::Colour (0xff5a2a7a) };
    juce::ColourGradient grad (stops[0], track.getX(), 0, stops[4], track.getRight(), 0, false);
    for (int i = 1; i < 4; ++i) grad.addColour (i / 4.0, stops[i]);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (track, 5.0f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawRoundedRectangle (track, 5.0f, 1.0f);

    const char* names[] = { "CLEAR", "HAZE", "FOG", "DRIZZLE", "STORM" };
    const float v = (float) slider.getValue();
    g.setFont (font (10.5f, true));
    for (int i = 0; i < 5; ++i)
    {
        const float x = track.getX() + track.getWidth() * i / 4.0f;
        const bool near = std::abs (v - i / 4.0f) < 0.125f;
        g.setColour (near ? col::bright : col::dim);
        g.drawText (names[i], juce::Rectangle<float> (x - 40, r.getY(), 80, 14), juce::Justification::centred);
        g.fillRect (x - 0.5f, track.getY() - 3, 1.0f, 3.0f);
    }
    // thumb + live modulation ghost
    const float tx = track.getX() + track.getWidth() * v;
    const float mod = proc.modNorm[forecast].load();
    if (std::abs (mod) > 0.002f)
    {
        const float mx = track.getX() + track.getWidth() * juce::jlimit (0.0f, 1.0f, v + mod);
        g.setColour (col::bright.withAlpha (0.5f));
        g.drawEllipse (mx - 7, track.getCentreY() - 7, 14, 14, 1.5f);
    }
    g.setColour (col::bright);
    g.fillEllipse (tx - 9, track.getCentreY() - 9, 18, 18);
    g.setColour (col::bg);
    g.drawEllipse (tx - 9, track.getCentreY() - 9, 18, 18, 2.0f);
}

// ---------------------------------------------------------------------------- Freeze
void FreezeButton::paint (juce::Graphics& g)
{
    const bool on = proc.value (freeze) > 0.5f || proc.engine.holdFreeze.load();
    const float amt = proc.engine.freezeAmt.load();
    auto r = getLocalBounds().toFloat().reduced (1.5f);
    g.setColour (on ? col::cyan.withAlpha (0.22f + 0.15f * amt) : col::panelHi);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (on ? col::cyan : col::line.brighter (0.3f));
    g.drawRoundedRectangle (r, 6.0f, on ? 2.0f : 1.0f);
    // snowflake
    const auto c = r.withWidth (r.getHeight()).getCentre().translated (8, 0);
    g.setColour (on ? col::bright : col::dim);
    for (int k = 0; k < 3; ++k)
    {
        const float a = k * juce::MathConstants<float>::pi / 3.0f;
        g.drawLine (c.x - 8 * std::cos (a), c.y - 8 * std::sin (a), c.x + 8 * std::cos (a), c.y + 8 * std::sin (a), 1.6f);
    }
    g.setFont (font (14.0f, true));
    g.drawText ("FREEZE", r.withTrimmedLeft (r.getHeight() + 4), juce::Justification::centredLeft);
}

void FreezeButton::mouseDown (const juce::MouseEvent&)
{
    downAt = juce::Time::getMillisecondCounter();
    proc.engine.holdFreeze = true;
    repaint();
}

void FreezeButton::mouseUp (const juce::MouseEvent&)
{
    proc.engine.holdFreeze = false;
    if (juce::Time::getMillisecondCounter() - downAt < 300)
        proc.setParamGesture (freeze, proc.value (freeze) > 0.5f ? 0.0f : 1.0f);
    repaint();
}

// ---------------------------------------------------------------------------- Snapshots
juce::Rectangle<float> SnapshotPads::pad (int i) const
{
    const float w = (getWidth() - 5 * 6.0f) / 6.0f;
    return { i * (w + 6.0f), 0.0f, w, (float) getHeight() };
}

void SnapshotPads::paint (juce::Graphics& g)
{
    const int active = proc.activeSnapshot.load();
    const float prog = proc.morphProgress();
    for (int i = 0; i < PluginProcessor::numSnapshots; ++i)
    {
        auto r = pad (i).reduced (1.0f);
        const bool set = proc.snapshotSet (i);
        const juce::Colour pal[] = { col::sun, col::pink, col::violet, col::cyan, col::gold, col::sepia };
        const auto c = pal[i];
        g.setColour (set ? c.withAlpha (i == active ? 0.35f : 0.14f) : col::panel);
        g.fillRoundedRectangle (r, 5.0f);
        if (i == active && prog < 1.0f)
        {
            g.setColour (c.withAlpha (0.45f));
            g.fillRoundedRectangle (r.withWidth (r.getWidth() * prog), 5.0f);
        }
        g.setColour (set ? c.withAlpha (i == active ? 1.0f : 0.6f) : col::line.brighter (0.2f));
        g.drawRoundedRectangle (r, 5.0f, i == active ? 2.0f : 1.0f);
        g.setColour (set ? col::bright : col::dim);
        g.setFont (font (12.0f, true));
        g.drawText (set ? juce::String (i + 1) : juce::String ("+"), r, juce::Justification::centred);
    }
}

void SnapshotPads::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < PluginProcessor::numSnapshots; ++i)
        if (pad (i).contains (e.position))
        {
            if (e.mods.isAltDown()) proc.clearSnapshot (i);
            else if (e.mods.isPopupMenu())
            {
                juce::PopupMenu m;
                m.addItem (1, "Save here");
                m.addItem (2, "Clear", proc.snapshotSet (i));
                m.showMenuAsync (juce::PopupMenu::Options(), [this, i] (int r)
                {
                    if (r == 1) proc.saveSnapshot (i);
                    if (r == 2) proc.clearSnapshot (i);
                    repaint();
                });
            }
            else if (e.mods.isShiftDown() || ! proc.snapshotSet (i)) proc.saveSnapshot (i);
            else proc.recallSnapshot (i);
            repaint();
            return;
        }
}

// ---------------------------------------------------------------------------- Hand looper
void HandLooper::paint (juce::Graphics& g)
{
    const int st = proc.engine.looperState.load();
    const float ph = proc.engine.looperPhase.load();
    auto r = recArea();
    const auto c = st == 1 ? col::warn : (st == 2 ? col::gold : col::line.brighter (0.3f));
    g.setColour (st == 0 ? col::panelHi : c.withAlpha (0.18f));
    g.fillRoundedRectangle (r, 5.0f);
    if (st != 0)
    {
        g.setColour (c.withAlpha (0.3f));
        g.fillRoundedRectangle (r.withWidth (r.getWidth() * ph), 5.0f);
    }
    g.setColour (c);
    g.drawRoundedRectangle (r, 5.0f, st == 0 ? 1.0f : 1.6f);
    g.setColour (st == 0 ? col::warn.withAlpha (0.8f) : c);
    g.fillEllipse (r.getX() + 9, r.getCentreY() - 4.5f, 9, 9);
    g.setColour (col::bright);
    g.setFont (font (11.5f, true));
    g.drawText (st == 1 ? "RECORDING" : (st == 2 ? "HAND LOOP" : "REC HAND"), r.withTrimmedLeft (24), juce::Justification::centredLeft);
    auto cr = clearArea();
    g.setColour (col::panelHi);
    g.fillRoundedRectangle (cr, 5.0f);
    g.setColour (col::line.brighter (0.3f));
    g.drawRoundedRectangle (cr, 5.0f, 1.0f);
    g.setColour (st == 0 ? col::dim : col::text);
    g.drawText ("CLEAR", cr, juce::Justification::centred);
}

void HandLooper::mouseDown (const juce::MouseEvent& e)
{
    if (clearArea().contains (e.position)) proc.engine.looperCmd = 2;
    else proc.engine.looperCmd = 1;
}

// ---------------------------------------------------------------------------- Timeline
int Timeline::firstBar() const
{
    const int cur = proc.engine.curBar.load();
    return std::max (0, cur - visibleBars + 8);
}

void Timeline::update()
{
    const int f = proc.engine.ghostFlash.load();
    if (f != lastFlash) { lastFlash = f; flash = 1.0f; }
    flash *= 0.9f;
    repaint();
}

void Timeline::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (col::panel);
    g.fillRoundedRectangle (r, 4.0f);
    const int cur = proc.engine.curBar.load();
    const int first = firstBar();
    const float bw = r.getWidth() / visibleBars;
    g.setFont (font (9.5f));
    for (int i = 0; i < visibleBars; ++i)
    {
        const int bar = first + i;
        auto cell = juce::Rectangle<float> (r.getX() + i * bw, r.getY(), bw, r.getHeight()).reduced (0.5f, 3.0f);
        if (proc.engine.barRecorded (bar))
        {
            g.setColour (col::sepia.withAlpha (0.18f));
            g.fillRect (cell);
        }
        if (bar == cur)
        {
            g.setColour (col::sun.withAlpha (0.55f));
            g.fillRect (cell);
        }
        if (bar % 8 == 0 && cur >= 0)
        {
            g.setColour (col::dim.withAlpha (0.8f));
            g.fillRect (cell.getX(), r.getY(), 1.0f, r.getHeight());
            g.drawText (juce::String (bar + 1), juce::Rectangle<float> (cell.getX() + 2, r.getY(), 30, r.getHeight()), juce::Justification::centredLeft);
        }
    }
    for (int k = 0; k < ml::Engine::maxBookmarks; ++k)
    {
        const int b = proc.engine.uiBookmarks[k].load();
        if (b < first || b >= first + visibleBars) continue;
        const float x = r.getX() + (b - first + 0.5f) * bw;
        const bool autoBm = proc.engine.uiBookmarkAuto[k].load();
        g.setColour (col::violet.withAlpha (0.35f + 0.5f * flash));
        g.fillEllipse (x - 7, r.getCentreY() - 7, 14, 14);
        g.setColour (col::fog);
        if (autoBm) g.drawEllipse (x - 4, r.getCentreY() - 4, 8, 8, 1.4f);
        else g.fillEllipse (x - 4, r.getCentreY() - 4, 8, 8);
    }
    if (cur < 0)
    {
        g.setColour (col::dim);
        g.setFont (font (10.5f));
        g.drawText ("GHOST TIMELINE - press play in Ableton to map the song by bars", r.reduced (8, 0), juce::Justification::centredLeft);
    }
}

void Timeline::mouseDown (const juce::MouseEvent& e)
{
    const float bw = getWidth() / (float) visibleBars;
    const int bar = firstBar() + (int) (e.position.x / bw);
    proc.engine.toggleBookmark (bar);
    repaint();
}
} // namespace ui
