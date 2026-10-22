#include "PluginEditor.h"
#include <BinaryData.h>
#include <cmath>

namespace
{
    const juce::Colour kAccent       (0xff8b6cff);
    const juce::Colour kAccentBright (0xffb6a4ff);
    const juce::Colour kBackground   (0xff0b0d12);
    const juce::Colour kPanel        (0xff141821);
    const juce::Colour kBorder       (0xff2b3240);

    struct KnobDef { const char* id; const char* name; };

    const KnobDef kKnobDefs[] =
    {
        { "size",       "TAMAÑO" },
        { "damping",    "ABSORCIÓN" },
        { "length",     "DURACIÓN" },
        { "deess",      "DE-ESSER" },
        { "lowCut",     "LOW CUT" },
        { "highCut",    "HIGH CUT" },
        { "drive",      "SATURACIÓN" },
        { "echo",       "ECO" },
        { "dry",        "VOZ SECA" },
        { "reverbMix",  "REVERB MIX" },
        { "tremoloRate","TREMOLO" }
    };

    const char* kCaptureIdle = "\xe2\x97\x8f  Capturar";
    const char* kCaptureActive = "\xe2\x96\xa0  Parar";
}

void RiserLabEditor::DragArea::setData (std::vector<float> newPeaks, const juce::File& newFile)
{
    peaks = std::move (newPeaks);
    file = newFile;
    repaint();
}

void RiserLabEditor::DragArea::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (kPanel);
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (kBorder);
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);

    if (peaks.empty())
    {
        g.setColour (juce::Colour (0xff8d96a8));
        g.setFont (13.0f);
        g.drawText (TXT ("LA FORMA DE ONDA DEL RISER APARECERÁ AQUÍ"),
                    getLocalBounds(), juce::Justification::centred);
        return;
    }

    g.setColour (kAccent);
    const float mid = r.getCentreY();
    const float h = r.getHeight() * 0.36f;

    for (size_t i = 0; i < peaks.size(); ++i)
    {
        const float x = r.getX() + 8.0f + ((float) i / (float) peaks.size()) * (r.getWidth() - 16.0f);
        const float a = juce::jmax (0.01f, peaks[i]) * h;
        g.drawLine (x, mid - a, x, mid + a, 1.4f);
    }

    g.setColour (juce::Colours::white.withAlpha (0.65f));
    g.setFont (11.0f);
    g.drawText (TXT ("ARRASTRA EL ARCHIVO AL DAW"),
                getLocalBounds().removeFromBottom (18), juce::Justification::centred);
}

void RiserLabEditor::DragArea::mouseDrag (const juce::MouseEvent& e)
{
    if (file.existsAsFile() && e.getDistanceFromDragStart() > 6)
        juce::DragAndDropContainer::performExternalDragDropOfFiles (
            juce::StringArray (file.getFullPathName()), false, this);
}

