#include "PluginEditor.h"

NoiseMu6ixEditor::NoiseMu6ixEditor (NoiseMu6ixProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      midiKeyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    logoButton.setTooltip ("Click to adjust plugin UI size");
    logoButton.onClick = [this] { showLogoResizeMenu(); };
    addAndMakeVisible (logoButton);

    mainPageBtn.onClick = [this] { switchPage (false); };
    editPageBtn.onClick = [this] { switchPage (true); };
    addAndMakeVisible (mainPageBtn);
    addAndMakeVisible (editPageBtn);

    auto setupSlider = [this] (juce::Slider& s, double min, double max, double step, double val)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 65, 22);
        s.setRange (min, max, step);
        s.setValue (val, juce::dontSendNotification);
        addAndMakeVisible (s);
    };

    setupSlider (masterVolSlider, -48.0, 12.0, 0.1, audioProcessor.masterVolumeDb);
    masterVolSlider.onValueChange = [this] { audioProcessor.masterVolumeDb = (float) masterVolSlider.getValue(); };

    setupSlider (attackSlider, 0.0, 2000.0, 1.0, audioProcessor.attackMs);
    attackSlider.onValueChange = [this] { audioProcessor.attackMs = (float) attackSlider.getValue(); };

    setupSlider (releaseSlider, 5.0, 5000.0, 1.0, audioProcessor.releaseMs);
    releaseSlider.onValueChange = [this] { audioProcessor.releaseMs = (float) releaseSlider.getValue(); };

    addAndMakeVisible (masterVolLabel);
    addAndMakeVisible (attackLabel);
    addAndMakeVisible (releaseLabel);

    quickLoadBtn.onClick = [this] { loadSamplesDialog(); };
    addAndMakeVisible (quickLoadBtn);

    addSampleBtn.onClick = [this] { loadSamplesDialog(); };
    removeSampleBtn.onClick = [this]
    {
        if (selectedZone >= 0)
        {
            audioProcessor.removeZone (selectedZone);
            selectedZone = -1;
            zoneTable.updateContent();
            repaint();
        }
    };
    addAndMakeVisible (addSampleBtn);
    addAndMakeVisible (removeSampleBtn);

    pitchToggle.setToggleState (audioProcessor.globalPitchEnabled, juce::dontSendNotification);
    pitchToggle.onClick = [this]
    {
        audioProcessor.globalPitchEnabled = pitchToggle.getToggleState();
    };
    addAndMakeVisible (pitchToggle);

    frrToggle.setToggleState (audioProcessor.frrEnabled, juce::dontSendNotification);
    frrToggle.onClick = [this] { audioProcessor.frrEnabled = frrToggle.getToggleState(); };
    addAndMakeVisible (frrToggle);

    fvlToggle.setToggleState (audioProcessor.fvlEnabled, juce::dontSendNotification);
    fvlToggle.onClick = [this] { audioProcessor.fvlEnabled = fvlToggle.getToggleState(); };
    addAndMakeVisible (fvlToggle);

    zoneTable.getHeader().addColumn ("#", 1, 36);
    zoneTable.getHeader().addColumn ("Sample / Zone Name", 2, 240);
    zoneTable.getHeader().addColumn ("Root", 3, 55);
    zoneTable.getHeader().addColumn ("Low Key", 4, 65);
    zoneTable.getHeader().addColumn ("High Key", 5, 65);
    zoneTable.getHeader().addColumn ("Vel Range", 6, 85);
    zoneTable.getHeader().addColumn ("K-Trk", 7, 55);
    zoneTable.getHeader().addColumn ("Loop", 8, 55);
    zoneTable.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff1c2024));
    addAndMakeVisible (zoneTable);

    setupSlider (rootKeySlider, 0, 127, 1, 60);
    setupSlider (lowKeySlider, 0, 127, 1, 0);
    setupSlider (highKeySlider, 0, 127, 1, 127);
    setupSlider (lowVelSlider, 0, 127, 1, 0);
    setupSlider (highVelSlider, 0, 127, 1, 127);
    setupSlider (loopStartSlider, 0.0, 60.0, 0.01, 0.1);
    setupSlider (loopEndSlider, 0.05, 60.0, 0.01, 4.0);
    setupSlider (crossfadeSlider, 0.0, 500.0, 1.0, 15.0);

    auto bindZoneEdit = [this] (juce::Slider& s)
    {
        s.onValueChange = [this] { applyControlsToSelectedZone(); };
    };
    bindZoneEdit (rootKeySlider);
    bindZoneEdit (lowKeySlider);
    bindZoneEdit (highKeySlider);
    bindZoneEdit (lowVelSlider);
    bindZoneEdit (highVelSlider);
    bindZoneEdit (loopStartSlider);
    bindZoneEdit (loopEndSlider);
    bindZoneEdit (crossfadeSlider);

    zonePitchBtn.onClick = [this] { applyControlsToSelectedZone(); };
    loopBtn.onClick = [this] { applyControlsToSelectedZone(); };
    addAndMakeVisible (zonePitchBtn);
    addAndMakeVisible (loopBtn);

    addAndMakeVisible (rootLbl);
    addAndMakeVisible (lowKLbl);
    addAndMakeVisible (highKLbl);
    addAndMakeVisible (lowVLbl);
    addAndMakeVisible (highVLbl);
    addAndMakeVisible (loopStartLbl);
    addAndMakeVisible (loopEndLbl);
    addAndMakeVisible (xfadeLbl);

    addAndMakeVisible (midiKeyboard);

    switchPage (false);
    setSize (900, 580);
}

