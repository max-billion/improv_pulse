#include "SfzInstrumentLoader.h"
#include <algorithm>
#include <cmath>
#include <future>
#include <iostream>
#include <map>
#include <set>

namespace sfz {

//==============================================================================
// SampleBufferCache Implementation
//==============================================================================
SampleBufferCache& SampleBufferCache::getInstance() {
    static SampleBufferCache instance;
    return instance;
}

SampleBufferCache::CachedSample SampleBufferCache::getOrLoad (
    const juce::File& wavFile,
    juce::AudioFormatManager& formatManager,
    double maxDurationSeconds) {

    const std::string key = wavFile.getFullPathName().toStdString();

    {
        std::lock_guard<std::mutex> lock (mutex);
        auto it = cache.find (key);
        if (it != cache.end())
            return it->second;
    }

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (wavFile));
    if (reader == nullptr || reader->lengthInSamples <= 0)
        return {};

    const double sr = reader->sampleRate;
    juce::int64 framesToRead = reader->lengthInSamples;
    if (maxDurationSeconds > 0.0) {
        const juce::int64 maxFrames = static_cast<juce::int64> (sr * maxDurationSeconds);
        framesToRead = std::min (framesToRead, maxFrames);
    }

    auto buffer = std::make_shared<juce::AudioBuffer<float>> (
        static_cast<int> (reader->numChannels),
        static_cast<int> (framesToRead));

    reader->read (buffer.get(), 0, static_cast<int> (framesToRead), 0, true, true);

    // Apply a smooth 40ms cosine fade-out at the end if sample was truncated
    if (framesToRead < reader->lengthInSamples && framesToRead > 2048) {
        const int fadeLen = std::min<int> (static_cast<int> (sr * 0.04), static_cast<int> (framesToRead / 4));
        const int fadeStart = static_cast<int> (framesToRead) - fadeLen;
        for (int ch = 0; ch < buffer->getNumChannels(); ++ch) {
            float* data = buffer->getWritePointer (ch);
            for (int i = 0; i < fadeLen; ++i) {
                const float phase = static_cast<float> (i) / static_cast<float> (fadeLen);
                const float gain = 0.5f * (1.0f + std::cos (juce::MathConstants<float>::pi * phase));
                data[fadeStart + i] *= gain;
            }
        }
    }

    CachedSample result;
    result.buffer = buffer;
    result.sampleRate = sr;
    result.numFrames = framesToRead;

    {
        std::lock_guard<std::mutex> lock (mutex);
        cache[key] = result;
    }

    return result;
}

void SampleBufferCache::clear() {
    std::lock_guard<std::mutex> lock (mutex);
    cache.clear();
}

size_t SampleBufferCache::getCachedFileCount() const {
    std::lock_guard<std::mutex> lock (mutex);
    return cache.size();
}

size_t SampleBufferCache::getTotalMemoryBytes() const {
    std::lock_guard<std::mutex> lock (mutex);
    size_t total = 0;
    for (const auto& kv : cache) {
        if (kv.second.buffer) {
            total += static_cast<size_t> (kv.second.buffer->getNumChannels())
                   * static_cast<size_t> (kv.second.buffer->getNumSamples())
                   * sizeof (float);
        }
    }
    return total;
}

//==============================================================================
// SfzRegionSound Implementation
//==============================================================================
SfzRegionSound::SfzRegionSound (const SfzRegion& regionInfo,
                                std::shared_ptr<const juce::AudioBuffer<float>> sampleBuffer,
                                double sampleRate)
    : region (regionInfo),
      buffer (std::move (sampleBuffer)),
      sourceSampleRate (sampleRate) {}

bool SfzRegionSound::appliesToNote (int midiNoteNumber) {
    return buffer != nullptr
        && region.trigger == "attack"
        && midiNoteNumber >= region.lokey
        && midiNoteNumber <= region.hikey;
}

bool SfzRegionSound::appliesToChannel (int /*midiChannel*/) {
    return true;
}

bool SfzRegionSound::appliesToVelocity (int midiVelocity) const noexcept {
    return midiVelocity >= region.lovel && midiVelocity <= region.hivel;
}

