#include "PluginEditor.h"
#include <BinaryData.h>
#include <cmath>

namespace
{
    const juce::Colour kAccent       (0xff7c5cff);
    const juce::Colour kAccentBright (0xff42e8ff);
    const juce::Colour kBackground   (0xff080b10);
    const juce::Colour kPanel        (0xff101722);
    const juce::Colour kBorder       (0xff202b3a);
    const juce::Colour kInk          (0xffe8f0fb);
    const juce::Colour kMuted        (0xff8c9bb0);

    struct KnobDef { const char* id; const char* name; };

    const KnobDef kKnobDefs[] =
    {
        { "size",       "SIZE" },
        { "damping",    "DAMPING" },
        { "length",     "LENGTH" },
        { "deess",      "DE-ESSER" },
        { "lowCut",     "LOW CUT" },
        { "highCut",    "HIGH CUT" },
        { "drive",      "DRIVE" },
        { "echo",       "ECHO" },
        { "dry",        "DRY VOICE" },
        { "reverbMix",  "REVERB MIX" },
        { "tremoloRate","TREMOLO RATE" }
    };
}

void RiserLabEditor::CaptureControl::paintButton (juce::Graphics& g, bool highlighted, bool pressed)
{
    auto b = getLocalBounds().toFloat();
    auto r = b.reduced (3.0f);

    g.setColour (juce::Colour (0x4022d3ee));
    g.drawRoundedRectangle (r.expanded (3.0f), 18.0f, 1.0f);
    g.setColour (juce::Colour (0xff111a27));
    g.fillRoundedRectangle (r, 16.0f);
    g.setColour (highlighted ? kAccentBright : kBorder);
    g.drawRoundedRectangle (r, 16.0f, pressed ? 2.2f : 1.2f);

    auto centre = r.getCentre();
    g.setColour (juce::Colour (0xff1a2738));
    g.fillEllipse (juce::Rectangle<float> (68.0f, 68.0f).withCentre (centre));

    g.setColour (active ? kAccentBright : kAccent);
    g.drawEllipse (juce::Rectangle<float> (68.0f, 68.0f).withCentre (centre), 2.0f);

    // One label only: no secondary text or status indicators inside the capture circle.
    g.setColour (kInk);
    g.setFont (juce::Font (11.0f, juce::Font::bold));
    auto actionText = juce::Rectangle<int> ((int) centre.x - 34, (int) centre.y - 12, 68, 24);
    g.drawFittedText ("CAPTURE", actionText,
                      juce::Justification::centred, 1);
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
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (kBorder);
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);

    if (peaks.empty())
    {
        g.setColour (kAccentBright);
        g.setFont (juce::Font (12.0f, juce::Font::bold));
        g.drawText (TXT ("DRAG & DROP  /  DAW EXPORT"),
                    getLocalBounds().removeFromTop (24), juce::Justification::centred);
        g.setColour (kMuted);
        g.setFont (11.0f);
        g.drawText (TXT ("WAVEFORM PREVIEW  •  GENERATE TO EXPORT"),
                    getLocalBounds().withTrimmedTop (22), juce::Justification::centred);
        return;
    }

    g.setColour (kAccentBright);
    const float mid = r.getCentreY();
    const float h = r.getHeight() * 0.36f;

    for (size_t i = 0; i < peaks.size(); ++i)
    {
        const float x = r.getX() + 8.0f + ((float) i / (float) peaks.size()) * (r.getWidth() - 16.0f);
        const float a = juce::jmax (0.01f, peaks[i]) * h;
        g.drawLine (x, mid - a, x, mid + a, 1.4f);
    }

    g.setColour (kInk);
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
    setResizeLimits (760, 650, 1400, 1100);
    setSize (1040, 790);

    logoImage = juce::ImageFileFormat::loadFrom (BinaryData::CrayLDRLogo_png,
                                                  (size_t) BinaryData::CrayLDRLogo_pngSize);

    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedItemIndex (proc.getCurrentProgram(), juce::dontSendNotification);
    presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedItemIndex()); };
    presetBox.setColour (juce::ComboBox::backgroundColourId, kPanel);
    presetBox.setColour (juce::ComboBox::outlineColourId, kBorder);
    presetBox.setColour (juce::ComboBox::textColourId, kInk);
    addAndMakeVisible (presetBox);

    captureButton.onClick = [this] { onCaptureClicked(); };
    captureButton.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    addAndMakeVisible (captureButton);

    previewButton.setButtonText (TXT ("PLAY"));
    previewButton.onClick = [this] { onPreviewClicked(); };
    previewButton.setTooltip (TXT ("Escucha el riser generado antes de arrastrarlo al DAW"));
    previewButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff12303a));
    previewButton.setColour (juce::TextButton::textColourOffId, kAccentBright);
    previewButton.setColour (juce::TextButton::textColourOnId, kAccentBright);
    addAndMakeVisible (previewButton);

    generateButton.setButtonText (TXT ("✦  GENERAR RISER"));
    generateButton.onClick = [this] { onGenerateClicked(); };
    generateButton.setTooltip (TXT ("Genera el riser con la voz capturada y los parámetros actuales"));
    generateButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff7138d8));
    generateButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    generateButton.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    addAndMakeVisible (generateButton);

    statusLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setColour (juce::Label::textColourId, kMuted);
    statusLabel.setFont (juce::Font (12.0f));
    addAndMakeVisible (statusLabel);

    reverbToggle.setButtonText (TXT ("REVERB"));
    reverbToggle.setColour (juce::ToggleButton::textColourId, kInk);
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
        k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 78, 19);
        k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                      juce::MathConstants<float>::pi * 2.75f, true);
        k.slider.setColour (juce::Slider::rotarySliderFillColourId, kAccent);
        k.slider.setColour (juce::Slider::rotarySliderOutlineColourId, kBorder);
        k.slider.setColour (juce::Slider::thumbColourId, kAccentBright);
        k.slider.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffc9b8ff));
        k.slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff090d15));
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
                const int idx = juce::jlimit (0, 4, (int) std::round (v));
                const char* values[] = { "1/2", "1/4", "1/8", "1/16", "1/32" };
                return juce::String (values[idx]);
            };

        addAndMakeVisible (k.slider);
        k.label.setText (TXT (kKnobDefs[i].name), juce::dontSendNotification);
        k.label.setJustificationType (juce::Justification::centred);
        k.label.setColour (juce::Label::textColourId, juce::Colour (0xffb8c4d8));
        k.label.setFont (juce::Font (10.0f, juce::Font::bold));
        addAndMakeVisible (k.label);
        k.attachment = std::make_unique<SliderAttachment> (proc.apvts, kKnobDefs[i].id, k.slider);
    }

    cutToggle.setButtonText (TXT ("CORTAR AL INICIO DE LA VOZ"));
    cutToggle.setColour (juce::ToggleButton::textColourId, kInk);
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
    juce::ColourGradient glow (juce::Colour (0xff25124a), bounds.getCentreX(), 0.0f,
                               kBackground, bounds.getCentreX(), bounds.getHeight(), false);
    g.setGradientFill (glow);
    g.fillRoundedRectangle (bounds.reduced (1.0f), 13.0f);

    g.setColour (juce::Colour (0xff171f30));
    g.drawRoundedRectangle (bounds.reduced (0.7f), 13.0f, 1.2f);

    auto header = getLocalBounds().reduced (20).removeFromTop (56);
    g.setColour (kBorder);
    g.drawLine ((float) header.getX(), (float) header.getBottom() + 8.0f,
                (float) getWidth() - 20.0f, (float) header.getBottom() + 8.0f, 1.0f);

    if (logoImage.isValid())
    {
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImageWithin (logoImage, logoArea.getX(), logoArea.getY(),
                           logoArea.getWidth(), logoArea.getHeight(),
                           juce::RectanglePlacement::centred);
    }

    const juce::Font brandFont ("Arial", 25.0f, juce::Font::bold);
    g.setFont (brandFont);
    g.setColour (kInk);
    g.drawText ("RiserLab", titleArea,
                juce::Justification::centredLeft, false);

    g.setColour (kAccentBright.withAlpha (0.85f));
    g.setFont (juce::Font (9.0f, juce::Font::bold));
    g.drawText (TXT ("SOUND DESIGN  /  PARAMETERS"),
                22, 151, 260, 16, juce::Justification::centredLeft);
}

