#pragma once

#include <cstdint>
#include <fstream>
#include <iostream>
#include <cstring>
#include <stdexcept>
#include <vector>

#include "AudioSignal.h"
#include "AudioComponents.h"

using namespace std;

#pragma pack(push, 1)
struct WAVHeader {
    char     riff[4];
    uint32_t chunkSize;
    char     wave[4];
    char     fmt[4];
    uint32_t subchunk1Size;
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
    char     data[4];
    uint32_t subchunk2Size;
};
#pragma pack(pop)

class AudioGenerator {
public:
    static AudioSignal generateSine(double freq, double duration, double amplitude = 0.8, int sampleRate = 44100) {
        int numSamples = static_cast<int>(duration * sampleRate);
        vector<double> samples(numSamples);
        for (int i = 0; i < numSamples; i++)
            samples[i] = amplitude * sin(2.0 * M_PI * freq * i / sampleRate);
        AudioSignal sig(samples, sampleRate);
        sig.setName("Sine " + to_string((int)freq) + "Hz");
        return sig;
    }

    static AudioSignal generateSquare(double freq, double duration, double amplitude = 0.6, int sampleRate = 44100) {
        int numSamples = static_cast<int>(duration * sampleRate);
        vector<double> samples(numSamples);
        for (int i = 0; i < numSamples; i++) {
            double t = static_cast<double>(i) / sampleRate;
            samples[i] = amplitude * (fmod(t * freq, 1.0) < 0.5 ? 1.0 : -1.0);
        }
        AudioSignal sig(samples, sampleRate);
        sig.setName("Square " + to_string((int)freq) + "Hz");
        return sig;
    }

    static AudioSignal generateTriangle(double freq, double duration, double amplitude = 0.8, int sampleRate = 44100) {
        int numSamples = static_cast<int>(duration * sampleRate);
        vector<double> samples(numSamples);
        for (int i = 0; i < numSamples; i++) {
            double t = fmod(static_cast<double>(i) / sampleRate * freq, 1.0);
            samples[i] = amplitude * (t < 0.5 ? 4.0 * t - 1.0 : 3.0 - 4.0 * t);
        }
        AudioSignal sig(samples, sampleRate);
        sig.setName("Triangle " + to_string((int)freq) + "Hz");
        return sig;
    }

    static AudioSignal generateWhiteNoise(double duration, double amplitude = 0.3, int sampleRate = 44100) {
        int numSamples = static_cast<int>(duration * sampleRate);
        vector<double> samples(numSamples);
        srand(42);
        for (int i = 0; i < numSamples; i++)
            samples[i] = amplitude * (2.0 * (rand() / (double)RAND_MAX) - 1.0);
        AudioSignal sig(samples, sampleRate);
        sig.setName("White Noise");
        return sig;
    }

    static AudioSignal generateDTMF(double f1, double f2, double duration, int sampleRate = 44100) {
        int numSamples = static_cast<int>(duration * sampleRate);
        vector<double> samples(numSamples);
        for (int i = 0; i < numSamples; i++) {
            double t = static_cast<double>(i) / sampleRate;
            samples[i] = 0.4 * sin(2.0 * M_PI * f1 * t) + 0.4 * sin(2.0 * M_PI * f2 * t);
        }
        AudioSignal sig(samples, sampleRate);
        sig.setName("DTMF " + to_string((int)f1) + "+" + to_string((int)f2) + "Hz");
        return sig;
    }
};

class AudioFile {
private:
    string path;
    string format;

public:
    AudioFile(const string& p = "", const string& fmt = "wav") : path(p), format(fmt) {}

    bool save(const AudioSignal& signal, const string& filepath = "") {
        string outPath = filepath.empty() ? path : filepath;
        if (outPath.empty()) return false;

        ofstream file(outPath, ios::binary);
        if (!file.is_open()) return false;

        auto& samples = signal.getSamples();
        int sr = signal.getSampleRate();
        int ch = signal.getNumChannels();
        int bitsPerSample = 16;
        uint32_t dataSize = samples.size() * (bitsPerSample / 8);

        WAVHeader header;
        memcpy(header.riff, "RIFF", 4);
        header.chunkSize = 36 + dataSize;
        memcpy(header.wave, "WAVE", 4);
        memcpy(header.fmt, "fmt ", 4);
        header.subchunk1Size = 16;
        header.audioFormat = 1;
        header.numChannels = ch;
        header.sampleRate = sr;
        header.bitsPerSample = bitsPerSample;
        header.byteRate = sr * ch * bitsPerSample / 8;
        header.blockAlign = ch * bitsPerSample / 8;
        memcpy(header.data, "data", 4);
        header.subchunk2Size = dataSize;

        file.write(reinterpret_cast<char*>(&header), sizeof(header));
        for (double s : samples) {
            int16_t pcm = static_cast<int16_t>(clamp(s, -1.0, 1.0) * 32767.0);
            file.write(reinterpret_cast<char*>(&pcm), sizeof(pcm));
        }
        return true;
    }

    AudioSignal createDemo(double freq, double duration, int sampleRate = 44100) {
        return AudioGenerator::generateSine(freq, duration, 0.8, sampleRate);
    }

    const string& getPath() const { return path; }
    const string& getFormat() const { return format; }
};