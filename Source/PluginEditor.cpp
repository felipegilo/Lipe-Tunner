#include "PluginEditor.h"

static juce::String noteLabel (int midi)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return juce::String (names[((midi % 12) + 12) % 12]) + juce::String (midi / 12 - 1);
}

TunerEditor::TunerEditor (TunerProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    // --- Instrumento
    auto* instParam = static_cast<juce::AudioParameterChoice*> (p.apvts.getParameter ("instrument"));
    instrumentBox.addItemList (instParam->choices, 1);
    instAtt = std::make_unique<APVTS::ComboBoxAttachment> (p.apvts, "instrument", instrumentBox);

    // --- Afinacao (preenchida manualmente conforme o instrumento)
    tuningBox.onChange = [this]
    {
        const int id = tuningBox.getSelectedId();
        if (id > 0 && stringMode)
            setTuningParam (id - 1);
    };

    // --- Sliders
    auto setupSlider = [] (juce::Slider& s, const juce::String& suffix)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 20);
        s.setTextValueSuffix (suffix);
    };
    setupSlider (refSlider, " Hz");
    setupSlider (gateSlider, " dB");
    refAtt  = std::make_unique<APVTS::SliderAttachment> (p.apvts, "refA", refSlider);
    gateAtt = std::make_unique<APVTS::SliderAttachment> (p.apvts, "gate", gateSlider);

    // --- Tema
    darkAtt = std::make_unique<APVTS::ButtonAttachment> (p.apvts, "dark", darkButton);

    // --- Rotulos
    instrumentLabel.setText ("Instr.", juce::dontSendNotification);
    tuningLabel.setText ("Afin.", juce::dontSendNotification);
    refLabel.setText ("A4", juce::dontSendNotification);
    gateLabel.setText ("Gate", juce::dontSendNotification);

    for (juce::Component* c : { (juce::Component*) &instrumentBox, (juce::Component*) &tuningBox,
                                (juce::Component*) &refSlider, (juce::Component*) &gateSlider,
                                (juce::Component*) &instrumentLabel, (juce::Component*) &tuningLabel,
                                (juce::Component*) &refLabel, (juce::Component*) &gateLabel,
                                (juce::Component*) &toneButton, (juce::Component*) &darkButton })
        addAndMakeVisible (c);

    // --- Estado inicial (sem resetar a afinacao salva)
    lastInstrument = (int) std::round (loadParam ("instrument"));
    lastDark = loadParam ("dark") > 0.5f;
    repopulateTunings (false);
    applyTheme();

    // --- Janela redimensionavel; tamanho lembrado entre sessoes
    setResizable (true, true);
    setResizeLimits (440, 420, 1100, 900);
    const int w = juce::jlimit (440, 1100, (int) p.apvts.state.getProperty ("uiW", 520));
    const int h = juce::jlimit (420, 900,  (int) p.apvts.state.getProperty ("uiH", 480));
    setSize (w, h);
    ready = true;

    startTimerHz (30);
}

TunerEditor::~TunerEditor()
{
    stopTimer();
    proc.toneFreq.store (0.0f);
}

void TunerEditor::setTuningParam (int idx)
{
    if (auto* prm = proc.apvts.getParameter ("tuning"))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) idx));
        prm->endChangeGesture();
    }
}

void TunerEditor::repopulateTunings (bool resetIndex)
{
    const auto& list = Tunings::get (lastInstrument);
    stringMode = lastInstrument > 0 && ! list.empty();

    tuningBox.clear (juce::dontSendNotification);
    detectedString = -1;

    if (! stringMode)
    {
        stringNotes.clear();
        selectedString = 0;
        tuningBox.addItem ("(modo cromatico)", 1);
        tuningBox.setSelectedId (1, juce::dontSendNotification);
        tuningBox.setEnabled (false);
        lastTuningIdx = -1;
        return;
    }

    tuningBox.setEnabled (true);
    for (size_t i = 0; i < list.size(); ++i)
        tuningBox.addItem (list[i].name, (int) i + 1);

    if (resetIndex)
        setTuningParam (0);

    const int idx = resetIndex ? 0
                               : juce::jlimit (0, (int) list.size() - 1, (int) std::round (loadParam ("tuning")));

    tuningBox.setSelectedId (idx + 1, juce::dontSendNotification);
    lastTuningIdx = idx;
    stringNotes = list[(size_t) idx].notes;
    selectedString = juce::jlimit (0, (int) stringNotes.size() - 1, selectedString);
}