//==============================================================================
// SfzVoice Implementation (Catmull-Rom 4-point cubic interpolation + SFZ gain)
//==============================================================================
SfzVoice::SfzVoice() {
    adsr.setSampleRate (44100.0);
}

bool SfzVoice::canPlaySound (juce::SynthesiserSound* sound) {
    return dynamic_cast<const SfzRegionSound*> (sound) != nullptr;
}

void SfzVoice::startNote (int midiNoteNumber, float velocity,
                          juce::SynthesiserSound* sound,
                          int /*currentPitchWheelPosition*/) {
    currentRegionSound = dynamic_cast<const SfzRegionSound*> (sound);
    if (currentRegionSound == nullptr || currentRegionSound->getAudioBuffer() == nullptr) {
        clearCurrentNote();
        return;
    }

    const auto& reg = currentRegionSound->getRegion();
    const double playbackSr = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;
    const double sourceSr = currentRegionSound->getSourceSampleRate();

    // Calculate pitch shift in semitones including pitch_keytrack and fine tune (cents)
    const double keyDelta = static_cast<double> (midiNoteNumber - reg.pitchKeycenter)
                          * (static_cast<double> (reg.pitchKeytrack) / 100.0);
    const double totalSemitones = keyDelta + (static_cast<double> (reg.tuneCents) / 100.0);

    pitchRatio = std::pow (2.0, totalSemitones / 12.0) * (sourceSr / playbackSr);

    // Start playback at exact sample frame offset specified by SFZ offset=...
    sourceSamplePosition = static_cast<double> (std::max<juce::int64> (0, reg.offsetSamples));

    // Calculate SFZ quadratic velocity gain + region dB volume
    const int midiVel = juce::jlimit (1, 127, juce::roundToInt (velocity * 127.0f));
    const float track = juce::jlimit (0.0f, 100.0f, reg.ampVeltrack) / 100.0f;
    const float velNorm = static_cast<float> (midiVel) / 127.0f;
    const float velCurve = (1.0f - track) + track * velNorm;
    const float velGain = velCurve * velCurve;

    const float dbGain = std::pow (10.0f, reg.volumeDb / 20.0f);
    constexpr float kPolyphonicHeadroom = 0.42f; // -7.5 dB per-voice headroom for polyphonic piano chords
    const float totalGain = velGain * dbGain * kPolyphonicHeadroom;

    noteGainL = totalGain;
    noteGainR = totalGain;

    noteReleasedWhilePedalDown = false;

    adsr.setSampleRate (playbackSr);
    juce::ADSR::Parameters params;
    params.attack = std::max (0.001f, reg.ampegAttack);
    params.decay = 0.0f;
    params.sustain = 1.0f;
    params.release = std::max (0.05f, reg.ampegRelease);
    adsr.setParameters (params);
    adsr.noteOn();
}

void SfzVoice::stopNote (float /*velocity*/, bool allowTailOff) {
    if (allowTailOff) {
        if (sustainPedalDown) {
            noteReleasedWhilePedalDown = true;
        } else {
            adsr.noteOff();
        }
    } else {
        // Fast 4ms anti-click release when voice stealing
        juce::ADSR::Parameters params = adsr.getParameters();
        params.release = 0.004f;
        adsr.setParameters (params);
        adsr.noteOff();
    }
}

void SfzVoice::pitchWheelMoved (int /*newPitchWheelValue*/) {}

void SfzVoice::controllerMoved (int controllerNumber, int newControllerValue) {
    if (controllerNumber == 64) { // Sustain pedal (CC64)
        const bool down = (newControllerValue >= 64);
        if (sustainPedalDown && ! down && noteReleasedWhilePedalDown) {
            noteReleasedWhilePedalDown = false;
            adsr.noteOff();
        }
        sustainPedalDown = down;
    }
}

namespace {
inline float cubicHermite (float y0, float y1, float y2, float y3, float t) noexcept {
    const float c0 = y1;
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * t + c2) * t + c1) * t + c0;
}
} // namespace

