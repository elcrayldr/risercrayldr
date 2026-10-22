#include "PluginEditor.h"
#include <BinaryData.h>
#include <cmath>

namespace
{
    const juce::Colour kAccent       (0xff8b5cf6);
    const juce::Colour kAccentBright (0xff22d3ee);
    const juce::Colour kBackground   (0xff090c12);
    const juce::Colour kPanel        (0xff111722);
    const juce::Colour kBorder       (0xff263345);
    const juce::Colour kInk          (0xffe8f0fb);
    const juce::Colour kMuted        (0xff8293a8);

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

    g.setColour (kMuted);
    g.setFont (juce::Font (10.0f, juce::Font::bold));
    g.drawFittedText ("RISER / INPUT", juce::Rectangle<int> ((int) centre.x - 58, (int) centre.y - 43, 116, 16),
                      juce::Justification::centred, 1);

    g.setColour (kInk);
    g.setFont (juce::Font (active ? 13.0f : 14.0f, juce::Font::bold));
    g.drawFittedText (active ? "STOP" : "CAPTURE",
                      juce::Rectangle<int> ((int) centre.x - 62, (int) centre.y - 18, 124, 30),
                      juce::Justification::centred, 1);

    g.setColour (active ? juce::Colour (0xffff637d) : kAccentBright);
    g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre ({ centre.x, centre.y + 42.0f }));

    g.setColour (kMuted);
    g.setFont (juce::Font (9.0f));
    g.drawFittedText (active ? "CAPTURE ACTIVE" : "READY TO CAPTURE",
                      juce::Rectangle<int> ((int) centre.x - 68, (int) centre.y + 49, 136, 14),
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
    setResizeLimits (700, 620, 1400, 1100);
    setSize (820, 760);

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

    generateButton.setButtonText (TXT ("GENERAR RISER"));
    generateButton.onClick = [this] { onGenerateClicked(); };
    generateButton.setColour (juce::TextButton::buttonColourId, kAccent);
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
        k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 17);
        k.slider.setColour (juce::Slider::rotarySliderFillColourId, kAccentBright);
        k.slider.setColour (juce::Slider::rotarySliderOutlineColourId, kBorder);
        k.slider.setColour (juce::Slider::thumbColourId, kAccentBright);
        k.slider.setColour (juce::Slider::textBoxTextColourId, kInk);
        k.slider.setColour (juce::Slider::textBoxBackgroundColourId, kPanel);
        k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
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
        k.label.setColour (juce::Label::textColourId, kInk);
        k.label.setFont (juce::Font (10.5f, juce::Font::bold));
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

    auto header = getLocalBounds().reduced (18).removeFromTop (38);
    g.setColour (kBorder);
    g.drawLine ((float) header.getX(), (float) header.getBottom() + 3.0f,
                (float) getWidth() - 18.0f, (float) header.getBottom() + 3.0f, 1.0f);

    g.setColour (kInk);
    g.setFont (juce::Font (22.0f, juce::Font::bold));
    g.drawText ("RISER", titleArea, juce::Justification::centredLeft);
    g.setColour (kAccentBright);
    auto title2 = titleArea;
    title2.setX (title2.getX() + 76);
    title2.setWidth (110);
    g.setFont (juce::Font (22.0f, juce::Font::bold));
    g.drawText ("CRAY LDR", title2, juce::Justification::centredLeft);

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
    auto header = area.removeFromTop (38);
    logoArea = header.removeFromRight (48).reduced (2);
    titleArea = header;
    presetBox.setBounds (area.getRight() - 170, 18, 152, 25);
    area.removeFromTop (5);

    auto hero = area.removeFromTop (202);
    const int d = juce::jmin (176, hero.getWidth() / 3);
    captureButton.setBounds (juce::Rectangle<int> (d, d)
                                 .withCentre ({ hero.getCentreX(), hero.getY() + 86 }));
    statusLabel.setBounds (hero.getX(), hero.getY() + 173, hero.getWidth(), 17);
    generateButton.setBounds (juce::Rectangle<int> (160, 28)
                                  .withCentre ({ hero.getCentreX(), hero.getBottom() - 1 }));

    auto reverbRow = area.removeFromTop (30);
    reverbToggle.setBounds (reverbRow.removeFromLeft (118));
    reverbRow.removeFromLeft (8);
    reverbTypeBox.setBounds (reverbRow.removeFromLeft (150));
    area.removeFromTop (3);

    auto grid = area.removeFromTop (juce::jmin (330, area.getHeight() * 3 / 5));
    const int rowHeight = grid.getHeight() / 3;
    const int cellWidth = grid.getWidth() / 4;
    for (int i = 0; i < (int) knobs.size(); ++i)
    {
        const int row = i / 4;
        const int col = i % 4;
        auto cell = juce::Rectangle<int> (grid.getX() + col * cellWidth,
                                           grid.getY() + row * rowHeight,
                                           col == 3 ? grid.getRight() - (grid.getX() + col * cellWidth) : cellWidth,
                                           rowHeight).reduced (4, 1);
        knobs[(size_t) i].label.setBounds (cell.removeFromTop (16));
        knobs[(size_t) i].slider.setBounds (cell);
    }
    area.removeFromTop (1);
    cutToggle.setBounds (area.removeFromTop (24));
    area.removeFromTop (4);
    dragArea.setBounds (area);
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

void RiserLabEditor::timerCallback()
{
    const bool capturing = proc.isCapturing();
    if (! capturing && captureButton.active)
    {
        captureButton.active = false;
        captureButton.repaint();
    }

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