RiserLabEditor::RiserLabEditor (RiserLabProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setResizable (true, true);
    setResizeLimits (700, 600, 1400, 1100);
    setSize (820, 720);

    logoImage = juce::ImageFileFormat::loadFrom (BinaryData::CrayLDRLogo_png,
                                                  (size_t) BinaryData::CrayLDRLogo_pngSize);

    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedItemIndex (proc.getCurrentProgram(), juce::dontSendNotification);
    presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedItemIndex()); };
    presetBox.setColour (juce::ComboBox::backgroundColourId, kPanel);
    presetBox.setColour (juce::ComboBox::outlineColourId, kBorder);
    presetBox.setColour (juce::ComboBox::textColourId, juce::Colours::white);
    addAndMakeVisible (presetBox);

    captureButton.setButtonText (TXT (kCaptureIdle));
    captureButton.onClick = [this] { onCaptureClicked(); };
    captureButton.setColour (juce::TextButton::buttonColourId, kPanel);
    captureButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    addAndMakeVisible (captureButton);

    generateButton.setButtonText (TXT ("GENERAR RISER"));
    generateButton.onClick = [this] { onGenerateClicked(); };
    generateButton.setColour (juce::TextButton::buttonColourId, kAccent);
    generateButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    addAndMakeVisible (generateButton);

    statusLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setColour (juce::Label::textColourId, juce::Colour (0xffaab2c2));
    statusLabel.setFont (juce::Font (12.0f));
    addAndMakeVisible (statusLabel);

    reverbToggle.setButtonText (TXT ("REVERB"));
    reverbToggle.setColour (juce::ToggleButton::textColourId, juce::Colours::white);
    reverbToggle.setColour (juce::ToggleButton::tickColourId, kAccentBright);
    addAndMakeVisible (reverbToggle);
    reverbAttachment = std::make_unique<ButtonAttachment> (proc.apvts, "reverbEnabled", reverbToggle);

    reverbTypeBox.addItem ("HALL", 1);
    reverbTypeBox.addItem ("PLATE", 2);
    reverbTypeBox.addItem ("ROOM", 3);
    reverbTypeBox.setColour (juce::ComboBox::backgroundColourId, kPanel);
    reverbTypeBox.setColour (juce::ComboBox::outlineColourId, kBorder);
    reverbTypeBox.setColour (juce::ComboBox::textColourId, kAccentBright);
    addAndMakeVisible (reverbTypeBox);
    reverbTypeAttachment = std::make_unique<ComboBoxAttachment> (proc.apvts, "reverbType", reverbTypeBox);

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];
        k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 17);
        k.slider.setColour (juce::Slider::rotarySliderFillColourId, kAccent);
        k.slider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff353d4d));
        k.slider.setColour (juce::Slider::thumbColourId, kAccentBright);
        k.slider.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
        k.slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff10131a));
        k.slider.setColour (juce::Slider::textBoxOutlineColourId, kBorder);
        k.slider.setScrollWheelEnabled (false);

        const juce::String id (kKnobDefs[i].id);
        if (id == "lowCut")
            k.slider.textFromValueFunction = [] (double v) { return juce::String ((int) std::round (v)) + " Hz"; };
        else if (id == "highCut")
            k.slider.textFromValueFunction = [] (double v) { return v >= 1000.0 ? juce::String (v / 1000.0, 1) + " kHz" : juce::String ((int) std::round (v)) + " Hz"; };
        else if (id == "reverbMix")
            k.slider.textFromValueFunction = [] (double v) { return juce::String ((int) std::round (v * 100.0)) + "%"; };
        else if (id == "tremoloRate")
            k.slider.textFromValueFunction = [] (double v)
            {
                const int idx = juce::jlimit (0, 3, (int) std::round (v));
                const char* values[] = { "1/16", "1/8", "1/4", "1/2" };
                return juce::String (values[idx]);
            };

        addAndMakeVisible (k.slider);
        k.label.setText (TXT (kKnobDefs[i].name), juce::dontSendNotification);
        k.label.setJustificationType (juce::Justification::centred);
        k.label.setColour (juce::Label::textColourId, juce::Colour (0xff9ba6bb));
        k.label.setFont (juce::Font (10.5f, juce::Font::bold));
        addAndMakeVisible (k.label);
        k.attachment = std::make_unique<SliderAttachment> (proc.apvts, kKnobDefs[i].id, k.slider);
    }

    cutToggle.setButtonText (TXT ("CORTAR AL INICIO DE LA VOZ"));
    cutToggle.setColour (juce::ToggleButton::textColourId, juce::Colour (0xffc2c9d6));
    addAndMakeVisible (cutToggle);
    cutAttachment = std::make_unique<ButtonAttachment> (proc.apvts, "cut", cutToggle);

    addAndMakeVisible (dragArea);
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
    auto bounds = getLocalBounds().toFloat();

    juce::ColourGradient bg (juce::Colour (0xff111520), 0.0f, 0.0f,
                             kBackground, 0.0f, bounds.getBottom(), false);
    g.setGradientFill (bg);
    g.fillAll();

    g.setColour (kAccent.withAlpha (0.75f));
    g.fillRect (0, 0, getWidth(), 2);

    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (23.0f, juce::Font::bold));
    g.drawText ("RISER", titleArea, juce::Justification::centredLeft);

    g.setColour (kAccentBright);
    g.setFont (juce::Font (23.0f, juce::Font::bold));
    auto title2 = titleArea;
    title2.setX (title2.getX() + 76);
    title2.setWidth (70);
    g.drawText ("LAB", title2, juce::Justification::centredLeft);

    if (logoImage.isValid())
    {
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImageWithin (logoImage, logoArea.getX(), logoArea.getY(),
                           logoArea.getWidth(), logoArea.getHeight(),
                           juce::RectanglePlacement::centred);
    }
}

