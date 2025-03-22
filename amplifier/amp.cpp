#include <iostream>
#include <portaudio.h>
#include <atomic>
#include <thread>
#include <cmath>
#include <termios.h>
#include <unistd.h>

#define SAMPLE_RATE 44100  
#define FRAMES_PER_BUFFER 256  

std::atomic<float> gain(2.0f);    
std::atomic<float> volume(1.0f);  
std::atomic<float> noiseThreshold(0.005f);  // Bisa diatur saat runtime

// Fungsi membaca keyboard tanpa ENTER (Linux)
char getKeyPress() {
    struct termios oldt, newt;
    char ch;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ch;
}

// **Noise Gate dengan Attack-Release**
float noiseGate(float sample, float threshold, float release = 0.99f) {
    static float gateLevel = 0.0f;
    if (fabs(sample) > threshold) {
        gateLevel = 1.0f;  // Biarkan suara masuk
    } else {
        gateLevel *= release;  // Perlahan-lahan turunkan noise
    }
    return sample * gateLevel;
}

// **Low-Pass Filter** (mengurangi noise frekuensi tinggi)
float lowPassFilter(float input, float previous, float alpha = 0.1f) {
    return alpha * input + (1 - alpha) * previous;
}

// **High-Pass Filter** (mengurangi hum frekuensi rendah)
float highPassFilter(float input, float previous, float alpha = 0.9f) {
    return alpha * (previous + input - previous);
}

// **Callback Audio PortAudio**
static int audioCallback(const void* inputBuffer, void* outputBuffer,
                         unsigned long framesPerBuffer,
                         const PaStreamCallbackTimeInfo* timeInfo,
                         PaStreamCallbackFlags statusFlags,
                         void* userData)
{
    float* in  = (float*)inputBuffer;
    float* out = (float*)outputBuffer;
    static float prevSample = 0.0f;

    if (inputBuffer == nullptr) return paContinue;

    for (unsigned int i = 0; i < framesPerBuffer; i++) {
        float sample = in[i];

        // **Noise Gate**
        sample = noiseGate(sample, noiseThreshold.load());

        // **Low-Pass & High-Pass Filtering**
        sample = lowPassFilter(sample, prevSample);
        sample = highPassFilter(sample, prevSample);
        prevSample = sample;

        // **Amplifikasi**
        out[i] = sample * gain.load() * volume.load();
        if (out[i] > 1.0f) out[i] = 1.0f;  
        if (out[i] < -1.0f) out[i] = -1.0f;
    }

    return paContinue;
}

// **Thread untuk mengontrol gain, volume, dan noise threshold**
void controlThread() {
    char ch;
    while (true) {
        ch = getKeyPress();
        if (ch == '+') gain.store(gain.load() + 0.1f);
        else if (ch == '-') gain.store(gain.load() - 0.1f);
        else if (ch == '[') volume.store(volume.load() - 0.1f);
        else if (ch == ']') volume.store(volume.load() + 0.1f);
        else if (ch == '{') noiseThreshold.store(noiseThreshold.load() - 0.001f);
        else if (ch == '}') noiseThreshold.store(noiseThreshold.load() + 0.001f);
        else if (ch == 'q') break;
        std::cout << "\rGain: " << gain.load() 
                  << " | Volume: " << volume.load() 
                  << " | Noise Gate: " << noiseThreshold.load() << "   " << std::flush;
    }
}

int main() {
    Pa_Initialize();

    PaStream* stream;
    Pa_OpenDefaultStream(&stream,
                         1, 1,  
                         paFloat32, SAMPLE_RATE, FRAMES_PER_BUFFER,
                         audioCallback, nullptr);

    Pa_StartStream(stream);
    std::cout << "Amplifier berjalan... Tekan '+/-' untuk gain, '[ ]' untuk volume, '{ }' untuk noise gate, 'q' untuk keluar.\n";

    std::thread control(controlThread);
    control.join();

    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();

    std::cout << "\nAmplifier dihentikan." << std::endl;
    return 0;
}

