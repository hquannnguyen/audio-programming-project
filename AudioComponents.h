#pragma once

#define _USE_MATH_DEFINES
#include <cmath>
#include <algorithm>
#include <string>
#include <vector>
#include <memory>
#include <iostream>

#include "AudioSignal.h"

using namespace std;

// =========== AudioProcessor ===========
class AudioProcessor {
private:
    double gain;
    double pan;

public:
    AudioProcessor(double g = 1.0, double p = 0.0) : gain(g), pan(p) {}

    void amplify(AudioSignal& signal, double factor) {
        auto samples = signal.getSamples();
        for (auto& s : samples) s *= factor;
        signal.setSamples(samples);
    }

    void normalize(AudioSignal& signal) {
        double peak = signal.getPeakAmplitude();
        if (peak == 0.0) return;
        auto samples = signal.getSamples();
        for (auto& s : samples) s /= peak;
        signal.setSamples(samples);
    }

    void invertPhase(AudioSignal& signal) {
        auto samples = signal.getSamples();
        for (auto& s : samples) s = -s;
        signal.setSamples(samples);
    }

    void fadeIn(AudioSignal& signal, int numFadeSamples) {
        auto samples = signal.getSamples();
        int N = min((size_t)numFadeSamples, samples.size());
        for (int i = 0; i < N; i++)
            samples[i] *= static_cast<double>(i) / N;
        signal.setSamples(samples);
    }

    void fadeOut(AudioSignal& signal, int numFadeSamples) {
        auto samples = signal.getSamples();
        int total = samples.size();
        int start = max(0, total - numFadeSamples);
        for (int i = start; i < total; i++)
            samples[i] *= static_cast<double>(total - i) / numFadeSamples;
        signal.setSamples(samples);
    }

    AudioSignal mix(const AudioSignal& a, const AudioSignal& b, double ratioA = 0.5) {
        auto sa = a.getSamples();
        auto sb = b.getSamples();
        size_t len = min(sa.size(), sb.size());
        vector<double> result(len);
        double ratioB = 1.0 - ratioA;
        for (size_t i = 0; i < len; i++)
            result[i] = clamp(sa[i] * ratioA + sb[i] * ratioB, -1.0, 1.0);
        return AudioSignal(result, a.getSampleRate(), a.getNumChannels(), a.getBitDepth());
    }

    double getGain() const { return gain; }
    double getPan() const { return pan; }
    void setGain(double g) { gain = g; }
    void setPan(double p) { pan = clamp(p, -1.0, 1.0); }
};

// =========== AudioFilter (abstract) ===========
class AudioFilter {
protected:
    double cutoffFreq;
    int order;
    double sampleRate;

public:
    AudioFilter(double cf, int ord, double sr)
        : cutoffFreq(cf), order(ord), sampleRate(sr) {}
    virtual ~AudioFilter() = default;
    virtual void apply(AudioSignal& signal) = 0;
    virtual string getFilterType() const = 0;

    void setCutoff(double f) { cutoffFreq = f; }
    double getCutoff() const { return cutoffFreq; }
    int getOrder() const { return order; }

    double computeAlpha() const {
        double dt = 1.0 / sampleRate;
        double RC = 1.0 / (2.0 * M_PI * cutoffFreq);
        return dt / (RC + dt);
    }
};

class LowPassFilter : public AudioFilter {
    double resonance;
public:
    LowPassFilter(double cutoff, double sr, double res = 0.707)
        : AudioFilter(cutoff, 1, sr), resonance(res) {}

    void apply(AudioSignal& signal) override {
        auto samples = signal.getSamples();
        if (samples.empty()) return;
        double alpha = computeAlpha();
        double prev = samples[0];
        for (size_t i = 1; i < samples.size(); i++) {
            samples[i] = prev + alpha * (samples[i] - prev);
            prev = samples[i];
        }
        signal.setSamples(samples);
    }

    string getFilterType() const override { return "Low-Pass Filter"; }
    void setResonance(double r) { resonance = r; }
    double getResonance() const { return resonance; }
};

class HighPassFilter : public AudioFilter {
    double rolloff;
public:
    HighPassFilter(double cutoff, double sr, double ro = 6.0)
        : AudioFilter(cutoff, 1, sr), rolloff(ro) {}