void SfzVoice::renderNextBlock (juce::AudioBuffer<float>& outputBuffer,
                                int startSample, int numSamples) {
    if (currentRegionSound == nullptr)
        return;

    const auto* audioData = currentRegionSound->getAudioBuffer();
    if (audioData == nullptr) {
        clearCurrentNote();
        return;
    }

    const int totalFrames = audioData->getNumSamples();
    const int numChannels = audioData->getNumChannels();
    if (totalFrames <= 4 || numChannels <= 0) {
        clearCurrentNote();
        return;
    }

    const float* inL = audioData->getReadPointer (0);
    const float* inR = numChannels > 1 ? audioData->getReadPointer (1) : inL;

    float* outL = outputBuffer.getWritePointer (0, startSample);
    float* outR = outputBuffer.getNumChannels() > 1
                ? outputBuffer.getWritePointer (1, startSample)
                : nullptr;

    for (int i = 0; i < numSamples; ++i) {
        const int pos = static_cast<int> (sourceSamplePosition);
        if (pos >= totalFrames - 3 || ! adsr.isActive()) {
            clearCurrentNote();
            currentRegionSound = nullptr;
            break;
        }

        const float frac = static_cast<float> (sourceSamplePosition - static_cast<double> (pos));
        const int i0 = std::max (0, pos - 1);
        const int i1 = pos;
        const int i2 = pos + 1;
        const int i3 = pos + 2;

        const float env = adsr.getNextSample();
        const float sampleL = cubicHermite (inL[i0], inL[i1], inL[i2], inL[i3], frac) * noteGainL * env;
        const float sampleR = cubicHermite (inR[i0], inR[i1], inR[i2], inR[i3], frac) * noteGainR * env;

        outL[i] += sampleL;
        if (outR != nullptr)
            outR[i] += sampleR;

        sourceSamplePosition += pitchRatio;
    }
}

//==============================================================================
// SfzSynthesiser Implementation
//==============================================================================
SfzSynthesiser::SfzSynthesiser() {
    setNoteStealingEnabled (true);
    // Allocate 64 polyphonic voices for rich piano chords and walking bass
    for (int i = 0; i < 64; ++i)
        addVoice (new SfzVoice());
}

void SfzSynthesiser::noteOn (int midiChannel, int midiNoteNumber, float velocity) {
    const juce::ScopedLock sl (lock);
    const int midiVel = juce::jlimit (1, 127, juce::roundToInt (velocity * 127.0f));

    SfzRegionSound* exactMatch = nullptr;
    SfzRegionSound* closestMatch = nullptr;
    int minVelDistance = 9999;

    for (auto* sound : sounds) {
        if (sound->appliesToNote (midiNoteNumber) && sound->appliesToChannel (midiChannel)) {
            if (auto* sfzSound = dynamic_cast<SfzRegionSound*> (sound)) {
                if (sfzSound->appliesToVelocity (midiVel)) {
                    exactMatch = sfzSound;
                    break;
                }

                // Track closest loaded velocity layer in case exact layer is still loading in background
                const auto& reg = sfzSound->getRegion();
                const int midVel = (reg.lovel + reg.hivel) / 2;
                const int dist = std::abs (midiVel - midVel);
                if (dist < minVelDistance) {
                    minVelDistance = dist;
                    closestMatch = sfzSound;
                }
            }
        }
    }

    if (exactMatch != nullptr) {
        triggerSound (exactMatch, midiChannel, midiNoteNumber, velocity);
    } else if (closestMatch != nullptr) {
        triggerSound (closestMatch, midiChannel, midiNoteNumber, velocity);
    }
}

void SfzSynthesiser::triggerSound (juce::SynthesiserSound* sound,
                                   int midiChannel,
                                   int midiNoteNumber,
                                   float velocity) {
    // Smoothly damp any currently ringing voice on the same key before re-striking
    for (auto* voice : voices) {
        if (voice->getCurrentlyPlayingNote() == midiNoteNumber && voice->isPlayingChannel (midiChannel))
            stopVoice (voice, 1.0f, true);
    }

    startVoice (findFreeVoice (sound, midiChannel, midiNoteNumber, isNoteStealingEnabled()),
                sound, midiChannel, midiNoteNumber, velocity);
}