TunerEditor::Palette TunerEditor::palette() const
{
    Palette p;
    if (loadParam ("dark") > 0.5f)
    {
        p.bg    = juce::Colour (0xff14161a);
        p.panel = juce::Colour (0xff22262d);
        p.text  = juce::Colours::white;
        p.sub   = juce::Colour (0xff9aa0a6);
        p.good  = juce::Colour (0xff3ddc84);
        p.warn  = juce::Colour (0xffffc107);
        p.bad   = juce::Colour (0xffff5252);
    }
    else
    {
        p.bg    = juce::Colour (0xfff2f3f5);
        p.panel = juce::Colour (0xffdde0e5);
        p.text  = juce::Colour (0xff1a1c20);
        p.sub   = juce::Colour (0xff6b7280);
        p.good  = juce::Colour (0xff1fa85a);
        p.warn  = juce::Colour (0xffd99a00);
        p.bad   = juce::Colour (0xffd93636);
    }
    return p;
}

void TunerEditor::applyTheme()
{
    const auto pal = palette();

    for (auto* l : { &instrumentLabel, &tuningLabel, &refLabel, &gateLabel })
        l->setColour (juce::Label::textColourId, pal.text);

    for (auto* b : { &toneButton, &darkButton })
    {
        b->setColour (juce::ToggleButton::textColourId, pal.text);
        b->setColour (juce::ToggleButton::tickColourId, pal.text);
        b->setColour (juce::ToggleButton::tickDisabledColourId, pal.sub);
    }

    for (auto* s : { &refSlider, &gateSlider })
    {
        s->setColour (juce::Slider::textBoxTextColourId, pal.text);
        s->setColour (juce::Slider::textBoxOutlineColourId, pal.panel);
    }

    for (auto* c : { &instrumentBox, &tuningBox })
    {
        c->setColour (juce::ComboBox::textColourId, pal.text);
        c->setColour (juce::ComboBox::backgroundColourId, pal.panel);
        c->setColour (juce::ComboBox::outlineColourId, pal.sub);
        c->setColour (juce::ComboBox::arrowColourId, pal.text);
    }

    repaint();
}

