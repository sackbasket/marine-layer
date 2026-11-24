#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "dsp/Presets.h"

using namespace ml;

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const auto* t = paramTable();
    for (int i = 0; i < numParams; ++i)
    {
        const auto& s = t[i];
        const juce::ParameterID pid { s.id, 1 };
        switch (s.type)
        {
            case PT::Bool:
                layout.add (std::make_unique<juce::AudioParameterBool> (pid, s.name, s.def > 0.5f));
                break;
            case PT::Choice:
            {
                juce::StringArray items;
                for (int k = 0; s.choices[k]; ++k) items.add (s.choices[k]);
                layout.add (std::make_unique<juce::AudioParameterChoice> (pid, s.name, items, (int) s.def));
                break;
            }
            case PT::Int:
                layout.add (std::make_unique<juce::AudioParameterInt> (pid, s.name, (int) s.min, (int) s.max, (int) s.def));
                break;
            case PT::Float:
            default:
            {
                juce::NormalisableRange<float> range (s.min, s.max);
                if (s.skewCentre > s.min && s.skewCentre < s.max) range.setSkewForCentre (s.skewCentre);
                const juce::String unit (s.unit);
                const int dec = s.decimals;
                const bool pct = unit == "%";
                const bool wx = unit == "wx";
                auto attrs = juce::AudioParameterFloatAttributes()
                    .withLabel (wx ? juce::String() : unit)
                    .withStringFromValueFunction ([unit, dec, pct, wx] (float v, int)
                    {
                        if (wx) return juce::String (forecastName (v));
                        if (pct) return juce::String (juce::roundToInt (v * 100.0f)) + " %";
                        if (unit == "Hz" && v >= 1000.0f) return juce::String (v / 1000.0f, 2) + " kHz";
                        if (unit == "st" || unit == "deg") return (v > 0.05f ? "+" : "") + juce::String (v, dec) + (unit == "st" ? " st" : juce::String::fromUTF8 ("\xc2\xb0"));
                        if (unit.isEmpty()) return juce::String (v, dec);
                        return juce::String (v, dec) + " " + unit;
                    })
                    .withValueFromStringFunction ([pct] (const juce::String& txt)
                    {
                        const float v = txt.retainCharacters ("0123456789.-").getFloatValue();
                        return pct ? v / 100.0f : v;
                    });
                layout.add (std::make_unique<juce::AudioParameterFloat> (pid, s.name, range, s.def, attrs));
                break;
            }
        }
    }
    return layout;
}

PluginProcessor::PluginProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "State", createLayout())
{
    const auto* t = paramTable();
    for (int i = 0; i < numParams; ++i)
    {
        params.push_back (apvts.getRawParameterValue (t[i].id));
        paramObjs.push_back (apvts.getParameter (t[i].id));
    }
    for (int i = 0; i < numMacros; ++i) macroNames.add (defaultMacroNames()[i]);
    loadFactoryPreset (0);
    undoStack.clear();
    startTimerHz (60);
}

PluginProcessor::~PluginProcessor() { stopTimer(); }

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return l.getMainInputChannelSet() == out;
}

void PluginProcessor::prepareToPlay (double sr, int bs)
{
    engine.prepare (sr, bs);
    monoTmp.setSize (1, std::max (16, bs));
    setLatencySamples (0);
}

void PluginProcessor::setParam (int p, float plain)
{
    if (auto* prm = param (p))
        prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
}

void PluginProcessor::setParamGesture (int p, float plain)
{
    if (auto* prm = param (p))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
        prm->endChangeGesture();
    }
}

