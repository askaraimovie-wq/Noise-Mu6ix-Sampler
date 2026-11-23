#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>

struct SampleZone
{
    juce::String name;
    juce::File file;
    juce::AudioBuffer<float> buffer;
    double sampleRate = 44100.0;

    int rootKey = 60;      // C4
    int lowKey = 0;
    int highKey = 127;
    int lowVel = 0;
    int highVel = 127;

    bool pitchTrack = true; // OFF for Drums, ON for melodic
    bool loopEnabled = false;
    double loopStartSec = 0.0;
    double loopEndSec = 4.0;
    double crossfadeMs = 15.0;

    float volumeDb = 0.0f;
    float pan = 0.0f;
    int tuneSemitones = 0;
};

struct ActiveVoice
{
    bool active = false;
    int zoneIndex = -1;
    int midiNote = 60;
    float velocity = 1.0f;
    double position = 0.0;
    double pitchRatio = 1.0;
    float gainL = 1.0f;
    float gainR = 1.0f;
    float envLevel = 0.0f;
    bool releasing = false;
    float filterStateL = 0.0f;
    float filterStateR = 0.0f;
    float filterCoeff = 1.0f;
};

class NoiseMu6ixProcessor : public juce::AudioProcessor
{
public:
    NoiseMu6ixProcessor();
    ~NoiseMu6ixProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Noise Mu6ix Sampler"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    int addZoneFromFile (const juce::File& file);
    void removeZone (int index);
    void clearZones();

    std::vector<SampleZone> zones;
    juce::CriticalSection zoneLock;

    bool globalPitchEnabled = true; // Master Pitch ON/OFF (turn OFF for Drums)
    bool frrEnabled = true;         // Fake Round-Robin ON/OFF
    bool fvlEnabled = true;         // Fake Velocity Layer ON/OFF
    float masterVolumeDb = 0.0f;
    float attackMs = 5.0f;
    float releaseMs = 250.0f;

    juce::MidiKeyboardState keyboardState;

private:
    void triggerNote (int midiNote, float velocity);
    void releaseNote (int midiNote);

    juce::AudioFormatManager formatManager;
    std::vector<ActiveVoice> voices;
    double currentSampleRate = 44100.0;
    juce::Random rng;
    int rrCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoiseMu6ixProcessor)
};
