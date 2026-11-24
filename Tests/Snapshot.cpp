// Renders the editor to a PNG while audio runs, so the UI can be checked
// without a DAW:  xvfb-run ./Snapshot out.png [advanced]
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <cmath>
#include <random>
#include <cstdlib>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    PluginProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);
    const bool adv = argc > 2 && juce::String (argv[2]) == "advanced";
    proc.advancedView = adv;
    proc.setParam (ml::g1Type, (float) ml::bodyFirefly);
    proc.setParam (ml::g2Type, (float) ml::bodyPendulum);
    proc.addEmitter (0.42f, 0.45f);
    proc.addEmitter (0.7f, -0.3f);
    proc.saveSnapshot (0);
    proc.saveSnapshot (2);
    proc.engine.toggleBookmark (3);
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    auto* pe = dynamic_cast<PluginEditor*> (ed.get());

    std::mt19937 rng (2); std::normal_distribution<float> nd (0.0f, 0.03f);
    juce::AudioBuffer<float> buf (2, 512); juce::MidiBuffer midi; double t = 0;
    const double f[] = { 130.81, 155.56, 196.0, 261.63 };
    for (int b = 0; b < 900; ++b)
    {
        for (int i = 0; i < 512; ++i, t += 1.0 / 48000.0)
        {
            double v = 0;
            for (double x : f) v += 0.1 * std::sin (2 * M_PI * x * t);
            const double beat = std::fmod (t, 0.5);
            v *= 0.6 + 0.4 * std::sin (t * 0.7);
            v += 0.5 * std::exp (-beat * 25.0) * std::sin (2 * M_PI * 70 * t) + nd (rng) * std::exp (-std::fmod (t + 0.25, 0.5) * 40.0) * 8.0;
            buf.setSample (0, i, (float) v); buf.setSample (1, i, (float) v * 0.9f);
        }
        if (b == 500 && std::getenv ("ML_HAND")) { proc.setParam (ml::handPull, 1.0f); proc.setParam (ml::handX, 0.25f); proc.setParam (ml::handY, 0.7f); }
        proc.processBlock (buf, midi);
        if (pe && b % 4 == 0) pe->tick();
    }
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    juce::File out (argc > 1 ? argv[1] : "/tmp/snapshot.png");
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat().writeImageToStream (img, os);
    return 0;
}