// ---------------------------------------------------------------------------- modulation matrix
void PluginProcessor::computeModulation()
{
    const auto* t = paramTable();
    for (int i = 0; i < numParams; ++i) eff[i] = base[i];
    float src[sCount] {};
    for (int m = 0; m < numMacros; ++m) src[sM1 + m] = base[m1 + m];
    src[sLife] = engine.srcLife.load();
    src[sInput] = std::min (1.0f, engine.inLevel.load() * 2.0f);
    src[sForecast] = base[forecast];
    src[sHandX] = engine.effHandX.load();
    src[sHandY] = engine.effHandY.load();
    for (int b = 0; b < numBodies; ++b)
    {
        src[sB1x + 2 * b] = engine.gravity.x (b);
        src[sB1y + 2 * b] = engine.gravity.y (b);
    }
    int nd = 0; const int* dst = dstParams (nd);
    float off[numParams] {};
    for (int s = 0; s < numSlots; ++s)
    {
        const int si = (int) base[s1Src + s * slotStride];
        const int di = (int) base[s1Dst + s * slotStride];
        const float amt = base[s1Amt + s * slotStride];
        if (si <= 0 || si >= sCount || di <= 0 || di >= nd || std::abs (amt) < 1e-4f) continue;
        off[dst[di]] += amt * src[si];
    }
    for (int i = 1; i < nd; ++i)
    {
        const int p = dst[i];
        if (std::abs (off[p]) < 1e-9f) { modNorm[p].store (0.0f, std::memory_order_relaxed); continue; }
        const auto& info = t[p];
        auto* prm = paramObjs[(size_t) p];
        // offsets act on the knob's 0..1 travel, so skewed knobs modulate musically
        const float n0 = prm->convertTo0to1 (base[p]);
        const float n1 = juce::jlimit (0.0f, 1.0f, n0 + off[p]);
        eff[p] = juce::jlimit (info.min, info.max, prm->convertFrom0to1 (n1));
        modNorm[p].store (n1 - n0, std::memory_order_relaxed);
    }
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0) return;
    for (int i = 0; i < numParams; ++i) base[i] = params[(size_t) i]->load (std::memory_order_relaxed);
    computeModulation();
    std::copy (eff, eff + numParams, engine.values);

    HostInfo h;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm()) h.bpm = *bpm;
            h.playing = pos->getIsPlaying();
            if (auto ppq = pos->getPpqPosition()) { h.ppq = *ppq; h.ppqValid = true; }
            if (auto ts = pos->getTimeSignature()) if (ts->denominator > 0) h.barBeats = ts->numerator * 4.0 / ts->denominator;
        }
    lastBpm = h.bpm; lastBarBeats = h.barBeats;

    float* l = buffer.getWritePointer (0);
    if (buffer.getNumChannels() > 1)
    {
        float* r = buffer.getWritePointer (1);
        engine.process (l, r, l, r, n, midi, h);
    }
    else
    {
        if (monoTmp.getNumSamples() < n) monoTmp.setSize (1, n, false, false, true);
        float* tmp = monoTmp.getWritePointer (0);
        std::copy (l, l + n, tmp);
        engine.process (l, l, l, tmp, n, midi, h);
        for (int i = 0; i < n; ++i) l[i] = 0.5f * (l[i] + tmp[i]);
    }
}

// ---------------------------------------------------------------------------- presets
int PluginProcessor::getNumPrograms() { return (int) factoryPresets().size(); }
const juce::String PluginProcessor::getProgramName (int i)
{
    const auto& p = factoryPresets();
    return i >= 0 && i < (int) p.size() ? juce::String (p[(size_t) i].name) : juce::String();
}
void PluginProcessor::setCurrentProgram (int i) { if (i >= 0 && i < getNumPrograms()) loadFactoryPreset (i); }

void PluginProcessor::applyValues (const float* v, bool includeFreeze)
{
    for (int i = 0; i < numParams; ++i)
    {
        if (! includeFreeze && (i == freeze || i == handX || i == handY || i == handPull || i == handGust)) continue;
        setParam (i, v[i]);
    }
}

void PluginProcessor::loadFactoryPreset (int index)
{
    const auto& list = factoryPresets();
    if (index < 0 || index >= (int) list.size()) return;
    pushUndo();
    const auto& p = list[(size_t) index];
    float v[numParams];
    defaultValues (v);
    for (auto& kv : p.values) v[kv.first] = kv.second;
    applyValues (v, false);
    macroNames.clear();
    for (int i = 0; i < numMacros; ++i)
        macroNames.add (i < (int) p.macroNames.size() ? juce::String (p.macroNames[(size_t) i]) : juce::String (defaultMacroNames()[i]));
    presetName = p.name;
    currentProgram = index;
    ++presetVersion;
}

void PluginProcessor::rollDice (bool wild)
{
    pushUndo();
    float v[numParams];
    for (int i = 0; i < numParams; ++i) v[i] = value (i);
    ml::rollDice (v, wild, (uint32_t) (juce::Time::getMillisecondCounter() * 2654435761u + (uint32_t) ++diceCount));
    applyValues (v, false);
    presetName = wild ? "Wild Dice" : "Dice";
    ++presetVersion;
}