//==============================================================================
// SfzParser Implementation
//==============================================================================
namespace {
using OpcodeMap = std::map<juce::String, juce::String>;

// Extracts key=value pairs from a string segment accurately even when values contain spaces
void parseOpcodesFromSegment (const juce::String& segment, OpcodeMap& targetMap) {
    struct KeyMatch {
        juce::String key;
        int keyStart = 0;
        int valStart = 0;
    };

    std::vector<KeyMatch> matches;
    const int len = segment.length();
    int i = 0;

    while (i < len) {
        // Look for '=', then walk backward to extract the opcode name
        const int eqPos = segment.indexOfChar (i, '=');
        if (eqPos < 0)
            break;

        int keyEnd = eqPos;
        int keyStart = keyEnd - 1;
        while (keyStart >= 0) {
            const juce::juce_wchar c = segment[keyStart];
            if (juce::CharacterFunctions::isLetterOrDigit (c) || c == '_' || c == '-')
                --keyStart;
            else
                break;
        }
        ++keyStart;

        if (keyStart < keyEnd) {
            KeyMatch m;
            m.key = segment.substring (keyStart, keyEnd).toLowerCase();
            m.keyStart = keyStart;
            m.valStart = eqPos + 1;
            matches.push_back (m);
        }
        i = eqPos + 1;
    }

    for (size_t idx = 0; idx < matches.size(); ++idx) {
        const int valStart = matches[idx].valStart;
        const int valEnd = (idx + 1 < matches.size()) ? matches[idx + 1].keyStart : len;
        juce::String value = segment.substring (valStart, valEnd).trim();
        targetMap[matches[idx].key] = value;
    }
}

int parseMidiKey (const juce::String& str, int defaultVal) {
    if (str.isEmpty())
        return defaultVal;
    if (str.containsOnly ("-0123456789"))
        return str.getIntValue();

    // Support note names like c4, a0, f#3 if present
    juce::String s = str.toLowerCase().trim();
    static const std::map<juce::String, int> noteOffsets = {
        {"c", 0}, {"c#", 1}, {"db", 1}, {"d", 2}, {"d#", 3}, {"eb", 3},
        {"e", 4}, {"f", 5}, {"f#", 6}, {"gb", 6}, {"g", 7}, {"g#", 8},
        {"ab", 8}, {"a", 9}, {"a#", 10}, {"bb", 10}, {"b", 11}
    };

    int noteLen = (s.length() >= 2 && (s[1] == '#' || s[1] == 'b')) ? 2 : 1;
    juce::String noteName = s.substring (0, noteLen);
    int octave = s.substring (noteLen).getIntValue();
    auto it = noteOffsets.find (noteName);
    if (it != noteOffsets.end())
        return (octave + 1) * 12 + it->second;
    return defaultVal;
}

float getFloat (const OpcodeMap& m, const juce::String& key, float def) {
    auto it = m.find (key);
    return (it != m.end()) ? it->second.getFloatValue() : def;
}

int getInt (const OpcodeMap& m, const juce::String& key, int def) {
    auto it = m.find (key);
    return (it != m.end()) ? it->second.getIntValue() : def;
}

juce::String getString (const OpcodeMap& m, const juce::String& key, const juce::String& def) {
    auto it = m.find (key);
    return (it != m.end()) ? it->second : def;
}
} // namespace

