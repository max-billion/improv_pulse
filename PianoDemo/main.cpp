#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "SfzInstrumentLoader.h"
#include <atomic>
#include <cmath>
#include <iostream>
#include <vector>

//==============================================================================
// E Minor 12-Bar Blues Sequencer (48 Beats @ 100 BPM)
//==============================================================================
struct ScheduledNote {
    double startBeat = 0.0;     // 0.0 .. 48.0
    double durationBeats = 1.0;
    int midiNote = 60;
    float velocity = 0.8f;      // 0.0 .. 1.0
};

struct BarInfo {
    int barNumber = 1;
    juce::String chordSymbol;
    juce::String romanNumeral;
    juce::String chordNotes;
    std::vector<int> bassNotes;     // 4 quarter-note walking bass notes
    std::vector<int> chordVoicing;  // RH chord voicing
};

class EMinorBluesSequencer {
public:
    EMinorBluesSequencer() {
        rebuildPattern (0, 0);
    }

    void setStyleAndArrangement (int turnaroundStyle, int arrangementStyle) {
        if (turnaroundStyle != currentTurnaround || arrangementStyle != currentArrangement) {
            currentTurnaround = turnaroundStyle;
            currentArrangement = arrangementStyle;
            rebuildPattern (currentTurnaround, currentArrangement);
        }
    }

    const std::vector<BarInfo>& getBars() const noexcept { return bars; }
    const std::vector<ScheduledNote>& getNotes() const noexcept { return notes; }

