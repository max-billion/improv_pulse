#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace sfz {

//==============================================================================
// Represents a single parsed <region> from an SFZ file (merged with <global>, <master>, <group>)
//==============================================================================
struct SfzRegion {
    juce::String samplePath;        // Raw path string from sample=... opcode
    juce::File resolvedFile;        // Absolute resolved WAV file on disk
    int lokey = 0;                  // Lowest MIDI note (0..127)
    int hikey = 127;                // Highest MIDI note (0..127)
    int lovel = 1;                  // Lowest MIDI velocity (1..127)
    int hivel = 127;                // Highest MIDI velocity (1..127)
    int pitchKeycenter = 60;        // Root MIDI key of sample
    int pitchKeytrack = 100;        // Cents per semitone (100 = standard chromatic)
    float tuneCents = 0.0f;         // Fine tuning in cents (-100..+100)
    juce::int64 offsetSamples = 0;  // Start sample frame offset from SFZ offset=...
    float volumeDb = 0.0f;          // Combined region + group volume in dB
    float ampVeltrack = 98.5f;      // Velocity sensitivity percentage (0..100)
    float ampegAttack = 0.002f;     // Attack time in seconds
    float ampegRelease = 1.0f;      // Release time in seconds (from ampeg_release / off_time)
    juce::String trigger = "attack";// "attack", "release", "release_key", etc.
    int locc20 = 0;                 // Sampled release CC threshold
    int locc21 = 0;                 // Hammer noise CC threshold
    int locc22 = 0;                 // Pedal noise CC threshold
    int locc23 = 0;                 // Pedal resonance CC threshold
    bool isPrimary = false;         // True for primary velocity layer (loaded first for instant play)
};

//==============================================================================
// Shared in-memory cache of decoded WAV audio buffers so regions sharing WAVs
// (and preset switches between flat/bass/treble .sfz files) do not duplicate RAM or disk I/O
//==============================================================================
class SampleBufferCache {
public:
    struct CachedSample {
        std::shared_ptr<const juce::AudioBuffer<float>> buffer;
        double sampleRate = 48000.0;
        juce::int64 numFrames = 0;
    };

    static SampleBufferCache& getInstance();

    CachedSample getOrLoad (const juce::File& wavFile,
                            juce::AudioFormatManager& formatManager,
                            double maxDurationSeconds = 10.0);

    void clear();
    size_t getCachedFileCount() const;
    size_t getTotalMemoryBytes() const;

private:
    SampleBufferCache() = default;
    mutable std::mutex mutex;
    std::unordered_map<std::string, CachedSample> cache;
};

//==============================================================================
// SynthesiserSound representing an SFZ region with a shared audio buffer
//==============================================================================
class SfzRegionSound : public juce::SynthesiserSound {
public:
    using Ptr = juce::ReferenceCountedObjectPtr<SfzRegionSound>;

    SfzRegionSound (const SfzRegion& regionInfo,
                    std::shared_ptr<const juce::AudioBuffer<float>> sampleBuffer,
                    double sampleRate);

    bool appliesToNote (int midiNoteNumber) override;
    bool appliesToChannel (int midiChannel) override;
    bool appliesToVelocity (int midiVelocity) const noexcept;

    const SfzRegion& getRegion() const noexcept { return region; }
    const juce::AudioBuffer<float>* getAudioBuffer() const noexcept { return buffer.get(); }
    double getSourceSampleRate() const noexcept { return sourceSampleRate; }

private:
    SfzRegion region;
    std::shared_ptr<const juce::AudioBuffer<float>> buffer;
    double sourceSampleRate = 48000.0;
};

//==============================================================================
// SynthesiserVoice implementing SFZ pitch tracking, cents tuning, sample offset,
// quadratic SFZ amp_veltrack velocity curve, dB gain, and cubic interpolation
//==============================================================================
class SfzVoice : public juce::SynthesiserVoice {
public:
    SfzVoice();

    bool canPlaySound (juce::SynthesiserSound* sound) override;
    void startNote (int midiNoteNumber, float velocity,
                    juce::SynthesiserSound* sound,
                    int currentPitchWheelPosition) override;
    void stopNote (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int newPitchWheelValue) override;
    void controllerMoved (int controllerNumber, int newControllerValue) override;
    using juce::SynthesiserVoice::renderNextBlock;
    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer,
                          int startSample, int numSamples) override;

private:
    double sourceSamplePosition = 0.0;
    double pitchRatio = 1.0;
    float noteGainL = 1.0f;
    float noteGainR = 1.0f;
    bool sustainPedalDown = false;
    bool noteReleasedWhilePedalDown = false;
    juce::ADSR adsr;
    const SfzRegionSound* currentRegionSound = nullptr;
};

//==============================================================================
// Custom Synthesiser routing noteOn by SFZ key and 16-layer velocity mapping,
// with automatic fallback to closest loaded velocity layer during progressive background loading
//==============================================================================
class SfzSynthesiser : public juce::Synthesiser {
public:
    SfzSynthesiser();

    void noteOn (int midiChannel, int midiNoteNumber, float velocity) override;

private:
    void triggerSound (juce::SynthesiserSound* sound, int midiChannel, int midiNoteNumber, float velocity);
};

//==============================================================================
// Parser for SFZ v1/v2 instrument definition files
//==============================================================================
class SfzParser {
public:
    struct ParseResult {
        juce::File sfzFile;
        std::vector<SfzRegion> regions;
        std::vector<juce::File> uniqueWavFiles;
        float globalAmpVeltrack = 98.5f;
        float globalAmpegRelease = 1.0f;
        juce::String errorMessage;
        bool success = false;
    };

    static ParseResult parseFile (const juce::File& sfzFile);
};

//==============================================================================
// Background multi-threaded loader for SFZ instruments and WAV sample banks
//==============================================================================
class AsyncSfzLoaderThread : public juce::Thread {
public:
    AsyncSfzLoaderThread (SfzSynthesiser& targetSynth,
                          const juce::File& sfzFileToLoad,
                          std::function<void()> onPrimaryReadyCallback = nullptr,
                          std::function<void()> onAllReadyCallback = nullptr);

    ~AsyncSfzLoaderThread() override;

    float getProgress() const noexcept { return progress.load(); }
    int getLoadedCount() const noexcept { return loadedCount.load(); }
    int getTotalCount() const noexcept { return totalCount.load(); }
    bool isPrimaryReady() const noexcept { return primaryReady.load(); }
    bool isAllReady() const noexcept { return allReady.load(); }
    juce::File getCurrentSfzFile() const { return sfzFile; }

    juce::String getStatusText() const {
        const juce::ScopedLock sl (statusLock);
        return statusMessage;
    }

    // Utility functions to locate the extracted Salamander Grand Piano SFZ files
    static juce::File findDefaultSfzFile();
    static std::vector<juce::File> findAvailableSfzPresets();

    void run() override;

private:
    void setStatus (const juce::String& text);

    SfzSynthesiser& synth;
    juce::File sfzFile;
    std::function<void()> onPrimaryReady;
    std::function<void()> onAllReady;

    std::atomic<float> progress { 0.0f };
    std::atomic<int> loadedCount { 0 };
    std::atomic<int> totalCount { 480 };
    std::atomic<bool> primaryReady { false };
    std::atomic<bool> allReady { false };

    mutable juce::CriticalSection statusLock;
    juce::String statusMessage { "Initializing SFZ Instrument Loader..." };
};

} // namespace sfz
