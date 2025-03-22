#include <JuceHeader.h>
#include <portaudio.h>
#include <fftw3.h>
#include <cmath>
#include <vector>
#include <map>

#define SAMPLE_RATE 44100
#define FRAMES_PER_BUFFER 1024
#define FFT_SIZE 2048

class TunerComponent : public juce::Component, public juce::Timer
{
public:
    TunerComponent()
    {
        setSize(400, 300);
        startTimerHz(30); // Update GUI 30 FPS

        // Setup Label
        addAndMakeVisible(frequencyLabel);
        addAndMakeVisible(noteLabel);

        frequencyLabel.setFont(juce::Font(24.0f, juce::Font::bold));
        noteLabel.setFont(juce::Font(40.0f, juce::Font::bold));
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(20.0f));

        // Draw Labels
        frequencyLabel.setText("Frekuensi: " + juce::String(currentFrequency) + " Hz", juce::dontSendNotification);
        noteLabel.setText("Nada: " + currentNote, juce::dontSendNotification);

        // Draw Bar Indicator
        float barWidth = getWidth() * (1.0f - std::abs(currentOffset / 50.0f));
        g.setColour(juce::Colours::green);
        g.fillRect(50, 200, barWidth, 20);
    }

    void timerCallback() override
    {
        repaint();
    }

    void updateFrequency(float frequency)
    {
        currentFrequency = frequency;
        auto detected = getClosestNoteWithOctave(frequency);
        currentNote = detected.first + juce::String(detected.second);
        currentOffset = (frequency - notes[detected.first] * pow(2, detected.second)) * 100.0f;
    }

private:
    juce::Label frequencyLabel, noteLabel;
    float currentFrequency = 0.0f;
    juce::String currentNote = "?";
    float currentOffset = 0.0f;

    std::map<std::string, float> notes = {
        {"C", 16.35}, {"C#", 17.32}, {"D", 18.35}, {"D#", 19.45}, {"E", 20.60},
        {"F", 21.83}, {"F#", 23.12}, {"G", 24.50}, {"G#", 25.96}, {"A", 27.50},
        {"A#", 29.14}, {"B", 30.87}
    };

    std::pair<std::string, int> getClosestNoteWithOctave(float freq)
    {
        std::string closestNote = "?";
        float minDiff = 1e9;
        int octave = 0;

        for (int o = 0; o <= 8; o++) {
            for (const auto& pair : notes) {
                float noteFreq = pair.second * pow(2, o);
                float diff = std::abs(freq - noteFreq);
                if (diff < minDiff) {
                    minDiff = diff;
                    closestNote = pair.first;
                    octave = o;
                }
            }
        }
        return {closestNote, octave};
    }
};

// **Main JUCE Window**
class TunerApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "Tuner Gitar Chromatic"; }
    void initialise(const juce::String&) override
    {
        mainWindow = std::make_unique<MainWindow>("Tuner Gitar", new TunerComponent(), *this);
        startAudio();
    }
    void shutdown() override
    {
        stopAudio();
        mainWindow = nullptr;
    }

private:
    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow(const juce::String& name, juce::Component* c, JUCEApplication& app)
            : DocumentWindow(name, juce::Colours::darkgrey, DocumentWindow::allButtons), app(app)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(c, true);
            centreWithSize(getWidth(), getHeight());
            setVisible(true);
        }
        void closeButtonPressed() override { app.systemRequestedQuit(); }
    private:
        JUCEApplication& app;
    };

    std::unique_ptr<MainWindow> mainWindow;
    PaStream* stream;
    std::vector<float> audioData;
    TunerComponent* tunerComponent = nullptr;

    static int audioCallback(const void* inputBuffer, void* outputBuffer,
                             unsigned long framesPerBuffer,
                             const PaStreamCallbackTimeInfo* timeInfo,
                             PaStreamCallbackFlags statusFlags,
                             void* userData)
    {
        auto* app = static_cast<TunerApplication*>(userData);
        if (!inputBuffer) return paContinue;

        const float* in = static_cast<const float*>(inputBuffer);
        for (unsigned long i = 0; i < framesPerBuffer; i++) {
            app->audioData[i] = in[i];
        }

        fftw_complex* out;
        fftw_plan plan;
        out = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * FFT_SIZE);

        std::vector<double> audioDataDouble(app->audioData.begin(), app->audioData.end());
        plan = fftw_plan_dft_r2c_1d(FFT_SIZE, audioDataDouble.data(), out, FFTW_ESTIMATE);
        fftw_execute(plan);

        int peakIndex = 0;
        double peakMagnitude = 0.0;
        for (int i = 1; i < FFT_SIZE / 2; i++) {
            double magnitude = sqrt(out[i][0] * out[i][0] + out[i][1] * out[i][1]);
            if (magnitude > peakMagnitude) {
                peakMagnitude = magnitude;
                peakIndex = i;
            }
        }

        float peakFrequency = (float)peakIndex * SAMPLE_RATE / FFT_SIZE;
        if (app->tunerComponent) {
            app->tunerComponent->updateFrequency(peakFrequency);
        }

        fftw_destroy_plan(plan);
        fftw_free(out);

        return paContinue;
    }

    void startAudio()
    {
        Pa_Initialize();
        PaStreamParameters inputParameters;
        inputParameters.device = Pa_GetDefaultInputDevice();
        inputParameters.channelCount = 1;
        inputParameters.sampleFormat = paFloat32;
        inputParameters.suggestedLatency = Pa_GetDeviceInfo(inputParameters.device)->defaultLowInputLatency;
        inputParameters.hostApiSpecificStreamInfo = nullptr;

        audioData.resize(FFT_SIZE, 0.0f);
        Pa_OpenStream(&stream, &inputParameters, nullptr, SAMPLE_RATE, FRAMES_PER_BUFFER, paClipOff, audioCallback, this);
        Pa_StartStream(stream);
    }

    void stopAudio()
    {
        Pa_StopStream(stream);
        Pa_CloseStream(stream);
        Pa_Terminate();
    }
};

// **JUCE Entry Point**
START_JUCE_APPLICATION(TunerApplication)

