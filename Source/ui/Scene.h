// The fog: the memory drawn as a glowing coastline (now on the left in sunset
// colours, ageing to sepia on the right), grain particles drifting over it,
// gravity bodies, emitters and your hand. Played with the mouse.
#pragma once
#include "Theme.h"
#include "../PluginProcessor.h"
#include <deque>

namespace ui
{
class Scene : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit Scene (PluginProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void update();

    void mouseMove (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMagnify (const juce::MouseEvent&, float scaleFactor) override;
    bool keyPressed (const juce::KeyPress&) override;
    bool keyStateChanged (bool isKeyDown) override;

private:
    juce::Point<float> toScreen (float x01, float y) const;
    juce::Point<float> fromScreen (juce::Point<float>) const;   // -> (x01, y -1..1)
    void drawBackground();
    void drawCoast (juce::Graphics&);
    void drawParticles (juce::Graphics&);
    void drawBodies (juce::Graphics&);
    void drawOverlay (juce::Graphics&);
    int bodyAt (juce::Point<float>) const;
    int emitterAt (juce::Point<float>) const;
    void setHand (juce::Point<float> screen, float pull, float gust);
    void nudge (int param, float delta);
    static juce::Image makeSprite (juce::Colour c, int size);

    PluginProcessor& proc;
    juce::Image bg, grain;
    juce::Image sprites[4], mist;
    std::deque<juce::Point<float>> trails[4];
    float time = 0, breathe = 0;

    enum class Drag { none, hand, body, emitter, stroke };
    Drag drag = Drag::none;
    int grabbed = -1, hover = -1, dragEmitter = -1;
    bool handGesture = false, spaceHeld = false, mouseInside = false;
    juce::Point<float> lastPos, mousePos; juce::uint32 lastMs = 0;
    float gust = 0, flickV = 0;
    juce::Point<float> flickDir;
    ml::Stroke stroke;
    std::vector<juce::Point<float>> strokeScreen;
    float pinchAcc = 0;
};
} // namespace ui