SfzParser::ParseResult SfzParser::parseFile (const juce::File& sfzFile) {
    ParseResult result;
    result.sfzFile = sfzFile;

    if (! sfzFile.existsAsFile()) {
        result.errorMessage = "SFZ file does not exist: " + sfzFile.getFullPathName();
        return result;
    }

    juce::StringArray lines;
    sfzFile.readLines (lines);

    OpcodeMap controlOpcodes;
    OpcodeMap globalOpcodes;
    OpcodeMap masterOpcodes;
    OpcodeMap groupOpcodes;
    OpcodeMap currentRegionOpcodes;

    enum class Section { None, Control, Global, Master, Group, Region, Other };
    Section currentSection = Section::None;

    const juce::File baseDir = sfzFile.getParentDirectory();
    std::set<std::string> uniqueWavPaths;

    auto finalizeCurrentRegion = [&]() {
        if (currentSection != Section::Region)
            return;

        OpcodeMap merged = globalOpcodes;
        for (const auto& kv : masterOpcodes) merged[kv.first] = kv.second;
        for (const auto& kv : groupOpcodes)  merged[kv.first] = kv.second;
        for (const auto& kv : currentRegionOpcodes) merged[kv.first] = kv.second;

        const juce::String sampleRel = getString (merged, "sample", "");
        if (sampleRel.isEmpty())
            return;

        // Check CC gates from <control> defaults (e.g. set_cc20=0 disables locc20=1 release/noise regions)
        const int cc20 = getInt (controlOpcodes, "set_cc20", 0);
        const int cc21 = getInt (controlOpcodes, "set_cc21", 0);
        const int cc22 = getInt (controlOpcodes, "set_cc22", 0);
        const int cc23 = getInt (controlOpcodes, "set_cc23", 0);

        const int locc20 = getInt (merged, "locc20", 0);
        const int locc21 = getInt (merged, "locc21", 0);
        const int locc22 = getInt (merged, "locc22", 0);
        const int locc23 = getInt (merged, "locc23", 0);

        if (cc20 < locc20 || cc21 < locc21 || cc22 < locc22 || cc23 < locc23)
            return;

        SfzRegion reg;
        reg.samplePath = sampleRel;
        const juce::String normalizedRel = sampleRel.replaceCharacter ('\\', '/');
        reg.resolvedFile = baseDir.getChildFile (normalizedRel);

        if (merged.find ("key") != merged.end()) {
            const int k = parseMidiKey (merged["key"], 60);
            reg.lokey = k;
            reg.hikey = k;
            reg.pitchKeycenter = k;
        } else {
            reg.lokey = parseMidiKey (getString (merged, "lokey", "0"), 0);
            reg.hikey = parseMidiKey (getString (merged, "hikey", "127"), 127);
            reg.pitchKeycenter = parseMidiKey (getString (merged, "pitch_keycenter", "60"), reg.lokey);
        }

        // Ignore non-keyboard pedal noise triggers with lokey=-1
        if (reg.lokey < 0 || reg.hikey < 0)
            return;

        reg.lovel = getInt (merged, "lovel", 1);
        reg.hivel = getInt (merged, "hivel", 127);
        reg.pitchKeytrack = getInt (merged, "pitch_keytrack", 100);
        reg.tuneCents = getFloat (merged, "tune", 0.0f);
        reg.offsetSamples = static_cast<juce::int64> (getInt (merged, "offset", 0));

        const float vol = getFloat (merged, "volume", 0.0f);
        const float groupVol = getFloat (merged, "group_volume", 0.0f);
        reg.volumeDb = vol + groupVol;

        reg.ampVeltrack = getFloat (merged, "amp_veltrack", 98.5f);
        reg.ampegAttack = getFloat (merged, "ampeg_attack", 0.002f);
        reg.ampegRelease = getFloat (merged, "ampeg_release", 1.0f);
        reg.trigger = getString (merged, "trigger", "attack").toLowerCase();
        reg.locc20 = locc20;
        reg.locc21 = locc21;
        reg.locc22 = locc22;
        reg.locc23 = locc23;

        // Only include attack regions for playable MIDI keyboard notes
        if (reg.trigger != "attack")
            return;

        // Mark medium-high velocity layer (covering velocity 85) as primary so all 88 keys
        // become playable in ~40ms while the remaining 15 velocity layers stream in background
        if (reg.lovel <= 85 && reg.hivel >= 85)
            reg.isPrimary = true;

        result.regions.push_back (reg);

        const std::string fullWavPath = reg.resolvedFile.getFullPathName().toStdString();
        if (uniqueWavPaths.insert (fullWavPath).second)
            result.uniqueWavFiles.push_back (reg.resolvedFile);
    };

    for (const auto& rawLine : lines) {
        // Strip '//' comments
        juce::String line = rawLine;
        const int commentPos = line.indexOf ("//");
        if (commentPos >= 0)
            line = line.substring (0, commentPos);

        line = line.trim();
        if (line.isEmpty())
            continue;

        int pos = 0;
        const int len = line.length();

        while (pos < len) {
            const int tagStart = line.indexOfChar (pos, '<');
            if (tagStart < 0) {
                // Remaining line belongs to current active section
                const juce::String segment = line.substring (pos).trim();
                if (segment.isNotEmpty()) {
                    if (currentSection == Section::Control) parseOpcodesFromSegment (segment, controlOpcodes);
                    else if (currentSection == Section::Global) parseOpcodesFromSegment (segment, globalOpcodes);
                    else if (currentSection == Section::Master) parseOpcodesFromSegment (segment, masterOpcodes);
                    else if (currentSection == Section::Group)  parseOpcodesFromSegment (segment, groupOpcodes);
                    else if (currentSection == Section::Region) parseOpcodesFromSegment (segment, currentRegionOpcodes);
                }
                break;
            }

            // Process any opcodes before '<'
            if (tagStart > pos) {
                const juce::String preSegment = line.substring (pos, tagStart).trim();
                if (preSegment.isNotEmpty()) {
                    if (currentSection == Section::Control) parseOpcodesFromSegment (preSegment, controlOpcodes);
                    else if (currentSection == Section::Global) parseOpcodesFromSegment (preSegment, globalOpcodes);
                    else if (currentSection == Section::Master) parseOpcodesFromSegment (preSegment, masterOpcodes);
                    else if (currentSection == Section::Group)  parseOpcodesFromSegment (preSegment, groupOpcodes);
                    else if (currentSection == Section::Region) parseOpcodesFromSegment (preSegment, currentRegionOpcodes);
                }
            }

            const int tagEnd = line.indexOfChar (tagStart, '>');
            if (tagEnd < 0)
                break;

            // Finalize previous region if we are leaving a <region> tag
            finalizeCurrentRegion();

            const juce::String tagName = line.substring (tagStart + 1, tagEnd).trim().toLowerCase();
            if (tagName == "control") {
                currentSection = Section::Control;
            } else if (tagName == "global") {
                currentSection = Section::Global;
                globalOpcodes.clear();
                masterOpcodes.clear();
                groupOpcodes.clear();
            } else if (tagName == "master") {
                currentSection = Section::Master;
                masterOpcodes.clear();
                groupOpcodes.clear();
            } else if (tagName == "group") {
                currentSection = Section::Group;
                groupOpcodes.clear();
            } else if (tagName == "region") {
                currentSection = Section::Region;
                currentRegionOpcodes.clear();
            } else {
                currentSection = Section::Other;
            }

            pos = tagEnd + 1;
        }
    }

    finalizeCurrentRegion();

    result.globalAmpVeltrack = getFloat (globalOpcodes, "amp_veltrack", 98.5f);
    result.globalAmpegRelease = getFloat (globalOpcodes, "ampeg_release", 1.0f);
    result.success = ! result.regions.empty();
    if (! result.success && result.errorMessage.isEmpty())
        result.errorMessage = "No playable <region> entries found in " + sfzFile.getFullPathName();

    return result;
}

