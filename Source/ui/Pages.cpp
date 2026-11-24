#include "Pages.h"

namespace ui
{
using namespace ml;

namespace
{
juce::String shortName (int p)
{
    switch (p)
    {
        case gsize: return "Size";
        case pspread: return "Spread";
        case windDir: return "Direction";
        case temp: return "Warmth";
        case snap: return "Key Follow";
        case memLen: return "Memory";
        case ghostAge: return "Age";
        case ghostRate: return "Rate";
        case revSize: return "Room";
        case revDecay: return "Decay";
        case fltCut: return "Cutoff";
        case fltRes: return "Reso";
        case bodyForce: return "Force";
        default: return paramTable()[p].name;
    }
}
} // namespace

ParamPage::ParamPage (PluginProcessor& p, std::initializer_list<int> params, juce::Colour c, const juce::String& n)
    : proc (p), note (n)
{
    for (int prm : params)
    {
        const auto& info = paramTable()[prm];
        if (info.type == PT::Choice)
        {
            side.push_back (std::make_unique<Choice> (proc, prm, shortName (prm)));
            addAndMakeVisible (*side.back());
        }
        else if (info.type == PT::Bool)
        {
            side.push_back (std::make_unique<Toggle> (proc, prm, shortName (prm), c));
            addAndMakeVisible (*side.back());
        }
        else
        {
            knobs.push_back (std::make_unique<Knob> (proc, prm, shortName (prm), c));
            addAndMakeVisible (*knobs.back());
        }
    }
}

void ParamPage::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    if (note.isNotEmpty()) r.removeFromBottom (16);
    if (! side.empty())
    {
        auto s = r.removeFromLeft (220);
        r.removeFromLeft (12);
        const int h = std::min (28, (s.getHeight() - 4 * ((int) side.size() - 1)) / std::max (1, (int) side.size()));
        for (auto& c : side) { c->setBounds (s.removeFromTop (h)); s.removeFromTop (4); }
    }
    if (knobs.empty()) return;
    const int kw = std::min (84, r.getWidth() / (int) knobs.size());
    const int perRow = std::max (1, r.getWidth() / kw);
    const int rows = ((int) knobs.size() + perRow - 1) / perRow;
    const int kh = std::min (96, r.getHeight() / rows);
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        const int rowI = (int) i / perRow, colI = (int) i % perRow;
        knobs[i]->setBounds (r.getX() + colI * kw, r.getY() + rowI * kh, kw, kh);
    }
}

void ParamPage::paint (juce::Graphics& g)
{
    if (note.isEmpty()) return;
    g.setColour (col::dim);
    g.setFont (font (10.5f));
    g.drawText (note, getLocalBounds().reduced (12, 4).removeFromBottom (14), juce::Justification::centredLeft);
}

void ParamPage::update() { for (auto& k : knobs) k->update(); }

// ---------------------------------------------------------------------------- gravity
GravityPage::GravityPage (PluginProcessor& p) : proc (p)
{
    for (int b = 0; b < numBodies; ++b)
    {
        types.push_back (std::make_unique<Choice> (proc, g1Type + b * bodyStride));
        addAndMakeVisible (*types.back());
        for (int k : { g1Speed, g1Energy })
        {
            knobs.push_back (std::make_unique<Knob> (proc, k + b * bodyStride, k == g1Speed ? "Speed" : "Energy", col::body (b)));
            addAndMakeVisible (*knobs.back());
        }
    }
    force = std::make_unique<Knob> (proc, bodyForce, "Push", col::fog);
    addAndMakeVisible (*force);
}

void GravityPage::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    force->setBounds (r.removeFromRight (90).withHeight (96));
    const int cw = r.getWidth() / numBodies;
    for (int b = 0; b < numBodies; ++b)
    {
        auto c = r.withX (r.getX() + b * cw).withWidth (cw - 10);
        c.removeFromTop (18);
        types[(size_t) b]->setBounds (c.removeFromTop (26));
        c.removeFromTop (6);
        auto k = c.removeFromTop (std::min (96, c.getHeight()));
        knobs[(size_t) (b * 2)]->setBounds (k.removeFromLeft (k.getWidth() / 2));
        knobs[(size_t) (b * 2 + 1)]->setBounds (k);
    }
}

void GravityPage::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().reduced (10, 8);
    r.removeFromRight (90);
    const int cw = r.getWidth() / numBodies;
    g.setFont (font (11.0f, true));
    for (int b = 0; b < numBodies; ++b)
    {
        g.setColour (col::body (b));
        g.fillEllipse ((float) (r.getX() + b * cw), (float) r.getY() + 4, 8, 8);
        g.setColour (col::text);
        g.drawText ("BODY " + juce::String (b + 1), r.getX() + b * cw + 14, r.getY(), 120, 16, juce::Justification::centredLeft);
    }
    g.setColour (col::dim);
    g.setFont (font (10.5f));
    g.drawText ("Bodies live in the fog and push the grains around. Drag them in the scene to fling them; springs get kicked by input hits.",
                getLocalBounds().reduced (12, 4).removeFromBottom (14), juce::Justification::centredLeft);
}

void GravityPage::update() { for (auto& k : knobs) k->update(); force->update(); }