NoiseMu6ixEditor::~NoiseMu6ixEditor() = default;

void NoiseMu6ixEditor::showLogoResizeMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("Noise Mu6ix UI Size");
    menu.addItem (1, "75% (Compact)", true, std::abs (currentScale - 0.75f) < 0.01f);
    menu.addItem (2, "100% (Default)", true, std::abs (currentScale - 1.0f) < 0.01f);
    menu.addItem (3, "125% (Medium)", true, std::abs (currentScale - 1.25f) < 0.01f);
    menu.addItem (4, "150% (Large)", true, std::abs (currentScale - 1.5f) < 0.01f);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&logoButton),
                        [this] (int result)
                        {
                            if (result == 1) applyUiScale (0.75f);
                            else if (result == 2) applyUiScale (1.0f);
                            else if (result == 3) applyUiScale (1.25f);
                            else if (result == 4) applyUiScale (1.5f);
                        });
}

void NoiseMu6ixEditor::applyUiScale (float scale)
{
    currentScale = scale;
    setScaleFactor (scale);
    setSize ((int) (900 * scale), (int) (580 * scale));
}

void NoiseMu6ixEditor::switchPage (bool showEdit)
{
    isEditPage = showEdit;

    mainPageBtn.setColour (juce::TextButton::buttonColourId, ! isEditPage ? juce::Colour (0xff2ea8c9) : juce::Colour (0xff343a40));
    editPageBtn.setColour (juce::TextButton::buttonColourId, isEditPage ? juce::Colour (0xff2ea8c9) : juce::Colour (0xff343a40));

    masterVolSlider.setVisible (! isEditPage);
    attackSlider.setVisible (! isEditPage);
    releaseSlider.setVisible (! isEditPage);
    masterVolLabel.setVisible (! isEditPage);
    attackLabel.setVisible (! isEditPage);
    releaseLabel.setVisible (! isEditPage);
    quickLoadBtn.setVisible (! isEditPage);

    addSampleBtn.setVisible (isEditPage);
    removeSampleBtn.setVisible (isEditPage);
    pitchToggle.setVisible (isEditPage);
    frrToggle.setVisible (isEditPage);
    fvlToggle.setVisible (isEditPage);
    zoneTable.setVisible (isEditPage);

    rootKeySlider.setVisible (isEditPage);
    lowKeySlider.setVisible (isEditPage);
    highKeySlider.setVisible (isEditPage);
    lowVelSlider.setVisible (isEditPage);
    highVelSlider.setVisible (isEditPage);
    zonePitchBtn.setVisible (isEditPage);
    loopBtn.setVisible (isEditPage);
    loopStartSlider.setVisible (isEditPage);
    loopEndSlider.setVisible (isEditPage);
    crossfadeSlider.setVisible (isEditPage);

    rootLbl.setVisible (isEditPage);
    lowKLbl.setVisible (isEditPage);
    highKLbl.setVisible (isEditPage);
    lowVLbl.setVisible (isEditPage);
    highVLbl.setVisible (isEditPage);
    loopStartLbl.setVisible (isEditPage);
    loopEndLbl.setVisible (isEditPage);
    xfadeLbl.setVisible (isEditPage);

    repaint();
}

