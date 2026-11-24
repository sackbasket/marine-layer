#include "Scene.h"

namespace ui
{
using namespace ml;

namespace
{
const char* noteName (int pc)
{
    static const char* n[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return n[((pc % 12) + 12) % 12];
}
juce::Colour ageColour (float x01)
{
    // now (left) = sunset orange -> pink -> violet -> sepia (oldest)
    const juce::Colour c[] = { col::sun, col::pink, col::violet.interpolatedWith (col::sepia, 0.35f), col::sepia };
    const float f = juce::jlimit (0.0f, 1.0f, x01) * 3.0f;
    const int i = std::min (2, (int) f);
    return c[i].interpolatedWith (c[i + 1], f - (float) i);
}
} // namespace

Scene::Scene (PluginProcessor& p) : proc (p)
{
    setWantsKeyboardFocus (true);
    setMouseClickGrabsKeyboardFocus (true);
    sprites[0] = makeSprite (col::fog, 32);
    sprites[1] = makeSprite (col::cyan, 32);
    sprites[2] = makeSprite (col::gold, 32);
    sprites[3] = makeSprite (col::bright, 32);
    mist = makeSprite (col::fog, 64);
    setTooltip ({});
}

juce::Image Scene::makeSprite (juce::Colour c, int size)
{
    juce::Image img (juce::Image::ARGB, size, size, true);
    juce::Graphics g (img);
    const float r = size * 0.5f;
    juce::ColourGradient grad (c.withAlpha (1.0f), r, r, c.withAlpha (0.0f), r + r, r, true);
    grad.addColour (0.25, c.withAlpha (0.55f));
    grad.addColour (0.6, c.withAlpha (0.12f));
    g.setGradientFill (grad);
    g.fillEllipse (0, 0, (float) size, (float) size);
    return img;
}

void Scene::resized() { drawBackground(); }

juce::Point<float> Scene::toScreen (float x01, float y) const
{
    const auto r = getLocalBounds().toFloat();
    return { r.getX() + x01 * r.getWidth(), r.getY() + (1.0f - (y + 1.0f) * 0.5f) * r.getHeight() };
}

juce::Point<float> Scene::fromScreen (juce::Point<float> p) const
{
    const auto r = getLocalBounds().toFloat();
    return { juce::jlimit (0.0f, 1.0f, (p.x - r.getX()) / r.getWidth()),
             juce::jlimit (-1.0f, 1.0f, 1.0f - 2.0f * (p.y - r.getY()) / r.getHeight()) };
}

void Scene::drawBackground()
{
    const int w = std::max (1, getWidth()), h = std::max (1, getHeight());
    bg = juce::Image (juce::Image::RGB, w, h, true);
    juce::Graphics g (bg);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1c1236), 0, 0, juce::Colour (0xff09070f), 0, (float) h, false));
    g.fillAll();
    // dusk glow low on the left where "now" is
    juce::ColourGradient glow (col::sun.withAlpha (0.16f), 0, h * 0.62f, col::sun.withAlpha (0.0f), w * 0.55f, h * 0.62f, true);
    g.setGradientFill (glow);
    g.fillRect (0, 0, w, h);
    // stars
    juce::Random rnd (7);
    for (int i = 0; i < 90; ++i)
    {
        const float x = rnd.nextFloat() * w, y = rnd.nextFloat() * h * 0.45f;
        g.setColour (col::bright.withAlpha (0.08f + 0.3f * rnd.nextFloat() * rnd.nextFloat()));
        g.fillEllipse (x, y, 1.6f, 1.6f);
    }
    // grainy sepia film on the old side
    grain = juce::Image (juce::Image::ARGB, w, h, true);
    juce::Graphics gg (grain);
    for (int i = 0; i < w * h / 60; ++i)
    {
        const float x = rnd.nextFloat() * w, y = rnd.nextFloat() * h;
        const float a = std::pow (x / w, 1.6f) * 0.22f * rnd.nextFloat();
        gg.setColour (col::sepia.withAlpha (a));
        gg.fillRect (x, y, 1.2f, 1.2f);
    }
}

