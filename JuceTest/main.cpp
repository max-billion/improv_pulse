#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <cmath>

class MainComponent : public juce::AudioAppComponent {
public:
    MainComponent() {
        setSize (400, 300);
        setAudioChannels (0, 2); // Stereo output
    }

    ~MainComponent() override {
        shutdownAudio();
    }

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override {
        juce::ignoreUnused (samplesPerBlockExpected);
        currentSampleRate = sampleRate;
        std::cout << "Audio Device Active - Sample Rate: " << currentSampleRate
                  << " Hz, Block Size: " << samplesPerBlockExpected << std::endl;
        phase = 0.0;
        const double targetFrequency = 440.0; // A440 tone
        phaseDelta = (targetFrequency * 2.0 * juce::MathConstants<double>::pi) / currentSampleRate;
    }

    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override {
        auto* device = deviceManager.getCurrentAudioDevice();
        if (device == nullptr) {
            bufferToFill.clearActiveBufferRegion();
            return;
        }
        auto* leftBuffer = bufferToFill.buffer->getWritePointer (0, bufferToFill.startSample);
        auto* rightBuffer = bufferToFill.buffer->getWritePointer (1, bufferToFill.startSample);
        for (int sample = 0; sample < bufferToFill.numSamples; ++sample) {
            const float sampleValue = std::sin (phase) * 0.1f; // Low volume safety
            leftBuffer[sample] = sampleValue;
            rightBuffer[sample] = sampleValue;
            phase += phaseDelta;
            if (phase >= juce::MathConstants<double>::twoPi)
                phase -= juce::MathConstants<double>::twoPi;
        }
    }

    void releaseResources() override {}

    void paint (juce::Graphics& g) override {
        g.fillAll (juce::Colours::darkgrey);
        g.setFont (18.0f);
        g.setColour (juce::Colours::white);
        juce::String msg = "JUCE 8 Installation Successful!\n\n";
        msg += "Audio Test Tone Active: 440Hz Sine Wave\n";
        msg += "Sample Rate: " + juce::String(currentSampleRate) + " Hz";
        g.drawFittedText (msg, getLocalBounds(), juce::Justification::centred, 4);
    }

private:
    double currentSampleRate = 0.0;
    double phase = 0.0;
    double phaseDelta = 0.0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

class JuceTestApplication : public juce::JUCEApplication {
public:
    JuceTestApplication() {}
    const juce::String getApplicationName() override { return "JuceTest"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }

    void initialise (const juce::String&) override {
        mainWindow.reset (new MainWindow (getApplicationName()));
    }

    void shutdown() override {
        mainWindow.reset();
    }

    class MainWindow : public juce::DocumentWindow {
    public:
        MainWindow (juce::String name)
            : DocumentWindow (name, juce::Colours::lightgrey, DocumentWindow::allButtons) {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent(), true);
            setResizable (false, false);
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

START_JUCE_APPLICATION (JuceTestApplication)
