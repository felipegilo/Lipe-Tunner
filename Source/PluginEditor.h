#pragma once
#include "PluginProcessor.h"
#include "Tunings.h"
#include <vector>
#include <memory>

class TunerEditor : public juce::AudioProcessorEditor,
                    private juce::Timer
{
public:
    explicit TunerEditor (TunerProcessor&);
    ~TunerEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    struct Layout  { juce::Rectangle<int> strings, note, freq, meter, msg; };
    struct Palette { juce::Colour bg, panel, text, sub, good, warn, bad; };

    Layout computeLayout() const;
    std::vector<juce::Rectangle<float>> getStringRects (juce::Rectangle<int> area) const;
    Palette palette() const;

    void timerCallback() override;
    void repopulateTunings (bool resetIndex);
    void applyTheme();
    void setTuningParam (int idx);
    float loadParam (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }

    static constexpr int controlsH = 170;

    TunerProcessor& proc;

    juce::ComboBox instrumentBox, tuningBox;
    juce::Slider refSlider, gateSlider;
    juce::Label instrumentLabel, tuningLabel, refLabel, gateLabel;
    juce::ToggleButton toneButton { "Tom de referencia" };
    juce::ToggleButton darkButton { "Tema escuro" };

    std::unique_ptr<APVTS::ComboBoxAttachment> instAtt;
    std::unique_ptr<APVTS::SliderAttachment>   refAtt, gateAtt;
    std::unique_ptr<APVTS::ButtonAttachment>   darkAtt;

    // estado da interface
    int  lastInstrument = -1, lastTuningIdx = -1;
    bool lastDark = true;
    bool stringMode = false;
    std::vector<int> stringNotes;
    int  selectedString = 0, detectedString = -1;

    bool hasNote = false;
    int  missCount = 0, lastKey = -1;
    float freq = 0.0f, cents = 0.0f, displayCents = 0.0f;
    juce::String noteName;

    bool ready = false; // so salva o tamanho depois de montar a janela

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TunerEditor)
};