void Scene::update()
{
    time += 1.0f / 60.0f;
    const float lvl = proc.engine.inLevel.load();
    breathe += (std::min (1.0f, lvl * 2.5f) - breathe) * 0.12f;
    for (int i = 0; i < numBodies; ++i)
    {
        auto& tr = trails[i];
        if ((int) proc.value (g1Type + i * bodyStride) == bodyOff) { tr.clear(); continue; }
        tr.push_back (toScreen ((proc.engine.gravity.x (i) + 1.0f) * 0.5f, proc.engine.gravity.y (i)));
        while (tr.size() > 30) tr.pop_front();
    }
    // a hand held still keeps its pull; gust fades
    if (drag == Drag::hand)
    {
        gust *= 0.85f;
        if (proc.value (handGust) > 0.002f || gust > 0.002f) proc.setParam (handGust, gust);
    }
    repaint();
}

// ---------------------------------------------------------------------------- painting
void Scene::paint (juce::Graphics& g)
{
    g.drawImageAt (bg, 0, 0);
    drawCoast (g);
    g.drawImageAt (grain, 0, 0);
    drawBodies (g);
    drawParticles (g);
    drawOverlay (g);
    g.setColour (col::line);
    g.drawRect (getLocalBounds(), 1);
}

void Scene::drawCoast (juce::Graphics& g)
{
    auto& e = proc.engine;
    const auto r = getLocalBounds().toFloat();
    const double sr = e.getSampleRate();
    const float memSec = e.uiMemSeconds.load();
    const int64_t w = e.uiWrite.load();
    const int64_t rec = e.recorded();
    const int cols = std::max (32, (int) (r.getWidth() / 3.0f));
    const float mid = r.getY() + r.getHeight() * 0.62f;
    const float amp = r.getHeight() * 0.3f;

    juce::Path top, fill;
    std::vector<float> pk ((size_t) cols + 1);
    for (int c = 0; c <= cols; ++c)
    {
        const float x01 = (float) c / cols;
        const double back = x01 * memSec * sr;
        float v = 0;
        if (back < (double) rec)
        {
            const int64_t a = (int64_t) ((double) w - back) / Engine::ovChunk;
            const int span = std::max (1, (int) (memSec * sr / cols / Engine::ovChunk));
            for (int k = 0; k < span; ++k) v = std::max (v, e.overviewPeak (a - k));
        }
        pk[(size_t) c] = std::pow (std::min (1.0f, v), 0.6f);
    }
    fill.startNewSubPath (r.getX(), mid);
    for (int c = 0; c <= cols; ++c) fill.lineTo (r.getX() + r.getWidth() * c / cols, mid - pk[(size_t) c] * amp);
    for (int c = cols; c >= 0; --c) fill.lineTo (r.getX() + r.getWidth() * c / cols, mid + pk[(size_t) c] * amp * 0.45f);
    fill.closeSubPath();
    for (int c = 0; c <= cols; ++c)
    {
        const auto p = juce::Point<float> (r.getX() + r.getWidth() * c / cols, mid - pk[(size_t) c] * amp);
        if (c == 0) top.startNewSubPath (p); else top.lineTo (p);
    }
    juce::ColourGradient grad (ageColour (0).withAlpha (0.22f), r.getX(), 0, ageColour (1).withAlpha (0.10f), r.getRight(), 0, false);
    grad.addColour (0.33, ageColour (0.33f).withAlpha (0.18f));
    grad.addColour (0.66, ageColour (0.66f).withAlpha (0.13f));
    g.setGradientFill (grad);
    g.fillPath (fill);
    juce::ColourGradient line (ageColour (0), r.getX(), 0, ageColour (1).withAlpha (0.6f), r.getRight(), 0, false);
    line.addColour (0.33, ageColour (0.33f));
    line.addColour (0.66, ageColour (0.66f).withAlpha (0.8f));
    g.setGradientFill (line);
    g.strokePath (top, juce::PathStrokeType (1.6f));
    // soft horizon
    g.setColour (col::sun.withAlpha (0.12f));
    g.fillRect (r.getX(), mid, r.getWidth(), 1.0f);

    // reach: where the fog thins out
    const float rx = r.getX() + r.getWidth() * proc.value (reach);
    for (float y = r.getY() + 4; y < r.getBottom(); y += 9)
    {
        g.setColour (col::fog.withAlpha (0.08f));
        g.fillRect (rx, y, 1.0f, 4.0f);
    }

    // record head (now), icy when frozen
    const float fz = e.freezeAmt.load();
    const auto hc = col::sun.interpolatedWith (col::cyan, fz);
    juce::ColourGradient hg (hc.withAlpha (0.55f + 0.4f * breathe), r.getX(), 0, hc.withAlpha (0.0f), r.getX() + 22 + 30 * breathe, 0, false);
    g.setGradientFill (hg);
    g.fillRect (r.getX(), r.getY(), 22 + 30 * breathe, r.getHeight());
    g.setColour (hc);
    g.fillRect (r.getX(), r.getY(), 2.0f, r.getHeight());
    if (fz > 0.02f)
    {
        g.setColour (col::cyan.withAlpha (0.06f * fz));
        g.fillRect (r);
    }
}