void TunerEditor::timerCallback()
{
    const float refA = loadParam ("refA");

    // --- Mudancas de tema / instrumento / afinacao (usuario ou automacao)
    const bool dark = loadParam ("dark") > 0.5f;
    if (dark != lastDark)
    {
        lastDark = dark;
        applyTheme();
    }

    const int instr = (int) std::round (loadParam ("instrument"));
    if (instr != lastInstrument)
    {
        lastInstrument = instr;
        repopulateTunings (true);
    }
    else if (stringMode)
    {
        const auto& list = Tunings::get (lastInstrument);
        const int tIdx = juce::jlimit (0, (int) list.size() - 1, (int) std::round (loadParam ("tuning")));
        if (tIdx != lastTuningIdx)
        {
            lastTuningIdx = tIdx;
            tuningBox.setSelectedId (tIdx + 1, juce::dontSendNotification);
            stringNotes = list[(size_t) tIdx].notes;
            selectedString = juce::jlimit (0, (int) stringNotes.size() - 1, selectedString);
        }
    }

    // --- Deteccao
    const float f = proc.detectedFreq.load();

    if (f > 0.0f)
    {
        const float n = 69.0f + 12.0f * std::log2 (f / refA);
        int target = 0, key = 0;

        if (stringMode && ! stringNotes.empty())
        {
            int best = 0;
            float bestDist = 1.0e9f;
            for (int i = 0; i < (int) stringNotes.size(); ++i)
            {
                const float d = std::abs (n - (float) stringNotes[(size_t) i]);
                if (d < bestDist) { bestDist = d; best = i; }
            }
            target = stringNotes[(size_t) best];
            key = best;
            detectedString = best;
        }
        else
        {
            target = (int) std::round (n);
            key = target;
            detectedString = -1;
        }

        cents = (n - (float) target) * 100.0f;
        freq = f;
        missCount = 0;

        if (! hasNote || key != lastKey)
            displayCents = cents;
        else
            displayCents += 0.35f * (cents - displayCents); // suaviza a agulha

        lastKey = key;
        noteName = noteLabel (target);
        hasNote = true;
    }
    else if (++missCount > 10)
    {
        hasNote = false;
        lastKey = -1;
        detectedString = -1;
    }

    // --- Tom de referencia
    if (toneButton.getToggleState())
    {
        int midi = 69; // A4 no modo cromatico
        if (stringMode && ! stringNotes.empty())
            midi = stringNotes[(size_t) juce::jlimit (0, (int) stringNotes.size() - 1, selectedString)];
        proc.toneFreq.store ((float) Tunings::midiToFreq (midi, refA));
    }
    else
    {
        proc.toneFreq.store (0.0f);
    }

    repaint();
}

TunerEditor::Layout TunerEditor::computeLayout() const
{
    Layout L;
    auto area = getLocalBounds().withTrimmedBottom (controlsH);

    if (stringMode)
        L.strings = area.removeFromTop (70).reduced (10, 8);

    L.msg   = area.removeFromBottom (44);
    L.meter = area.removeFromBottom (70).reduced (30, 10);
    L.freq  = area.removeFromBottom (28);
    L.note  = area;
    return L;
}

std::vector<juce::Rectangle<float>> TunerEditor::getStringRects (juce::Rectangle<int> area) const
{
    std::vector<juce::Rectangle<float>> rects;
    const int n = (int) stringNotes.size();
    if (n == 0 || area.isEmpty())
        return rects;

    const float gap = 10.0f;
    const float d = juce::jmin (52.0f, (float) area.getHeight(),
                                ((float) area.getWidth() - gap * (float) (n - 1)) / (float) n);
    const float total = d * (float) n + gap * (float) (n - 1);
    float x = (float) area.getCentreX() - total / 2.0f;
    const float y = (float) area.getCentreY() - d / 2.0f;

    for (int i = 0; i < n; ++i)
    {
        rects.push_back (juce::Rectangle<float> (x, y, d, d));
        x += d + gap;
    }
    return rects;
}

void TunerEditor::mouseDown (const juce::MouseEvent& e)
{
    if (! stringMode)
        return;

    const auto rects = getStringRects (computeLayout().strings);
    for (int i = 0; i < (int) rects.size(); ++i)
    {
        if (rects[(size_t) i].contains (e.position))
        {
            if (selectedString == i && toneButton.getToggleState())
            {
                toneButton.setToggleState (false, juce::dontSendNotification);
            }
            else
            {
                selectedString = i;
                toneButton.setToggleState (true, juce::dontSendNotification);
            }
            repaint();
            return;
        }
    }
}