void RiserLabEditor::resized()
{
    auto area = getLocalBounds().reduced (20);
    auto header = area.removeFromTop (58);
    logoArea = header.removeFromLeft (46).reduced (2);
    titleArea = header.removeFromLeft (250);
    presetBox.setBounds (area.getRight() - 160, 29, 150, 28);

    area.removeFromTop (14);

    auto actions = area.removeFromTop (72);
    captureButton.setBounds (actions.removeFromLeft (138).reduced (2, 1));
    actions.removeFromLeft (12);
    generateButton.setBounds (actions.removeFromLeft (174).reduced (0, 9));
    actions.removeFromLeft (10);
    previewButton.setBounds (actions.removeFromLeft (112).reduced (0, 9));
    actions.removeFromLeft (10);
    statusLabel.setBounds (actions.reduced (0, 15));

    area.removeFromTop (10);

    auto optionRow = area.removeFromTop (30);
    reverbToggle.setBounds (optionRow.removeFromLeft (105));
    optionRow.removeFromLeft (8);
    reverbTypeBox.setBounds (optionRow.removeFromLeft (112).reduced (0, 2));
    optionRow.removeFromLeft (22);
    cutToggle.setBounds (optionRow.removeFromLeft (260));

    area.removeFromTop (14);

    auto footerHeight = juce::jlimit (92, 132, area.getHeight() / 6);
    auto dragBounds = area.removeFromBottom (footerHeight);
    area.removeFromBottom (12);
    auto grid = area;
    const int columns = 4;
    const int rows = 3;
    const int cellWidth = grid.getWidth() / columns;
    const int cellHeight = grid.getHeight() / rows;

    for (int i = 0; i < (int) knobs.size(); ++i)
    {
        const int row = i / columns;
        const int col = i % columns;
        auto cell = juce::Rectangle<int> (grid.getX() + col * cellWidth,
                                           grid.getY() + row * cellHeight,
                                           col == columns - 1 ? grid.getRight() - (grid.getX() + col * cellWidth) : cellWidth,
                                           cellHeight).reduced (5, 2);
        knobs[(size_t) i].label.setBounds (cell.removeFromTop (18));
        cell.removeFromTop (1);
        knobs[(size_t) i].slider.setBounds (cell);
    }

    dragArea.setBounds (dragBounds);
}