    void rebuildPattern (int turnaroundStyle, int arrangementStyle) {
        bars.clear();
        notes.clear();

        // Define the 12 bars of E minor blues
        // Turnaround Style 0: Traditional Minor Blues (Bm7 -> Am7 -> Em7 -> B7)
        // Turnaround Style 1: Jazz/Blues Minor Turnaround (C7 -> B7#9 -> Em7 -> B7#9)
        const BarInfo em7Bar = {
            1, "Em7", "i7", "E - G - B - D",
            { 40, 43, 47, 50 },         // E2, G2, B2, D3
            { 55, 59, 62, 64 }          // G3, B3, D4, E4
        };
        const BarInfo am7Bar = {
            5, "Am7", "iv7", "A - C - E - G",
            { 33, 36, 40, 43 },         // A1, C2, E2, G2
            { 55, 60, 64, 67 }          // G3, C4, E4, G4
        };
        const BarInfo bm7Bar = {
            9, "Bm7", "v7", "B - D - F# - A",
            { 35, 38, 42, 45 },         // B1, D2, F#2, A2
            { 57, 62, 66, 69 }          // A3, D4, F#4, A4
        };
        const BarInfo b7Bar = {
            12, "B7", "V7", "B - D# - F# - A",
            { 35, 39, 42, 45 },         // B1, D#2, F#2, A2
            { 57, 63, 66, 69 }          // A3, D#4, F#4, A4
        };
        const BarInfo c7Bar = {
            9, "C7", "bVI7", "C - E - G - Bb",
            { 36, 40, 43, 46 },         // C2, E2, G2, Bb2
            { 55, 58, 64, 67 }          // G3, Bb3, E4, G4
        };
        const BarInfo b7Sharp9Bar = {
            10, "B7#9", "V7#9", "B - D# - A - D",
            { 35, 39, 45, 47 },         // B1, D#2, A2, B2
            { 57, 63, 66, 70 }          // A3, D#4, F#4, Bb4(A#4/D5)
        };

        for (int b = 0; b < 12; ++b) {
            BarInfo info;
            if (b < 4 || b == 6 || b == 7 || b == 10) {
                info = em7Bar;
            } else if (b == 4 || b == 5) {
                info = am7Bar;
            } else if (b == 8) {
                info = (turnaroundStyle == 0) ? bm7Bar : c7Bar;
            } else if (b == 9) {
                info = (turnaroundStyle == 0) ? am7Bar : b7Sharp9Bar;
            } else {
                info = (turnaroundStyle == 0) ? b7Bar : b7Sharp9Bar;
            }
            info.barNumber = b + 1;
            bars.push_back (info);
        }

        // Build scheduled notes across 48 beats (12 bars * 4 beats)
        for (int barIdx = 0; barIdx < 12; ++barIdx) {
            const double barStart = barIdx * 4.0;
            const auto& bar = bars[static_cast<size_t> (barIdx)];

            if (arrangementStyle == 0) {
                // Arrangement 0: Authentic Blues Piano Groove
                // 1) Left Hand: Swung 4-beat walking bassline with subtle ghost pickups
                for (int beat = 0; beat < 4; ++beat) {
                    const double beatPos = barStart + beat;
                    const int bassNote = bar.bassNotes[static_cast<size_t> (beat)];
                    const float vel = (beat == 0) ? 0.86f : ((beat == 2) ? 0.78f : 0.72f);
                    notes.push_back ({ beatPos, 0.88, bassNote, vel });

                    // Add swung eighth-note bass pickup on beat 2-and and 4-and
                    if (beat == 1 || beat == 3) {
                        int pickupNote = bassNote + 2;
                        if (beat == 3 && barIdx < 11) {
                            pickupNote = bars[static_cast<size_t> (barIdx + 1)].bassNotes[0] - 1; // chromatic approach
                        }
                        notes.push_back ({ beatPos + 0.66, 0.30, pickupNote, 0.56f });
                    }
                }

                // 2) Right Hand: Swung blues chord comping (Charleston / blues syncopation)
                // Beat 1: Dotted quarter chord stab
                for (int n : bar.chordVoicing)
                    notes.push_back ({ barStart + 0.0, 1.35, n, 0.76f });

                // Beat 2-and: Swung upbeat syncopated chord
                for (int n : bar.chordVoicing)
                    notes.push_back ({ barStart + 1.66, 0.90, n, 0.68f });

                // Beat 4: Quarter chord or E minor blues scale fill
                if (barIdx == 3 || barIdx == 7 || barIdx == 11) {
                    // Blues scale melodic turnaround lick in octave 4-5 (E minor pentatonic/blues: E G A Bb B D E)
                    const std::vector<std::pair<double, int>> lick = {
                        { 2.66, 64 }, // E4
                        { 3.00, 67 }, // G4
                        { 3.33, 69 }, // A4
                        { 3.50, 70 }, // Bb4 (blue note)
                        { 3.66, 71 }  // B4
                    };
                    for (const auto& item : lick)
                        notes.push_back ({ barStart + item.first, 0.28, item.second, 0.82f });
                } else {
                    for (int n : bar.chordVoicing)
                        notes.push_back ({ barStart + 3.0, 0.85, n, 0.64f });
                }
            }
            else if (arrangementStyle == 1) {
                // Arrangement 1: Rhythmic Comping (LH Root+5th Shuffle + RH Chords)
                const int root = bar.bassNotes[0];
                const int fifth = root + 7;
                const int sixth = root + 9;

                // LH Shuffle pattern
                for (int beat = 0; beat < 4; ++beat) {
                    const double bPos = barStart + beat;
                    notes.push_back ({ bPos, 0.55, root, (beat % 2 == 0) ? 0.84f : 0.72f });
                    notes.push_back ({ bPos, 0.55, (beat % 2 == 0) ? fifth : sixth, 0.74f });
                }

                // RH Syncopated chords on Beat 1 and Beat 2-and & Beat 4
                for (int n : bar.chordVoicing) {
                    notes.push_back ({ barStart + 0.0, 1.4, n, 0.75f });
                    notes.push_back ({ barStart + 1.66, 1.1, n, 0.68f });
                    notes.push_back ({ barStart + 3.0, 0.8, n, 0.70f });
                }
            }
            else {
                // Arrangement 2: Steady Block Chords (Clean 4-Beat Voicings)
                const int root = bar.bassNotes[0];
                const int octaveRoot = root + 12;
                for (int beat = 0; beat < 4; ++beat) {
                    const double bPos = barStart + beat;
                    const float v = (beat == 0) ? 0.84f : 0.68f;
                    notes.push_back ({ bPos, 0.88, root, v });
                    notes.push_back ({ bPos, 0.88, octaveRoot, v * 0.9f });
                    for (int n : bar.chordVoicing)
                        notes.push_back ({ bPos, 0.88, n, v });
                }
            }
        }
    }

private:
    int currentTurnaround = 0;
    int currentArrangement = 0;
    std::vector<BarInfo> bars;
    std::vector<ScheduledNote> notes;
};

