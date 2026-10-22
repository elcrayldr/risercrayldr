#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    struct Preset
    {
        const char* name;
        float size, damping, length, deess, lowCut, highCut, drive, echo, dry, reverbMix, reverbType, tremoloRate;
    };

    const Preset kPresets[] =
    {
        { "Cl\xc3\xa1sico",         0.80f, 0.40f, 3.0f, 0.60f, 20.0f, 18000.0f, 0.00f, 0.00f, 0.0f, 1.00f, 0.0f, 0.0f },
        { "Est\xc3\xa9reo",          1.00f, 0.20f, 5.0f, 0.70f, 80.0f, 16000.0f, 0.00f, 0.25f, 0.0f, 0.85f, 1.0f, 1.0f },
        { "Oscuro saturado",        0.60f, 0.80f, 2.5f, 0.50f, 120.0f, 8000.0f, 0.60f, 0.00f, 0.0f, 0.80f, 2.0f, 2.0f },
        { "Eco brillante",          0.70f, 0.30f, 3.5f, 0.80f, 60.0f, 19000.0f, 0.10f, 0.50f, 0.0f, 0.90f, 1.0f, 0.0f }
    };

    constexpr int kNumPresets = (int) (sizeof (kPresets) / sizeof (kPresets[0]));
}

juce::AudioProcessorValueTreeState::ParameterLayout RiserLabProcessor::createLayout()
{
    using F = juce::AudioParameterFloat;
    using R = juce::NormalisableRange<float>;

    juce::AudioProcessorValueTreeState::ParameterLayout l;

    l.add (std::make_unique<F> (juce::ParameterID { "size", 1 },    TXT ("Tama\xc3\xb1o"),
                                R (0.1f, 1.0f, 0.001f), 0.8f));
    l.add (std::make_unique<F> (juce::ParameterID { "damping", 1 }, TXT ("Absorci\xc3\xb3n"),
                                R (0.0f, 1.0f, 0.001f), 0.4f));
    l.add (std::make_unique<F> (juce::ParameterID { "length", 1 },  TXT ("Duraci\xc3\xb3n (s)"),
                                R (0.5f, 8.0f, 0.01f), 3.0f));
    l.add (std::make_unique<F> (juce::ParameterID { "deess", 1 },   "De-esser",
                                R (0.0f, 1.0f, 0.001f), 0.6f));
    l.add (std::make_unique<F> (juce::ParameterID { "lowCut", 1 },  "Low Cut",
                                R (20.0f, 1000.0f, 1.0f), 20.0f));
    l.add (std::make_unique<F> (juce::ParameterID { "highCut", 1 }, "High Cut",
                                R (1000.0f, 20000.0f, 1.0f), 20000.0f));
    l.add (std::make_unique<F> (juce::ParameterID { "drive", 1 },   TXT ("Saturaci\xc3\xb3n"),
                                R (0.0f, 1.0f, 0.001f), 0.0f));
    l.add (std::make_unique<F> (juce::ParameterID { "echo", 1 },    "Eco",
                                R (0.0f, 1.0f, 0.001f), 0.0f));
    l.add (std::make_unique<F> (juce::ParameterID { "dry", 1 },     "Voz seca",
                                R (0.0f, 1.0f, 0.001f), 0.0f));
    l.add (std::make_unique<F> (juce::ParameterID { "reverbMix", 1 }, "Reverb Mix",
                                R (0.0f, 1.0f, 0.001f), 1.0f));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "reverbType", 1 },
                                "Reverb Type", juce::StringArray { "Hall", "Plate", "Room" }, 0));
    l.add (std::make_unique<F> (juce::ParameterID { "tremoloRate", 1 }, "Tremolo Rate",
                                R (0.0f, 3.0f, 1.0f), 0.0f));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "reverbEnabled", 1 },
                                "Reverb On", true));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "cut", 1 },
                                TXT ("Cortar en el inicio de la voz"), true));
    return l;
}

RiserLabProcessor::RiserLabProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
}

void RiserLabProcessor::prepareToPlay (double sampleRate, int)
{
    sampleRateHz = sampleRate;
    const int capacity = (int) (kMaxCaptureSeconds * sampleRate);

    juce::SpinLock::ScopedLockType lock (captureLock);
    if (captureBuffer.getNumSamples() != capacity)
    {
        captureBuffer.setSize (2, capacity);
        captureBuffer.clear();
        captureLength = 0;
    }
}

bool RiserLabProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == out;
}

void RiserLabProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // El audio pasa sin cambios; si esta capturando, ademas se copia.
    if (! capturing.load())
        return;

    const juce::SpinLock::ScopedTryLockType lock (captureLock);
    if (! lock.isLocked())
        return;

    const int pos = captureLength.load();
    const int numToCopy = juce::jmin (buffer.getNumSamples(), captureBuffer.getNumSamples() - pos);

    if (numToCopy > 0 && buffer.getNumChannels() > 0)
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            const float* src = buffer.getReadPointer (juce::jmin (ch, buffer.getNumChannels() - 1));
            captureBuffer.copyFrom (ch, pos, src, numToCopy);
        }
        captureLength = pos + numToCopy;
    }

    if (numToCopy < buffer.getNumSamples())
        capturing = false; // buffer lleno
}