void NoiseMu6ixEditor::loadSamplesDialog()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Select Audio Samples (.wav / .aiff)",
                                                      juce::File{},
                                                      "*.wav;*.aif;*.aiff;*.flac");
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::canSelectMultipleItems,
                              [this] (const juce::FileChooser& fc)
                              {
                                  for (const auto& f : fc.getResults())
                                      audioProcessor.addZoneFromFile (f);

                                  zoneTable.updateContent();
                                  if (audioProcessor.zones.size() > 0 && selectedZone < 0)
                                  {
                                      selectedZone = 0;
                                      zoneTable.selectRow (0);
                                      refreshFromSelectedZone();
                                  }
                                  repaint();
                              });
}

void NoiseMu6ixEditor::refreshFromSelectedZone()
{
    const juce::ScopedLock sl (audioProcessor.zoneLock);
    if (selectedZone < 0 || selectedZone >= (int) audioProcessor.zones.size())
        return;

    const auto& z = audioProcessor.zones[(size_t) selectedZone];
    rootKeySlider.setValue (z.rootKey, juce::dontSendNotification);
    lowKeySlider.setValue (z.lowKey, juce::dontSendNotification);
    highKeySlider.setValue (z.highKey, juce::dontSendNotification);
    lowVelSlider.setValue (z.lowVel, juce::dontSendNotification);
    highVelSlider.setValue (z.highVel, juce::dontSendNotification);
    zonePitchBtn.setToggleState (z.pitchTrack, juce::dontSendNotification);
    loopBtn.setToggleState (z.loopEnabled, juce::dontSendNotification);
    loopStartSlider.setValue (z.loopStartSec, juce::dontSendNotification);
    loopEndSlider.setValue (z.loopEndSec, juce::dontSendNotification);
    crossfadeSlider.setValue (z.crossfadeMs, juce::dontSendNotification);
}

void NoiseMu6ixEditor::applyControlsToSelectedZone()
{
    const juce::ScopedLock sl (audioProcessor.zoneLock);
    if (selectedZone < 0 || selectedZone >= (int) audioProcessor.zones.size())
        return;

    auto& z = audioProcessor.zones[(size_t) selectedZone];
    z.rootKey = (int) rootKeySlider.getValue();
    z.lowKey = (int) lowKeySlider.getValue();
    z.highKey = (int) highKeySlider.getValue();
    z.lowVel = (int) lowVelSlider.getValue();
    z.highVel = (int) highVelSlider.getValue();
    z.pitchTrack = zonePitchBtn.getToggleState();
    z.loopEnabled = loopBtn.getToggleState();
    z.loopStartSec = loopStartSlider.getValue();
    z.loopEndSec = loopEndSlider.getValue();
    z.crossfadeMs = crossfadeSlider.getValue();

    zoneTable.repaint();
    repaint();
}

int NoiseMu6ixEditor::getNumRows()
{
    const juce::ScopedLock sl (audioProcessor.zoneLock);
    return (int) audioProcessor.zones.size();
}