void RiserLabEditor::onCaptureClicked()
{
    if (! proc.isCapturing())
    {
        lastMessage.clear();
        proc.startCapture();
        captureButton.active = true;
        captureButton.repaint();
    }
    else
    {
        proc.stopCapture();
        captureButton.active = false;
        captureButton.repaint();
    }
}

void RiserLabEditor::onGenerateClicked()
{
    proc.stopCapture();
    captureButton.active = false;
    captureButton.repaint();

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

void RiserLabEditor::onPreviewClicked()
{
    if (proc.isPreviewPlaying())
    {
        proc.setPreviewPlaying (false);
        previewButton.setButtonText (TXT ("PLAY"));
        lastMessage = TXT ("Vista previa detenida.");
        return;
    }

    if (! proc.getRiserFile().existsAsFile())
    {
        lastMessage = TXT ("Genera un riser antes de escucharlo.");
        return;
    }

    proc.setPreviewPlaying (true);
    if (proc.isPreviewPlaying())
    {
        previewButton.setButtonText (TXT ("STOP"));
        lastMessage = TXT ("Reproduciendo vista previa...");
    }
    else
        lastMessage = TXT ("No hay audio para reproducir.");
}

void RiserLabEditor::timerCallback()
{
    const bool capturing = proc.isCapturing();
    if (! capturing && captureButton.active)
    {
        captureButton.active = false;
        captureButton.repaint();
    }

    if (! proc.isPreviewPlaying() && previewButton.getButtonText() != TXT ("PLAY"))
        previewButton.setButtonText (TXT ("PLAY"));

    juce::String msg;
    const double seconds = proc.getCapturedSeconds();

    if (capturing)
        msg = TXT ("Capturando... ") + juce::String (seconds, 1) + " s";
    else if (lastMessage.isNotEmpty())
        msg = lastMessage;
    else if (proc.isPreviewPlaying())
        msg = TXT ("Reproduciendo riser...");
    else if (seconds > 0.0)
        msg = TXT ("Capturado: ") + juce::String (seconds, 1) + " s";
    else
        msg = TXT ("Pulsa Capturar y reproduce la voz");

    if (statusLabel.getText() != msg)
        statusLabel.setText (msg, juce::dontSendNotification);
}