void Scene::drawParticles (juce::Graphics& g)
{
    auto& wthr = proc.engine.weather;
    const auto& v = wthr.vis[wthr.visIndex.load (std::memory_order_acquire)];
    // painted wind
    for (int s = 0; s < Weather::maxStrokes; ++s)
    {
        const int n = v.strokeN[s];
        if (n < 2) continue;
        juce::Path p;
        for (int k = 0; k < n; ++k)
        {
            const auto q = toScreen (v.strokeX[s][k], v.strokeY[s][k]);
            if (k == 0) p.startNewSubPath (q); else p.lineTo (q);
        }
        g.setColour (col::cyan.withAlpha (0.25f * std::min (1.0f, v.strokeTtl[s])));
        g.strokePath (p, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (col::cyan.withAlpha (0.6f * std::min (1.0f, v.strokeTtl[s])));
        g.strokePath (p, juce::PathStrokeType (1.2f));
    }
    const int n = std::min (v.n, Weather::maxParticles);
    const float base = 0.55f + 0.45f * breathe;
    // the fog itself: big soft mist around every particle, breathing with the input
    for (int i = 0; i < n; ++i)
    {
        const auto& q = v.p[i];
        const auto s = toScreen (q.x, q.y);
        const float size = 70.0f + 70.0f * q.z;
        g.setOpacity (juce::jlimit (0.0f, 1.0f, std::min (q.life * 2.0f, 1.0f) * (0.07f + 0.08f * base + 0.06f * q.flash)));
        g.drawImage (mist, juce::Rectangle<float> (s.x - size * 0.5f, s.y - size * 0.32f, size, size * 0.64f));
    }
    for (int i = 0; i < n; ++i)
    {
        const auto& q = v.p[i];
        const auto s = toScreen (q.x, q.y);
        const float size = 10.0f + 22.0f * q.z * (0.6f + 0.4f * std::min (1.0f, proc.value (gsize) / 400.0f));
        const float alpha = juce::jlimit (0.0f, 1.0f, std::min (q.life * 3.0f, 1.0f) * (0.45f + 0.45f * base));
        const auto& spr = q.src == 0 ? sprites[0] : (q.src % 2 ? sprites[1] : sprites[2]);
        // tint by age of the memory it reads: sunset near now, sepia when old
        g.setOpacity (alpha * 0.8f);
        g.drawImage (spr, juce::Rectangle<float> (s.x - size * 0.5f, s.y - size * 0.5f, size, size));
        const auto tint = ageColour (q.x);
        g.setColour (tint.withAlpha (alpha * (0.35f + 0.65f * q.flash)));
        const float core = 2.0f + 3.0f * q.flash;
        g.fillEllipse (s.x - core, s.y - core, core * 2, core * 2);
        if (q.flash > 0.5f)
        {
            g.setOpacity (q.flash * 0.6f * alpha);
            g.drawImage (sprites[3], juce::Rectangle<float> (s.x - size * 0.35f, s.y - size * 0.35f, size * 0.7f, size * 0.7f));
        }
    }
    g.setOpacity (1.0f);

    // emitters: little fountains
    for (int e = 0; e < 4; ++e)
    {
        if (! proc.engine.emOn[e].load()) continue;
        const auto p = toScreen (proc.engine.emX[e].load(), proc.engine.emY[e].load());
        const bool doomed = drag == Drag::emitter && dragEmitter == e && ! getLocalBounds().toFloat().expanded (-2).contains (mousePos);
        const auto c = doomed ? col::warn : (e % 2 ? col::cyan : col::gold);
        const float pulse = 0.5f + 0.5f * std::sin (time * 3.0f + e);
        g.setColour (c.withAlpha (0.18f + 0.15f * pulse));
        g.fillEllipse (p.x - 14, p.y - 14, 28, 28);
        g.setColour (c);
        g.drawEllipse (p.x - 7, p.y - 7, 14, 14, 1.6f);
        juce::Path tri;
        tri.addTriangle (p.x - 4, p.y + 3, p.x + 4, p.y + 3, p.x, p.y - 5);
        g.fillPath (tri);
    }

    // hand (live or looped)
    const float pull = proc.engine.effHandPull.load();
    const float gst = proc.engine.effHandGust.load();
    if (std::abs (pull) > 0.001f || gst > 0.002f)
    {
        const auto hp = toScreen (proc.engine.effHandX.load(), proc.engine.effHandY.load() * 2.0f - 1.0f);
        const bool looped = proc.engine.looperState.load() == 2 && drag != Drag::hand;
        const auto c = pull >= 0 ? col::sun : col::pink;
        const float rr = 26.0f + 10.0f * std::sin (time * 6.0f) * (pull >= 0 ? -1.0f : 1.0f) + 30.0f * gst;
        g.setColour (c.withAlpha (0.12f));
        g.fillEllipse (hp.x - rr, hp.y - rr, rr * 2, rr * 2);
        if (looped)
        {
            juce::Path ring; ring.addEllipse (hp.x - rr, hp.y - rr, rr * 2, rr * 2);
            juce::Path dashed; const float d[] = { 4.0f, 5.0f };
            juce::PathStrokeType (1.4f).createDashedStroke (dashed, ring, d, 2);
            g.setColour (col::gold.withAlpha (0.8f));
            g.fillPath (dashed);
        }
        else
        {
            g.setColour (c.withAlpha (0.8f));
            g.drawEllipse (hp.x - rr, hp.y - rr, rr * 2, rr * 2, 1.4f);
        }
    }
    if (drag == Drag::stroke && strokeScreen.size() > 1)
    {
        juce::Path p; p.startNewSubPath (strokeScreen[0]);
        for (size_t k = 1; k < strokeScreen.size(); ++k) p.lineTo (strokeScreen[k]);
        g.setColour (col::cyan.withAlpha (0.8f));
        g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

void Scene::drawBodies (juce::Graphics& g)
{
    const auto centre = toScreen (0.5f, 0.0f);
    for (int i = 0; i < numBodies; ++i)
    {
        const int type = (int) proc.value (g1Type + i * bodyStride);
        if (type == bodyOff) continue;
        const auto c = col::body (i);
        const auto& b = proc.engine.gravity.bodies[i];
        const auto p = toScreen ((proc.engine.gravity.x (i) + 1.0f) * 0.5f, proc.engine.gravity.y (i));
        if (type == bodyPendulum)
        {
            const auto e = toScreen ((b.outEx.load() + 1.0f) * 0.5f, b.outEy.load());
            g.setColour (c.withAlpha (0.35f));
            g.drawLine (centre.x, centre.y, e.x, e.y, 1.2f);
            g.drawLine (e.x, e.y, p.x, p.y, 1.2f);
            g.fillEllipse (centre.x - 2.5f, centre.y - 2.5f, 5, 5);
        }
        else if (type == bodySpring)
        {
            juce::Path sp;
            const auto d = p - centre;
            const float len = std::max (1.0f, d.getDistanceFromOrigin());
            const juce::Point<float> nrm (-d.y / len * 5.0f, d.x / len * 5.0f);
            sp.startNewSubPath (centre);
            for (int k = 1; k < 14; ++k) sp.lineTo (centre + d * ((float) k / 14) + nrm * ((k & 1) ? 1.0f : -1.0f));
            sp.lineTo (p);
            g.setColour (c.withAlpha (0.3f));
            g.strokePath (sp, juce::PathStrokeType (1.0f));
        }
        else if (type == bodyOrbit)
        {
            juce::ColourGradient sun (c.withAlpha (0.22f), centre.x, centre.y, c.withAlpha (0.0f), centre.x + 26, centre.y, true);
            g.setGradientFill (sun);
            g.fillEllipse (centre.x - 26, centre.y - 26, 52, 52);
        }
        const auto& tr = trails[i];
        for (size_t k = 1; k < tr.size(); ++k)
        {
            const float a = (float) k / (float) tr.size();
            g.setColour (c.withAlpha (a * 0.4f));
            g.drawLine (tr[k - 1].x, tr[k - 1].y, tr[k].x, tr[k].y, 1.0f + 2.0f * a);
        }
        float flicker = type == bodyFirefly ? 0.7f + 0.3f * std::sin (time * 7.0f + i * 2.0f) : 1.0f;
        const float rad = grabbed == i ? 11.0f : (hover == i ? 10.0f : 8.0f);
        juce::ColourGradient glow (c.withAlpha (0.45f * flicker), p.x, p.y, c.withAlpha (0.0f), p.x + rad * 3.2f, p.y, true);
        g.setGradientFill (glow);
        g.fillEllipse (p.x - rad * 3.2f, p.y - rad * 3.2f, rad * 6.4f, rad * 6.4f);
        g.setColour (c.interpolatedWith (col::bright, 0.35f));
        g.fillEllipse (p.x - rad * 0.6f, p.y - rad * 0.6f, rad * 1.2f, rad * 1.2f);
    }
}

void Scene::drawOverlay (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (12, 8);
    auto& e = proc.engine;
    // what it hears
    juce::String key;
    const int sc = e.uiScale.load();
    if (e.uiKeyFromMidi.load()) key = juce::String ("MIDI ") + noteName (e.uiRoot.load());
    else if (e.listener.keyValid.load() || (int) proc.value (ml::key) > 0)
        key = juce::String (noteName (e.uiRoot.load())) + " " + juce::String (kScale[sc]).toLowerCase();
    else key = "...";
    juce::String txt = "listening: " + key;
    if (e.listener.chordValid.load() && e.inLevel.load() > 0.003f)
        txt << u8 ("  \xc2\xb7  chord ") << noteName (e.listener.chordRoot.load()) << (e.listener.chordMinor.load() ? "m" : "");
    g.setFont (font (13.0f, true));
    g.setColour (col::bright.withAlpha (0.85f));
    g.drawText (txt, r.removeFromTop (18), juce::Justification::centredLeft);

    auto tr = getLocalBounds().toFloat().reduced (12, 8);
    g.setFont (font (11.5f, true));
    g.setColour (col::fog.withAlpha (0.8f));
    const int lst = (int) proc.value (listen);
    g.drawText (juce::String (forecastName (proc.engine.values[forecast])).toUpperCase() + u8 ("  \xc2\xb7  ") + juce::String (e.activeGrains.load()) + " grains"
                + (lst > 0 ? u8 ("  \xc2\xb7  ") + juce::String (kListen[lst]).toUpperCase() : juce::String()),
                tr.removeFromTop (18), juce::Justification::centredRight);

    // time labels under the coast
    auto bl = getLocalBounds().toFloat().reduced (8, 6);
    g.setFont (font (10.0f));
    g.setColour (col::dim);
    g.drawText ("now", bl.removeFromBottom (14).removeFromLeft (60), juce::Justification::centredLeft);
    g.drawText ("-" + juce::String (e.uiMemSeconds.load(), 1) + " s", getLocalBounds().toFloat().reduced (8, 6).removeFromBottom (14).removeFromRight (80), juce::Justification::centredRight);

    if (mouseInside && drag == Drag::none)
    {
        g.setColour (col::dim.withAlpha (0.9f));
        g.setFont (font (10.5f));
        g.drawText (u8 ("drag: pull  \xc2\xb7  right-drag: blow  \xc2\xb7  shift-drag: paint wind  \xc2\xb7  double-click: emitter  \xc2\xb7  scroll: humidity  \xc2\xb7  shift-scroll: size  \xc2\xb7  space: freeze"),
                    getLocalBounds().toFloat().reduced (8, 6).removeFromBottom (14), juce::Justification::centred);
    }
    if (e.recorded() < (int64_t) (0.1 * e.getSampleRate()))
    {
        g.setColour (col::fog.withAlpha (0.6f));
        g.setFont (font (15.0f, true));
        g.drawText ("play something: Marine Layer remembers what it hears", getLocalBounds().toFloat(), juce::Justification::centred);
    }
}

// ---------------------------------------------------------------------------- mouse
int Scene::bodyAt (juce::Point<float> m) const
{
    for (int i = numBodies - 1; i >= 0; --i)
    {
        if ((int) proc.value (g1Type + i * bodyStride) == bodyOff) continue;
        const auto p = toScreen ((proc.engine.gravity.x (i) + 1.0f) * 0.5f, proc.engine.gravity.y (i));
        if (p.getDistanceFrom (m) < 16.0f) return i;
    }
    return -1;
}

int Scene::emitterAt (juce::Point<float> m) const
{
    for (int e = 0; e < 4; ++e)
        if (proc.engine.emOn[e].load() && toScreen (proc.engine.emX[e].load(), proc.engine.emY[e].load()).getDistanceFrom (m) < 14.0f) return e;
    return -1;
}

void Scene::mouseEnter (const juce::MouseEvent&) { mouseInside = true; grabKeyboardFocus(); }
void Scene::mouseExit (const juce::MouseEvent&) { mouseInside = false; hover = -1; }

void Scene::mouseMove (const juce::MouseEvent& e)
{
    mousePos = e.position;
    hover = bodyAt (e.position);
    setMouseCursor (hover >= 0 || emitterAt (e.position) >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::CrosshairCursor);
}

void Scene::setHand (juce::Point<float> s, float pull, float gst)
{
    const auto w = fromScreen (s);
    proc.setParam (handX, w.x);
    proc.setParam (handY, (w.y + 1.0f) * 0.5f);
    proc.setParam (handPull, pull);
    proc.setParam (handGust, gst);
}

void Scene::mouseDown (const juce::MouseEvent& e)
{
    mousePos = lastPos = e.position;
    lastMs = juce::Time::getMillisecondCounter();
    gust = 0; flickV = 0;
    grabbed = bodyAt (e.position);
    if (grabbed >= 0)
    {
        drag = Drag::body;
        auto& b = proc.engine.gravity.bodies[grabbed];
        const auto w = fromScreen (e.position);
        b.grabX.store (w.x * 2.0f - 1.0f); b.grabY.store (w.y);
        b.grabbed.store (true);
        return;
    }
    dragEmitter = emitterAt (e.position);
    if (dragEmitter >= 0) { drag = Drag::emitter; return; }
    if (e.mods.isShiftDown())
    {
        drag = Drag::stroke;
        stroke = ml::Stroke {};
        strokeScreen.clear();
        strokeScreen.push_back (e.position);
        const auto w = fromScreen (e.position);
        stroke.x[0] = w.x; stroke.y[0] = w.y; stroke.n = 1;
        return;
    }
    drag = Drag::hand;
    const float pull = (e.mods.isPopupMenu() || e.mods.isAltDown()) ? -1.0f : 1.0f;
    for (int p : { (int) handX, (int) handY, (int) handPull, (int) handGust }) proc.param (p)->beginChangeGesture();
    handGesture = true;
    setHand (e.position, pull, 0.0f);
}

void Scene::mouseDrag (const juce::MouseEvent& e)
{
    mousePos = e.position;
    const auto now = juce::Time::getMillisecondCounter();
    const float dt = std::max (1.0f, (float) (now - lastMs)) / 1000.0f;
    const auto d = e.position - lastPos;
    switch (drag)
    {
        case Drag::body:
        {
            auto& b = proc.engine.gravity.bodies[grabbed];
            const auto w = fromScreen (e.position);
            b.grabX.store (w.x * 2.0f - 1.0f); b.grabY.store (w.y);
            break;
        }
        case Drag::emitter:
        {
            const auto w = fromScreen (e.position);
            proc.moveEmitter (dragEmitter, w.x, w.y);
            break;
        }
        case Drag::stroke:
            if (e.position.getDistanceFrom (strokeScreen.back()) > 7.0f && stroke.n < ml::Stroke::maxPts)
            {
                strokeScreen.push_back (e.position);
                const auto w = fromScreen (e.position);
                stroke.x[stroke.n] = w.x; stroke.y[stroke.n] = w.y; ++stroke.n;
            }
            break;
        case Drag::hand:
        {
            // drag speed = gust (screen widths per second)
            const float speed = d.getDistanceFromOrigin() / dt / std::max (1.0f, (float) getWidth());
            gust = std::max (gust, juce::jlimit (0.0f, 1.0f, speed * 0.6f));
            flickV = flickV * 0.5f + 0.5f * speed;
            if (d.getDistanceFromOrigin() > 0.5f) flickDir = d;
            setHand (e.position, proc.value (handPull), gust);
            break;
        }
        case Drag::none: break;
    }
    lastPos = e.position; lastMs = now;
}

void Scene::mouseUp (const juce::MouseEvent& e)
{
    switch (drag)
    {
        case Drag::body:
            proc.engine.gravity.bodies[grabbed].grabbed.store (false);
            grabbed = -1;
            break;
        case Drag::emitter:
            if (! getLocalBounds().toFloat().expanded (-2).contains (e.position)) proc.removeEmitter (dragEmitter);
            dragEmitter = -1;
            break;
        case Drag::stroke:
            if (stroke.n >= 2) { stroke.ttl = 5.0f; proc.engine.addStroke (stroke); }
            strokeScreen.clear();
            break;
        case Drag::hand:
        {
            // quick flick = scatter burst
            const bool recent = juce::Time::getMillisecondCounter() - lastMs < 60;
            if (recent && flickV > 1.2f)
            {
                const auto w = fromScreen (e.position);
                proc.engine.burst (w.x, w.y, juce::jlimit (0.3f, 1.5f, flickV * 0.4f));
            }
            setHand (e.position, 0.0f, 0.0f);
            if (handGesture) for (int p : { (int) handX, (int) handY, (int) handPull, (int) handGust }) proc.param (p)->endChangeGesture();
            handGesture = false;
            break;
        }
        case Drag::none: break;
    }
    drag = Drag::none;
}

void Scene::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (bodyAt (e.position) >= 0) return;
    const int em = emitterAt (e.position);
    if (em >= 0) { proc.removeEmitter (em); return; }
    const auto w = fromScreen (e.position);
    proc.addEmitter (w.x, w.y);
}

void Scene::nudge (int p, float delta)
{
    auto* prm = proc.param (p);
    prm->beginChangeGesture();
    prm->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, prm->getValue() + delta));
    prm->endChangeGesture();
}

void Scene::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const float d = (std::abs (w.deltaY) > std::abs (w.deltaX) ? w.deltaY : w.deltaX) * (w.isReversed ? -1.0f : 1.0f) * 0.25f;
    nudge (e.mods.isShiftDown() ? gsize : humidity, d);
}

void Scene::mouseMagnify (const juce::MouseEvent&, float s) { nudge (gsize, (s - 1.0f) * 0.5f); }

bool Scene::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::spaceKey) { spaceHeld = true; proc.engine.holdFreeze = true; return true; }
    return false;
}

bool Scene::keyStateChanged (bool)
{
    if (spaceHeld && ! juce::KeyPress::isKeyCurrentlyDown (juce::KeyPress::spaceKey))
    {
        spaceHeld = false; proc.engine.holdFreeze = false;
        return true;
    }
    return false;
}
} // namespace ui