void NoiseMu6ixEditor::paintRowBackground (juce::Graphics& g, int rowNumber, int, int, bool rowIsSelected)
{
    if (rowIsSelected)
        g.fillAll (juce::Colour (0xff2c5d72));
    else
        g.fillAll (rowNumber % 2 == 0 ? juce::Colour (0xff22272c) : juce::Colour (0xff1c2024));
}

void NoiseMu6ixEditor::paintCell (juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool)
{
    const juce::ScopedLock sl (audioProcessor.zoneLock);
    if (rowNumber < 0 || rowNumber >= (int) audioProcessor.zones.size())
        return;

    const auto& z = audioProcessor.zones[(size_t) rowNumber];
    g.setColour (juce::Colours::white);
    g.setFont (13.0f);

    juce::String text;
    if (columnId == 1) text = juce::String (rowNumber + 1);
    else if (columnId == 2) text = z.name;
    else if (columnId == 3) text = juce::MidiMessage::getMidiNoteName (z.rootKey, true, true, 4);
    else if (columnId == 4) text = juce::MidiMessage::getMidiNoteName (z.lowKey, true, true, 4);
    else if (columnId == 5) text = juce::MidiMessage::getMidiNoteName (z.highKey, true, true, 4);
    else if (columnId == 6) text = juce::String (z.lowVel) + "-" + juce::String (z.highVel);
    else if (columnId == 7) text = z.pitchTrack ? "ON" : "OFF";
    else if (columnId == 8) text = z.loopEnabled ? "ON" : "OFF";

    g.drawText (text, 6, 0, width - 10, height, juce::Justification::centredLeft, true);
}

void NoiseMu6ixEditor::selectedRowsChanged (int lastRowSelected)
{
    selectedZone = lastRowSelected;
    refreshFromSelectedZone();
    repaint();
}

void NoiseMu6ixEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff181b1f));

    g.setColour (juce::Colour (0xff23282e));
    g.fillRect (0, 0, getWidth(), 46);

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (17.5f, juce::Font::bold));
    g.drawText ("Noise Mu6ix Sampler", 154, 8, 280, 30, juce::Justification::centredLeft);

    if (! isEditPage)
    {
        g.setColour (juce::Colour (0xff23282e));
        g.fillRoundedRectangle (20.0f, 62.0f, (float) getWidth() - 40.0f, 180.0f, 6.0f);

        g.setColour (juce::Colours::lightgrey);
        g.setFont (15.0f);
        g.drawText ("Loaded Zones: " + juce::String ((int) audioProcessor.zones.size())
                    + "   |   Pitch: " + (audioProcessor.globalPitchEnabled ? "ON" : "OFF")
                    + "   |   FRR: " + (audioProcessor.frrEnabled ? "ON" : "OFF")
                    + "   |   FVL: " + (audioProcessor.fvlEnabled ? "ON" : "OFF"),
                    36, 76, getWidth() - 72, 26, juce::Justification::centredLeft);
    }
    else
    {
        juce::Rectangle<int> waveArea (20, 305, 420, 145);
        g.setColour (juce::Colour (0xff121518));
        g.fillRect (waveArea);
        g.setColour (juce::Colour (0xff3a424a));
        g.drawRect (waveArea);

        const juce::ScopedLock sl (audioProcessor.zoneLock);
        if (selectedZone >= 0 && selectedZone < (int) audioProcessor.zones.size())
        {
            const auto& z = audioProcessor.zones[(size_t) selectedZone];
            int total = z.buffer.getNumSamples();
            if (total > 1)
            {
                const float* data = z.buffer.getReadPointer (0);
                g.setColour (juce::Colour (0xff56c8e8));
                int w = waveArea.getWidth();
                int midY = waveArea.getCentreY();
                float halfH = waveArea.getHeight() * 0.45f;

                for (int x = 0; x < w; ++x)
                {
                    int idx = (int) ((double) x / (double) w * (double) total);
                    float s = juce::jlimit (-1.0f, 1.0f, data[idx]);
                    g.drawVerticalLine (waveArea.getX() + x, midY - std::abs (s) * halfH, midY + std::abs (s) * halfH);
                }

                if (z.loopEnabled)
                {
                    double totalSec = (double) total / z.sampleRate;
                    int xStart = waveArea.getX() + (int) (juce::jlimit (0.0, 1.0, z.loopStartSec / totalSec) * w);
                    int xEnd = waveArea.getX() + (int) (juce::jlimit (0.0, 1.0, z.loopEndSec / totalSec) * w);
                    g.setColour (juce::Colours::orange);
                    g.drawVerticalLine (xStart, (float) waveArea.getY(), (float) waveArea.getBottom());
                    g.drawVerticalLine (xEnd, (float) waveArea.getY(), (float) waveArea.getBottom());
                }
            }
        }
    }

    int footerY = getHeight() - 24;
    g.setColour (juce::Colour (0xff121518));
    g.fillRect (0, footerY, getWidth(), 24);

    g.setColour (juce::Colours::grey);
    g.setFont (12.0f);
    g.drawText ("Noise Mu6ix Studio  |  Ready", 14, footerY, 300, 24, juce::Justification::centredLeft);
    g.drawText ("v" + juce::String (NMS_VERSION_STRING), getWidth() - 140, footerY, 126, 24, juce::Justification::centredRight);
}

