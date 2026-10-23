#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <algorithm>
#include <cmath>
#include <vector>

// Parametros que usa el motor para generar el riser.
struct RiserSettings
{
    float size = 0.8f;         // tamano de la sala (reverb)
    float damping = 0.4f;      // absorcion de agudos
    float tailSeconds = 3.0f;  // duracion del riser
    float deess = 0.6f;        // cantidad de reduccion de sibilancia
    float lowCutHz = 20.0f;
    float highCutHz = 20000.0f;
    float reverbMix = 1.0f;
    int reverbType = 0;
    bool reverbEnabled = true;
    int tremoloRate = 0;
    double tempoBpm = 120.0;
    float drive = 0.0f;        // saturacion
    float echo = 0.0f;         // eco
    float dry = 0.0f;          // voz original mezclada (solo si no se corta)
    bool cutAtVocal = true;    // el archivo termina justo donde empieza la voz
};

namespace RiserEngine
{
    // Quita el silencio del principio y del final del audio capturado.
    inline void trimSilence (juce::AudioBuffer<float>& b, float threshold = 0.003f)
    {
        const int n = b.getNumSamples();
        const int numCh = b.getNumChannels();

        auto loud = [&] (int i)
        {
            for (int c = 0; c < numCh; ++c)
                if (std::abs (b.getSample (c, i)) > threshold)
                    return true;
            return false;
        };

        int s = 0, e = n;
        while (s < n && ! loud (s)) ++s;
        while (e > s && ! loud (e - 1)) --e;

        if (e <= s)
        {
            b.setSize (numCh, 0);
            return;
        }

        juce::AudioBuffer<float> t (numCh, e - s);
        for (int c = 0; c < numCh; ++c)
            t.copyFrom (c, 0, b, c, s, e - s);
        b = std::move (t);
    }

    // De-esser sencillo: detecta las "s" (agudos fuertes) y las atenua.
    inline void deEss (juce::AudioBuffer<float>& b, double sr, float amount)
    {
        if (amount <= 0.001f)
            return;

        const int n = b.getNumSamples();
        const int numCh = b.getNumChannels();
        const double cutoff = 6000.0;

        // Pasada 1: nivel maximo de la banda de agudos
        float peak = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
        {
            juce::IIRFilter hp;
            hp.setCoefficients (juce::IIRCoefficients::makeHighPass (sr, cutoff));
            const float* d = b.getReadPointer (ch);
            for (int i = 0; i < n; ++i)
                peak = std::max (peak, std::abs (hp.processSingleSampleRaw (d[i])));
        }

        if (peak < 1.0e-6f)
            return;

        const float threshold = peak * 0.3f;
        const float release = (float) std::exp (-1.0 / (0.02 * sr));

        // Pasada 2: atenuar la banda cuando supera el umbral
        for (int ch = 0; ch < numCh; ++ch)
        {
            juce::IIRFilter hp;
            hp.setCoefficients (juce::IIRCoefficients::makeHighPass (sr, cutoff));
            float* d = b.getWritePointer (ch);
            float env = 0.0f;

            for (int i = 0; i < n; ++i)
            {
                const float x = d[i];
                const float band = hp.processSingleSampleRaw (x);
                const float a = std::abs (band);
                env = a > env ? a : env * release;

                float g = env > threshold ? threshold / env : 1.0f;
                g = 1.0f + (g - 1.0f) * amount;

                d[i] = (x - band) + band * g;
            }
        }
    }

