#include "PluginEditor.h"

namespace
{
    const juce::Colour kAccent (0xff7c5cff);
    const juce::Colour kBackground (0xff12141a);

    struct KnobDef { const char* id; const char* name; };

    const KnobDef kKnobDefs[] =
    {
        { "size",    "Tama\xc3\xb1o" },
        { "damping", "Absorci\xc3\xb3n" },
        { "length",  "Duraci\xc3\xb3n (s)" },
        { "deess",   "De-esser" },
        { "sweep",   "Filtro" },
        { "drive",   "Saturaci\xc3\xb3n" },
        { "echo",    "Eco" },
        { "dry",     "Voz seca" }
    };

    const char* kCaptureIdle = "\xe2\x97\x8f Capturar";
    const char* kCaptureActive = "\xe2\x96\xa0 Parar";
}

// ---------------- DragArea ----------------

void RiserLabEditor::DragArea::setData (std::vector<float> newPeaks, const juce::File& newFile)
{
    peaks = std::move (newPeaks);
    file = newFile;
    repaint();
}

void RiserLabEditor::DragArea::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();

    g.setColour (juce::Colour (0xff1c1f26));
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (juce::Colour (0xff3a3f4b));
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);

    if (peaks.empty())
    {
        g.setColour (juce::Colours::grey);
        g.setFont (14.0f);
        g.drawText (TXT ("Aqu\xc3\xad aparecer\xc3\xa1 el riser. Arr\xc3\xa1stralo a tu DAW."),
                    getLocalBounds(), juce::Justification::centred);
        return;
    }

    g.setColour (kAccent);
    const float mid = r.getCentreY();
    const float h = r.getHeight() * 0.38f;

    for (size_t i = 0; i < peaks.size(); ++i)
    {
        const float x = r.getX() + 6.0f + ((float) i / (float) peaks.size()) * (r.getWidth() - 12.0f);
        const float a = juce::jmax (0.01f, peaks[i]) * h;
        g.drawLine (x, mid - a, x, mid + a, 1.5f);
    }

    g.setColour (juce::Colours::white.withAlpha (0.7f));
    g.setFont (12.0f);
    g.drawText (TXT ("Arrastra este archivo al DAW"),
                getLocalBounds().removeFromBottom (20), juce::Justification::centred);
}

void RiserLabEditor::DragArea::mouseDrag (const juce::MouseEvent& e)
{
    if (file.existsAsFile() && e.getDistanceFromDragStart() > 6)
        juce::DragAndDropContainer::performExternalDragDropOfFiles (
            juce::StringArray (file.getFullPathName()), false, this);
}

// ---------------- Editor ----------------

RiserLabEditor::RiserLabEditor (RiserLabProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setSize (560, 480);

    // Presets
    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedItemIndex (proc.getCurrentProgram(), juce::dontSendNotification);
    presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedItemIndex()); };
    addAndMakeVisible (presetBox);

    // Botones
    captureButton.setButtonText (TXT (kCaptureIdle));
    captureButton.onClick = [this] { onCaptureClicked(); };
    addAndMakeVisible (captureButton);

    generateButton.setButtonText ("Generar riser");
    generateButton.setColour (juce::TextButton::buttonColourId, kAccent);
    generateButton.onClick = [this] { onGenerateClicked(); };
    addAndMakeVisible (generateButton);

    statusLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (statusLabel);

    // Knobs
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];

        k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
        k.slider.setColour (juce::Slider::rotarySliderFillColourId, kAccent);
        addAndMakeVisible (k.slider);

        k.label.setText (TXT (kKnobDefs[i].name), juce::dontSendNotification);
        k.label.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (k.label);

        k.attachment = std::make_unique<SliderAttachment> (proc.apvts, kKnobDefs[i].id, k.slider);
    }

    // Opcion de corte
    cutToggle.setButtonText (TXT ("Cortar en el inicio de la voz (el riser termina donde empieza la voz)"));
    addAndMakeVisible (cutToggle);
    cutAttachment = std::make_unique<ButtonAttachment> (proc.apvts, "cut", cutToggle);

    addAndMakeVisible (dragArea);

    // Si ya habia un riser generado (al reabrir la ventana), mostrarlo
    auto peaks = proc.getRiserPeaks (300);
    if (! peaks.empty())
        dragArea.setData (std::move (peaks), proc.getRiserFile());

    startTimerHz (15);
}

RiserLabEditor::~RiserLabEditor()
{
    stopTimer();
}

void RiserLabEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBackground);

    g.setColour (juce::Colours::white);
    g.setFont (22.0f);
    g.drawText ("RiserLab", titleArea, juce::Justification::centredLeft);
}

void RiserLabEditor::resized()
{
    auto area = getLocalBounds().reduced (16);

    // Titulo + presets
    auto top = area.removeFromTop (30);
    presetBox.setBounds (top.removeFromRight (170));
    titleArea = top;
    area.removeFromTop (8);

    // Botones + estado
    auto row = area.removeFromTop (36);
    captureButton.setBounds (row.removeFromLeft (150));
    row.removeFromLeft (8);
    generateButton.setBounds (row.removeFromLeft (150));
    row.removeFromLeft (12);
    statusLabel.setBounds (row);
    area.removeFromTop (8);

    // Knobs: 2 filas de 4
    for (int r = 0; r < 2; ++r)
    {
        auto knobRow = area.removeFromTop (106);
        const int w = knobRow.getWidth() / 4;

        for (int c = 0; c < 4; ++c)
        {
            auto cell = knobRow.removeFromLeft (w);
            auto& k = knobs[(size_t) (r * 4 + c)];
            k.label.setBounds (cell.removeFromTop (16));
            k.slider.setBounds (cell);
        }
    }

    area.removeFromTop (4);
    cutToggle.setBounds (area.removeFromTop (28));
    area.removeFromTop (8);
    dragArea.setBounds (area);
}

void RiserLabEditor::onCaptureClicked()
{
    if (! proc.isCapturing())
    {
        lastMessage.clear();
        proc.startCapture();
        captureButton.setButtonText (TXT (kCaptureActive));
    }
    else
    {
        proc.stopCapture();
        captureButton.setButtonText (TXT (kCaptureIdle));
    }
}

void RiserLabEditor::onGenerateClicked()
{
    proc.stopCapture();
    captureButton.setButtonText (TXT (kCaptureIdle));

    juce::String error;
    if (proc.generateRiser (error))
    {
        dragArea.setData (proc.getRiserPeaks (300), proc.getRiserFile());
        lastMessage = TXT ("\xc2\xa1Riser listo! Arr\xc3\xa1stralo a tu pista.");
    }
    else
    {
        lastMessage = error;
    }
}

void RiserLabEditor::timerCallback()
{
    const bool capturing = proc.isCapturing();

    // Si el buffer se llena, la captura se para sola: sincroniza el boton
    if (! capturing && captureButton.getButtonText() == TXT (kCaptureActive))
        captureButton.setButtonText (TXT (kCaptureIdle));

    juce::String msg;
    const double seconds = proc.getCapturedSeconds();

    if (capturing)
        msg = TXT ("Capturando\xe2\x80\xa6 ") + juce::String (seconds, 1) + " s";
    else if (lastMessage.isNotEmpty())
        msg = lastMessage;
    else if (seconds > 0.0)
        msg = TXT ("Capturado: ") + juce::String (seconds, 1) + " s";
    else
        msg = TXT ("Pulsa Capturar y reproduce la voz en el DAW");

    if (statusLabel.getText() != msg)
        statusLabel.setText (msg, juce::dontSendNotification);
}
