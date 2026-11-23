#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

NoiseMu6ixProcessor::NoiseMu6ixProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    formatManager.registerBasicFormats();
    voices.resize (64);
}

void NoiseMu6ixProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
}

void NoiseMu6ixProcessor::releaseResources()
{
    for (auto& v : voices)
        v.active = false;
}

int NoiseMu6ixProcessor::addZoneFromFile (const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr)
        return -1;

    SampleZone z;
    z.name = file.getFileNameWithoutExtension();
    z.file = file;
    z.sampleRate = reader->sampleRate;
    z.buffer.setSize ((int) reader->numChannels, (int) reader->lengthInSamples);
    reader->read (&z.buffer, 0, (int) reader->lengthInSamples, 0, true, true);

    double totalSec = (double) reader->lengthInSamples / reader->sampleRate;
    z.loopStartSec = juce::jmin (0.2, totalSec * 0.2);
    z.loopEndSec = juce::jmax (z.loopStartSec + 0.05, totalSec * 0.9);
    z.pitchTrack = globalPitchEnabled;

    const juce::ScopedLock sl (zoneLock);
    zones.push_back (std::move (z));
    return (int) zones.size() - 1;
}

void NoiseMu6ixProcessor::removeZone (int index)
{
    const juce::ScopedLock sl (zoneLock);
    if (index >= 0 && index < (int) zones.size())
    {
        for (auto& v : voices)
            v.active = false;
        zones.erase (zones.begin() + index);
    }
}

void NoiseMu6ixProcessor::clearZones()
{
    const juce::ScopedLock sl (zoneLock);
    for (auto& v : voices)
        v.active = false;
    zones.clear();
}

