#include <iostream>
#include <vector>
#include <cmath>
#include <portaudio.h>
#include <fftw3.h>
#include <unistd.h>  // Untuk usleep()
#include <map>

#define SAMPLE_RATE 44100
#define FRAMES_PER_BUFFER 1024
#define FFT_SIZE 2048  // Ukuran FFT harus lebih besar dari FRAMES_PER_BUFFER

// **Daftar nada standar (frekuensi referensi)**
std::map<std::string, float> notes = {
    {"C", 16.35}, {"C#", 17.32}, {"D", 18.35}, {"D#", 19.45}, {"E", 20.60},
    {"F", 21.83}, {"F#", 23.12}, {"G", 24.50}, {"G#", 25.96}, {"A", 27.50},
    {"A#", 29.14}, {"B", 30.87}
};

// **Menentukan nada dan oktaf berdasarkan frekuensi**
std::pair<std::string, int> getClosestNoteWithOctave(float freq) {
    std::string closestNote = "?";
    float minDiff = 1e9;
    int octave = 0;

    for (int o = 0; o <= 8; o++) { // Oktaf dari 0 sampai 8
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

// **Callback Audio**
static int audioCallback(const void* inputBuffer, void* outputBuffer,
                         unsigned long framesPerBuffer,
                         const PaStreamCallbackTimeInfo* timeInfo,
                         PaStreamCallbackFlags statusFlags,
                         void* userData)
{
    const float* in = (const float*)inputBuffer;
    if (!in) return paContinue;

    static std::vector<float> audioData(FFT_SIZE, 0.0f);
    static int sampleIndex = 0;

    // Simpan data untuk FFT
    for (unsigned long i = 0; i < framesPerBuffer; i++) {
        audioData[sampleIndex] = in[i];
        sampleIndex = (sampleIndex + 1) % FFT_SIZE;
    }

    // Jika sudah terkumpul cukup data, lakukan FFT
    if (sampleIndex == 0) {
        fftw_complex* out;
        fftw_plan plan;
        out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * FFT_SIZE);

        // **Konversi float ke double untuk FFTW3**
        std::vector<double> audioDataDouble(audioData.begin(), audioData.end());

        plan = fftw_plan_dft_r2c_1d(FFT_SIZE, audioDataDouble.data(), out, FFTW_ESTIMATE);
        fftw_execute(plan);

        // **Cari frekuensi dominan**
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
        auto detected = getClosestNoteWithOctave(peakFrequency);
        std::string detectedNote = detected.first;
        int detectedOctave = detected.second;

        // **Update tampilan CLI tanpa menumpuk teks**
        std::cout << "\033[2J\033[H";  // Hapus layar dan pindahkan kursor ke atas
        std::cout << "🎸 Tuner Gitar Fajar Julyana\n";
        std::cout << "============================\n";
        std::cout << "Frekuensi Detected: " << peakFrequency << " Hz\n";
        std::cout << "Nada: " << detectedNote << detectedOctave << "\n";
        std::cout << "============================\n";

        fftw_destroy_plan(plan);
        fftw_free(out);
    }

    return paContinue;
}

int main() {
    Pa_Initialize();

    // **Konfigurasi input**
    PaStreamParameters inputParameters;
    inputParameters.device = Pa_GetDefaultInputDevice();
    if (inputParameters.device == paNoDevice) {
        std::cerr << "Error: Tidak ada perangkat input tersedia.\n";
        return 1;
    }

    inputParameters.channelCount = 1;
    inputParameters.sampleFormat = paFloat32;
    inputParameters.suggestedLatency = Pa_GetDeviceInfo(inputParameters.device)->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = nullptr;

    PaStream* stream;
    PaError err = Pa_OpenStream(&stream,
                                &inputParameters,
                                nullptr,
                                SAMPLE_RATE,
                                FRAMES_PER_BUFFER,
                                paClipOff,
                                audioCallback,
                                nullptr);

    if (err != paNoError) {
        std::cerr << "Error membuka stream: " << Pa_GetErrorText(err) << "\n";
        Pa_Terminate();
        return 1;
    }

    Pa_StartStream(stream);

    std::cout << "🎸 Jalankan tuner... (Tekan CTRL+C untuk berhenti)\n";
    while (true) {
        usleep(100000);  // Tunggu 100ms sebelum update tampilan
    }

    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();

    return 0;
}

