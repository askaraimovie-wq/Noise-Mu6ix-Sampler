#pragma once
#include "PluginProcessor.h"

#ifndef NMS_VERSION_STRING
 #define NMS_VERSION_STRING "1.0.1"
#endif

// Custom Vector Logo Button for "Noise Mu6ix" (click to open UI size menu)
class NoiseMu6ixLogoButton : public juce::Button
{
public:
    NoiseMu6ixLogoButton() : juce::Button ("Noise Mu6ix Logo") {}

    void paintButton (juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (1.0f);

        juce::Colour bg = isButtonDown ? juce::Colour (0xff1f2a33)
                        : isMouseOverButton ? juce::Colour (0xff2b3844)
                                            : juce::Colour (0xff222b33);
        g.setColour (bg);
        g.fillRoundedRectangle (bounds, 5.0f);

        g.setColour (juce::Colour (0xff49c6e5));
        g.drawRoundedRectangle (bounds, 5.0f, 1.3f);

        float iconX = bounds.getX() + 9.0f;
        float midY = bounds.getCentreY();
        const float bars[5] = { 5.0f, 11.0f, 15.0f, 9.0f, 6.0f };
        g.setColour (juce::Colour (0xff49c6e5));
        for (int i = 0; i < 5; ++i)
        {
            g.fillRoundedRectangle (iconX + i * 4.0f, midY - bars[i] * 0.5f, 2.4f, bars[i], 1.0f);
        }

        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
        g.drawText ("NOISE MU6IX", (int) (iconX + 24.0f), 0, (int) (bounds.getWidth() - 36.0f), getHeight(),
                    juce::Justification::centredLeft, true);
    }
};

class NoiseMu6ixEditor : public juce::AudioProcessorEditor,
                         private juce::TableListBoxModel
{
public:
    explicit NoiseMu6ixEditor (NoiseMu6ixProcessor&);
    ~NoiseMu6ixEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    int getNumRows() override;
    void paintRowBackground (juce::Graphics&, int rowNumber, int width, int height, bool rowIsSelected) override;
    void paintCell (juce::Graphics&, int rowNumber, int columnId, int width, int height, bool rowIsSelected) override;
    void selectedRowsChanged (int lastRowSelected) override;

private:
    void switchPage (bool showEdit);
    void showLogoResizeMenu();
    void applyUiScale (float scale);
    void refreshFromSelectedZone();
    void applyControlsToSelectedZone();
    void loadSamplesDialog();

    NoiseMu6ixProcessor& audioProcessor;
    bool isEditPage = false;
    int selectedZone = -1;
    float currentScale = 1.0f;

    NoiseMu6ixLogoButton logoButton;
    juce::TextButton mainPageBtn { "MAIN PAGE" };
    juce::TextButton editPageBtn { "EDIT PAGE" };

    juce::Slider masterVolSlider;
    juce::Slider attackSlider;
    juce::Slider releaseSlider;
    juce::Label masterVolLabel { {}, "VOLUME (dB)" };
    juce::Label attackLabel { {}, "ATTACK (ms)" };
    juce::Label releaseLabel { {}, "RELEASE (ms)" };
    juce::TextButton quickLoadBtn { "LOAD SAMPLES..." };

    juce::TextButton addSampleBtn { "+ ADD WAV" };
    juce::TextButton removeSampleBtn { "- REMOVE" };
    juce::ToggleButton pitchToggle { "PITCH (ON/OFF)" };
    juce::ToggleButton frrToggle { "FRR (Fake RR)" };
    juce::ToggleButton fvlToggle { "FVL (Fake Vel)" };

    juce::TableListBox zoneTable { "Zones", this };

    juce::Slider rootKeySlider, lowKeySlider, highKeySlider;
    juce::Slider lowVelSlider, highVelSlider;
    juce::ToggleButton zonePitchBtn { "K-Trk Pitch" };
    juce::ToggleButton loopBtn { "LOOP" };
    juce::Slider loopStartSlider, loopEndSlider, crossfadeSlider;

    juce::Label rootLbl { {}, "Root" }, lowKLbl { {}, "Low Key" }, highKLbl { {}, "High Key" };
    juce::Label lowVLbl { {}, "Low Vel" }, highVLbl { {}, "High Vel" };
    juce::Label loopStartLbl { {}, "Loop Start (s)" }, loopEndLbl { {}, "Loop End (s)" }, xfadeLbl { {}, "Crossfade (ms)" };

    juce::MidiKeyboardComponent midiKeyboard;
    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoiseMu6ixEditor)
};