//==============================================================================
// Main Audio + GUI Component
//==============================================================================
class MainComponent : public juce::AudioAppComponent,
                      private juce::Timer {
public:
    MainComponent()
        : keyboardComponent (keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard) {
        setSize (980, 680);

        // Setup UI controls
        addAndMakeVisible (playPauseButton);
        playPauseButton.setButtonText ("Pause");
        playPauseButton.onClick = [this] {
            isPlaying.store (! isPlaying.load());
            playPauseButton.setButtonText (isPlaying.load() ? "Pause" : "Play");
        };

        addAndMakeVisible (restartButton);
        restartButton.setButtonText ("Restart Bar 1");
        restartButton.onClick = [this] {
            requestJumpToBeat.store (0.0);
        };

        addAndMakeVisible (turnaroundCombo);
        turnaroundCombo.addItem ("Traditional E Minor Blues (Bm7 -> Am7 -> Em7 -> B7)", 1);
        turnaroundCombo.addItem ("Jazz/Modern Minor Blues (C7 -> B7#9 -> Em7 -> B7#9)", 2);
        turnaroundCombo.setSelectedId (1, juce::dontSendNotification);
        turnaroundCombo.onChange = [this] { updateSequencerSettings(); };

        addAndMakeVisible (arrangementCombo);
        arrangementCombo.addItem ("Blues Groove (Walking Bass + Comping + Licks)", 1);
        arrangementCombo.addItem ("Rhythmic Shuffle Comping (LH Shuffle + RH Stabs)", 2);
        arrangementCombo.addItem ("Steady Block Chords (Quarter-Note Voicings)", 3);
        arrangementCombo.setSelectedId (1, juce::dontSendNotification);
        arrangementCombo.onChange = [this] { updateSequencerSettings(); };

        // Populate SFZ Preset selector (from sfz_instrument_loader)
        availableSfzPresets = sfz::AsyncSfzLoaderThread::findAvailableSfzPresets();
        addAndMakeVisible (sfzPresetCombo);
        for (size_t i = 0; i < availableSfzPresets.size(); ++i) {
            const juce::String fname = availableSfzPresets[i].getFileNameWithoutExtension();
            juce::String label = fname;
            if (fname.containsIgnoreCase ("flat.Recommended"))
                label = "SFZ Tone: Flat (Recommended)";
            else if (fname.containsIgnoreCase ("bass1.0db"))
                label = "SFZ Tone: Bass +1.0 dB";
            else if (fname.containsIgnoreCase ("bass1.5db"))
                label = "SFZ Tone: Bass +1.5 dB";
            else if (fname.containsIgnoreCase ("treble0.5db"))
                label = "SFZ Tone: Treble +0.5 dB";
            else if (fname.containsIgnoreCase ("treble1.0db"))
                label = "SFZ Tone: Treble +1.0 dB";
            else if (fname.containsIgnoreCase ("treble1.5db"))
                label = "SFZ Tone: Treble +1.5 dB";
            else if (fname.containsIgnoreCase ("treble2.0db"))
                label = "SFZ Tone: Treble +2.0 dB";
            else if (fname.containsIgnoreCase ("treble2.5db"))
                label = "SFZ Tone: Treble +2.5 dB";

            sfzPresetCombo.addItem (label, static_cast<int> (i + 1));
        }
        if (! availableSfzPresets.empty()) {
            sfzPresetCombo.setSelectedId (1, juce::dontSendNotification);
        }
        sfzPresetCombo.onChange = [this] {
            const int idx = sfzPresetCombo.getSelectedId() - 1;
            if (idx >= 0 && idx < static_cast<int> (availableSfzPresets.size())) {
                loadSfzInstrument (availableSfzPresets[static_cast<size_t> (idx)]);
            }
        };

        addAndMakeVisible (tempoSlider);
        tempoSlider.setRange (60.0, 160.0, 1.0);
        tempoSlider.setValue (100.0, juce::dontSendNotification);
        tempoSlider.setTextValueSuffix (" BPM");
        tempoSlider.onValueChange = [this] {
            bpm.store (tempoSlider.getValue());
        };

        addAndMakeVisible (resetTempoButton);
        resetTempoButton.setButtonText ("100 BPM");
        resetTempoButton.onClick = [this] {
            tempoSlider.setValue (100.0, juce::sendNotificationSync);
        };

        addAndMakeVisible (volumeSlider);
        volumeSlider.setRange (0.0, 1.5, 0.01);
        volumeSlider.setValue (0.95, juce::dontSendNotification);
        volumeSlider.onValueChange = [this] {
            masterGain.store (static_cast<float> (volumeSlider.getValue()));
        };

        // Configure 88-key interactive keyboard display
        addAndMakeVisible (keyboardComponent);
        keyboardComponent.setAvailableRange (21, 108); // A0 to C8
        keyboardComponent.setKeyWidth (16.5f);
        keyboardComponent.setScrollButtonsVisible (false);

        // Start audio immediately and launch SFZ loader background thread
        setAudioChannels (0, 2);
        audioStarted = true;

        const juce::File defaultSfz = availableSfzPresets.empty()
                                    ? sfz::AsyncSfzLoaderThread::findDefaultSfzFile()
                                    : availableSfzPresets[0];
        loadSfzInstrument (defaultSfz);

        startTimerHz (30); // 30 FPS UI refresh
    }

    ~MainComponent() override {
        stopTimer();
        if (sampleLoader)
            sampleLoader->stopThread (4000);
        shutdownAudio();
    }

    void loadSfzInstrument (const juce::File& sfzFile) {
        if (sampleLoader)
            sampleLoader->stopThread (4000);

        allNotesOffPending.store (true);
        sampleLoader = std::make_unique<sfz::AsyncSfzLoaderThread> (
            synth,
            sfzFile,
            [this] {
                if (! audioStarted) {
                    setAudioChannels (0, 2);
                    audioStarted = true;
                }
            });
        sampleLoader->startThread();
    }

    void updateSequencerSettings() {
        const juce::ScopedLock sl (sequencerLock);
        sequencer.setStyleAndArrangement (
            turnaroundCombo.getSelectedId() - 1,
            arrangementCombo.getSelectedId() - 1);
        allNotesOffPending.store (true);
    }

    //==============================================================================
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override {
        juce::ignoreUnused (samplesPerBlockExpected);
        currentSampleRate.store (sampleRate);
        synth.setCurrentPlaybackSampleRate (sampleRate);
        keyboardState.reset();
        std::cout << "PianoDemo Audio Active - Sample Rate: " << sampleRate
                  << " Hz, Tempo: " << bpm.load() << " BPM" << std::endl;
    }

    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override {
        bufferToFill.clearActiveBufferRegion();

        const double sr = currentSampleRate.load();
        if (sr <= 0.0)
            return;

        juce::MidiBuffer midiBuffer;
        const int numSamples = bufferToFill.numSamples;

        if (allNotesOffPending.exchange (false)) {
            for (int n = 21; n <= 108; ++n)
                midiBuffer.addEvent (juce::MidiMessage::noteOff (1, n), 0);
        }

        // Handle user jump request
        const double jumpBeat = requestJumpToBeat.exchange (-1.0);
        if (jumpBeat >= 0.0) {
            currentLoopBeat = jumpBeat;
            for (int n = 21; n <= 108; ++n)
                midiBuffer.addEvent (juce::MidiMessage::noteOff (1, n), 0);
        }

        if (isPlaying.load() && sampleLoader && sampleLoader->isPrimaryReady()) {
            const double currentBpm = bpm.load();
            const double beatsPerSecond = currentBpm / 60.0;
            const double beatsPerSample = beatsPerSecond / sr;
            const double blockBeats = numSamples * beatsPerSample;

            const double startBeat = currentLoopBeat;
            const double endBeat = startBeat + blockBeats;

            const juce::ScopedLock sl (sequencerLock);
            const auto& notes = sequencer.getNotes();

            auto checkAndSchedule = [&] (double windowStart, double windowEnd, int sampleOffsetBase) {
                for (const auto& note : notes) {
                    // Check Note-On
                    if (note.startBeat >= windowStart && note.startBeat < windowEnd) {
                        const int offset = sampleOffsetBase + juce::jlimit (
                            0, numSamples - 1,
                            static_cast<int> ((note.startBeat - windowStart) / beatsPerSample));
                        midiBuffer.addEvent (
                            juce::MidiMessage::noteOn (1, note.midiNote, note.velocity),
                            offset);
                    }

                    // Check Note-Off
                    double noteEnd = note.startBeat + note.durationBeats;
                    if (noteEnd >= 48.0)
                        noteEnd -= 48.0;

                    if (noteEnd >= windowStart && noteEnd < windowEnd) {
                        const int offset = sampleOffsetBase + juce::jlimit (
                            0, numSamples - 1,
                            static_cast<int> ((noteEnd - windowStart) / beatsPerSample));
                        midiBuffer.addEvent (
                            juce::MidiMessage::noteOff (1, note.midiNote),
                            offset);
                    }
                }
            };

            if (endBeat < 48.0) {
                checkAndSchedule (startBeat, endBeat, 0);
                currentLoopBeat = endBeat;
            } else {
                // Wrap around 48-beat (12-bar) boundary seamlessly
                const double beatsBeforeWrap = 48.0 - startBeat;
                const int samplesBeforeWrap = juce::jlimit (
                    0, numSamples, static_cast<int> (beatsBeforeWrap / beatsPerSample));
                checkAndSchedule (startBeat, 48.0, 0);
                const double wrappedEnd = endBeat - 48.0;
                checkAndSchedule (0.0, wrappedEnd, samplesBeforeWrap);
                currentLoopBeat = wrappedEnd;
            }

            displayBeat.store (currentLoopBeat);
        }

        // Process keyboard state so both sequencer notes and user mouse clicks update the UI keyboard
        keyboardState.processNextMidiBuffer (midiBuffer, 0, numSamples, true);

        // Render Accurate Salamander Grand Piano V6.2 SFZ audio
        synth.renderNextBlock (*bufferToFill.buffer, midiBuffer,
                               bufferToFill.startSample, numSamples);

        // Apply master gain
        bufferToFill.buffer->applyGain (bufferToFill.startSample, numSamples, masterGain.load());
    }

    void releaseResources() override {}

    //==============================================================================
    void timerCallback() override {
        repaint();
    }

    void mouseDown (const juce::MouseEvent& event) override {
        // Allow clicking on any of the 12 bar cards to jump immediately to that bar
        for (size_t i = 0; i < barCardBounds.size(); ++i) {
            if (barCardBounds[i].contains (event.getPosition())) {
                requestJumpToBeat.store (static_cast<double> (i) * 4.0);
                break;
            }
        }
    }

    void resized() override {
        auto area = getLocalBounds().reduced (14);

        // Top header takes 76px
        area.removeFromTop (76);

        // Control strip takes 40px
        auto controlRow = area.removeFromTop (40);
        playPauseButton.setBounds (controlRow.removeFromLeft (90).reduced (3));
        restartButton.setBounds (controlRow.removeFromLeft (115).reduced (3));
        turnaroundCombo.setBounds (controlRow.removeFromLeft (255).reduced (3));
        arrangementCombo.setBounds (controlRow.removeFromLeft (245).reduced (3));
        resetTempoButton.setBounds (controlRow.removeFromRight (75).reduced (3));
        tempoSlider.setBounds (controlRow.removeFromRight (165).reduced (3));

        auto volRow = area.removeFromTop (32);
        volumeSlider.setBounds (volRow.removeFromRight (210).reduced (2));
        sfzPresetCombo.setBounds (volRow.removeFromRight (250).reduced (2));

        // Bottom piano keyboard takes 115px
        auto keyboardArea = area.removeFromBottom (115);
        keyboardComponent.setBounds (keyboardArea);

        area.removeFromBottom (10);

        // Compute 4x3 grid for the 12 bars
        barGridArea = area;
        barCardBounds.clear();
        const int cols = 4;
        const int rows = 3;
        const int cardW = barGridArea.getWidth() / cols;
        const int cardH = barGridArea.getHeight() / rows;

        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                barCardBounds.push_back (
                    juce::Rectangle<int> (barGridArea.getX() + c * cardW,
                                          barGridArea.getY() + r * cardH,
                                          cardW, cardH).reduced (5));
            }
        }
    }

    void paint (juce::Graphics& g) override {
        // Dark studio background gradient
        juce::ColourGradient bgGrad (
            juce::Colour (0xff161922), 0.0f, 0.0f,
            juce::Colour (0xff0f1117), 0.0f, static_cast<float> (getHeight()), false);
        g.setGradientFill (bgGrad);
        g.fillAll();

        // 1. Header Banner
        auto headerArea = getLocalBounds().reduced (14).removeFromTop (70);
        g.setColour (juce::Colour (0xff1e2330));
        g.fillRoundedRectangle (headerArea.toFloat(), 8.0f);
        g.setColour (juce::Colour (0xff343d52));
        g.drawRoundedRectangle (headerArea.toFloat(), 8.0f, 1.2f);

        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (21.0f, juce::Font::bold));
        g.drawText ("Accurate Salamander Grand Piano V6.2 — 12-Bar E Minor Blues",
                    headerArea.getX() + 16, headerArea.getY() + 8, 630, 28,
                    juce::Justification::centredLeft);

        const double activeBeat = displayBeat.load();
        const int currentBarIdx = juce::jlimit (0, 11, static_cast<int> (activeBeat / 4.0));
        const int currentBeatInBar = juce::jlimit (0, 3, static_cast<int> (std::fmod (activeBeat, 4.0)));

        // Status & sample progress
        if (sampleLoader) {
            g.setFont (juce::FontOptions (13.5f));
            g.setColour (juce::Colour (0xffa6b4d0));
            g.drawText (sampleLoader->getStatusText(),
                        headerArea.getX() + 16, headerArea.getY() + 38, 640, 22,
                        juce::Justification::centredLeft);

            // Progress bar
            const float prog = sampleLoader->getProgress();
            if (prog < 1.0f) {
                juce::Rectangle<float> progBar (static_cast<float> (headerArea.getRight() - 230),
                                                static_cast<float> (headerArea.getY() + 42),
                                                210.0f, 12.0f);
                g.setColour (juce::Colour (0xff2a3040));
                g.fillRoundedRectangle (progBar, 5.0f);
                g.setColour (juce::Colour (0xff38bdf8));
                g.fillRoundedRectangle (progBar.withWidth (progBar.getWidth() * prog), 5.0f);
            }
        }

        // Live Bar / Beat / BPM Badge
        juce::String badgeText = "BAR " + juce::String (currentBarIdx + 1)
                                 + "  |  BEAT " + juce::String (currentBeatInBar + 1)
                                 + "  |  " + juce::String (bpm.load(), 0) + " BPM";
        g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        g.setColour (juce::Colour (0xff38bdf8));
        g.drawText (badgeText,
                    headerArea.getRight() - 280, headerArea.getY() + 10, 265, 26,
                    juce::Justification::centredRight);

        // Volume label & tip
        g.setFont (juce::FontOptions (13.0f));
        g.setColour (juce::Colour (0xff94a3b8));
        g.drawText ("Volume:", volumeSlider.getX() - 62, volumeSlider.getY(), 58, 28,
                    juce::Justification::centredRight);
        g.drawText ("Click any bar card below to jump directly to that chord",
                    20, volumeSlider.getY(), 400, 28,
                    juce::Justification::centredLeft);

        // 2. 12-Bar Chord Progression Grid
        const juce::ScopedLock sl (sequencerLock);
        const auto& bars = sequencer.getBars();

        for (size_t i = 0; i < bars.size() && i < barCardBounds.size(); ++i) {
            const auto& bar = bars[i];
            const auto card = barCardBounds[i].toFloat();
            const bool isCurrentBar = (static_cast<int> (i) == currentBarIdx);

            // Card background
            if (isCurrentBar) {
                juce::ColourGradient activeGrad (
                    juce::Colour (0xff1e3a5f), card.getX(), card.getY(),
                    juce::Colour (0xff17253a), card.getX(), card.getBottom(), false);
                g.setGradientFill (activeGrad);
                g.fillRoundedRectangle (card, 10.0f);
                g.setColour (juce::Colour (0xff38bdf8));
                g.drawRoundedRectangle (card, 10.0f, 2.5f);
            } else {
                g.setColour (juce::Colour (0xff1b202e));
                g.fillRoundedRectangle (card, 10.0f);
                g.setColour (juce::Colour (0xff2d3548));
                g.drawRoundedRectangle (card, 10.0f, 1.2f);
            }

            // Bar Number & Roman Numeral header
            g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
            g.setColour (isCurrentBar ? juce::Colour (0xff7dd3fc) : juce::Colour (0xff64748b));
            g.drawText ("BAR " + juce::String (bar.barNumber),
                        static_cast<int> (card.getX() + 12), static_cast<int> (card.getY() + 8),
                        80, 18, juce::Justification::centredLeft);

            g.drawText (bar.romanNumeral,
                        static_cast<int> (card.getRight() - 62), static_cast<int> (card.getY() + 8),
                        50, 18, juce::Justification::centredRight);

            // Large Chord Symbol
            g.setFont (juce::FontOptions (28.0f, juce::Font::bold));
            g.setColour (isCurrentBar ? juce::Colours::white : juce::Colour (0xffe2e8f0));
            g.drawText (bar.chordSymbol,
                        static_cast<int> (card.getX() + 12), static_cast<int> (card.getY() + 28),
                        static_cast<int> (card.getWidth() - 24), 34,
                        juce::Justification::centredLeft);

            // Voicing Notes text
            g.setFont (juce::FontOptions (12.0f));
            g.setColour (isCurrentBar ? juce::Colour (0xffbae6fd) : juce::Colour (0xff94a3b8));
            g.drawText (bar.chordNotes,
                        static_cast<int> (card.getX() + 12), static_cast<int> (card.getY() + 64),
                        static_cast<int> (card.getWidth() - 24), 18,
                        juce::Justification::centredLeft);

            // 4 Beat Dots at bottom of card
            const float dotRadius = 4.5f;
            const float dotSpacing = 18.0f;
            const float startDotX = card.getRight() - 16.0f - (3 * dotSpacing);
            const float dotY = card.getBottom() - 14.0f;

            for (int beat = 0; beat < 4; ++beat) {
                const float cx = startDotX + beat * dotSpacing;
                if (isCurrentBar && beat == currentBeatInBar) {
                    g.setColour (juce::Colour (0xfffacc15)); // Gold active beat LED
                    g.fillEllipse (cx - dotRadius - 1.5f, dotY - dotRadius - 1.5f,
                                   (dotRadius + 1.5f) * 2.0f, (dotRadius + 1.5f) * 2.0f);
                } else if (isCurrentBar && beat < currentBeatInBar) {
                    g.setColour (juce::Colour (0xff38bdf8));
                    g.fillEllipse (cx - dotRadius, dotY - dotRadius, dotRadius * 2.0f, dotRadius * 2.0f);
                } else {
                    g.setColour (juce::Colour (0xff334155));
                    g.fillEllipse (cx - dotRadius, dotY - dotRadius, dotRadius * 2.0f, dotRadius * 2.0f);
                }
            }
        }
    }