void NoiseMu6ixProcessor::triggerNote (int midiNote, float velocity)
{
    const juce::ScopedLock sl (zoneLock);
    int vel127 = juce::jlimit (0, 127, (int) std::round (velocity * 127.0f));

    for (int zi = 0; zi < (int) zones.size(); ++zi)
    {
        const auto& z = zones[(size_t) zi];
        if (midiNote < z.lowKey || midiNote > z.highKey)
            continue;
        if (vel127 < z.lowVel || vel127 > z.highVel)
            continue;

        ActiveVoice* target = nullptr;
        for (auto& v : voices)
        {
            if (! v.active)
            {
                target = &v;
                break;
            }
        }
        if (target == nullptr)
            target = &voices[0];

        target->active = true;
        target->zoneIndex = zi;
        target->midiNote = midiNote;
        target->velocity = velocity;
        target->releasing = false;
        target->envLevel = (attackMs <= 1.0f) ? 1.0f : 0.0f;
        target->filterStateL = 0.0f;
        target->filterStateR = 0.0f;

        bool usePitch = globalPitchEnabled && z.pitchTrack;
        double semitoneDiff = usePitch ? (double) (midiNote - z.rootKey + z.tuneSemitones)
                                       : (double) z.tuneSemitones;

        // Fake Round-Robin (FRR) micro-variation
        double rrPitchCents = 0.0;
        float rrGainMod = 1.0f;
        double rrStartSamples = 0.0;
        if (frrEnabled)
        {
            rrCounter = (rrCounter + 1) % 8;
            float r1 = rng.nextFloat() * 2.0f - 1.0f;
            float r2 = rng.nextFloat() * 2.0f - 1.0f;
            rrPitchCents = r1 * (usePitch ? 6.0 : 18.0);
            rrGainMod = 1.0f + r2 * 0.06f;
            rrStartSamples = (double) (rng.nextFloat() * 0.002 * z.sampleRate);
        }

        target->position = rrStartSamples;
        target->pitchRatio = (z.sampleRate / currentSampleRate)
                             * std::pow (2.0, (semitoneDiff + rrPitchCents / 100.0) / 12.0);

        // Fake Velocity Layer (FVL) dynamic brightness & curve
        float velGain = velocity;
        if (fvlEnabled)
        {
            velGain = std::pow (velocity, 1.35f);
            float cutoffHz = 900.0f + std::pow (velocity, 1.6f) * 18500.0f;
            float rc = 1.0f / (2.0f * juce::MathConstants<float>::pi * cutoffHz);
            float dt = 1.0f / (float) currentSampleRate;
            target->filterCoeff = dt / (rc + dt);
        }
        else
        {
            target->filterCoeff = 1.0f;
        }

        float zoneGain = juce::Decibels::decibelsToGain (z.volumeDb + masterVolumeDb) * velGain * rrGainMod;
        float panAngle = (z.pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
        target->gainL = zoneGain * std::cos (panAngle);
        target->gainR = zoneGain * std::sin (panAngle);
    }
}

void NoiseMu6ixProcessor::releaseNote (int midiNote)
{
    for (auto& v : voices)
    {
        if (v.active && v.midiNote == midiNote)
            v.releasing = true;
    }
}

void NoiseMu6ixProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    keyboardState.processNextMidiBuffer (midiMessages, 0, buffer.getNumSamples(), true);

    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn())
            triggerNote (msg.getNoteNumber(), msg.getFloatVelocity());
        else if (msg.isNoteOff())
            releaseNote (msg.getNoteNumber());
    }

    const juce::ScopedLock sl (zoneLock);
    int numSamples = buffer.getNumSamples();
    float* outL = buffer.getWritePointer (0);
    float* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : outL;

    float attackStep = (attackMs > 0.5f) ? (1.0f / (float) (attackMs * 0.001 * currentSampleRate)) : 1.0f;
    float releaseStep = (releaseMs > 0.5f) ? (1.0f / (float) (releaseMs * 0.001 * currentSampleRate)) : 0.01f;

    for (auto& v : voices)
    {
        if (! v.active)
            continue;
        if (v.zoneIndex < 0 || v.zoneIndex >= (int) zones.size())
        {
            v.active = false;
            continue;
        }

        const auto& z = zones[(size_t) v.zoneIndex];
        int totalLen = z.buffer.getNumSamples();
        if (totalLen < 2)
        {
            v.active = false;
            continue;
        }

        const float* srcL = z.buffer.getReadPointer (0);
        const float* srcR = buffer.getNumChannels() > 1 ? z.buffer.getReadPointer (1) : srcL;

        double loopStart = juce::jlimit (0.0, (double) (totalLen - 2), z.loopStartSec * z.sampleRate);
        double loopEnd = juce::jlimit (loopStart + 16.0, (double) (totalLen - 1), z.loopEndSec * z.sampleRate);
        double xfadeSamples = juce::jlimit (0.0, (loopEnd - loopStart) * 0.45, z.crossfadeMs * 0.001 * z.sampleRate);

        for (int i = 0; i < numSamples; ++i)
        {
            if (v.releasing)
            {
                v.envLevel -= releaseStep;
                if (v.envLevel <= 0.0001f)
                {
                    v.active = false;
                    break;
                }
            }
            else if (v.envLevel < 1.0f)
            {
                v.envLevel = juce::jmin (1.0f, v.envLevel + attackStep);
            }

            if (z.loopEnabled && v.position >= loopEnd)
            {
                double overshoot = v.position - loopEnd;
                v.position = loopStart + std::fmod (overshoot, (loopEnd - loopStart));
            }
            else if (! z.loopEnabled && v.position >= totalLen - 1)
            {
                v.active = false;
                break;
            }

            int idx0 = (int) v.position;
            int idx1 = juce::jmin (totalLen - 1, idx0 + 1);
            float frac = (float) (v.position - (double) idx0);

            float sL = srcL[idx0] + frac * (srcL[idx1] - srcL[idx0]);
            float sR = srcR[idx0] + frac * (srcR[idx1] - srcR[idx0]);

            // Smooth loop crossfade near loopEnd
            if (z.loopEnabled && xfadeSamples > 4.0 && v.position > (loopEnd - xfadeSamples))
            {
                float alpha = (float) ((v.position - (loopEnd - xfadeSamples)) / xfadeSamples);
                alpha = juce::jlimit (0.0f, 1.0f, alpha);
                double wrapPos = loopStart + (v.position - (loopEnd - xfadeSamples));
                int w0 = juce::jlimit (0, totalLen - 1, (int) wrapPos);
                int w1 = juce::jlimit (0, totalLen - 1, w0 + 1);
                float wFrac = (float) (wrapPos - (double) w0);
                float wL = srcL[w0] + wFrac * (srcL[w1] - srcL[w0]);
                float wR = srcR[w0] + wFrac * (srcR[w1] - srcR[w0]);
                sL = sL * (1.0f - alpha) + wL * alpha;
                sR = sR * (1.0f - alpha) + wR * alpha;
            }

            // FVL one-pole dynamic lowpass
            v.filterStateL += v.filterCoeff * (sL - v.filterStateL);
            v.filterStateR += v.filterCoeff * (sR - v.filterStateR);

            outL[i] += v.filterStateL * v.gainL * v.envLevel;
            if (buffer.getNumChannels() > 1)
                outR[i] += v.filterStateR * v.gainR * v.envLevel;

            v.position += v.pitchRatio;
        }
    }
}