void NoiseMu6ixEditor::resized()
{
    logoButton.setBounds (12, 8, 132, 30);
    mainPageBtn.setBounds (getWidth() - 250, 8, 110, 30);
    editPageBtn.setBounds (getWidth() - 130, 8, 110, 30);

    quickLoadBtn.setBounds (36, 115, 160, 32);
    masterVolLabel.setBounds (220, 115, 110, 26);
    masterVolSlider.setBounds (335, 115, 240, 26);
    attackLabel.setBounds (220, 152, 110, 26);
    attackSlider.setBounds (335, 152, 240, 26);
    releaseLabel.setBounds (220, 189, 110, 26);
    releaseSlider.setBounds (335, 189, 240, 26);

    addSampleBtn.setBounds (20, 56, 100, 28);
    removeSampleBtn.setBounds (128, 56, 95, 28);
    pitchToggle.setBounds (245, 56, 145, 28);
    frrToggle.setBounds (400, 56, 130, 28);
    fvlToggle.setBounds (540, 56, 130, 28);

    zoneTable.setBounds (20, 92, getWidth() - 40, 200);

    int rightX = 455;
    int lblW = 105;
    int sldW = getWidth() - rightX - lblW - 25;

    rootLbl.setBounds (rightX, 305, lblW, 22);       rootKeySlider.setBounds (rightX + lblW, 305, sldW, 22);
    lowKLbl.setBounds (rightX, 330, lblW, 22);       lowKeySlider.setBounds (rightX + lblW, 330, sldW, 22);
    highKLbl.setBounds (rightX, 355, lblW, 22);      highKeySlider.setBounds (rightX + lblW, 355, sldW, 22);
    lowVLbl.setBounds (rightX, 380, lblW, 22);       lowVelSlider.setBounds (rightX + lblW, 380, sldW, 22);
    highVLbl.setBounds (rightX, 405, lblW, 22);      highVelSlider.setBounds (rightX + lblW, 405, sldW, 22);

    zonePitchBtn.setBounds (20, 456, 110, 24);
    loopBtn.setBounds (135, 456, 80, 24);
    loopStartLbl.setBounds (220, 456, 95, 24);       loopStartSlider.setBounds (315, 456, 140, 24);
    loopEndLbl.setBounds (465, 456, 90, 24);         loopEndSlider.setBounds (555, 456, 140, 24);
    xfadeLbl.setBounds (700, 430, 105, 22);          crossfadeSlider.setBounds (700, 456, 180, 24);

    midiKeyboard.setBounds (20, getHeight() - 92, getWidth() - 40, 62);
}
