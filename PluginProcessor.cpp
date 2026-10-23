#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    struct Preset
    {
        const char* name;
        float size, damping, length, deess, lowCut, highCut, drive, echo, dry, reverbMix, reverbType, tremoloRate;
    };

    // 50 presets de fabrica con combinaciones radicalmente distintas.
    const Preset kPresets[] =
    {
        { "CLASSIC LIFT", 0.25f, 0.10f, 1.00f, 0.10f, 20.00f, 6000.00f, 0.00f, 0.00f, 0.25f, 0.30f, 0.0f, 0.0f },
        { "NEON HALO", 0.45f, 0.05f, 1.50f, 0.35f, 35.00f, 10000.00f, 0.15f, 0.45f, 0.00f, 0.55f, 1.0f, 1.0f },
        { "DARK MATTER", 0.72f, 0.60f, 2.00f, 0.60f, 60.00f, 14000.00f, 0.35f, 0.35f, 0.00f, 0.75f, 2.0f, 2.0f },
        { "LASER GATE", 0.95f, 0.85f, 2.50f, 0.80f, 90.00f, 18000.00f, 0.60f, 0.95f, 0.00f, 0.90f, 1.0f, 3.0f },
        { "ALIEN CHOIR", 0.58f, 0.35f, 3.00f, 0.95f, 120.00f, 9000.00f, 0.85f, 0.25f, 0.25f, 1.00f, 0.0f, 4.0f },
        { "CYBER STORM", 0.88f, 0.20f, 3.50f, 0.25f, 180.00f, 6000.00f, 0.25f, 0.85f, 0.00f, 0.65f, 2.0f, 2.0f },
        { "GLASS CATHEDRAL", 0.35f, 0.15f, 4.00f, 0.50f, 250.00f, 10000.00f, 0.50f, 0.15f, 0.00f, 0.40f, 1.0f, 1.0f },
        { "SUB ZERO", 0.99f, 0.75f, 5.00f, 0.70f, 400.00f, 14000.00f, 0.75f, 0.10f, 0.00f, 0.85f, 0.0f, 4.0f },
        { "ACID BLOOM", 0.64f, 0.90f, 6.00f, 0.00f, 80.00f, 18000.00f, 0.10f, 0.65f, 0.25f, 0.20f, 2.0f, 3.0f },
        { "BLACK HOLE", 0.18f, 0.45f, 7.50f, 0.40f, 150.00f, 9000.00f, 0.95f, 0.55f, 0.00f, 0.95f, 1.0f, 0.0f },
        { "STARFALL", 0.45f, 0.75f, 2.50f, 0.60f, 90.00f, 8000.00f, 0.85f, 0.10f, 0.00f, 0.75f, 1.0f, 3.0f },
        { "DIGITAL ANGEL", 0.72f, 0.90f, 3.00f, 0.80f, 120.00f, 12000.00f, 0.25f, 0.65f, 0.00f, 0.90f, 2.0f, 4.0f },
        { "BROKEN SIGNAL", 0.95f, 0.45f, 3.50f, 0.95f, 180.00f, 16000.00f, 0.50f, 0.55f, 0.25f, 1.00f, 1.0f, 2.0f },
        { "HYPERSPACE", 0.58f, 0.10f, 4.00f, 0.25f, 250.00f, 19500.00f, 0.75f, 0.00f, 0.00f, 0.65f, 0.0f, 1.0f },
        { "VOID SCREAM", 0.88f, 0.05f, 5.00f, 0.50f, 400.00f, 15000.00f, 0.10f, 0.45f, 0.00f, 0.40f, 2.0f, 4.0f },
        { "METAL RAIN", 0.35f, 0.60f, 6.00f, 0.70f, 80.00f, 8000.00f, 0.95f, 0.35f, 0.00f, 0.85f, 1.0f, 3.0f },
        { "PHANTOM TAIL", 0.99f, 0.85f, 7.50f, 0.00f, 150.00f, 12000.00f, 0.00f, 0.95f, 0.25f, 0.20f, 0.0f, 0.0f },
        { "PLASMA BURST", 0.64f, 0.35f, 1.00f, 0.40f, 20.00f, 16000.00f, 0.15f, 0.25f, 0.00f, 0.95f, 2.0f, 0.0f },
        { "DEEP ORBIT", 0.18f, 0.20f, 1.50f, 0.10f, 35.00f, 19500.00f, 0.35f, 0.85f, 0.00f, 0.30f, 1.0f, 1.0f },
        { "GLITCH RIOT", 0.25f, 0.15f, 2.00f, 0.35f, 60.00f, 15000.00f, 0.60f, 0.15f, 0.00f, 0.55f, 0.0f, 2.0f },
        { "CRYSTAL DROP", 0.72f, 0.35f, 4.00f, 0.95f, 250.00f, 10000.00f, 0.10f, 0.25f, 0.25f, 1.00f, 2.0f, 1.0f },
        { "GHOST MACHINE", 0.95f, 0.20f, 5.00f, 0.25f, 400.00f, 14000.00f, 0.95f, 0.85f, 0.00f, 0.65f, 1.0f, 4.0f },
        { "NIGHT DRIVE", 0.58f, 0.15f, 6.00f, 0.50f, 80.00f, 18000.00f, 0.00f, 0.15f, 0.00f, 0.40f, 0.0f, 3.0f },
        { "QUANTUM FLARE", 0.88f, 0.75f, 7.50f, 0.70f, 150.00f, 9000.00f, 0.15f, 0.10f, 0.00f, 0.85f, 2.0f, 0.0f },
        { "DYSTOPIA", 0.35f, 0.90f, 1.00f, 0.00f, 20.00f, 6000.00f, 0.35f, 0.65f, 0.25f, 0.20f, 1.0f, 0.0f },
        { "HEAVEN'S GATE", 0.99f, 0.45f, 1.50f, 0.40f, 35.00f, 10000.00f, 0.60f, 0.55f, 0.00f, 0.95f, 0.0f, 1.0f },
        { "RITUAL", 0.64f, 0.10f, 2.00f, 0.10f, 60.00f, 14000.00f, 0.85f, 0.00f, 0.00f, 0.30f, 2.0f, 2.0f },
        { "WARP SPEED", 0.18f, 0.05f, 2.50f, 0.35f, 90.00f, 18000.00f, 0.25f, 0.45f, 0.00f, 0.55f, 1.0f, 3.0f },
        { "DARK ANGEL", 0.25f, 0.60f, 3.00f, 0.60f, 120.00f, 9000.00f, 0.50f, 0.35f, 0.25f, 0.75f, 0.0f, 4.0f },
        { "FROZEN LASER", 0.45f, 0.85f, 3.50f, 0.80f, 180.00f, 6000.00f, 0.75f, 0.95f, 0.00f, 0.90f, 1.0f, 2.0f },
        { "CHROME WAVE", 0.95f, 0.05f, 7.50f, 0.50f, 150.00f, 12000.00f, 0.35f, 0.45f, 0.00f, 0.40f, 1.0f, 0.0f },
        { "RED GIANT", 0.58f, 0.60f, 1.00f, 0.70f, 20.00f, 16000.00f, 0.60f, 0.35f, 0.00f, 0.85f, 0.0f, 0.0f },
        { "VHS NIGHTMARE", 0.88f, 0.85f, 1.50f, 0.00f, 35.00f, 19500.00f, 0.85f, 0.95f, 0.25f, 0.20f, 2.0f, 1.0f },
        { "SYNTH TITAN", 0.35f, 0.35f, 2.00f, 0.40f, 60.00f, 15000.00f, 0.25f, 0.25f, 0.00f, 0.95f, 1.0f, 2.0f },
        { "SHATTERED SKY", 0.99f, 0.20f, 2.50f, 0.10f, 90.00f, 8000.00f, 0.50f, 0.85f, 0.00f, 0.30f, 0.0f, 3.0f },
        { "ALIEN ENGINE", 0.64f, 0.15f, 3.00f, 0.35f, 120.00f, 12000.00f, 0.75f, 0.15f, 0.00f, 0.55f, 2.0f, 4.0f },
        { "DREAM EATER", 0.18f, 0.75f, 3.50f, 0.60f, 180.00f, 16000.00f, 0.10f, 0.10f, 0.25f, 0.75f, 1.0f, 2.0f },
        { "ELECTRIC TEMPLE", 0.25f, 0.90f, 4.00f, 0.80f, 250.00f, 19500.00f, 0.95f, 0.65f, 0.00f, 0.90f, 0.0f, 1.0f },
        { "TIME FRACTURE", 0.45f, 0.45f, 5.00f, 0.95f, 400.00f, 15000.00f, 0.00f, 0.55f, 0.00f, 1.00f, 1.0f, 4.0f },
        { "VOID RUNNER", 0.72f, 0.10f, 6.00f, 0.25f, 80.00f, 8000.00f, 0.15f, 0.00f, 0.00f, 0.65f, 2.0f, 3.0f },
        { "ULTRAVIOLET", 0.58f, 0.90f, 2.00f, 0.00f, 60.00f, 14000.00f, 0.50f, 0.65f, 0.25f, 0.20f, 0.0f, 2.0f },
        { "BLACK ICE", 0.88f, 0.45f, 2.50f, 0.40f, 90.00f, 18000.00f, 0.75f, 0.55f, 0.00f, 0.95f, 2.0f, 3.0f },
        { "INFERNO RISE", 0.35f, 0.10f, 3.00f, 0.10f, 120.00f, 9000.00f, 0.10f, 0.00f, 0.00f, 0.30f, 1.0f, 4.0f },
        { "STAR ENGINE", 0.99f, 0.05f, 3.50f, 0.35f, 180.00f, 6000.00f, 0.95f, 0.45f, 0.00f, 0.55f, 0.0f, 2.0f },
        { "GHOST FREQUENCY", 0.64f, 0.60f, 4.00f, 0.60f, 250.00f, 10000.00f, 0.00f, 0.35f, 0.25f, 0.75f, 2.0f, 1.0f },
        { "NEBULA CRASH", 0.18f, 0.85f, 5.00f, 0.80f, 400.00f, 14000.00f, 0.15f, 0.95f, 0.00f, 0.90f, 1.0f, 4.0f },
        { "RADIANT CHAOS", 0.25f, 0.35f, 6.00f, 0.95f, 80.00f, 18000.00f, 0.35f, 0.25f, 0.00f, 1.00f, 0.0f, 3.0f },
        { "TUNNEL VISION", 0.45f, 0.20f, 7.50f, 0.25f, 150.00f, 9000.00f, 0.60f, 0.85f, 0.00f, 0.65f, 1.0f, 0.0f },
        { "FINAL BOSS", 0.72f, 0.15f, 1.00f, 0.50f, 20.00f, 6000.00f, 0.85f, 0.15f, 0.25f, 0.40f, 2.0f, 0.0f },
        { "ENDLESS ASCENT", 0.95f, 0.75f, 1.50f, 0.70f, 35.00f, 10000.00f, 0.25f, 0.10f, 0.00f, 0.85f, 1.0f, 1.0f }
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
                                R (0.0f, 4.0f, 1.0f), 0.0f));
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

    // Vista previa del riser generado por la salida del plugin.
    if (previewPlaying.load())
    {
        juce::CriticalSection::ScopedTryLockType lock (riserLock);
        if (lock.isLocked() && riserBuffer.getNumSamples() > 0)
        {
            const int pos = previewPosition.load();
            const int count = juce::jmin (buffer.getNumSamples(), riserBuffer.getNumSamples() - pos);
            if (count > 0)
            {
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                {
                    float* dst = buffer.getWritePointer (ch);
                    const float* src = riserBuffer.getReadPointer (juce::jmin (ch, riserBuffer.getNumChannels() - 1), pos);
                    juce::FloatVectorOperations::copy (dst, src, count);
                    juce::FloatVectorOperations::clear (dst + count, buffer.getNumSamples() - count);
                }
                previewPosition = pos + count;
            }
            if (count <= 0 || previewPosition.load() >= riserBuffer.getNumSamples())
                previewPlaying = false;
        }
        else
            previewPlaying = false;
        return;
    }

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

void RiserLabProcessor::setPreviewPlaying (bool shouldPlay)
{
    if (shouldPlay)
    {
        juce::ScopedLock sl (riserLock);
        if (riserBuffer.getNumSamples() <= 0)
        {
            previewPlaying = false;
            return;
        }
        previewPosition = 0;
        previewPlaying = true;
    }
    else
    {
        previewPlaying = false;
        previewPosition = 0;
    }
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