//==============================================================================
// AsyncSfzLoaderThread Implementation
//==============================================================================
AsyncSfzLoaderThread::AsyncSfzLoaderThread (
    SfzSynthesiser& targetSynth,
    const juce::File& sfzFileToLoad,
    std::function<void()> onPrimaryReadyCallback,
    std::function<void()> onAllReadyCallback)
    : juce::Thread ("AsyncSfzLoader"),
      synth (targetSynth),
      sfzFile (sfzFileToLoad),
      onPrimaryReady (std::move (onPrimaryReadyCallback)),
      onAllReady (std::move (onAllReadyCallback)) {}

AsyncSfzLoaderThread::~AsyncSfzLoaderThread() {
    stopThread (5000);
}

void AsyncSfzLoaderThread::setStatus (const juce::String& text) {
    const juce::ScopedLock sl (statusLock);
    statusMessage = text;
}

juce::File AsyncSfzLoaderThread::findDefaultSfzFile() {
    std::vector<juce::File> searchRoots = {
        juce::File::getCurrentWorkingDirectory(),
        juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory(),
        juce::File ("/usr/local/google/home/mhorowitzgelb/improv_pulse")
    };

    const std::vector<juce::String> candidateRelPaths = {
        "sfz_instrument_loader/salamander_piano/sfz_daw/Accurate-SalamanderGrandPiano_flat.Recommended.sfz",
        "sfz_instrument_loader/sfz_daw/Accurate-SalamanderGrandPiano_flat.Recommended.sfz",
        "salamander_piano/sfz_daw/Accurate-SalamanderGrandPiano_flat.Recommended.sfz",
        "sfz_daw/Accurate-SalamanderGrandPiano_flat.Recommended.sfz"
    };

    for (auto dir : searchRoots) {
        for (int depth = 0; depth < 5; ++depth) {
            for (const auto& rel : candidateRelPaths) {
                juce::File candidate = dir.getChildFile (rel);
                if (candidate.existsAsFile())
                    return candidate;
            }
            dir = dir.getParentDirectory();
        }
    }

    return juce::File ("/usr/local/google/home/mhorowitzgelb/improv_pulse/sfz_instrument_loader/salamander_piano/sfz_daw/Accurate-SalamanderGrandPiano_flat.Recommended.sfz");
}

