#include <iostream>
#include <vector>
#include <cmath>
#include <portaudio.h>
#include <sndfile.h>
#include <ncurses.h>

#define SAMPLE_RATE 44100
#define FRAMES_PER_BUFFER 256
#define MAX_DELAY 44100  // 1 detik delay (44100 sampel)
#define MAX_GAIN 2.0f
#define FLANGER_DEPTH 5  // Kedalaman flanger dalam ms
#define CHORUS_DEPTH 10  // Kedalaman chorus dalam ms
#define LFO_RATE 0.5f    // Frekuensi LFO dalam Hz

struct AudioData {
    std::vector<float> delayBuffer;
    std::vector<float> flangerBuffer;
    std::vector<float> chorusBuffer;
    size_t delayIndex = 0;
    size_t flangerIndex = 0;
    size_t chorusIndex = 0;
    float gain = 1.0f;
    float delayMix = 0.3f;
    float reverbMix = 0.2f;
    float flangerMix = 0.2f;
    float chorusMix = 0.2f;
    bool recording = true;
    float lfoPhase = 0.0f;
    SNDFILE *outfile;
    SF_INFO sfinfo;
};

// Efek distorsi (clipping)
float applyDistortion(float sample, float drive) {
    sample *= drive;
    return std::max(-1.0f, std::min(1.0f, sample));
}

// Efek delay (echo)
float applyDelay(AudioData &data, float sample) {
    float delayedSample = data.delayBuffer[data.delayIndex];
    data.delayBuffer[data.delayIndex] = sample;
    data.delayIndex = (data.delayIndex + 1) % MAX_DELAY;
    return sample + delayedSample * data.delayMix;
}

// Efek reverb sederhana
float applyReverb(float sample, float reverbMix) {
    return sample * (1.0f - reverbMix) + (sample * reverbMix * 0.5f);
}

// Efek flanger (delay dengan LFO)
float applyFlanger(AudioData &data, float sample) {
    int delaySamples = (FLANGER_DEPTH * SAMPLE_RATE) / 1000;
    int lfoOffset = static_cast<int>((sin(data.lfoPhase) + 1) * 0.5 * delaySamples);
    int index = (data.flangerIndex + MAX_DELAY - lfoOffset) % MAX_DELAY;

    float delayedSample = data.flangerBuffer[index];
    data.flangerBuffer[data.flangerIndex] = sample;
    data.flangerIndex = (data.flangerIndex + 1) % MAX_DELAY;
    data.lfoPhase += (2 * M_PI * LFO_RATE) / SAMPLE_RATE;

    return sample + delayedSample * data.flangerMix;
}

// Efek chorus (pitch shifting delay dengan LFO)
float applyChorus(AudioData &data, float sample) {
    int delaySamples = (CHORUS_DEPTH * SAMPLE_RATE) / 1000;
    int lfoOffset = static_cast<int>((sin(data.lfoPhase) + 1) * 0.5 * delaySamples);
    int index = (data.chorusIndex + MAX_DELAY - lfoOffset) % MAX_DELAY;

    float delayedSample = data.chorusBuffer[index];
    data.chorusBuffer[data.chorusIndex] = sample;
    data.chorusIndex = (data.chorusIndex + 1) % MAX_DELAY;
    data.lfoPhase += (2 * M_PI * (LFO_RATE / 2)) / SAMPLE_RATE;  // Lebih lambat dari flanger

    return sample + delayedSample * data.chorusMix;
}

// Callback audio
static int audioCallback(const void *inputBuffer, void *outputBuffer,
                         unsigned long framesPerBuffer,
                         const PaStreamCallbackTimeInfo *timeInfo,
                         PaStreamCallbackFlags statusFlags,
                         void *userData) {
    AudioData *data = (AudioData *)userData;
    const float *in = (const float *)inputBuffer;
    float *out = (float *)outputBuffer;

    for (unsigned long i = 0; i < framesPerBuffer; i++) {
        float sample = in[i] * data->gain;

        // Terapkan efek
        sample = applyDistortion(sample, 2.0f);
        sample = applyDelay(*data, sample);
        sample = applyFlanger(*data, sample);
        sample = applyChorus(*data, sample);
        sample = applyReverb(sample, data->reverbMix);

        out[i] = sample;

        // Simpan ke file jika recording aktif
        if (data->recording) {
            sf_write_float(data->outfile, &sample, 1);
        }
    }

    return paContinue;
}

void showUI(float gain, float delayMix, float reverbMix, float flangerMix, float chorusMix) {
    clear();
    printw("Guitar Amp Live - CLI UI\n");
    printw("========================\n");
    printw("Gain    : %.1f (W/S to adjust)\n", gain);
    printw("Delay   : %.1f (A/D to adjust)\n", delayMix);
    printw("Reverb  : %.1f (Q/E to adjust)\n", reverbMix);
    printw("Flanger : %.1f (R/F to adjust)\n", flangerMix);
    printw("Chorus  : %.1f (T/G to adjust)\n", chorusMix);
    printw("ESC to stop recording.\n");
    refresh();
}

int main() {
    Pa_Initialize();
    initscr();
    cbreak();
    noecho();
    timeout(100);

    AudioData data;
    data.delayBuffer.resize(MAX_DELAY, 0.0f);
    data.flangerBuffer.resize(MAX_DELAY, 0.0f);
    data.chorusBuffer.resize(MAX_DELAY, 0.0f);

    data.sfinfo.samplerate = SAMPLE_RATE;
    data.sfinfo.channels = 1;
    data.sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
    data.outfile = sf_open("guitar_amp_output.wav", SFM_WRITE, &data.sfinfo);

    PaStream *stream;
    Pa_OpenDefaultStream(&stream, 1, 1, paFloat32, SAMPLE_RATE, FRAMES_PER_BUFFER, audioCallback, &data);
    Pa_StartStream(stream);

    int ch;
    while ((ch = getch()) != 27) {  // ESC untuk keluar
        if (ch == 'w') data.gain = std::min(data.gain + 0.1f, MAX_GAIN);
        if (ch == 's') data.gain = std::max(data.gain - 0.1f, 0.1f);
        if (ch == 'a') data.delayMix = std::max(data.delayMix - 0.1f, 0.0f);
        if (ch == 'd') data.delayMix = std::min(data.delayMix + 0.1f, 1.0f);
        if (ch == 'q') data.reverbMix = std::max(data.reverbMix - 0.1f, 0.0f);
        if (ch == 'e') data.reverbMix = std::min(data.reverbMix + 0.1f, 1.0f);
        if (ch == 'r') data.flangerMix = std::min(data.flangerMix + 0.1f, 1.0f);
        if (ch == 'f') data.flangerMix = std::max(data.flangerMix - 0.1f, 0.0f);
        if (ch == 't') data.chorusMix = std::min(data.chorusMix + 0.1f, 1.0f);
        if (ch == 'g') data.chorusMix = std::max(data.chorusMix - 0.1f, 0.0f);
        showUI(data.gain, data.delayMix, data.reverbMix, data.flangerMix, data.chorusMix);
    }

    data.recording = false;
    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    sf_close(data.outfile);

    endwin();
    Pa_Terminate();
    std::cout << "Recording saved as 'guitar_amp_output.wav'\n";
    return 0;
}