void RiserLabEditor::resized()
{
    auto area = getLocalBounds().reduced (18);

    auto header = area.removeFromTop (46);
    logoArea = header.removeFromRight (54).reduced (3);
    titleArea = header;
    area.removeFromTop (8);

    auto actionRow = area.removeFromTop (36);
    presetBox.setBounds (actionRow.removeFromRight (178));
    actionRow.removeFromRight (10);
    captureButton.setBounds (actionRow.removeFromLeft (132));
    actionRow.removeFromLeft (8);
    generateButton.setBounds (actionRow.removeFromLeft (146));
    actionRow.removeFromLeft (14);
    statusLabel.setBounds (actionRow);
    area.removeFromTop (9);

    auto reverbRow = area.removeFromTop (34);
    reverbToggle.setBounds (reverbRow.removeFromLeft (118));
    reverbRow.removeFromLeft (8);
    reverbTypeBox.setBounds (reverbRow.removeFromLeft (150));
    area.removeFromTop (8);

    const int gridHeight = juce::jmin (350, juce::jmax (270, area.getHeight() * 3 / 5));
    auto grid = area.removeFromTop (gridHeight);
    const int rowHeight = grid.getHeight() / 3;
    const int cellWidth = grid.getWidth() / 4;

    for (int i = 0; i < (int) knobs.size(); ++i)
    {
        const int row = i / 4;
        const int col = i % 4;
        auto cell = juce::Rectangle<int> (grid.getX() + col * cellWidth,
                                           grid.getY() + row * rowHeight,
                                           col == 3 ? grid.getRight() - (grid.getX() + col * cellWidth) : cellWidth,
                                           rowHeight).reduced (4, 2);
        auto labelArea = cell.removeFromTop (18);
        knobs[(size_t) i].label.setBounds (labelArea);
        knobs[(size_t) i].slider.setBounds (cell);
    }

    area.removeFromTop (2);
    cutToggle.setBounds (area.removeFromTop (26));
    area.removeFromTop (5);
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
        lastMessage = TXT ("RISER GENERADO. ARRASTRA AL DAW.");
    }
    else
    {
        lastMessage = error;
    }
}

void RiserLabEditor::timerCallback()
{
    const bool capturing = proc.isCapturing();
    if (! capturing && captureButton.getButtonText() == TXT (kCaptureActive))
        captureButton.setButtonText (TXT (kCaptureIdle));

    juce::String msg;
    const double seconds = proc.getCapturedSeconds();

    if (capturing)
        msg = TXT ("Capturando... ") + juce::String (seconds, 1) + " s";
    else if (lastMessage.isNotEmpty())
        msg = lastMessage;
    else if (seconds > 0.0)
        msg = TXT ("Capturado: ") + juce::String (seconds, 1) + " s";
    else
        msg = TXT ("Pulsa Capturar y reproduce la voz");

    if (statusLabel.getText() != msg)
        statusLabel.setText (msg, juce::dontSendNotification);
}