std::vector<juce::File> AsyncSfzLoaderThread::findAvailableSfzPresets() {
    std::vector<juce::File> presets;
    const juce::File defaultSfz = findDefaultSfzFile();
    if (! defaultSfz.existsAsFile())
        return presets;

    const juce::File dawDir = defaultSfz.getParentDirectory();
    juce::Array<juce::File> found;
    dawDir.findChildFiles (found, juce::File::findFiles, false, "*.sfz");

    for (const auto& f : found)
        presets.push_back (f);

    std::sort (presets.begin(), presets.end(), [](const juce::File& a, const juce::File& b) {
        // Put "Recommended" first
        const bool aRec = a.getFileName().containsIgnoreCase ("Recommended");
        const bool bRec = b.getFileName().containsIgnoreCase ("Recommended");
        if (aRec != bRec)
            return aRec;
        return a.getFileName() < b.getFileName();
    });

    return presets;
}

void AsyncSfzLoaderThread::run() {
    setStatus ("Parsing SFZ instrument definition: " + sfzFile.getFileName() + "...");

    const auto parseResult = SfzParser::parseFile (sfzFile);
    if (! parseResult.success) {
        setStatus ("Error loading SFZ: " + parseResult.errorMessage);
        return;
    }

    const int totalUniqueWavs = static_cast<int> (parseResult.uniqueWavFiles.size());
    totalCount.store (totalUniqueWavs);
    loadedCount.store (0);
    progress.store (0.0f);

    synth.clearSounds();

    // Separate regions into Primary (1 layer per key = 30 unique WAVs) and Secondary (remaining 15 layers)
    std::vector<SfzRegion> primaryRegions;
    std::vector<SfzRegion> secondaryRegions;
    std::set<std::string> primaryWavPaths;
    std::vector<juce::File> primaryWavFiles;
    std::vector<juce::File> secondaryWavFiles;

    for (const auto& reg : parseResult.regions) {
        if (reg.isPrimary) {
            primaryRegions.push_back (reg);
            const std::string p = reg.resolvedFile.getFullPathName().toStdString();
            if (primaryWavPaths.insert (p).second)
                primaryWavFiles.push_back (reg.resolvedFile);
        } else {
            secondaryRegions.push_back (reg);
        }
    }

    for (const auto& wav : parseResult.uniqueWavFiles) {
        const std::string p = wav.getFullPathName().toStdString();
        if (primaryWavPaths.find (p) == primaryWavPaths.end())
            secondaryWavFiles.push_back (wav);
    }

    auto getDurationLimitForFile = [](const juce::File& f) -> double {
        // Determine optimal max duration based on pitch number in filename (e.g. 021_A0v01.wav -> pitch 21)
        const int pitch = f.getFileName().substring (0, 3).getIntValue();
        if (pitch > 0 && pitch < 48) return 9.0;
        if (pitch >= 48 && pitch < 72) return 7.0;
        return 5.5;
    };

    std::atomic<int> count { 0 };
    const unsigned int numThreads = std::max (2u, std::min (8u, std::thread::hardware_concurrency()));

    auto loadWavBatchParallel = [&](const std::vector<juce::File>& wavBatch) {
        std::vector<std::future<void>> futures;
        std::atomic<size_t> nextIdx { 0 };

        for (unsigned int t = 0; t < numThreads; ++t) {
            futures.push_back (std::async (std::launch::async, [&]() {
                juce::AudioFormatManager localFormatMgr;
                localFormatMgr.registerBasicFormats();

                while (! threadShouldExit()) {
                    const size_t idx = nextIdx.fetch_add (1);
                    if (idx >= wavBatch.size())
                        break;

                    const juce::File& wavFile = wavBatch[idx];
                    if (wavFile.existsAsFile()) {
                        SampleBufferCache::getInstance().getOrLoad (
                            wavFile, localFormatMgr, getDurationLimitForFile (wavFile));
                    }

                    const int current = ++count;
                    loadedCount.store (current);
                    progress.store (static_cast<float> (current) / static_cast<float> (std::max (1, totalUniqueWavs)));
                }
            }));
        }

        for (auto& f : futures)
            f.wait();
    };

    // 1. Load primary WAV files in parallel (~30 files across 8 threads -> ~35 milliseconds)
    setStatus ("Loading primary Accurate Salamander Grand Piano layer (48kHz 24-bit)...");
    loadWavBatchParallel (primaryWavFiles);

    if (threadShouldExit())
        return;

    // Register primary regions with SfzSynthesiser immediately
    {
        juce::AudioFormatManager dummyMgr;
        dummyMgr.registerBasicFormats();
        for (const auto& reg : primaryRegions) {
            auto cached = SampleBufferCache::getInstance().getOrLoad (reg.resolvedFile, dummyMgr);
            if (cached.buffer != nullptr)
                synth.addSound (new SfzRegionSound (reg, cached.buffer, cached.sampleRate));
        }
    }

    primaryReady.store (true);
    setStatus ("Playing 100 BPM E Minor Blues (streaming 16 velocity layers in background...)");
    if (onPrimaryReady) {
        if (auto* mm = juce::MessageManager::getInstanceWithoutCreating())
            mm->callAsync (onPrimaryReady);
        else
            onPrimaryReady();
    }

    // 2. Load secondary WAV files in parallel chunks so UI progress updates smoothly
    const size_t chunkSize = 32;
    for (size_t offset = 0; offset < secondaryWavFiles.size(); offset += chunkSize) {
        if (threadShouldExit())
            return;

        const size_t end = std::min (secondaryWavFiles.size(), offset + chunkSize);
        std::vector<juce::File> chunk (secondaryWavFiles.begin() + static_cast<ptrdiff_t> (offset),
                                       secondaryWavFiles.begin() + static_cast<ptrdiff_t> (end));
        loadWavBatchParallel (chunk);
    }

    if (threadShouldExit())
        return;

    // Register all remaining secondary regions
    {
        juce::AudioFormatManager dummyMgr;
        dummyMgr.registerBasicFormats();
        for (const auto& reg : secondaryRegions) {
            auto cached = SampleBufferCache::getInstance().getOrLoad (reg.resolvedFile, dummyMgr);
            if (cached.buffer != nullptr)
                synth.addSound (new SfzRegionSound (reg, cached.buffer, cached.sampleRate));
        }
    }

    allReady.store (true);
    progress.store (1.0f);

    const size_t memMb = SampleBufferCache::getInstance().getTotalMemoryBytes() / (1024 * 1024);
    setStatus ("Accurate Salamander Grand Piano V6.2 Ready — 88 Keys, 16 Velocity Layers ("
               + juce::String (totalUniqueWavs) + " WAV samples, "
               + juce::String (parseResult.regions.size()) + " regions, "
               + juce::String (memMb) + " MB)");

    if (onAllReady) {
        if (auto* mm = juce::MessageManager::getInstanceWithoutCreating())
            mm->callAsync (onAllReady);
        else
            onAllReady();
    }
}

} // namespace sfz