// ---------------------------------------------------------------------------- state
juce::ValueTree PluginProcessor::makeStateTree (bool forPreset, const juce::String& name)
{
    juce::ValueTree t ("MarineLayer");
    t.setProperty ("version", 1, nullptr);
    t.setProperty ("name", name, nullptr);
    t.setProperty ("macros", macroNames.joinIntoString ("|"), nullptr);
    juce::String em;
    for (int i = 0; i < 4; ++i)
        if (engine.emOn[i].load()) em << engine.emX[i].load() << "," << engine.emY[i].load() << ";";
    t.setProperty ("emitters", em, nullptr);
    t.appendChild (apvts.copyState(), nullptr);
    if (! forPreset)
    {
        t.setProperty ("uiScale", uiScale.load(), nullptr);
        t.setProperty ("advanced", advancedView.load(), nullptr);
        juce::StringArray bm;
        for (int b : engine.getBookmarks()) bm.add (juce::String (b));
        t.setProperty ("bookmarks", bm.joinIntoString (","), nullptr);
        juce::ValueTree sn ("Snapshots");
        for (int i = 0; i < numSnapshots; ++i)
        {
            const auto& s = snaps[(size_t) i];
            if (! s.set) continue;
            juce::ValueTree c ("Snap");
            c.setProperty ("index", i, nullptr);
            juce::MemoryBlock mb (s.v.data(), s.v.size() * sizeof (float));
            c.setProperty ("values", mb.toBase64Encoding(), nullptr);
            juce::String e;
            for (int k = 0; k < 4; ++k) if (s.em[k]) e << s.ex[k] << "," << s.ey[k] << ";";
            c.setProperty ("emitters", e, nullptr);
            sn.appendChild (c, nullptr);
        }
        t.appendChild (sn, nullptr);
    }
    return t;
}

static void parseEmitters (const juce::String& s, bool* on, float* x, float* y)
{
    for (int i = 0; i < 4; ++i) on[i] = false;
    auto items = juce::StringArray::fromTokens (s, ";", "");
    int k = 0;
    for (auto& it : items)
    {
        if (it.trim().isEmpty() || k >= 4) continue;
        auto xy = juce::StringArray::fromTokens (it, ",", "");
        if (xy.size() < 2) continue;
        on[k] = true; x[k] = juce::jlimit (0.0f, 1.0f, xy[0].getFloatValue()); y[k] = juce::jlimit (-1.0f, 1.0f, xy[1].getFloatValue());
        ++k;
    }
}

void PluginProcessor::applyStateTree (const juce::ValueTree& t, bool fromPreset)
{
    auto st = t.getChildWithName (apvts.state.getType());
    if (st.isValid())
    {
        if (fromPreset)
        {
            for (int i = 0; i < numParams; ++i)
            {
                if (i == freeze || i == handX || i == handY || i == handPull || i == handGust) continue;
                auto c = st.getChildWithProperty ("id", paramTable()[i].id);
                if (c.isValid()) setParam (i, (float) (double) c.getProperty ("value"));
            }
        }
        else apvts.replaceState (st);
    }
    presetName = t.getProperty ("name", presetName).toString();
    auto mn = juce::StringArray::fromTokens (t.getProperty ("macros").toString(), "|", "");
    for (int i = 0; i < numMacros; ++i)
        macroNames.set (i, i < mn.size() && mn[i].isNotEmpty() ? mn[i] : juce::String (defaultMacroNames()[i]));
    bool on[4]; float x[4] {}, y[4] {};
    parseEmitters (t.getProperty ("emitters").toString(), on, x, y);
    for (int i = 0; i < 4; ++i) { engine.emX[i] = x[i]; engine.emY[i] = y[i]; engine.emOn[i] = on[i]; }
    if (! fromPreset)
    {
        uiScale.store ((float) (double) t.getProperty ("uiScale", 1.0));
        advancedView.store ((bool) t.getProperty ("advanced", false));
        std::vector<int> bm;
        for (auto& s : juce::StringArray::fromTokens (t.getProperty ("bookmarks").toString(), ",", ""))
            if (s.isNotEmpty()) bm.push_back (s.getIntValue());
        engine.setBookmarks (bm);
        for (auto& s : snaps) s.set = false;
        auto sn = t.getChildWithName ("Snapshots");
        for (int c = 0; c < sn.getNumChildren(); ++c)
        {
            auto ch = sn.getChild (c);
            const int i = ch.getProperty ("index", -1);
            if (i < 0 || i >= numSnapshots) continue;
            juce::MemoryBlock mb;
            if (! mb.fromBase64Encoding (ch.getProperty ("values").toString())) continue;
            auto& s = snaps[(size_t) i];
            s.v.assign ((const float*) mb.getData(), (const float*) mb.getData() + mb.getSize() / sizeof (float));
            s.v.resize (numParams, 0.0f);
            parseEmitters (ch.getProperty ("emitters").toString(), s.em, s.ex, s.ey);
            s.set = true;
        }
    }
    ++presetVersion;
}

void PluginProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = makeStateTree (false, presetName).createXml()) copyXmlToBinary (*xml, dest);
}

void PluginProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto t = juce::ValueTree::fromXml (*xml);
        if (t.hasType ("MarineLayer")) applyStateTree (t, false);
        else if (t.hasType (apvts.state.getType())) apvts.replaceState (t);
    }
}

