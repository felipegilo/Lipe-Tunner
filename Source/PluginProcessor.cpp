#include "PluginProcessor.h"
#include "PluginEditor.h"

static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "refA", 1 }, "Reference A4",
        NormalisableRange<float> (415.0f, 466.0f, 0.1f), 440.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "gate", 1 }, "Gate (dB)",
        NormalisableRange<float> (-80.0f, -20.0f, 1.0f), -55.0f));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "instrument", 1 }, "Instrumento",
        StringArray { "Cromatico", "Violao / Guitarra (6)", "Baixo (4)", "Baixo (5)", "Ukulele" }, 0));

    layout.add (std::make_unique<AudioParameterInt> (
        ParameterID { "tuning", 1 }, "Afinacao", 0, 9, 0));

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { "dark", 1 }, "Tema escuro", true));

    return layout;
}

TunerProcessor::TunerProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      juce::Thread ("Tuner analysis"),
      apvts (*this, nullptr, "STATE", createLayout())
{
}

TunerProcessor::~TunerProcessor()
{
    stopThread (1000);
}

void TunerProcessor::prepareToPlay (double sampleRate, int)
{
    stopThread (1000);

    currentSampleRate = sampleRate;
    tonePhase = 0.0;
    toneGain = 0.0f;

    detector.prepare (sampleRate);
    required = detector.getRequiredSamples();

    ring.fill (0.0f);
    writePos.store (0);
    detectedFreq.store (-1.0f);

    startThread();
}

bool TunerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void TunerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numChannels == 0 || numSamples == 0)
        return;

    // 1) Copia a ENTRADA (mono) para o buffer circular, antes de qualquer tom.
    const int pos = writePos.load (std::memory_order_relaxed);
    for (int i = 0; i < numSamples; ++i)
    {
        float s = 0.0f;
        for (int c = 0; c < numChannels; ++c)
            s += buffer.getReadPointer (c)[i];
        ring[(size_t) ((pos + i) & ringMask)] = s / (float) numChannels;
    }
    writePos.store ((pos + numSamples) & ringMask, std::memory_order_release);

    // 2) Tom de referencia (senoide com fade para nao dar estalo)
    const float tf = toneFreq.load();
    if (tf > 0.0f)
        lastToneFreq = tf;

    const float targetGain = tf > 0.0f ? 0.15f : 0.0f;

    if (targetGain > 0.0f || toneGain > 1.0e-5f)
    {
        const double inc = juce::MathConstants<double>::twoPi * (double) lastToneFreq / currentSampleRate;

        for (int i = 0; i < numSamples; ++i)
        {
            toneGain += (targetGain - toneGain) * 0.002f;
            const float s = toneGain * (float) std::sin (tonePhase);
            tonePhase += inc;
            if (tonePhase >= juce::MathConstants<double>::twoPi)
                tonePhase -= juce::MathConstants<double>::twoPi;

            for (int c = 0; c < numChannels; ++c)
                buffer.getWritePointer (c)[i] += s;
        }

        if (targetGain == 0.0f && toneGain < 1.0e-5f)
            toneGain = 0.0f;
    }
}

void TunerProcessor::run()
{
    std::vector<float> window ((size_t) required);
    std::vector<float> recent;
    int misses = 0;

    while (! threadShouldExit())
    {
        wait (40);
        if (threadShouldExit()) break;

        const int pos = writePos.load (std::memory_order_acquire);
        for (int i = 0; i < required; ++i)
            window[(size_t) i] = ring[(size_t) ((pos - required + i) & ringMask)];

        double sum = 0.0;
        for (float s : window) sum += (double) s * s;
        const float db = 20.0f * std::log10 ((float) std::sqrt (sum / required) + 1.0e-9f);
        detectedLevelDb.store (db);

        const float gate = apvts.getRawParameterValue ("gate")->load();
        if (db < gate)
        {
            detectedFreq.store (-1.0f);
            recent.clear();
            continue;
        }

        const auto r = detector.detect (window.data());
        if (r.frequency > 0.0f && r.clarity > 0.8f)
        {
            misses = 0;
            recent.push_back (r.frequency);
            if (recent.size() > 5) recent.erase (recent.begin());

            auto sorted = recent;
            std::sort (sorted.begin(), sorted.end());
            detectedFreq.store (sorted[sorted.size() / 2]); // mediana
        }
        else if (++misses > 3)
        {
            detectedFreq.store (-1.0f);
            recent.clear();
        }
    }
}

juce::AudioProcessorEditor* TunerProcessor::createEditor()
{
    return new TunerEditor (*this);
}

// O estado inclui todos os parametros (instrumento, afinacao, A4, gate, tema)
// e tambem o tamanho da janela (propriedades uiW / uiH).
void TunerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void TunerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TunerProcessor();
}