// ---------------------------------------------------------------------------- matrix
MatrixPage::MatrixPage (PluginProcessor& p) : proc (p)
{
    for (int s = 0; s < numSlots; ++s)
    {
        srcs.push_back (std::make_unique<Choice> (proc, s1Src + s * slotStride));
        dsts.push_back (std::make_unique<Choice> (proc, s1Dst + s * slotStride));
        auto sl = std::make_unique<juce::Slider> (juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
        sl->setColour (juce::Slider::trackColourId, col::violet);
        sl->setTooltip ("Amount");
        sl->setDoubleClickReturnValue (true, 0.0);
        atts.push_back (std::make_unique<APVTS::SliderAttachment> (proc.apvts, paramTable()[s1Amt + s * slotStride].id, *sl));
        amts.push_back (std::move (sl));
        addAndMakeVisible (*srcs.back()); addAndMakeVisible (*dsts.back()); addAndMakeVisible (*amts.back());
    }
}

void MatrixPage::resized()
{
    auto r = getLocalBounds().reduced (10, 6);
    r.removeFromBottom (14);
    const int half = r.getWidth() / 2;
    const int rows = numSlots / 2;
    const int rh = r.getHeight() / rows;
    for (int s = 0; s < numSlots; ++s)
    {
        auto row = juce::Rectangle<int> (r.getX() + (s / rows) * half, r.getY() + (s % rows) * rh, half - 16, rh).reduced (0, 2);
        row.removeFromLeft (22);
        const int w = row.getWidth();
        srcs[(size_t) s]->setBounds (row.removeFromLeft (w * 32 / 100)); row.removeFromLeft (6);
        dsts[(size_t) s]->setBounds (row.removeFromLeft (w * 36 / 100)); row.removeFromLeft (6);
        amts[(size_t) s]->setBounds (row);
    }
}

void MatrixPage::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().reduced (10, 6);
    r.removeFromBottom (14);
    const int half = r.getWidth() / 2, rows = numSlots / 2, rh = r.getHeight() / rows;
    g.setFont (font (10.0f, true));
    g.setColour (col::dim);
    for (int s = 0; s < numSlots; ++s)
        g.drawText (juce::String (s + 1), r.getX() + (s / rows) * half, r.getY() + (s % rows) * rh, 18, rh, juce::Justification::centredRight);
    g.setFont (font (10.5f));
    g.drawText ("Macros 1-6, Life, input level, Forecast, your hand and the bodies can move any knob. Most presets route the 6 macros here.",
                getLocalBounds().reduced (12, 2).removeFromBottom (14), juce::Justification::centredLeft);
}

// ---------------------------------------------------------------------------- tabs
Pages::Pages (PluginProcessor& p) : proc (p)
{
    const char* names[] = { "GRAINS", "WEATHER", "LISTEN + KEY", "MEMORY + GHOSTS", "CHARACTER", "GRAVITY", "MATRIX" };
    for (int i = 0; i < 7; ++i)
    {
        auto* b = tabs.add (new juce::TextButton (names[i]));
        b->setClickingTogglesState (false);
        b->onClick = [this, i] { show (i); };
        addAndMakeVisible (b);
    }
    pages.push_back (std::make_unique<ParamPage> (proc, std::initializer_list<int> { window, fsafe, humidity, gsize, reach, pitch, pspread, glide, reverse, formant, width },
                                                  col::sun, "Formant Safe keeps voices human when repitched (PSOLA). Formant shifts the throat on purpose."));
    pages.push_back (std::make_unique<ParamPage> (proc, std::initializer_list<int> { forecast, wind, windDir, turb, ml::gravity, temp, life },
                                                  col::cyan, "Wind pushes the fog, Turbulence swirls it, Gravity makes pitches fall, Warmth makes young grains rise. Life = slow drift."));
    pages.push_back (std::make_unique<ParamPage> (proc, std::initializer_list<int> { listen, key, scale, midiMode, snap },
                                                  col::pink, "Tonal = only steady pitched moments (choirs). Transient = only hits (glitch). Air = breaths and noise. MIDI notes can set the key or play the cloud."));
    pages.push_back (std::make_unique<ParamPage> (proc, std::initializer_list<int> { memSync, memBars, ghostAuto, memLen, nostalgia, feedback, ghosts, ghostAge, ghostRate },
                                                  col::sepia, "Nostalgia = how fast old memories wear (darker, wobblier, dustier). Ghosts replay bookmarked bars later in the song, in time."));
    pages.push_back (std::make_unique<ParamPage> (proc, std::initializer_list<int> { fltType, driveType, tone, fltCut, fltRes, drive, ml::smog, space, revSize, revDecay, shimmer, duck },
                                                  col::violet, "All character effects work on the cloud only; your dry signal stays untouched. Ducking lets the cloud bloom in the gaps."));
    pages.push_back (std::make_unique<GravityPage> (proc));
    pages.push_back (std::make_unique<MatrixPage> (proc));
    for (auto& pg : pages) addChildComponent (*pg);
    show (0);
}

void Pages::show (int i)
{
    current = i;
    for (int k = 0; k < (int) pages.size(); ++k) pages[(size_t) k]->setVisible (k == i);
    for (int k = 0; k < tabs.size(); ++k) tabs[k]->setToggleState (k == i, juce::dontSendNotification);
}

void Pages::resized()
{
    auto r = getLocalBounds();
    auto t = r.removeFromTop (26);
    const int tw = t.getWidth() / tabs.size();
    for (auto* b : tabs) b->setBounds (t.removeFromLeft (tw).reduced (2, 0));
    r.removeFromTop (4);
    for (auto& pg : pages) pg->setBounds (r);
}

void Pages::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().withTrimmedTop (30).toFloat();
    g.setColour (col::panel);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (col::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
}

void Pages::update()
{
    auto* pg = pages[(size_t) current].get();
    if (auto* pp = dynamic_cast<ParamPage*> (pg)) pp->update();
    else if (auto* gp = dynamic_cast<GravityPage*> (pg)) gp->update();
}
} // namespace ui