void NoiseMu6ixProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree state ("NOISE_MU6IX_SAMPLER_STATE");
    state.setProperty ("globalPitch", globalPitchEnabled, nullptr);
    state.setProperty ("frr", frrEnabled, nullptr);
    state.setProperty ("fvl", fvlEnabled, nullptr);
    state.setProperty ("masterVol", masterVolumeDb, nullptr);
    state.setProperty ("attack", attackMs, nullptr);
    state.setProperty ("release", releaseMs, nullptr);

    const juce::ScopedLock sl (zoneLock);
    for (const auto& z : zones)
    {
        juce::ValueTree zt ("ZONE");
        zt.setProperty ("path", z.file.getFullPathName(), nullptr);
        zt.setProperty ("root", z.rootKey, nullptr);
        zt.setProperty ("lowKey", z.lowKey, nullptr);
        zt.setProperty ("highKey", z.highKey, nullptr);
        zt.setProperty ("lowVel", z.lowVel, nullptr);
        zt.setProperty ("highVel", z.highVel, nullptr);
        zt.setProperty ("pitchTrack", z.pitchTrack, nullptr);
        zt.setProperty ("loop", z.loopEnabled, nullptr);
        zt.setProperty ("loopStart", z.loopStartSec, nullptr);
        zt.setProperty ("loopEnd", z.loopEndSec, nullptr);
        zt.setProperty ("xfade", z.crossfadeMs, nullptr);
        zt.setProperty ("vol", z.volumeDb, nullptr);
        zt.setProperty ("pan", z.pan, nullptr);
        zt.setProperty ("tune", z.tuneSemitones, nullptr);
        state.appendChild (zt, nullptr);
    }

    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void NoiseMu6ixProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml == nullptr)
        return;

    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid())
        return;

    globalPitchEnabled = state.getProperty ("globalPitch", true);
    frrEnabled = state.getProperty ("frr", true);
    fvlEnabled = state.getProperty ("fvl", true);
    masterVolumeDb = state.getProperty ("masterVol", 0.0f);
    attackMs = state.getProperty ("attack", 5.0f);
    releaseMs = state.getProperty ("release", 250.0f);

    clearZones();
    for (int i = 0; i < state.getNumChildren(); ++i)
    {
        auto zt = state.getChild (i);
        juce::File f (zt.getProperty ("path").toString());
        if (f.existsAsFile())
        {
            int idx = addZoneFromFile (f);
            if (idx >= 0)
            {
                const juce::ScopedLock sl (zoneLock);
                auto& z = zones[(size_t) idx];
                z.rootKey = zt.getProperty ("root", 60);
                z.lowKey = zt.getProperty ("lowKey", 0);
                z.highKey = zt.getProperty ("highKey", 127);
                z.lowVel = zt.getProperty ("lowVel", 0);
                z.highVel = zt.getProperty ("highVel", 127);
                z.pitchTrack = zt.getProperty ("pitchTrack", true);
                z.loopEnabled = zt.getProperty ("loop", false);
                z.loopStartSec = zt.getProperty ("loopStart", 0.0);
                z.loopEndSec = zt.getProperty ("loopEnd", 4.0);
                z.crossfadeMs = zt.getProperty ("xfade", 15.0);
                z.volumeDb = zt.getProperty ("vol", 0.0f);
                z.pan = zt.getProperty ("pan", 0.0f);
                z.tuneSemitones = zt.getProperty ("tune", 0);
            }
        }
    }
}

juce::AudioProcessorEditor* NoiseMu6ixProcessor::createEditor()
{
    return new NoiseMu6ixEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NoiseMu6ixProcessor();
}
