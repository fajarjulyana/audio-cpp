#include <iostream>
#include <vector>
#include <sndfile.h>
#include <portaudio.h>

#define SAMPLE_RATE 44100
#define FRAMES_PER_BUFFER 512

struct AudioData {
    std::vector<float> buffer1;
    std::vector<float> buffer2;
    std::vector<float> outputBuffer;
    size_t index = 0;
};

static int audioCallback(const void* inputBuffer, void* outputBuffer,
                         unsigned long framesPerBuffer,
                         const PaStreamCallbackTimeInfo* timeInfo,
                         PaStreamCallbackFlags statusFlags,
                         void* userData) {
    AudioData* data = (AudioData*)userData;
    float* out = (float*)outputBuffer;
    
    for (unsigned long i = 0; i < framesPerBuffer * 2; ++i) {
        if (data->index < data->buffer1.size() && data->index < data->buffer2.size()) {
            float mixedSample = (data->buffer1[data->index] * 0.5f) + (data->buffer2[data->index] * 0.5f);
            out[i] = mixedSample;
            data->outputBuffer.push_back(mixedSample);
            data->index++;
        } else {
            out[i] = 0.0f;
        }
    }
    
    return paContinue;
}

void mixAndSaveAudio(const std::string& file1, const std::string& file2, const std::string& output) {
    SF_INFO sfinfo1, sfinfo2;
    SNDFILE *infile1 = sf_open(file1.c_str(), SFM_READ, &sfinfo1);
    SNDFILE *infile2 = sf_open(file2.c_str(), SFM_READ, &sfinfo2);
    
    if (!infile1 || !infile2) {
        std::cerr << "Error opening files!" << std::endl;
        return;
    }
    
    if (sfinfo1.samplerate != sfinfo2.samplerate || sfinfo1.channels != sfinfo2.channels) {
        std::cerr << "Files must have the same sample rate and channels!" << std::endl;
        return;
    }
    
    AudioData audioData;
    int frames = std::min(sfinfo1.frames, sfinfo2.frames);
    audioData.buffer1.resize(frames * sfinfo1.channels);
    audioData.buffer2.resize(frames * sfinfo2.channels);
    
    sf_readf_float(infile1, audioData.buffer1.data(), frames);
    sf_readf_float(infile2, audioData.buffer2.data(), frames);
    
    sf_close(infile1);
    sf_close(infile2);
    
    Pa_Initialize();
    PaStream* stream;
    Pa_OpenDefaultStream(&stream, 0, sfinfo1.channels, paFloat32, SAMPLE_RATE, FRAMES_PER_BUFFER, audioCallback, &audioData);
    Pa_StartStream(stream);
    
    std::cout << "Playing mixed audio... Press Enter to stop." << std::endl;
    std::cin.get();
    
    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();
    
    // Save mixed audio to file
    SF_INFO sfinfoOut = sfinfo1;
    SNDFILE *outfile = sf_open(output.c_str(), SFM_WRITE, &sfinfoOut);
    
    if (!outfile) {
        std::cerr << "Error creating output file!" << std::endl;
        return;
    }
    
    sf_writef_float(outfile, audioData.outputBuffer.data(), audioData.outputBuffer.size() / sfinfo1.channels);
    sf_close(outfile);
    
    std::cout << "Playback finished. Output saved to " << output << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <file1> <file2> <output>" << std::endl;
        return 1;
    }
    
    mixAndSaveAudio(argv[1], argv[2], argv[3]);
    return 0;
}