    void apply(AudioSignal& signal) override {
        auto samples = signal.getSamples();
        if (samples.empty()) return;
        double alpha = 1.0 - computeAlpha();
        double prevIn = samples[0];
        double prevOut = samples[0];
        for (size_t i = 1; i < samples.size(); i++) {
            double out = alpha * (prevOut + samples[i] - prevIn);
            prevIn = samples[i];
            prevOut = out;
            samples[i] = out;
        }
        signal.setSamples(samples);
    }

    string getFilterType() const override { return "High-Pass Filter"; }
    void setRolloff(double r) { rolloff = r; }
};

class BandPassFilter : public AudioFilter {
    double lowCutoff, highCutoff;
public:
    BandPassFilter(double low, double high, double sr)
        : AudioFilter((low + high)/2, 2, sr), lowCutoff(low), highCutoff(high) {}

    void apply(AudioSignal& signal) override {
        LowPassFilter lpf(highCutoff, sampleRate);
        HighPassFilter hpf(lowCutoff, sampleRate);
        lpf.apply(signal);
        hpf.apply(signal);
    }

    string getFilterType() const override { return "Band-Pass Filter"; }
};

// =========== AudioEffect ===========
class AudioEffect {
private:
    double wetDry;
    double delayMs;

public:
    AudioEffect(double wd = 0.5, double dms = 100.0) : wetDry(wd), delayMs(dms) {}

    void echo(AudioSignal& signal, double delaySeconds, double feedback = 0.4) {
        auto samples = signal.getSamples();
        int sr = signal.getSampleRate();
        int delaySamples = static_cast<int>(delaySeconds * sr);
        if (delaySamples <= 0 || delaySamples >= (int)samples.size()) return;
        vector<double> result = samples;
        for (size_t i = delaySamples; i < samples.size(); i++)
            result[i] = clamp(samples[i] + samples[i - delaySamples] * feedback, -1.0, 1.0);
        signal.setSamples(result);
    }

    void reverb(AudioSignal& signal, double roomSize = 0.5) {
        auto samples = signal.getSamples();
        int sr = signal.getSampleRate();
        vector<int> taps = {
            (int)(0.030 * sr * roomSize),
            (int)(0.060 * sr * roomSize),
            (int)(0.120 * sr * roomSize)
        };
        vector<double> gains = { 0.5, 0.3, 0.15 };
        vector<double> result = samples;
        for (int t = 0; t < (int)taps.size(); t++) {
            int tap = taps[t];
            if (tap <= 0) continue;
            for (size_t i = tap; i < samples.size(); i++)
                result[i] = clamp(result[i] + samples[i - tap] * gains[t] * wetDry, -1.0, 1.0);
        }
        signal.setSamples(result);
    }

    void chorus(AudioSignal& signal, double rate = 1.5, double depth = 0.003) {
        auto samples = signal.getSamples();
        int sr = signal.getSampleRate();
        vector<double> result(samples.size());
        for (size_t i = 0; i < samples.size(); i++) {
            double lfo = depth * sr * sin(2.0 * M_PI * rate * i / sr);
            int offset = static_cast<int>(lfo);
            int idx = clamp((int)i + offset, 0, (int)samples.size() - 1);
            result[i] = clamp(samples[i] * (1.0 - wetDry) + samples[idx] * wetDry, -1.0, 1.0);
        }
        signal.setSamples(result);
    }

    void distort(AudioSignal& signal, double threshold = 0.5) {
        auto samples = signal.getSamples();
        for (auto& s : samples) {
            s = clamp(s * 3.0, -threshold, threshold);
            s /= threshold;
        }
        signal.setSamples(samples);
    }

    void tremolo(AudioSignal& signal, double rate = 5.0, double depth = 0.5) {
        auto samples = signal.getSamples();
        int sr = signal.getSampleRate();
        for (size_t i = 0; i < samples.size(); i++) {
            double lfo = 1.0 - depth * (0.5 + 0.5 * sin(2.0 * M_PI * rate * i / sr));
            samples[i] *= lfo;
        }
        signal.setSamples(samples);
    }

    void setWetDry(double wd) { wetDry = clamp(wd, 0.0, 1.0); }
    double getWetDry() const { return wetDry; }
    void setDelayMs(double d) { delayMs = d; }
    double getDelayMs() const { return delayMs; }
};