void TunerEditor::paint (juce::Graphics& g)
{
    const auto pal = palette();
    g.fillAll (pal.bg);

    const auto L = computeLayout();
    const float ac = std::abs (displayCents);
    const auto state = ac < 5.0f ? pal.good : (ac < 15.0f ? pal.warn : pal.bad);

    // --- Cordas (modo por corda)
    if (stringMode)
    {
        const auto rects = getStringRects (L.strings);
        for (int i = 0; i < (int) rects.size(); ++i)
        {
            const auto r = rects[(size_t) i];
            const bool det = hasNote && i == detectedString;

            g.setColour (det ? state : pal.panel);
            g.fillEllipse (r);

            if (toneButton.getToggleState() && i == selectedString)
            {
                g.setColour (pal.text);
                g.drawEllipse (r.reduced (1.0f), 2.0f);
            }

            g.setColour (det ? juce::Colours::black : pal.text);
            g.setFont (r.getWidth() * 0.36f);
            g.drawText (noteLabel (stringNotes[(size_t) i]), r.toNearestInt(), juce::Justification::centred);
        }
    }

    // --- Medidor
    const auto meter = L.meter;
    g.setColour (pal.panel);
    g.fillRoundedRectangle (meter.toFloat(), 6.0f);

    const float halfW = (float) meter.getWidth() / 2.0f;
    const float cx = (float) meter.getCentreX();

    g.setColour (pal.good.withAlpha (0.18f));
    g.fillRect (cx - halfW * 0.1f, (float) meter.getY(), halfW * 0.2f, (float) meter.getHeight());

    for (int c = -50; c <= 50; c += 10)
    {
        const float x = cx + (float) c / 50.0f * halfW;
        const float len = (c == 0) ? (float) meter.getHeight() : 14.0f;
        g.setColour (c == 0 ? pal.text.withAlpha (0.8f) : pal.sub);
        g.drawLine (x, (float) meter.getBottom() - len, x, (float) meter.getBottom(), c == 0 ? 2.0f : 1.0f);
    }

    if (hasNote)
    {
        g.setColour (state);
        g.setFont (juce::jlimit (40.0f, 150.0f, (float) L.note.getHeight() * 0.9f));
        g.drawText (noteName, L.note, juce::Justification::centred);

        g.setColour (pal.sub);
        g.setFont (18.0f);
        g.drawText (juce::String (freq, 1) + " Hz", L.freq, juce::Justification::centred);

        const float clamped = juce::jlimit (-50.0f, 50.0f, displayCents);
        const float x = cx + clamped / 50.0f * halfW;
        g.setColour (state);
        g.fillRoundedRectangle (x - 3.0f, (float) meter.getY() + 3.0f, 6.0f, (float) meter.getHeight() - 6.0f, 3.0f);

        const juce::String msg = ac < 5.0f ? "Afinado"
                               : (displayCents < 0.0f ? "Aperte a corda" : "Afrouxe a corda");
        g.setFont (22.0f);
        g.drawText (msg + "  (" + juce::String (displayCents, 0) + " cents)", L.msg, juce::Justification::centred);
    }
    else
    {
        g.setColour (pal.sub);
        g.setFont (26.0f);
        g.drawText ("Toque uma corda", L.note, juce::Justification::centred);
    }
}

void TunerEditor::resized()
{
    auto c = getLocalBounds().removeFromBottom (controlsH).reduced (16, 8);
    const int rowH = 28, gap = 6;

    auto r1 = c.removeFromTop (rowH);  c.removeFromTop (gap);
    auto r2 = c.removeFromTop (rowH);  c.removeFromTop (gap);
    auto r3 = c.removeFromTop (rowH);  c.removeFromTop (gap);
    auto r4 = c.removeFromTop (rowH);

    auto half = r1.removeFromLeft (r1.getWidth() / 2);
    instrumentLabel.setBounds (half.removeFromLeft (52));
    instrumentBox.setBounds (half.reduced (4, 0));
    tuningLabel.setBounds (r1.removeFromLeft (52));
    tuningBox.setBounds (r1.reduced (4, 0));

    refLabel.setBounds (r2.removeFromLeft (52));
    refSlider.setBounds (r2);

    gateLabel.setBounds (r3.removeFromLeft (52));
    gateSlider.setBounds (r3);

    toneButton.setBounds (r4.removeFromLeft (r4.getWidth() / 2));
    darkButton.setBounds (r4);

    if (ready)
    {
        proc.apvts.state.setProperty ("uiW", getWidth(), nullptr);
        proc.apvts.state.setProperty ("uiH", getHeight(), nullptr);
    }
}