    // Tres voicings Hall/Plate/Room, con mezcla dry/wet.
    inline void applyReverb (juce::AudioBuffer<float>& b, double sr, const RiserSettings& s)
    {
        if (! s.reverbEnabled || s.reverbMix <= 0.0001f)
            return;

        juce::AudioBuffer<float> dry;
        dry.makeCopyOf (b);

        juce::Reverb rv;
        rv.setSampleRate (sr);
        juce::Reverb::Parameters p;
        p.roomSize = s.size;
        p.damping = s.damping;
        p.wetLevel = 1.0f;
        p.dryLevel = 0.0f;
        p.width = 1.0f;
        p.freezeMode = 0.0f;

        if (s.reverbType == 1) // Plate: brillante y denso
        {
            p.roomSize = juce::jlimit (0.45f, 0.85f, s.size);
            p.damping = juce::jlimit (0.05f, 0.35f, s.damping * 0.45f);
            p.width = 0.90f;
        }
        else if (s.reverbType == 2) // Room: corto y controlado
        {
            p.roomSize = juce::jlimit (0.15f, 0.48f, s.size * 0.48f);
            p.damping = juce::jlimit (0.45f, 0.90f, 0.45f + s.damping * 0.45f);
            p.width = 0.70f;
        }

        rv.setParameters (p);
        rv.processStereo (b.getWritePointer (0), b.getWritePointer (1), b.getNumSamples());
        const float mix = juce::jlimit (0.0f, 1.0f, s.reverbMix);

        for (int ch = 0; ch < b.getNumChannels(); ++ch)
        {
            float* wetData = b.getWritePointer (ch);
            const float* dryData = dry.getReadPointer (ch);
            for (int i = 0; i < b.getNumSamples(); ++i)
                wetData[i] = dryData[i] * (1.0f - mix) + wetData[i] * mix;
        }
    }

    inline void applyFilters (juce::AudioBuffer<float>& b, double sr, float lowCutHz, float highCutHz)
    {
        const double maxCut = sr * 0.45;
        const double low = juce::jlimit (20.0, maxCut - 1.0, (double) lowCutHz);
        const double high = juce::jlimit (low + 1.0, maxCut, (double) highCutHz);

        for (int ch = 0; ch < b.getNumChannels(); ++ch)
        {
            float* data = b.getWritePointer (ch);
            juce::IIRFilter hp, lp;
            hp.setCoefficients (juce::IIRCoefficients::makeHighPass (sr, low));
            lp.setCoefficients (juce::IIRCoefficients::makeLowPass (sr, high));
            hp.processSamples (data, b.getNumSamples());
            lp.processSamples (data, b.getNumSamples());
        }
    }

    inline void applyTremolo (juce::AudioBuffer<float>& b, double sr, const RiserSettings& s)
    {
        const int division = (s.tremoloRate == 0 ? 2 : s.tremoloRate == 1 ? 4 : s.tremoloRate == 2 ? 8 : s.tremoloRate == 3 ? 16 : 32);
        const double bpm = juce::jlimit (20.0, 300.0, s.tempoBpm);
        const double cyclesPerSecond = bpm / 60.0 * (4.0 / (double) division);
        const double phaseStep = juce::MathConstants<double>::twoPi * cyclesPerSecond / sr;
        const float depth = 0.65f;
        const int n = b.getNumSamples();

        for (int i = 0; i < n; ++i)
        {
            const float lfo = (float) (0.5 - 0.5 * std::cos (phaseStep * i));
            const float gain = 1.0f - depth * lfo;
            for (int ch = 0; ch < b.getNumChannels(); ++ch)
                b.getWritePointer (ch)[i] *= gain;
        }
    }

    // Filtro pasa-altos que sube de frecuencia a lo largo del riser.
    inline void applySweep (juce::AudioBuffer<float>& b, double sr, float amount)
    {
        if (amount <= 0.001f)
            return;

        const int n = b.getNumSamples();
        const double fStart = 80.0;
        const double fEnd = std::min (80.0 + (double) amount * 7000.0, sr * 0.45);

        for (int ch = 0; ch < b.getNumChannels(); ++ch)
        {
            juce::IIRFilter f;
            float* d = b.getWritePointer (ch);

            for (int i = 0; i < n; ++i)
            {
                if ((i & 31) == 0)
                {
                    const double t = (double) i / (double) n;
                    const double freq = fStart * std::pow (fEnd / fStart, t);
                    f.setCoefficients (juce::IIRCoefficients::makeHighPass (sr, freq));
                }
                d[i] = f.processSingleSampleRaw (d[i]);
            }
        }
    }

    // Saturacion suave (tanh).
    inline void applyDrive (juce::AudioBuffer<float>& b, float amount)
    {
        if (amount <= 0.001f)
            return;

        const float k = 1.0f + amount * 10.0f;
        const float norm = std::tanh (k);

        for (int ch = 0; ch < b.getNumChannels(); ++ch)
        {
            float* d = b.getWritePointer (ch);
            for (int i = 0; i < b.getNumSamples(); ++i)
                d[i] = std::tanh (k * d[i]) / norm;
        }
    }

