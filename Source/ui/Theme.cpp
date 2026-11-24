#include "Theme.h"

namespace ui
{
juce::Font font (float h, bool bold) { return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain)); }

LookAndFeel::LookAndFeel()
{
    setColour (juce::Label::textColourId, col::text);
    setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::backgroundColourId, col::panelHi);
    setColour (juce::ComboBox::textColourId, col::text);
    setColour (juce::ComboBox::outlineColourId, col::line);
    setColour (juce::ComboBox::arrowColourId, col::dim);
    setColour (juce::PopupMenu::backgroundColourId, col::panelHi);
    setColour (juce::PopupMenu::textColourId, col::text);
    setColour (juce::PopupMenu::headerTextColourId, col::sun);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::sun.withAlpha (0.25f));
    setColour (juce::PopupMenu::highlightedTextColourId, col::bright);
    setColour (juce::TextButton::buttonColourId, col::panelHi);
    setColour (juce::TextButton::buttonOnColourId, col::sun.withAlpha (0.3f));
    setColour (juce::TextButton::textColourOffId, col::text);
    setColour (juce::TextButton::textColourOnId, col::bright);
    setColour (juce::TextEditor::backgroundColourId, col::panelHi);
    setColour (juce::TextEditor::textColourId, col::bright);
    setColour (juce::TextEditor::highlightColourId, col::sun.withAlpha (0.35f));
    setColour (juce::TextEditor::outlineColourId, col::line);
    setColour (juce::TextEditor::focusedOutlineColourId, col::sun);
    setColour (juce::CaretComponent::caretColourId, col::sun);
    setColour (juce::TooltipWindow::backgroundColourId, col::panelHi);
    setColour (juce::TooltipWindow::textColourId, col::text);
    setColour (juce::TooltipWindow::outlineColourId, col::line);
    setColour (juce::AlertWindow::backgroundColourId, col::panel);
    setColour (juce::AlertWindow::textColourId, col::text);
    setColour (juce::AlertWindow::outlineColourId, col::line);
}

void LookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (col::panelHi);
    g.setColour (col::line);
    g.drawRect (0, 0, w, h);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);
    const auto& props = s.getProperties();
    const bool bipolar = props.getWithDefault ("bipolar", false);
    const float mod = (float) (double) props.getWithDefault ("mod", 0.0);
    const bool hasMod = props.getWithDefault ("hasMod", false);
    auto b = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (3.0f);
    const float r = std::min (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float ang = start + pos * (end - start);
    const float track = std::max (2.5f, r * 0.12f);
    const float ar = r - track * 0.5f - 1.0f;

    // body
    const float br = r * 0.66f;
    g.setGradientFill (juce::ColourGradient (col::panelHi.brighter (0.12f), c.x, c.y - br, col::bg, c.x, c.y + br, false));
    g.fillEllipse (c.x - br, c.y - br, br * 2, br * 2);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawEllipse (c.x - br, c.y - br, br * 2, br * 2, 1.0f);

    juce::Path bgArc;
    bgArc.addCentredArc (c.x, c.y, ar, ar, 0, start, end, true);
    g.setColour (col::line);
    g.strokePath (bgArc, juce::PathStrokeType (track, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float from = bipolar ? (start + end) * 0.5f : start;
    juce::Path arc;
    arc.addCentredArc (c.x, c.y, ar, ar, 0, std::min (from, ang), std::max (from, ang), true);
    const auto on = s.isEnabled() ? accent : col::dim;
    g.setGradientFill (juce::ColourGradient (on.withRotatedHue (-0.04f), c.x - r, c.y + r, on.withRotatedHue (0.03f).brighter (0.2f), c.x + r, c.y - r, false));
    g.strokePath (arc, juce::PathStrokeType (track, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // live modulation: thin outer arc + dot
    if (hasMod && std::abs (mod) > 0.002f)
    {
        const float mpos = juce::jlimit (0.0f, 1.0f, pos + mod);
        const float mang = start + mpos * (end - start);
        juce::Path m;
        const float mr = r + 0.5f;
        m.addCentredArc (c.x, c.y, mr, mr, 0, std::min (ang, mang), std::max (ang, mang), true);
        g.setColour (col::bright.withAlpha (0.75f));
        g.strokePath (m, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.fillEllipse (c.x + mr * std::sin (mang) - 2.2f, c.y - mr * std::cos (mang) - 2.2f, 4.4f, 4.4f);
    }

    const float p0 = br * 0.25f, p1 = br * 0.85f;
    g.setColour (col::bright);
    g.drawLine (c.x + p0 * std::sin (ang), c.y - p0 * std::cos (ang), c.x + p1 * std::sin (ang), c.y - p1 * std::cos (ang), std::max (1.6f, r * 0.07f));
}

void LookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider& s)
{
    // bipolar horizontal bar (mod matrix amounts)
    auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).withSizeKeepingCentre ((float) w, 6.0f);
    g.setColour (col::line);
    g.fillRoundedRectangle (r, 3.0f);
    const float mid = r.getCentreX();
    const float px = juce::jlimit (r.getX(), r.getRight(), pos);
    const auto c = s.findColour (juce::Slider::trackColourId);
    g.setColour (c);
    g.fillRoundedRectangle (juce::Rectangle<float> (std::min (mid, px), r.getY(), std::abs (px - mid), r.getHeight()), 3.0f);
    g.setColour (col::dim);
    g.fillRect (mid - 0.5f, r.getY() - 2, 1.0f, r.getHeight() + 4);
    g.setColour (col::bright);
    g.fillEllipse (px - 4.5f, r.getCentreY() - 4.5f, 9.0f, 9.0f);
}

void LookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool hl, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    const auto onCol = b.findColour (juce::ToggleButton::tickColourId);
    g.setColour (on ? onCol.withAlpha (0.2f) : (hl ? col::line : col::panelHi));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (on ? onCol : col::line.brighter (0.25f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    // LED
    const float d = std::min (7.0f, r.getHeight() * 0.35f);
    auto led = juce::Rectangle<float> (r.getX() + 7, r.getCentreY() - d * 0.5f, d, d);
    g.setColour (on ? onCol : col::dim.withAlpha (0.5f));
    g.fillEllipse (led);
    if (on) { g.setColour (onCol.withAlpha (0.3f)); g.fillEllipse (led.expanded (2.5f)); }
    g.setColour (on ? col::bright : col::text);
    g.setFont (font (11.5f, true));
    g.drawText (b.getButtonText().toUpperCase(), r.withTrimmedLeft (d + 10), juce::Justification::centred);
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& base, bool hl, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    auto c = b.getToggleState() ? col::sun.withAlpha (0.25f) : base;
    if (down) c = c.brighter (0.15f); else if (hl) c = c.brighter (0.08f);
    g.setColour (c);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (b.getToggleState() ? col::sun : col::line.brighter (0.25f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

void LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (font (11.5f, true));
    g.setColour (b.getToggleState() ? col::bright : col::text);
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

void LookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& cb)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (cb.isMouseOver() ? col::panelHi.brighter (0.06f) : col::panelHi);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (col::line.brighter (0.25f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    juce::Path p;
    const float ax = (float) w - 10.0f, ay = (float) h * 0.5f;
    p.addTriangle (ax - 3.5f, ay - 1.5f, ax + 3.5f, ay - 1.5f, ax, ay + 2.5f);
    g.setColour (col::dim);
    g.fillPath (p);
}

void LookAndFeel::positionComboBoxText (juce::ComboBox& cb, juce::Label& l)
{
    l.setBounds (4, 0, cb.getWidth() - 18, cb.getHeight());
    l.setFont (font (12.0f));
    l.setJustificationType (juce::Justification::centredLeft);
}
} // namespace ui