void PluginProcessor::pushUndo()
{
    undoStack.push_back (makeStateTree (true, presetName));
    if (undoStack.size() > 40) undoStack.erase (undoStack.begin());
}

bool PluginProcessor::undo()
{
    if (undoStack.empty()) return false;
    auto t = undoStack.back();
    undoStack.pop_back();
    applyStateTree (t, true);
    return true;
}

juce::File PluginProcessor::userPresetFolder()
{
    auto f = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
#if JUCE_MAC
                 .getChildFile ("Application Support")
#endif
                 .getChildFile ("Marine Layer").getChildFile ("Presets");
    f.createDirectory();
    return f;
}

bool PluginProcessor::savePresetFile (const juce::File& f, const juce::String& name)
{
    auto t = makeStateTree (true, name);
    if (auto xml = t.createXml())
        if (xml->writeTo (f)) { presetName = name; ++presetVersion; return true; }
    return false;
}

bool PluginProcessor::loadPresetFile (const juce::File& f)
{
    auto xml = juce::XmlDocument::parse (f);
    if (! xml) return false;
    auto t = juce::ValueTree::fromXml (*xml);
    if (! t.hasType ("MarineLayer")) return false;
    pushUndo();
    applyStateTree (t, true);
    presetName = f.getFileNameWithoutExtension();
    return true;
}

// ---------------------------------------------------------------------------- snapshots
bool PluginProcessor::snapshotSkips (int p)
{
    return p == freeze || p == handX || p == handY || p == handPull || p == handGust
        || p == morph || p == loopLen || p == output;
}

void PluginProcessor::saveSnapshot (int i)
{
    auto& s = snaps[(size_t) i];
    s.v.resize (numParams);
    for (int p = 0; p < numParams; ++p) s.v[(size_t) p] = value (p);
    for (int k = 0; k < 4; ++k) { s.em[k] = engine.emOn[k].load(); s.ex[k] = engine.emX[k].load(); s.ey[k] = engine.emY[k].load(); }
    s.set = true;
    activeSnapshot = i;
}

void PluginProcessor::recallSnapshot (int i)
{
    const auto& s = snaps[(size_t) i];
    if (! s.set) return;
    activeSnapshot = i;
    for (int k = 0; k < 4; ++k) { engine.emX[k] = s.ex[k]; engine.emY[k] = s.ey[k]; engine.emOn[k] = s.em[k]; }
    const int m = (int) value (morph);
    const double bb = lastBarBeats.load();
    const double beats[] = { 0, 1, 2, bb, 2 * bb, 4 * bb };
    morphDur = beats[juce::jlimit (0, 5, m)] * 60.0 / std::max (20.0, lastBpm.load());
    morphFrom.resize (numParams); morphTo = s.v;
    for (int p = 0; p < numParams; ++p) morphFrom[(size_t) p] = value (p);
    if (morphDur <= 0.0)
    {
        for (int p = 0; p < numParams; ++p) if (! snapshotSkips (p)) setParam (p, morphTo[(size_t) p]);
        morphing = false;
        return;
    }
    morphT = 0; morphing = true; morphLastMs = juce::Time::getMillisecondCounter();
}

void PluginProcessor::timerCallback()
{
    if (! morphing) return;
    const auto now = juce::Time::getMillisecondCounter();
    morphT = std::min (1.0, morphT + (now - morphLastMs) / 1000.0 / morphDur);
    morphLastMs = now;
    const float t = (float) morphT;
    const float e = t * t * (3.0f - 2.0f * t);
    for (int p = 0; p < numParams; ++p)
    {
        if (snapshotSkips (p)) continue;
        auto* prm = param (p);
        const float a = prm->convertTo0to1 (morphFrom[(size_t) p]), b = prm->convertTo0to1 (morphTo[(size_t) p]);
        if (paramTable()[p].type == PT::Float) prm->setValueNotifyingHost (a + (b - a) * e);
        else if (t >= 0.5f && std::abs (prm->getValue() - b) > 1e-6f) prm->setValueNotifyingHost (b);
    }
    if (morphT >= 1.0) morphing = false;
}

// ---------------------------------------------------------------------------- emitters
int PluginProcessor::addEmitter (float x, float y)
{
    for (int i = 0; i < 4; ++i)
        if (! engine.emOn[i].load()) { engine.emX[i] = x; engine.emY[i] = y; engine.emOn[i] = true; return i; }
    return -1;
}
void PluginProcessor::moveEmitter (int i, float x, float y) { if (i >= 0 && i < 4) { engine.emX[i] = x; engine.emY[i] = y; } }
void PluginProcessor::removeEmitter (int i) { if (i >= 0 && i < 4) engine.emOn[i] = false; }

juce::AudioProcessorEditor* PluginProcessor::createEditor() { return new PluginEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PluginProcessor(); }