    // Eco con realimentacion.
    inline void applyEcho (juce::AudioBuffer<float>& b, double sr, float amount)
    {
        if (amount <= 0.001f)
            return;

        const int delay = std::max (1, (int) (0.28 * sr));
        const float feedback = 0.45f;
        const float mix = amount * 0.7f;

        for (int ch = 0; ch < b.getNumChannels(); ++ch)
        {
            std::vector<float> line ((size_t) delay, 0.0f);
            int idx = 0;
            float* d = b.getWritePointer (ch);

            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                const float delayed = line[(size_t) idx];
                const float x = d[i];
                d[i] = x + delayed * mix;
                line[(size_t) idx] = x + delayed * feedback;
                if (++idx >= delay) idx = 0;
            }
        }
    }

    // Fundidos cortos (evitan clics) y normalizacion a -1 dBFS.
    inline void finish (juce::AudioBuffer<float>& b, double sr)
    {
        const int n = b.getNumSamples();
        if (n <= 0)
            return;

        const int fadeIn = std::min (n, (int) (0.010 * sr));
        const int fadeOut = std::min (n, (int) (0.002 * sr));

        for (int ch = 0; ch < b.getNumChannels(); ++ch)
        {
            float* d = b.getWritePointer (ch);
            for (int i = 0; i < fadeIn; ++i)
                d[i] *= (float) i / (float) fadeIn;
            for (int i = 0; i < fadeOut; ++i)
                d[n - 1 - i] *= (float) i / (float) fadeOut;
        }

        float peak = 0.0f;
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            peak = std::max (peak, b.getMagnitude (ch, 0, n));

        if (peak > 1.0e-6f)
            b.applyGain (0.89f / peak);
    }

    // Genera el riser completo a partir del audio capturado (estereo).
    //
    // Truco clasico del reverse reverb:
    //   1) se da la vuelta al audio, 2) se le aplica reverb con cola,
    //   3) se vuelve a dar la vuelta a todo.
    // Resultado: la reverb "crece" hacia el inicio de la voz.
    inline juce::AudioBuffer<float> render (const juce::AudioBuffer<float>& captured,
                                            double sr, const RiserSettings& s)
    {
        juce::AudioBuffer<float> src;
        src.makeCopyOf (captured);
        trimSilence (src);

        const int n = src.getNumSamples();
        if (n < 64 || src.getNumChannels() < 2)
            return juce::AudioBuffer<float>();

        deEss (src, sr, s.deess);

        const int tail = std::max (1, (int) ((double) s.tailSeconds * sr));
        const int total = n + tail;

        // 1) audio invertido seguido de silencio para la cola
        juce::AudioBuffer<float> wet (2, total);
        wet.clear();
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < n; ++i)
                wet.setSample (ch, i, src.getSample (ch, n - 1 - i));

        // 2) reverb
        applyReverb (wet, sr, s);

        // 3) dar la vuelta otra vez
        for (int ch = 0; ch < 2; ++ch)
            std::reverse (wet.getWritePointer (ch), wet.getWritePointer (ch) + total);

        // Ahora: [0, tail) = riser que crece, [tail, total) = zona de la voz.
        juce::AudioBuffer<float> out;

        if (s.cutAtVocal)
        {
            out.setSize (2, tail);
            for (int ch = 0; ch < 2; ++ch)
                out.copyFrom (ch, 0, wet, ch, 0, tail);
        }
        else
        {
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < n; ++i)
                    wet.addSample (ch, tail + i, src.getSample (ch, i) * s.dry);
            out = std::move (wet);
        }

        applyFilters (out, sr, s.lowCutHz, s.highCutHz);
        applyDrive (out, s.drive);
        applyEcho (out, sr, s.echo);
        applyTremolo (out, sr, s);
        finish (out, sr);

        return out;
    }

    // Guarda el buffer como WAV de 24 bits.
    inline bool writeWav (const juce::AudioBuffer<float>& b, double sr, const juce::File& file)
    {
        file.deleteFile();
        std::unique_ptr<juce::FileOutputStream> stream (file.createOutputStream());
        if (stream == nullptr)
            return false;

        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> writer (
            wav.createWriterFor (stream.get(), sr, (unsigned int) b.getNumChannels(), 24, {}, 0));

        if (writer == nullptr)
            return false;

        stream.release(); // el writer ya es dueno del stream
        return writer->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    }
}