void RiserLabProcessor::startCapture()
{
    {
        juce::SpinLock::ScopedLockType lock (captureLock);
        captureLength = 0;
    }
    capturing = true;
}

void RiserLabProcessor::stopCapture()
{
    capturing = false;
}

double RiserLabProcessor::getCapturedSeconds() const
{
    return (double) captureLength.load() / sampleRateHz;
}

bool RiserLabProcessor::generateRiser (juce::String& error)
{
    juce::AudioBuffer<float> in;

    {
        juce::SpinLock::ScopedLockType lock (captureLock);
        const int n = captureLength.load();

        if (n < (int) (0.1 * sampleRateHz))
        {
            error = TXT ("Primero captura audio (m\xc3\xadnimo 0,1 s).");
            return false;
        }

        in.setSize (2, n);
        for (int ch = 0; ch < 2; ++ch)
            in.copyFrom (ch, 0, captureBuffer, ch, 0, n);
    }

    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    RiserSettings s;
    s.size        = get ("size");
    s.damping     = get ("damping");
    s.tailSeconds = get ("length");
    s.deess       = get ("deess");
    s.lowCutHz    = get ("lowCut");
    s.highCutHz   = get ("highCut");
    s.drive       = get ("drive");
    s.echo        = get ("echo");
    s.dry         = get ("dry");
    s.reverbMix   = get ("reverbMix");
    s.reverbType  = (int) std::round (get ("reverbType"));
    s.reverbEnabled = get ("reverbEnabled") > 0.5f;
    s.tremoloRate = (int) std::round (get ("tremoloRate"));
    s.tempoBpm = 120.0;
    if (auto* playHead = getPlayHead())
        if (auto position = playHead->getPosition())
            if (auto bpm = position->getBpm())
                s.tempoBpm = *bpm;
    s.cutAtVocal  = get ("cut") > 0.5f;

    auto out = RiserEngine::render (in, sampleRateHz, s);

    if (out.getNumSamples() == 0)
    {
        error = TXT ("El audio capturado est\xc3\xa1 vac\xc3\xado o en silencio.");
        return false;
    }

    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("RiserLab");
    dir.createDirectory();
    auto file = dir.getChildFile ("riser_" + juce::String (juce::Time::currentTimeMillis()) + ".wav");

    if (! RiserEngine::writeWav (out, sampleRateHz, file))
    {
        error = TXT ("No se pudo guardar el archivo WAV.");
        return false;
    }

    {
        juce::ScopedLock sl (riserLock);
        riserBuffer = std::move (out);
        riserFile = file;
    }

    return true;
}

juce::File RiserLabProcessor::getRiserFile() const
{
    juce::ScopedLock sl (riserLock);
    return riserFile;
}

std::vector<float> RiserLabProcessor::getRiserPeaks (int bins) const
{
    juce::ScopedLock sl (riserLock);

    std::vector<float> peaks;
    const int n = riserBuffer.getNumSamples();
    if (n <= 0 || bins <= 0)
        return peaks;

    peaks.assign ((size_t) bins, 0.0f);

    for (int b = 0; b < bins; ++b)
    {
        const int s = (int) ((juce::int64) b * n / bins);
        const int e = (int) ((juce::int64) (b + 1) * n / bins);
        const int len = juce::jmax (1, e - s);

        float m = 0.0f;
        for (int ch = 0; ch < riserBuffer.getNumChannels(); ++ch)
            m = juce::jmax (m, riserBuffer.getMagnitude (ch, s, juce::jmin (len, n - s)));

        peaks[(size_t) b] = m;
    }

    return peaks;
}

// --- Presets de fabrica ---

int RiserLabProcessor::getNumPrograms() { return kNumPresets; }

const juce::String RiserLabProcessor::getProgramName (int index)
{
    if (index < 0 || index >= kNumPresets)
        return {};
    return TXT (kPresets[index].name);
}

void RiserLabProcessor::setCurrentProgram (int index)
{
    if (index < 0 || index >= kNumPresets)
        return;

    currentProgram = index;
    const auto& p = kPresets[index];

    auto set = [this] (const char* id, float value)
    {
        if (auto* prm = apvts.getParameter (id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (value));
    };

    set ("size", p.size);
    set ("damping", p.damping);
    set ("length", p.length);
    set ("deess", p.deess);
    set ("lowCut", p.lowCut);
    set ("highCut", p.highCut);
    set ("reverbMix", p.reverbMix);
    set ("reverbType", p.reverbType);
    set ("tremoloRate", p.tremoloRate);
    set ("drive", p.drive);
    set ("echo", p.echo);
    set ("dry", p.dry);
}

// --- Estado ---

void RiserLabProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void RiserLabProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* RiserLabProcessor::createEditor()
{
    return new RiserLabEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RiserLabProcessor();
}