private:
    sfz::SfzSynthesiser synth;
    std::unique_ptr<sfz::AsyncSfzLoaderThread> sampleLoader;
    std::vector<juce::File> availableSfzPresets;
    EMinorBluesSequencer sequencer;
    juce::CriticalSection sequencerLock;

    juce::MidiKeyboardState keyboardState;
    juce::MidiKeyboardComponent keyboardComponent;

    juce::TextButton playPauseButton;
    juce::TextButton restartButton;
    juce::ComboBox turnaroundCombo;
    juce::ComboBox arrangementCombo;
    juce::ComboBox sfzPresetCombo;
    juce::Slider tempoSlider;
    juce::TextButton resetTempoButton;
    juce::Slider volumeSlider;

    juce::Rectangle<int> barGridArea;
    std::vector<juce::Rectangle<int>> barCardBounds;

    std::atomic<double> currentSampleRate { 44100.0 };
    std::atomic<double> bpm { 100.0 };
    std::atomic<float> masterGain { 0.95f };
    std::atomic<bool> isPlaying { true };
    std::atomic<bool> allNotesOffPending { false };
    std::atomic<double> requestJumpToBeat { -1.0 };
    std::atomic<double> displayBeat { 0.0 };

    double currentLoopBeat = 0.0;
    bool audioStarted = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

//==============================================================================
// Application Entry Point
//==============================================================================
class PianoDemoApplication : public juce::JUCEApplication {
public:
    PianoDemoApplication() {}
    const juce::String getApplicationName() override { return "PianoDemo"; }
    const juce::String getApplicationVersion() override { return "2.0.0"; }

    void initialise (const juce::String&) override {
        mainWindow.reset (new MainWindow (getApplicationName()));
    }

    void shutdown() override {
        mainWindow.reset();
    }

    class MainWindow : public juce::DocumentWindow {
    public:
        MainWindow (juce::String name)
            : DocumentWindow (name, juce::Colour (0xff161922), DocumentWindow::allButtons) {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent(), true);
            setResizable (true, true);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }

        void closeButtonPressed() override {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (PianoDemoApplication)
