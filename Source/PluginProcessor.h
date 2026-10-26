#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include "YinPitchDetector.h"

class TunerProcessor : public juce::AudioProcessor,
                       private juce::Thread
{
public:
    TunerProcessor();
    ~TunerProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Lipe Tunner"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // Resultado da analise (lido pela interface)
    std::atomic<float> detectedFreq { -1.0f };
    std::atomic<float> detectedLevelDb { -100.0f };

    // Tom de referencia (0 = desligado). Definido pela interface.
    std::atomic<float> toneFreq { 0.0f };

private:
    void run() override; // thread de analise

    static constexpr int ringSize = 1 << 16;
    static constexpr int ringMask = ringSize - 1;
    std::array<float, ringSize> ring {};
    std::atomic<int> writePos { 0 };

    YinPitchDetector detector;
    int required = 4096;

    // gerador do tom de referencia
    double currentSampleRate = 44100.0;
    double tonePhase = 0.0;
    float toneGain = 0.0f;
    float lastToneFreq = 440.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TunerProcessor)
};
